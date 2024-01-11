/* EPICS device driver interface.
 *
 * This file implements generic EPICS device support.
 *
 * The following record types are supported:
 *      longin, longout, ai, ao, bi, bo, stringin, stringout, mbbi, mbbo,
 *      waveform. */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include <devSup.h>
#include <recSup.h>
#include <dbScan.h>
#include <epicsExport.h>
#include <initHooks.h>

#include <alarm.h>
#include <dbFldTypes.h>
#include <recGbl.h>
#include <dbCommon.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <waveformRecord.h>

#include <dbBase.h>
#include <dbAddr.h>
#include <dbAccessDefs.h>
#include <dbLock.h>

#include "error.h"
#include "hashtable.h"
#include "persistence_internal.h"
#include "epics_extra_internal.h"

#include "epics_device.h"


/* Epics processing return codes. */
#define EPICS_OK        0
#define EPICS_ERROR     1
#define NO_CONVERT      2       // Special code for ai/ao conversion


/* Maximum length of record prefix. */
#define MAX_NAME_PREFIX_COUNT       8
#define MAX_NAME_PREFIX_LENGTH      80
#define MAX_NAME_SEPARATOR_LENGTH   8


/****************************************************************************/
/*                   Core Record Publishing and Lookup                      */
/****************************************************************************/

static struct hash_table *hash_table = NULL;

/* Mutex used to initialise record if not specified in record initialiser. */
static pthread_mutex_t *default_mutex = NULL;


/* This is the core of the generic EPICS record implementation.  There are
 * essentially three underlying classes of record: IN records, OUT records and
 * WAVEFORM records, each with slightly different support. */
struct epics_record {
    char *key;                      // Name of lookup for record
    enum record_type record_type;
    const char *record_name;        // Full record name, once bound
    unsigned int max_length;        // Waveform length

    /* The following fields are shared between pairs of record classes. */
    IOSCANPVT ioscanpvt;            // Used for I/O intr enabled records
    bool ioscan_pending;            // Set for early record triggering
    bool persist;                   // Set for persistently written data
    enum epics_alarm_severity severity;    // Reported record status
    void *context;                  // Context for all user callbacks
    pthread_mutex_t *mutex;         // Lock for record processing

    /* This field is used for out and waveform records. */
    bool disable_write;             // Used for write_out_record

    /* The following fields are record class specific. */
    union {
        // IN record support
        struct {
            bool (*read)(void *context, void *result);
            struct timespec timestamp;  // Timestamp explicitly set
            bool set_time;              // Whether to use timestamp
        } in;
        // OUT record support
        struct {
            bool (*write)(void *context, void *value);
            bool (*init)(void *context, void *result);
            void *save_value;       // Used to restore after rejected write
        } out;
        // WAVEFORM record support
        struct {
            enum waveform_type field_type;
            void (*process)(void *context, void *array, unsigned int *length);
            void (*init)(void *context, void *array, unsigned int *length);
        } waveform;
    };
};


/* Generic argument types. */
_DECLARE_IN_ARGS_(in, void);
_DECLARE_OUT_ARGS_(out, void);


/* Returns the size of data to be reserved for the record_base::WriteData
 * field.  This is only used for output records. */
static size_t write_data_size(enum record_type record_type)
{
    switch (record_type)
    {
        case RECORD_TYPE_longout:   return sizeof(TYPEOF(longout));
        case RECORD_TYPE_ulongout:  return sizeof(TYPEOF(ulongout));
        case RECORD_TYPE_ao:        return sizeof(TYPEOF(ao));
        case RECORD_TYPE_bo:        return sizeof(TYPEOF(bo));
        case RECORD_TYPE_stringout: return sizeof(TYPEOF(stringout));
        case RECORD_TYPE_mbbo:      return sizeof(TYPEOF(mbbo));
        default: ASSERT_FAIL();
    }
}


/* The types used here must match the types used for record interfacing. */
static enum PERSISTENCE_TYPES record_type_to_persistence(
    enum record_type record_type)
{
    switch (record_type)
    {
        case RECORD_TYPE_longout:   return PERSISTENT_int;
        case RECORD_TYPE_ulongout:  return PERSISTENT_int;
        case RECORD_TYPE_ao:        return PERSISTENT_double;
        case RECORD_TYPE_bo:        return PERSISTENT_bool;
        case RECORD_TYPE_stringout: return PERSISTENT_string;
        case RECORD_TYPE_mbbo:      return PERSISTENT_short;
        default: ASSERT_FAIL();
    }
}

static enum PERSISTENCE_TYPES waveform_type_to_persistence(
    enum waveform_type waveform_type)
{
    switch (waveform_type)
    {
        case waveform_TYPE_char:    return PERSISTENT_char;
        case waveform_TYPE_short:   return PERSISTENT_short;
        case waveform_TYPE_int:     return PERSISTENT_int;
        case waveform_TYPE_float:   return PERSISTENT_float;
        case waveform_TYPE_double:  return PERSISTENT_double;
        default: ASSERT_FAIL();
    }
}


/* Converts record_type into printable name. */
static const char *get_type_name(enum record_type record_type)
{
    static const char *names[] = {
        "longin",       "longin",       "longout",      "longout",
        "ai",           "ao",           "bi",           "bo",
        "stringin",     "stringout",    "mbbi",         "mbbo",
        "waveform" };
    if (record_type < ARRAY_SIZE(names))
        return names[record_type];
    else
        return "(invalid)";
}


/* We handle errors by reporting them and then just dying.  A bit crude, but
 * mostly there's nothing to be done with the error. */
static void fail_on_error(error__t error)
{
    ASSERT_OK(!error_report(error));
}


bool format_epics_string(EPICS_STRING *s, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int length = vsnprintf(s->s, sizeof(s->s), format, args);
    va_end(args);
    return (size_t) length < sizeof(s->s);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                        Record name and key generation                     */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct name_prefix {
    unsigned int count;
    size_t length;
    char prefix[MAX_NAME_PREFIX_LENGTH];
    char separator[MAX_NAME_SEPARATOR_LENGTH];
    size_t offsets[MAX_NAME_PREFIX_COUNT];
};

static struct name_prefix name_prefix = {
    .count = 0,
    .length = 0,
    .separator = ":",
};


void set_record_name_separator(const char *separator)
{
    fail_on_error(
        TEST_OK_(strlen(separator) < sizeof(name_prefix.separator),
            "Separator \"%s\" too long", separator));
    strcpy(name_prefix.separator, separator);
}


void push_record_name_prefix(const char *prefix)
{
    size_t prefix_length = strlen(prefix);
    size_t separator_length = strlen(name_prefix.separator);
    size_t new_length = name_prefix.length + prefix_length + separator_length;

    fail_on_error(
        TEST_OK_(name_prefix.count < ARRAY_SIZE(name_prefix.offsets),
            "Too many record name prefixes specified")  ?:
        TEST_OK_(new_length < ARRAY_SIZE(name_prefix.prefix),
            "Record name prefix too long"));

    char *current_prefix = name_prefix.prefix;
    current_prefix += name_prefix.length;
    strcpy(current_prefix, prefix);
    current_prefix += prefix_length;
    strcpy(current_prefix, name_prefix.separator);
    name_prefix.offsets[name_prefix.count] = name_prefix.length;
    name_prefix.length = new_length;
    name_prefix.count += 1;
}


void pop_record_name_prefix(void)
{
    fail_on_error(TEST_OK_(name_prefix.count > 0,
        "No record name prefix to pop"));
    name_prefix.count -= 1;
    size_t new_length = name_prefix.offsets[name_prefix.count];
    name_prefix.prefix[new_length] = '\0';
    name_prefix.length = new_length;
}


/* Construct key by concatenating record_type and name. */
#define BUILD_KEY(key, name, record_type) \
    char key[strlen(name) + 20]; \
    sprintf(key, "%s:%s", get_type_name(record_type), name)

/* Constructs key with name including the name prefix. */
#define BUILD_KEY_PREFIX(key, name, record_type) \
    char key[name_prefix.length + strlen(name) + 20]; \
    sprintf(key, "%s:%s%s", get_type_name(record_type), \
        name_prefix.prefix, name)


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                          Record publishing API                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* For each of the three record classes (IN, OUT, WAVEFORM) we extract the
 * appropriate fields from the given arguments, which are guaranteed to be of
 * the correct type, and perform any extra initialisation. */

static void initialise_in_fields(
    struct epics_record *base, const struct record_args_in *in_args)
{
    base->in.set_time = in_args->set_time;
    base->in.read = in_args->read;
    base->max_length = 1;
    base->context = in_args->context;
    base->mutex = in_args->mutex ?: default_mutex;
    if (in_args->io_intr)
        scanIoInit(&base->ioscanpvt);
}

static void initialise_out_fields(
    struct epics_record *base, const struct record_args_out *out_args)
{
    base->out.write = out_args->write;
    base->out.init = out_args->init;
    base->out.save_value = malloc(write_data_size(base->record_type));
    base->max_length = 1;
    base->context = out_args->context;
    base->mutex = out_args->mutex ?: default_mutex;
    base->persist = out_args->persist;
    if (base->persist)
        create_persistent_waveform(base->key,
            record_type_to_persistence(base->record_type), 1);
}

static void initialise_waveform_fields(
    struct epics_record *base, const struct waveform_args_void *waveform_args)
{
    base->waveform.field_type = waveform_args->field_type;
    base->waveform.process = waveform_args->process;
    base->waveform.init = waveform_args->init;
    base->max_length = waveform_args->max_length;
    base->context = waveform_args->context;
    base->mutex = waveform_args->mutex ?: default_mutex;
    base->persist = waveform_args->persist;
    if (base->persist)
        create_persistent_waveform(base->key,
            waveform_type_to_persistence(waveform_args->field_type),
            waveform_args->max_length);
    if (waveform_args->io_intr)
        scanIoInit(&base->ioscanpvt);
}


/* Publishes record of given type with given name as specified by record type
 * specific arguments. */
struct epics_record *publish_epics_record(
    enum record_type record_type, const char *name, const void *args)
{
    struct epics_record *base = malloc(sizeof(struct epics_record));

    /* Construct lookup key of form <record-type>:<name>. */
    BUILD_KEY_PREFIX(key, name, record_type);
    base->record_type = record_type;
    base->key = strdup(key);

    base->record_name = NULL;
    base->ioscanpvt = NULL;
    base->ioscan_pending = false;
    base->persist = false;
    base->severity = (enum epics_alarm_severity) epicsSevNone;
    base->disable_write = false;

    switch (record_type)
    {
        case RECORD_TYPE_longin:    case RECORD_TYPE_ulongin:
        case RECORD_TYPE_ai:        case RECORD_TYPE_bi:
        case RECORD_TYPE_stringin:  case RECORD_TYPE_mbbi:
            initialise_in_fields(base, args);
            break;
        case RECORD_TYPE_longout:   case RECORD_TYPE_ulongout:
        case RECORD_TYPE_ao:        case RECORD_TYPE_bo:
        case RECORD_TYPE_stringout: case RECORD_TYPE_mbbo:
            initialise_out_fields(base, args);
            break;
        case RECORD_TYPE_waveform:
            initialise_waveform_fields(base, args);
            break;
    }

    void *old_key = hash_table_insert(hash_table, base->key, base);
    fail_on_error(TEST_OK_(!old_key, "Record \"%s\" already exists!", key));
    return base;
}


struct epics_record *lookup_epics_record(
    enum record_type record_type, const char *name)
{
    BUILD_KEY(key, name, record_type);
    struct epics_record *result = hash_table_lookup(hash_table, key);
    fail_on_error(TEST_OK_(result, "Lookup %s failed", key));
    return result;
}


/* Checks whether the given record is an IN record for validating the trigger
 * and other update methods. */
static bool is_in_or_waveform(struct epics_record *base)
{
    switch (base->record_type)
    {
        case RECORD_TYPE_longin:    case RECORD_TYPE_ulongin:
        case RECORD_TYPE_ai:        case RECORD_TYPE_bi:
        case RECORD_TYPE_stringin:  case RECORD_TYPE_mbbi:
        case RECORD_TYPE_waveform:
            return true;
        default:
            return false;
    }
}


void set_record_severity(
    struct epics_record *base, enum epics_alarm_severity severity)
{
    ASSERT_OK(is_in_or_waveform(base));

    base->severity = severity;
}


void set_record_timestamp(
    struct epics_record *base, const struct timespec *timestamp)
{
    ASSERT_OK(is_in_or_waveform(base));
    ASSERT_OK(base->in.set_time);

    base->in.timestamp = *timestamp;
}


void trigger_record(struct epics_record *base)
{
    ASSERT_OK(is_in_or_waveform(base));
    ASSERT_OK(base->ioscanpvt);

    base->ioscan_pending = true;
    scanIoRequest(base->ioscanpvt);
}


static void init_hook(initHookState state)
{
    if (state == initHookAfterInterruptAccept)
    {
        /* Now we have to do something rather dirty.  It turns out that any
         * trigger_record events signalled before this point have simply been
         * ignored.  We'll walk the complete record database and retrigger them
         * now.  Fortunately we'll only ever get this event the once. */
        const void *key;
        void *value;
        for (int ix = 0; hash_table_walk(hash_table, &ix, &key, &value); )
        {
            const struct epics_record *base = value;
            if (base->ioscan_pending  &&  base->ioscanpvt)
                scanIoRequest(base->ioscanpvt);
        }
    }
}

error__t initialise_epics_device(void)
{
    if (hash_table == NULL)
    {
        hash_table = hash_table_create(false);
        initHookRegister(init_hook);
        initialise_epics_extra();
        initialise_persistent_state();
    }
    return ERROR_OK;
}


unsigned int check_unused_record_bindings(bool verbose)
{
    int hash_ix = 0;
    void *value;
    unsigned int count = 0;
    while (hash_table_walk(hash_table, &hash_ix, NULL, &value))
    {
        struct epics_record *record = value;
        if (!record->record_name)
        {
            count += 1;
            if (verbose)
                printf("%s not bound\n", record->key);
        }
    }
    return count;
}


pthread_mutex_t *set_default_epics_device_mutex(pthread_mutex_t *mutex)
{
    pthread_mutex_t *old_mutex = default_mutex;
    default_mutex = mutex;
    return old_mutex;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                 Support for direct writing to OUT records                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Checks whether the given record is an OUT record for validating write. */
static bool is_out_record(enum record_type record_type)
{
    switch (record_type)
    {
        case RECORD_TYPE_longout:   case RECORD_TYPE_ulongout:
        case RECORD_TYPE_ao:        case RECORD_TYPE_bo:
        case RECORD_TYPE_stringout: case RECORD_TYPE_mbbo:
            return true;
        default:
            return false;
    }
}


/* Returns DBR code associated with record type.  This needs to be compatible
 * with the data type returned by TYPEOF(record). */
static short record_type_dbr(enum record_type record_type)
{
    switch (record_type)
    {
        case RECORD_TYPE_longin:    return DBR_LONG;
        case RECORD_TYPE_ulongin:   return DBR_LONG;
        case RECORD_TYPE_ai:        return DBR_DOUBLE;
        case RECORD_TYPE_bi:        return DBR_CHAR;
        case RECORD_TYPE_stringin:  return DBR_STRING;
        case RECORD_TYPE_mbbi:      return DBR_SHORT;

        case RECORD_TYPE_longout:   return DBR_LONG;
        case RECORD_TYPE_ulongout:  return DBR_LONG;
        case RECORD_TYPE_ao:        return DBR_DOUBLE;
        case RECORD_TYPE_bo:        return DBR_CHAR;
        case RECORD_TYPE_stringout: return DBR_STRING;
        case RECORD_TYPE_mbbo:      return DBR_SHORT;

        default: ASSERT_FAIL();
    }
}

static short waveform_type_dbr(enum waveform_type waveform_type)
{
    switch (waveform_type)
    {
        case waveform_TYPE_char:    return DBR_CHAR;
        case waveform_TYPE_short:   return DBR_SHORT;
        case waveform_TYPE_int:     return DBR_LONG;
        case waveform_TYPE_float:   return DBR_FLOAT;
        case waveform_TYPE_double:  return DBR_DOUBLE;
        default: ASSERT_FAIL();
    }
}


/* Used to convert an internal record name to the associated dbaddr value and
 * performs sanity validation.  If this fails we just die! */
static void record_to_dbaddr(
    enum record_type record_type, struct epics_record *record,
    unsigned int length, struct dbAddr *dbaddr)
{
    fail_on_error(
        TEST_OK_(record->record_type == record_type,
            "%s is %s (%d), not %s (%d)", record->key,
            get_type_name(record->record_type), record->record_type,
            get_type_name(record_type), record_type)  ?:
        TEST_OK_(length <= record->max_length, "Length request too long")  ?:

        // The writing API needs a dbAddr to describe the target
        TEST_OK(record->record_name)  ?:
        TEST_OK_(dbNameToAddr(record->record_name, dbaddr) == 0,
            "Unable to find record %s", record->record_name));
}


/* Wrapper around dbPutField to write value to EPICS database. */
static bool _write_out_record(
    enum record_type record_type, struct epics_record *record,
    short dbr_type, const void *value, unsigned int length, bool process)
{
    struct dbAddr dbaddr;
    record_to_dbaddr(record_type, record, length, &dbaddr);

    /* Write the desired value under the database lock: we disable writing if
     * processing was not requested. */
    dbScanLock(dbaddr.precord);
    record->disable_write = !process;
    bool put_ok = dbPutField(&dbaddr, dbr_type, value, (long) length) == 0;
    record->disable_write = false;
    dbScanUnlock(dbaddr.precord);
    return put_ok;
}

bool _write_out_record_value(
    enum record_type record_type, struct epics_record *record,
    const void *value, bool process)
{
    // Validate the arguments to prevent disaster
    fail_on_error(TEST_OK_(
        is_out_record(record_type),
        "%s is not an output type", get_type_name(record_type)));
    return _write_out_record(
        record_type, record, record_type_dbr(record_type), value, 1, process);
}

bool _write_out_record_waveform(
    enum waveform_type waveform_type, struct epics_record *record,
    const void *value, unsigned int length, bool process)
{
    return _write_out_record(
        RECORD_TYPE_waveform, record, waveform_type_dbr(waveform_type),
        value, length, process);
}


/* Wrapper around dbGetField to read value from EPICS. */
static void _read_record(
    enum record_type record_type, struct epics_record *record,
    short dbr_type, void *value, unsigned int length)
{
    long get_length = (long) length;
    struct dbAddr dbaddr;
    record_to_dbaddr(record_type, record, length, &dbaddr);
    fail_on_error(
        TEST_OK(dbGetField(
            &dbaddr, dbr_type, value, NULL, &get_length, NULL) == 0)  ?:
        TEST_OK_((unsigned int) get_length == length,
            "Failed to get all values"));
}

void _read_record_value(
    enum record_type record_type, struct epics_record *record, void *value)
{
    _read_record(record_type, record, record_type_dbr(record_type), value, 1);
}

void _read_record_waveform(
    enum waveform_type waveform_type, struct epics_record *record,
    void *value, unsigned int length)
{
    _read_record(
        RECORD_TYPE_waveform, record, waveform_type_dbr(waveform_type),
        value, length);
}



/*****************************************************************************/
/*                                                                           */
/*                   Specialised Record Support Methods                      */
/*                                                                           */
/*****************************************************************************/

/* A number of tricksy functions designed to support very simple and uniform
 * access on top of the rather more general framework developed here. */

#define DEFINE_READ_VAR(record) \
    _DECLARE_READ_VAR(record) \
    { \
        const TYPEOF(record) *variable = context; \
        *value = *variable; \
        return true; \
    }

#define DEFINE_WRITE_VAR(record) \
    _DECLARE_WRITE_VAR(record) \
    { \
        TYPEOF(record) *variable = context; \
        *variable = *value; \
        return true; \
    }

#define DEFINE_READER(record) \
    _DECLARE_READER(record) \
    { \
        TYPEOF(record) (*reader)(void) = context; \
        *value = reader(); \
        return true; \
    }

#define DEFINE_WRITER(record) \
    _DECLARE_WRITER(record) \
    { \
        void (*writer)(TYPEOF(record)) = context; \
        writer(*value); \
        return true; \
    }

#define DEFINE_WRITER_B(record) \
    _DECLARE_WRITER_B(record) \
    { \
        bool (*writer)(TYPEOF(record)) = context; \
        return writer(*value); \
    }

_FOR_IN_RECORDS(DEFINE_READ_VAR,)
_FOR_OUT_RECORDS(DEFINE_READ_VAR,)
_FOR_OUT_RECORDS(DEFINE_WRITE_VAR,)
_FOR_IN_RECORDS(DEFINE_READER,)
_FOR_OUT_RECORDS(DEFINE_WRITER,)
_FOR_OUT_RECORDS(DEFINE_WRITER_B,)

bool _publish_trigger_bi(void *context, bool *value)
{
    *value = true;
    return true;
}

bool _publish_action_bo(void *context, bool *value)
{
    void (*action)(void) = context;
    action();
    return true;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Waveform adapters. */

/* These adapters all implement simplified waveform access, reading or writing a
 * fixed length variable, or calling an action (with uncomitted direction) with
 * a fixed length waveform.  In all cases the true length of the waveform is
 * written back as part of the processing.  This turns out to be sensible, as a
 * caput of a partial waveform leaves the rest of the waveform undisturbed, so
 * can sensibly be treated as an action on the full waveform. */

struct waveform_context {
    unsigned int size;
    unsigned int max_length;
    unsigned int *current_length;
    void *context;
};

void *_make_waveform_context(
    unsigned int size, unsigned int max_length, unsigned int *current_length,
    void *context)
{
    struct waveform_context *info = malloc(sizeof(struct waveform_context));
    *info = (struct waveform_context) {
        .size = size,
        .max_length = max_length,
        .current_length = current_length,
        .context = context,
    };
    return info;
}

void _publish_waveform_write_var(
    void *context, void *array, unsigned int *length)
{
    struct waveform_context *info = context;
    memcpy(info->context, array, info->max_length * info->size);
    /* If we can do something with the new length then update our current record
     * of the waveform length, otherwise reset the external length back to
     * maximum length. */
    if (info->current_length)
        *info->current_length = *length;
    else
        *length = info->max_length;
}

void _publish_waveform_read_var(
    void *context, void *array, unsigned int *length)
{
    struct waveform_context *info = context;
    memcpy(array, info->context, info->max_length * info->size);
    if (info->current_length)
        *length = MIN(*info->current_length, info->max_length);
    else
        *length = info->max_length;
}

void _publish_waveform_action(void *context, void *array, unsigned int *length)
{
    struct waveform_context *info = context;
    void (*action)(void *) = info->context;
    action(array);
    *length = info->max_length;
}


/*****************************************************************************/
/*                                                                           */
/*                   Record Device Support Implementation                    */
/*                                                                           */
/*****************************************************************************/

static __thread struct epics_record *current_epics_record = NULL;

/* Need to allow for possibility of recursion during record procession. */
#define PUSH_CURRENT_RECORD(record) \
    struct epics_record *_saved_record = current_epics_record; \
    current_epics_record = record
#define POP_CURRENT_RECORD(record) \
    current_epics_record = _saved_record


struct epics_record *get_current_epics_record(void)
{
    return current_epics_record;
}


/* Looks up the record and records it in dpvt if found.  Also take care to
 * ensure that only one EPICS record binds to any one instance. */
static error__t init_record_common(
    dbCommon *pr, const char *name, enum record_type record_type)
{
    BUILD_KEY(key, name, record_type);
    struct epics_record *base = hash_table_lookup(hash_table, key);
    return
        TEST_OK_(base, "No handler found for %s", key)  ?:
        TEST_OK_(base->record_name == NULL,
            "%s already bound to %s", key, base->record_name)  ?:
        DO(base->record_name = pr->name; pr->dpvt = base)  ?:
        TEST_OK_(
            (pr->scan == menuScanI_O_Intr) == (base->ioscanpvt != NULL),
            "%s has inconsistent scan menu (%d) and ioscanpvt (%p)",
            key, pr->scan, base->ioscanpvt);
}


/* Common I/O Intr scanning support. */
static long get_ioint_common(int cmd, dbCommon *pr, IOSCANPVT *ioscanpvt)
{
    struct epics_record *base = pr->dpvt;
    if (base == NULL)
        return EPICS_ERROR;
    else
    {
        *ioscanpvt = base->ioscanpvt;
        return EPICS_OK;
    }
}


/* The following two macros are designed to adapt between the external
 * representation of PV data and the internal representation when they're not
 * pointer assignment compatible.  Unfortunately this is a very hacky solution
 * (for instance, call is forced to return a bool), and in fact the only record
 * type we need to adapt is bi/bo to bool. */
#define SIMPLE_ADAPTER(call, type, value, args...) \
    ( { \
        COMPILE_ASSERT(sizeof(value) == sizeof(type)); \
        call(args, &value); \
    } )

#define COPY_ADAPTER(call, type, value, args...) \
    ( { \
        type __value = value; \
        bool __ok = call(args, &__value); \
        value = __value; \
        __ok; \
    } )

#define STRING_ADAPTER(call, type, value, args...) \
    call(args, CAST_FROM_TO(char *, EPICS_STRING *, value))


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                          Input record processing.                         */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static error__t init_in_record(dbCommon *pr)
{
    struct epics_record *base = pr->dpvt;
    return TEST_OK_(
        base->in.set_time == (pr->tse == epicsTimeEventDeviceTime),
        "Inconsistent timestamping (%d/%d) for %s",
            base->in.set_time, pr->tse, base->key);
}

static bool process_in_record(dbCommon *pr, void *result)
{
    struct epics_record *base = pr->dpvt;
    if (base == NULL)
        return false;

    if (base->mutex)  pthread_mutex_lock(base->mutex);
    PUSH_CURRENT_RECORD(base);
    bool ok = base->in.read(base->context, result);
    POP_CURRENT_RECORD();
    if (base->mutex)  pthread_mutex_unlock(base->mutex);

    recGblSetSevr(pr, READ_ALARM, base->severity);
    if (base->in.set_time)
        epicsTimeFromTimespec(&pr->time, &base->in.timestamp);
    pr->udf = !ok;
    return ok;
}

#define DEFINE_PROCESS_IN(record, PROC_OK, ADAPTER) \
    static long read_##record(record##Record *pr) \
    { \
        bool ok = ADAPTER(process_in_record, \
            TYPEOF(record), pr->val, (dbCommon *) pr); \
        return ok ? PROC_OK : EPICS_ERROR; \
    }

#define DEFINE_INIT_IN(record) \
    static long init_record_##record(record##Record *pr) \
    { \
        error__t error = \
            init_record_common((dbCommon *) pr, \
                pr->inp.value.instio.string, RECORD_TYPE_##record)  ?: \
            init_in_record((dbCommon *) pr); \
        return error_report(error) ? EPICS_ERROR : EPICS_OK; \
    }

#define DEFINE_DEFAULT_IN(record, PROC_OK, ADAPTER) \
    DEFINE_INIT_IN(record) \
    DEFINE_PROCESS_IN(record, PROC_OK, ADAPTER)



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                        Output record processing.                          */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Perform a simulation of record processing event, including a synthetic
 * initial timestamp.  This is useful after initialising an output record with
 * driver provided values. */
static void post_init_process(dbCommon *pr)
{
    pr->udf = false;
    recGblResetAlarms(pr);
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    epicsTimeFromTimespec(&pr->time, &timestamp);
}


/* Common out record initialisation: we look in a couple of places for an
 * initial value.  If there's a persistent value stored used that, otherwise
 * call init if defined.  This is saved so we can restore rejected writes. */
static bool init_out_record(dbCommon *pr, unsigned int value_size, void *result)
{
    struct epics_record *base = pr->dpvt;
    PUSH_CURRENT_RECORD(base);
    bool read_ok =
        (base->persist   &&  read_persistent_variable(base->key, result))  ||
        (base->out.init  &&  base->out.init(base->context, result));
    POP_CURRENT_RECORD();
    if (read_ok)
        post_init_process(pr);
    else
        memset(result, 0, value_size);
    memcpy(base->out.save_value, result, value_size);

    return true;
}


/* Common out record processing.  If writing fails then restore saved value,
 * otherwise maintain saved and persistent settings. */
static bool process_out_record(
    dbCommon *pr, unsigned int value_size, void *result)
{
    struct epics_record *base = pr->dpvt;
    if (base == NULL)
        return false;

    bool write_ok = base->disable_write;
    if (!base->disable_write)
    {
        if (base->mutex)  pthread_mutex_lock(base->mutex);
        PUSH_CURRENT_RECORD(base);
        write_ok = base->out.write(base->context, result);
        POP_CURRENT_RECORD();
        if (base->mutex)  pthread_mutex_unlock(base->mutex);
    }

    if (write_ok)
    {
        /* On successful update take a record (in case we have to revert) and
         * update the persistent record. */
        memcpy(base->out.save_value, result, value_size);
        if (base->persist)
            write_persistent_variable(base->key, result);
        return true;
    }
    else
    {
        /* On a rejected update restore the value from backing store. */
        memcpy(result, base->out.save_value, value_size);
        return false;
    }
}


#define DEFINE_PROCESS_OUT(record, ADAPTER) \
    static long write_##record(record##Record *pr) \
    { \
        bool ok = ADAPTER(process_out_record, \
            TYPEOF(record), pr->val, \
            (dbCommon *) pr, sizeof(TYPEOF(record))); \
        return ok ? EPICS_OK : EPICS_ERROR; \
    }

#define DEFINE_INIT_OUT(record, INIT_OK, ADAPTER, MLST) \
    static long init_record_##record(record##Record *pr) \
    { \
        error__t error = init_record_common((dbCommon *) pr, \
            pr->out.value.instio.string, RECORD_TYPE_##record); \
        if (!error) \
        { \
            ADAPTER(init_out_record, \
                TYPEOF(record), pr->val, \
                (dbCommon *) pr, sizeof(TYPEOF(record))); \
            MLST(pr->mlst = (typeof(pr->mlst)) pr->val); \
        } \
        return error_report(error) ? EPICS_ERROR : INIT_OK; \
    }


#define do_MLST(arg)    arg
#define no_MLST(arg)

#define DEFINE_DEFAULT_OUT(record, INIT_OK, ADAPTER, MLST) \
    DEFINE_INIT_OUT(record, INIT_OK, ADAPTER, MLST) \
    DEFINE_PROCESS_OUT(record, ADAPTER)



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                    IN/OUT Device Driver Implementations                   */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DEFINE_IN_OUT(in, out, CONVERT, ADAPTER, MLST) \
    DEFINE_DEFAULT_IN(in,   CONVERT,  ADAPTER) \
    DEFINE_DEFAULT_OUT(out, CONVERT,  ADAPTER, MLST)

/* Mostly we can use simple boilerplate for the process routines. */
DEFINE_IN_OUT(longin,   longout,   EPICS_OK,   SIMPLE_ADAPTER, do_MLST)
DEFINE_IN_OUT(ai,       ao,        NO_CONVERT, SIMPLE_ADAPTER, do_MLST)
DEFINE_IN_OUT(bi,       bo,        NO_CONVERT, COPY_ADAPTER,   do_MLST)
DEFINE_IN_OUT(stringin, stringout, EPICS_OK,   STRING_ADAPTER, no_MLST)
DEFINE_IN_OUT(mbbi,     mbbo,      NO_CONVERT, SIMPLE_ADAPTER, do_MLST)

/* Also need dummy special_linconv routines for ai and ao. */
static long linconv_ai(aiRecord *pr, int cmd) { return EPICS_OK; }
static long linconv_ao(aoRecord *pr, int cmd) { return EPICS_OK; }



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                       Waveform Record Implementation                      */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Reading a waveform doesn't fit into the fairly uniform pattern established
 * for the other record types. */


/* Routine to validate record type: ensure that we don't mismatch the record
 * declarations in the code and in the database. */
static error__t check_waveform_type(
    waveformRecord *pr, struct epics_record *base)
{
    /* Validate that the names we're using for the int types match what EPICS
     * expects.  These correspond to the names used in enum waveform_type. */
    COMPILE_ASSERT(sizeof(short) == sizeof(epicsInt16));
    COMPILE_ASSERT(sizeof(int) == sizeof(epicsInt32));

    epicsEnum16 expected = DBF_NOACCESS;
    switch (base->waveform.field_type)
    {
        case waveform_TYPE_void:    break;
        case waveform_TYPE_char:    expected = DBF_CHAR;    break;
        case waveform_TYPE_short:   expected = DBF_SHORT;   break;
        case waveform_TYPE_int:     expected = DBF_LONG;    break;
        case waveform_TYPE_float:   expected = DBF_FLOAT;   break;
        case waveform_TYPE_double:  expected = DBF_DOUBLE;  break;
    }
    return
        TEST_OK_(pr->ftvl == expected,
            "Array %s.FTVL mismatch %d != %d (%d)",
            base->key, pr->ftvl, expected, base->waveform.field_type)  ?:
        TEST_OK_(pr->nelm == base->max_length,
            "Array %s wrong length, %d != %u",
            base->key, (int) pr->nelm, base->max_length);
}


/* After binding to the record base try to load from persistence storage; if
 * that fails, try for an (optional) init method. */
static long init_record_waveform(waveformRecord *pr)
{
    error__t error =
        init_record_common(
            (dbCommon *) pr, pr->inp.value.instio.string,
            RECORD_TYPE_waveform)  ?:
        check_waveform_type(pr, pr->dpvt);
    if (error_report(error))
    {
        pr->dpvt = NULL;
        return EPICS_ERROR;
    }

    struct epics_record *base = pr->dpvt;
    unsigned int nord = 0;
    bool read_ok =
        base->persist  &&
        read_persistent_waveform(base->key, pr->bptr, &nord);
    if (!read_ok  &&  base->waveform.init)
    {
        nord = pr->nelm;
        base->waveform.init(base->context, pr->bptr, &nord);
        read_ok = true;
    }
    pr->nord = nord;
    pr->udf = !read_ok;

    post_init_process((dbCommon *) pr);
    return EPICS_OK;
}


static long process_waveform(waveformRecord *pr)
{
    struct epics_record *base = pr->dpvt;
    if (base == NULL)
        return EPICS_ERROR;

    if (!base->disable_write)
    {
        unsigned int nord = pr->nord;
        if (base->mutex)  pthread_mutex_lock(base->mutex);
        PUSH_CURRENT_RECORD(base);
        base->waveform.process(base->context, pr->bptr, &nord);
        POP_CURRENT_RECORD();
        if (base->mutex)  pthread_mutex_unlock(base->mutex);
        pr->nord = nord;
    }

    if (base->persist)
        write_persistent_waveform(base->key, pr->bptr, pr->nord);

    recGblSetSevr(pr, READ_ALARM, base->severity);

    /* Note that waveform record support ignores the return code! */
    return EPICS_OK;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                          Device Driver Definitions                        */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DEFINE_DEVICE(record, length, args...) \
    static struct record##Device record##_epics_device = \
    { \
        length, \
        NULL, \
        NULL, \
        init_record_##record, \
        get_ioint_common, \
        args \
    }; \
    epicsExportAddress(dset, record##_epics_device)

#include "recordDevice.h"

/* The epicsExportAddress macro used in the definitions below casts a structure
 * pointer via (char*) and thus generates a cast alignment error.  We want to
 * just ignore this here. */
#pragma GCC diagnostic ignored "-Wcast-align"
DEFINE_DEVICE(longin,    5, read_longin);
DEFINE_DEVICE(longout,   5, write_longout);
DEFINE_DEVICE(ai,        6, read_ai,  linconv_ai);
DEFINE_DEVICE(ao,        6, write_ao, linconv_ao);
DEFINE_DEVICE(bi,        5, read_bi);
DEFINE_DEVICE(bo,        5, write_bo);
DEFINE_DEVICE(stringin,  5, read_stringin);
DEFINE_DEVICE(stringout, 5, write_stringout);
DEFINE_DEVICE(mbbi,      5, read_mbbi);
DEFINE_DEVICE(mbbo,      5, write_mbbo);
DEFINE_DEVICE(waveform,  5, process_waveform);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                          Utility Functions                                */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void dump_epics_device_db(FILE *output)
{
    const void *key;
    void *value;
    for (int ix = 0; hash_table_walk(hash_table, &ix, &key, &value);)
    {
        const struct epics_record *base = value;
        fprintf(output, "\t%s\n", base->key);
    }
}
