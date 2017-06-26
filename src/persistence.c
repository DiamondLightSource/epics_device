/* Implemention of persistent state. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "error.h"
#include "hashtable.h"

#include "persistence_internal.h"
#include "persistence.h"



/* Used to store information about individual persistent variables. */
struct persistent_variable {
    const struct persistent_action *action;
    const char *name;
    size_t max_length;
    size_t length;
    char variable[0];
};


/* Used to implement core persistent actions, essentially converting values to
 * and from external strings. */
struct persistent_action {
    size_t size;
    /* Write value to output buffer (which must be long enough!), returns number
     * of characters written. */
    int (*write)(FILE *out, const void *variable);
    /* Reads value from given character buffer, advancing the character pointer
     * past the characters read, returns false and generates an error message if
     * there is a parsing error. */
    bool (*read)(const char **in, void *variable);
};



/******************************************************************************/
/* Reading and writing basic values. */

#define EPICS_STRING_LENGTH     40

#define DEFINE_WRITE(type, format) \
    static int write_##type(FILE *out, const void *variable) \
    { \
        type value; \
        memcpy(&value, variable, sizeof(type)); \
        return fprintf(out, format, value); \
    }

DEFINE_WRITE(int8_t, "%d")
DEFINE_WRITE(int16_t, "%d")
DEFINE_WRITE(int32_t, "%d")
DEFINE_WRITE(float, "%.8g")
DEFINE_WRITE(double, "%.17g")


static bool check_number(const char *start, const char *end)
{
    return TEST_OK_(end > start  &&  errno == 0, "Error converting number");
}



#define DEFINE_READ_NUM(type, convert, extra...) \
    static bool read_##type(const char **string, void *variable) \
    { \
        errno = 0; \
        const char *start = *string; \
        char *end; \
        type result = (type) convert(start, &end, ##extra); \
        memcpy(variable, &result, sizeof(type)); \
        *string = end; \
        return check_number(start, *string); \
    }

DEFINE_READ_NUM(int8_t, strtol, 10)
DEFINE_READ_NUM(int16_t, strtol, 10)
DEFINE_READ_NUM(int32_t, strtol, 10)
DEFINE_READ_NUM(float, strtof)
DEFINE_READ_NUM(double, strtod)


static int write_bool(FILE *out, const void *variable)
{
    bool value = *(const bool *) variable;
    fputc(value ? 'Y' : 'N', out);
    return 1;
}

/* Allow Y or 1 for true, N or 0 for false, though we only write Y/N. */
static bool read_bool(const char **in, void *variable)
{
    char ch = *(*in)++;
    *(bool *) variable = ch == 'Y'  ||  ch == '1';
    return TEST_NULL_(strchr("YN10", ch), "Invalid boolean value");
}


/* We go for the simplest possible escaping: octal escape for everything.  Alas,
 * this can quadruple the size of the output to 160 chars. */
static int write_string(FILE *out, const void *variable)
{
    int length = 2;      // Account for enclosing quotes
    const char *string = variable;
    fputc('"', out);
    for (int i = 0; i < EPICS_STRING_LENGTH; i ++)
    {
        char ch = string[i];
        if (ch == '\0')
            break;
        else if (' ' <= ch  &&  ch <= '~'  &&  ch != '"'  &&  ch != '\\')
        {
            fputc(ch, out);
            length += 1;
        }
        else
        {
            fprintf(out, "\\%03o", (unsigned char) ch);
            length += 4;
        }
    }
    fputc('"', out);
    return length;
}

/* Parses three octal digits as part of an escape sequence. */
static bool parse_octal(const char **in, char *result)
{
    unsigned int value = 0;
    bool ok = true;
    for (int i = 0; ok  &&  i < 3; i ++)
    {
        unsigned int ch = (unsigned int) *(*in)++;
        ok = TEST_OK_('0' <= ch  &&  ch <= '7', "Expected octal digit");
        value = (value << 3) + (ch - '0');
    }
    *result = (char) value;
    return ok;
}

/* We go for the most witless string parsing possible, must be double quoted,
 * and we only recognise octal character escapes. */
static bool read_string(const char **in, void *variable)
{
    char *string = variable;
    memset(variable, 0, EPICS_STRING_LENGTH);
    bool ok = TEST_OK_(*(*in)++ == '"', "Expected quoted string");
    for (int i = 0; ok  &&  i < EPICS_STRING_LENGTH; i ++)
    {
        char ch = *(*in)++;
        if (ch == '"')
            return true;
        else if (ch == '\\')
            ok = parse_octal(in, &string[i]);
        else
        {
            string[i] = ch;
            ok = TEST_OK_(' ' <= ch  &&  ch <= '~', "Invalid string character");
        }
    }
    return ok  &&  TEST_OK_(*(*in)++ == '"', "Missing closing quote");
}


/* Note that this table is indexed by PERSISTENCE_TYPES, so any changes in one
 * must be reflected in the other. */
static const struct persistent_action persistent_actions[] = {
    { sizeof(bool),         write_bool,     read_bool },
    { sizeof(int8_t),       write_int8_t,   read_int8_t },
    { sizeof(int16_t),      write_int16_t,  read_int16_t },
    { sizeof(int32_t),      write_int32_t,  read_int32_t },
    { sizeof(float),        write_float,    read_float },
    { sizeof(double),       write_double,   read_double },
    { EPICS_STRING_LENGTH,  write_string,   read_string },
};


/******************************************************************************/

/* Lookup table of persistent variables. */
static struct hash_table *variable_table;
/* Flag set if persistent state needs to be written to disk. */
static bool persistence_dirty = false;
/* Persistence loaded from and written to this file. */
static const char *state_filename = NULL;
/* How long to wait between persistence wakeups. */
static int persistence_interval;

/* To ensure state is updated in a timely way we have a background thread
 * responsible for this. */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t psignal = PTHREAD_COND_INITIALIZER;
/* Used to signal thread termination. */
static bool thread_running = true;
/* Thread handle used for shutdown. */
static pthread_t persistence_thread_id;

#define LOCK()      pthread_mutex_lock(&mutex);
#define UNLOCK()    pthread_mutex_unlock(&mutex);



/* Creates new persistent variable. */
void create_persistent_waveform(
    const char *name, enum PERSISTENCE_TYPES type, size_t max_length)
{
    /* If you try to create a persistent PV without having first initialised the
     * persistence layer then you'll get this error. */
    ASSERT_NULL(variable_table);

    const struct persistent_action *action = &persistent_actions[type];
    struct persistent_variable *persistence =
        malloc(sizeof(struct persistent_variable) + max_length * action->size);
    persistence->action = action;
    persistence->name = strdup(name);
    persistence->max_length = max_length;
    persistence->length = 0;

    LOCK();
    hash_table_insert(variable_table, persistence->name, persistence);
    UNLOCK();
}


static struct persistent_variable *lookup_persistence(const char *name)
{
    struct persistent_variable *persistence =
        hash_table_lookup(variable_table, name);
    if (persistence == NULL)
        print_error("Persistent variable %s not found", name);
    return persistence;
}


/* Updates variable from value stored on disk. */
bool read_persistent_waveform(const char *name, void *variable, size_t *length)
{
    LOCK();
    struct persistent_variable *persistence = lookup_persistence(name);
    bool ok = persistence != NULL  &&  persistence->length > 0;
    if (ok)
    {
        memcpy(variable, persistence->variable,
            persistence->length * persistence->action->size);
        *length = persistence->length;
    }
    UNLOCK();
    return ok;
}

bool read_persistent_variable(const char *name, void *variable)
{
    size_t length;
    return
        read_persistent_waveform(name, variable, &length)  &&
        TEST_OK_(length == 1,
            "Persistent variable %s length mismatch: %zd != 1",
            name, length);
}


/* Writes value to persistent variable. */
void write_persistent_waveform(
    const char *name, const void *value, size_t length)
{
    LOCK();
    struct persistent_variable *persistence = lookup_persistence(name);
    if (persistence != NULL)
    {
        /* Don't force a write of the persistence file if nothing has actually
         * changed. */
        size_t size = length * persistence->action->size;
        persistence_dirty =
            persistence_dirty  ||
            persistence->length != length  ||
            memcmp(persistence->variable, value, size);

        persistence->length = length;
        memcpy(persistence->variable, value, size);
    }
    UNLOCK();
}

void write_persistent_variable(const char *name, const void *value)
{
    write_persistent_waveform(name, value, 1);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Reading state file. */

#define READ_BUFFER_SIZE    1024

struct line_buffer {
    FILE *file;
    int line_number;
    char line[READ_BUFFER_SIZE];
};


/* Reads one line into the line buffer, returning false if an error is detected
 * and setting *eof accordingly.  Also ensures that we've read a complete
 * newline terminated line.  This will also fail if the input file doesn't end
 * with a newline.  The terminating newline character is then deleted. */
static bool read_line(struct line_buffer *line, bool *eof)
{
    errno = 0;
    *eof = fgets(line->line, READ_BUFFER_SIZE, line->file) == NULL;
    if (*eof)
        return TEST_OK_(errno == 0, "Error reading state file");
    else
    {
        size_t len = strlen(line->line);
        line->line_number += 1;
        return
            TEST_OK_(len > 0  &&  line->line[len - 1] == '\n',
                "Line %d truncated?", line->line_number)  &&
            DO(line->line[len - 1] = '\0');
    }
}


/* Skips leading whitespace and refills the line buffer if a line continuation
 * character is encountered. */
static bool fill_line_buffer(struct line_buffer *line, const char **cursor)
{
    while (**cursor == ' ')
        *cursor += 1;
    bool ok = true;
    if (**cursor == '\\')
    {
        bool eof;
        ok = read_line(line, &eof)  &&
            TEST_OK_(!eof, "End of file after line continuation");
        if (ok)
        {
            *cursor = line->line;
            while (**cursor == ' ')
                *cursor += 1;
        }
    }
    return ok;
}


/* Flushes any trailing continuation lines. */
static void flush_continuation(struct line_buffer *line)
{
    size_t line_len;
    bool ok = true;
    bool discard = false;
    int first_line = 0, last_line = 0;
    while (
        line_len = strlen(line->line),
        ok  &&  line_len > 0  &&  line->line[line_len - 1] == '\\')
    {
        bool eof;
        ok = read_line(line, &eof)  &&
            TEST_OK_(!eof, "End of file after line continuation");

        /* Record discard parameters so we can make one report when done. */
        if (!discard)
        {
            discard = true;
            first_line = line->line_number;
        }
        last_line = line->line_number;
    }

    if (discard)
    {
        if (first_line == last_line)
            printf("Discarding line %d\n", first_line);
        else
            printf("Discarding lines %d-%d\n", first_line, last_line);
    }
}


/* Parses the right hand side of a <key>=<value> assignment, taking into account
 * the possibility that the value can extend over multiple lines.  A final \ is
 * used to flag line continuation. */
static bool parse_value(
    struct line_buffer *line, const char *cursor,
    struct persistent_variable *persistence)
{
    void *variable = persistence->variable;
    size_t size = persistence->action->size;
    size_t length = 0;
    bool ok = true;
    for (; ok  &&  *cursor != '\0'  &&  length < persistence->max_length;
         length ++)
    {
        ok = fill_line_buffer(line, &cursor)  &&
            persistence->action->read(&cursor, variable);
        variable += size;
    }
    persistence->length = ok ? length : 0;
    return ok  &&  TEST_OK_(*cursor == '\0', "Unexpected extra characters");
}


/* Parse variable assignment of the form <key>=<value> using the parsing method
 * associated with <key>, or fail if <key> not known. */
static bool parse_assignment(struct line_buffer *line)
{
    char *equal;
    struct persistent_variable *persistence = NULL;
    bool ok =
        TEST_NULL_(equal = strchr(line->line, '='), "Missing =")  &&
        DO(*equal++ = '\0')  &&
        TEST_NULL_(persistence = hash_table_lookup(variable_table, line->line),
            "Persistence key \"%s\" not found", line->line)  &&
        parse_value(line, equal, persistence);

    /* Report location of error and flush any continuation lines. */
    if (!ok)
    {
        print_error("Error parsing %s on line %d of state file %s",
            persistence ? persistence->name : "(unknown)",
            line->line_number, state_filename);
        flush_continuation(line);
    }
    return ok;
}


static bool parse_persistence_file(const char *filename, bool check_parse)
{
    struct line_buffer line = {
        .file = fopen(filename, "r"),
        .line_number = 0 };
    if (!TEST_NULL_IO_(line.file, "Unable to open state file %s", filename))
        /* If persistence file isn't found we report open failure but don't
         * fail -- this isn't really an error. */
        return true;

    bool ok = true, eof = false;
    while (ok  &&  read_line(&line, &eof)  &&  !eof)
    {
        /* Skip lines beginning with # and blank lines. */
        if (line.line[0] != '#'  &&  line.line[0] != '\n')
            /* Although line parsing can fail, we ignore the errors unless
             * check_parse is set.  This means that we will complete the loading
             * of a broken state file (for example, if keys have changed), error
             * messages will be printed, but this function will succeed. */
            ok = parse_assignment(&line)  ||  !check_parse;
    }
    IGNORE(TEST_IO(fclose(line.file)));
    return ok;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Writing state file. */

static void write_lines(
    FILE *out, const char *name, const struct persistent_variable *persistence)
{
    const void *variable = persistence->variable;
    size_t size = persistence->action->size;
    int line_length = fprintf(out, "%s=", name);
    for (size_t i = 0; i < persistence->length; i ++)
    {
        if (line_length > 72)
        {
            fputs(" \\\n ", out);
            line_length = 0;
        }
        else if (i != 0)
        {
            fputc(' ', out);
            line_length += 1;
        }
        line_length += persistence->action->write(out, variable);
        variable += size;
    }
    fputc('\n', out);
}


/* Writes persistent state to given file. */
static bool write_persistent_state(const char *filename)
{
    FILE *out;
    bool ok = TEST_NULL_IO_(out = fopen(filename, "w"),
        "Unable to write persistent state: cannot open \"%s\"", filename);
    if (!ok)
        return false;

    /* Start with a timestamp log. */
    char out_buffer[40];
    time_t now = time(NULL);
    const char *timestamp = ctime_r(&now, out_buffer);
    ok = TEST_OK(fprintf(out, "# Written: %s", timestamp) > 0);

    int ix = 0;
    const void *key;
    void *value;
    while (ok  &&  hash_table_walk(variable_table, &ix, &key, &value))
    {
        const char *name = key;
        struct persistent_variable *persistence = value;
        if (persistence->length > 0)
            write_lines(out, name, persistence);
    }
    fclose(out);
    return ok;
}

/* Updates persistent state via a backup file to avoid data loss (assuming
 * rename is implemented as an OS atomic action). */
bool update_persistent_state(void)
{
    if (persistence_dirty  &&  state_filename != NULL)
    {
        /* By writing to a backup file first we can then rely on the OS
         * implementing rename as an atomic operation to achieve a safe atomic
         * update of the stored state. */
        size_t name_len = strlen(state_filename);
        char backup_file[name_len + strlen(".backup") + 1];
        sprintf(backup_file, "%s.backup", state_filename);
        return
            write_persistent_state(backup_file)  &&
            TEST_IO(rename(backup_file, state_filename));
    }
    else
        return true;
}



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Top level control. */


/* Wrapper to wait for a pthread condition with a timeout. */
#define NSECS   1000000000
static bool pwait_timeout(int secs, long nsecs)
{
    struct timespec timeout;
    ASSERT_IO(clock_gettime(CLOCK_REALTIME, &timeout));
    timeout.tv_sec += secs;
    timeout.tv_nsec += nsecs;

    if (timeout.tv_nsec >= NSECS)
    {
        timeout.tv_nsec -= NSECS;
        timeout.tv_sec += 1;
    }
    int rc = pthread_cond_timedwait(&psignal, &mutex, &timeout);
    return rc != ETIMEDOUT;
}


/* This thread is responsible for ensuring the persistent state file is up to
 * date.  It wakes up periodically to check if anything has changed, and if it
 * has, ensures the state file is updated.  The file is also written on shutdown
 * if necessary. */
static void *persistence_thread(void *context)
{
    LOCK();
    while (thread_running)
    {
        pwait_timeout(persistence_interval, 0);
        update_persistent_state();
        persistence_dirty = false;
    }
    UNLOCK();
    return NULL;
}


/* Must be called before marking any variables as persistent. */
void initialise_persistent_state(void)
{
    variable_table = hash_table_create(false);  // We look after name lifetime
}


bool load_persistent_state(
    const char *file_name, int save_interval, bool check_parse)
{
    /* It's more robust to take a copy of the passed filename, places fewer
     * demands on the caller. */
    state_filename = strdup(file_name);
    persistence_interval = save_interval;

    LOCK();
    bool ok = parse_persistence_file(state_filename, check_parse);
    UNLOCK();

    return
        ok  &&
        IF(persistence_thread_id == 0,
            TEST_PTHREAD(pthread_create(
                &persistence_thread_id, NULL, persistence_thread, NULL)));
}


/* Writes out persistent state file if necessary.  All we have to do in fact is
 * wake up the responsible thread and then wait for it to complete. */
void terminate_persistent_state(void)
{
    LOCK();
    thread_running = false;
    pthread_cond_signal(&psignal);
    UNLOCK();
    pthread_join(persistence_thread_id, NULL);
}
