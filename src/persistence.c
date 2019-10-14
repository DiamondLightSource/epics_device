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
    unsigned int max_length;
    unsigned int length;
    char variable[0];
};


/* Used to implement core persistent actions, essentially converting values to
 * and from external strings. */
struct persistent_action {
    unsigned int size;
    /* Write value to output buffer (which must be long enough!), returns number
     * of characters written. */
    int (*write)(FILE *out, const void *variable);
    /* Reads value from given character buffer, advancing the character pointer
     * past the characters read, returns false and generates an error message if
     * there is a parsing error. */
    error__t (*read)(const char **in, void *variable);
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

DEFINE_WRITE(int8_t,  "%d")
DEFINE_WRITE(int16_t, "%d")
DEFINE_WRITE(int32_t, "%d")
DEFINE_WRITE(float,   "%.8g")
DEFINE_WRITE(double,  "%.17g")


static error__t check_number(const char *start, const char *end)
{
    return TEST_OK_(end > start  &&  errno == 0, "Error converting number");
}



#define DEFINE_READ_NUM(type, convert, extra...) \
    static error__t read_##type(const char **string, void *variable) \
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
static error__t read_bool(const char **in, void *variable)
{
    char ch = *(*in)++;
    *(bool *) variable = ch == 'Y'  ||  ch == '1';
    return TEST_OK_(strchr("YN10", ch), "Invalid boolean value");
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
static error__t parse_octal(const char **in, char *result)
{
    unsigned int value = 0;
    error__t error = ERROR_OK;
    for (unsigned int i = 0; !error  &&  i < 3; i ++)
    {
        unsigned int ch = (unsigned int) *(*in)++;
        error = TEST_OK_('0' <= ch  &&  ch <= '7', "Expected octal digit");
        value = (value << 3) + (ch - '0');
    }
    *result = (char) value;
    return error;
}

/* We go for the most witless string parsing possible, must be double quoted,
 * and we only recognise octal character escapes. */
static error__t read_string(const char **in, void *variable)
{
    char *string = variable;
    memset(variable, 0, EPICS_STRING_LENGTH);
    error__t error = TEST_OK_(*(*in)++ == '"', "Expected quoted string");
    for (unsigned int i = 0; !error  &&  i < EPICS_STRING_LENGTH; i ++)
    {
        char ch = *(*in)++;
        if (ch == '"')
            return ERROR_OK;
        else if (ch == '\\')
            error = parse_octal(in, &string[i]);
        else
        {
            string[i] = ch;
            error = TEST_OK_(' ' <= ch  &&  ch <= '~',
                "Invalid string character");
        }
    }
    return error  ?:  TEST_OK_(*(*in)++ == '"', "Missing closing quote");
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



/* Creates new persistent variable. */
void create_persistent_waveform(
    const char *name, enum PERSISTENCE_TYPES type, unsigned int max_length)
{
    /* If you try to create a persistent PV without having first initialised the
     * persistence layer then you'll get this error. */
    ASSERT_OK(variable_table);

    const struct persistent_action *action = &persistent_actions[type];
    struct persistent_variable *persistence =
        malloc(sizeof(struct persistent_variable) + max_length * action->size);
    persistence->action = action;
    persistence->name = strdup(name);
    persistence->max_length = max_length;
    persistence->length = 0;

    WITH_MUTEX(mutex)
        hash_table_insert(variable_table, persistence->name, persistence);
}


static struct persistent_variable *lookup_persistence(const char *name)
{
    struct persistent_variable *persistence =
        hash_table_lookup(variable_table, name);
    error_report(TEST_OK_(persistence,
        "Persistent variable %s not found", name));
    return persistence;
}


/* Updates variable from value stored on disk. */
bool read_persistent_waveform(
    const char *name, void *variable, unsigned int *length)
{
    bool ok;
    WITH_MUTEX(mutex)
    {
        struct persistent_variable *persistence = lookup_persistence(name);
        ok = persistence != NULL  &&  persistence->length > 0;
        if (ok)
        {
            memcpy(variable, persistence->variable,
                persistence->length * persistence->action->size);
            *length = persistence->length;
        }
    }
    return ok;
}

bool read_persistent_variable(const char *name, void *variable)
{
    unsigned int length;
    bool ok = read_persistent_waveform(name, variable, &length);
    if (ok)
        ASSERT_OK(length == 1);
    return ok;
}


/* Writes value to persistent variable. */
void write_persistent_waveform(
    const char *name, const void *value, unsigned int length)
{
    WITH_MUTEX(mutex)
    {
        struct persistent_variable *persistence = lookup_persistence(name);
        if (persistence != NULL)
        {
            /* Don't force a write of the persistence file if nothing has
             * actually changed. */
            unsigned int size = length * persistence->action->size;
            persistence_dirty =
                persistence_dirty  ||
                persistence->length != length  ||
                memcmp(persistence->variable, value, size);

            persistence->length = length;
            memcpy(persistence->variable, value, size);
        }
    }
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
static error__t read_line(struct line_buffer *line, bool *eof)
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
                "Line %d truncated?", line->line_number)  ?:
            DO(line->line[len - 1] = '\0');
    }
}


/* Skips leading whitespace and refills the line buffer if a line continuation
 * character is encountered. */
static error__t fill_line_buffer(struct line_buffer *line, const char **cursor)
{
    while (**cursor == ' ')
        *cursor += 1;
    error__t error = ERROR_OK;
    if (**cursor == '\\')
    {
        bool eof;
        error = read_line(line, &eof)  ?:
            TEST_OK_(!eof, "End of file after line continuation");
        if (!error)
        {
            *cursor = line->line;
            while (**cursor == ' ')
                *cursor += 1;
        }
    }
    return error;
}


/* Flushes any trailing continuation lines. */
static void flush_continuation(struct line_buffer *line)
{
    size_t line_len;
    error__t error = ERROR_OK;
    bool discard = false;
    int first_line = 0, last_line = 0;
    while (
        line_len = strlen(line->line),
        !error  &&  line_len > 0  &&  line->line[line_len - 1] == '\\')
    {
        bool eof;
        error = read_line(line, &eof)  ?:
            TEST_OK_(!eof, "End of file after line continuation");

        /* Record discard parameters so we can make one report when done. */
        if (!discard)
        {
            discard = true;
            first_line = line->line_number;
        }
        last_line = line->line_number;
    }
    error_report(error);

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
static error__t parse_value(
    struct line_buffer *line, const char *cursor,
    struct persistent_variable *persistence)
{
    void *variable = persistence->variable;
    unsigned int size = persistence->action->size;
    unsigned int length = 0;
    error__t error = ERROR_OK;
    for (; !error  &&  *cursor != '\0'  &&  length < persistence->max_length;
         length ++)
    {
        error = fill_line_buffer(line, &cursor)  ?:
            persistence->action->read(&cursor, variable);
        variable += size;
    }
    persistence->length = error ? 0 : length;
    return
        error  ?:
        TEST_OK_(*cursor == '\0', "Unexpected extra characters");
}


/* Parse variable assignment of the form <key>=<value> using the parsing method
 * associated with <key>, or fail if <key> not known. */
static error__t parse_assignment(struct line_buffer *line)
{
    char *equal;
    struct persistent_variable *persistence = NULL;
    error__t error =
        TEST_OK_(equal = strchr(line->line, '='), "Missing =")  ?:
        DO(*equal++ = '\0')  ?:
        TEST_OK_(persistence = hash_table_lookup(variable_table, line->line),
            "Persistence key \"%s\" not found", line->line)  ?:
        parse_value(line, equal, persistence);

    /* Report location of error and flush any continuation lines. */
    if (error)
    {
        error_extend(error,
            "Error parsing %s on line %d of state file %s",
            persistence ? persistence->name : "(unknown)",
            line->line_number, state_filename);
        flush_continuation(line);
    }
    return error;
}


static error__t parse_persistence_file(const char *filename, bool check_parse)
{
    struct line_buffer line = {
        .file = fopen(filename, "r"),
        .line_number = 0 };
    error__t error = TEST_OK_IO_(line.file,
        "Unable to open state file %s", filename);
    if (error_report(error))
        /* If persistence file isn't found we report open failure but don't
         * fail -- this isn't really an error. */
        return ERROR_OK;

    while (!error)
    {
        bool eof = false;
        error = read_line(&line, &eof);
        if (error  ||  eof)
            break;

        /* Skip lines beginning with # and blank lines. */
        if (line.line[0] != '#'  &&  line.line[0] != '\n')
        {
            error = parse_assignment(&line);
            if (!check_parse)
            {
                /* Although line parsing can fail, we only report errors if
                 * check_parse is not set.  This means that we will complete the
                 * loading of a broken state file (for example, if keys have
                 * changed), error messages will be printed, but this function
                 * will succeed. */
                error_report(error);
                error = ERROR_OK;
            }
        }
    }
    fclose(line.file);
    return error;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Writing state file. */

static void write_lines(
    FILE *out, const char *name, const struct persistent_variable *persistence)
{
    const void *variable = persistence->variable;
    unsigned int size = persistence->action->size;
    int line_length = fprintf(out, "%s=", name);
    for (unsigned int i = 0; i < persistence->length; i ++)
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
static error__t write_persistent_state(const char *filename)
{
    FILE *out;
    error__t error =
        TEST_OK_IO_(out = fopen(filename, "w"),
            "Unable to write persistent state: cannot open \"%s\"",
            filename);
    if (error)
        return error;

    /* Start with a timestamp log. */
    char out_buffer[40];
    time_t now = time(NULL);
    const char *timestamp = ctime_r(&now, out_buffer);
    fprintf(out, "# Written: %s", timestamp);

    int ix = 0;
    const void *key;
    void *value;
    while (hash_table_walk(variable_table, &ix, &key, &value))
    {
        const char *name = key;
        struct persistent_variable *persistence = value;
        if (persistence->length > 0)
            write_lines(out, name, persistence);
    }
    fclose(out);
    return ERROR_OK;
}


/* Updates persistent state via a backup file to avoid data loss (assuming
 * rename is implemented as an OS atomic action). */
error__t update_persistent_state(void)
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
            write_persistent_state(backup_file)  ?:
            TEST_IO(rename(backup_file, state_filename));
    }
    else
        return ERROR_OK;
}



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Top level control. */


/* Wrapper to wait for a pthread condition with a timeout. */
#define NSECS   1000000000
static void pwait_timeout(int secs, long nsecs)
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
    pthread_cond_timedwait(&psignal, &mutex, &timeout);
}


/* This thread is responsible for ensuring the persistent state file is up to
 * date.  It wakes up periodically to check if anything has changed, and if it
 * has, ensures the state file is updated.  The file is also written on shutdown
 * if necessary. */
static void *persistence_thread(void *context)
{
    WITH_MUTEX(mutex)
    {
        while (thread_running)
        {
            pwait_timeout(persistence_interval, 0);
            error_report(update_persistent_state());
            persistence_dirty = false;
        }
    }
    return NULL;
}


/* Must be called before marking any variables as persistent. */
void initialise_persistent_state(void)
{
    variable_table = hash_table_create(false);  // We look after name lifetime
}


error__t load_persistent_state(
    const char *file_name, int save_interval, bool check_parse)
{
    /* It's more robust to take a copy of the passed filename, places fewer
     * demands on the caller. */
    state_filename = strdup(file_name);
    persistence_interval = save_interval;

    error__t error;
    WITH_MUTEX(mutex)
        error = parse_persistence_file(state_filename, check_parse);

    return
        error  ?:
        IF(persistence_thread_id == 0,
            TEST_PTHREAD(pthread_create(
                &persistence_thread_id, NULL, persistence_thread, NULL)));
}


/* Writes out persistent state file if necessary.  All we have to do in fact is
 * wake up the responsible thread and then wait for it to complete. */
void terminate_persistent_state(void)
{
    /* If no state filename given then do nothing. */
    if (!state_filename)
        return;

    WITH_MUTEX(mutex)
    {
        thread_running = false;
        pthread_cond_signal(&psignal);
    }
    pthread_join(persistence_thread_id, NULL);
}
