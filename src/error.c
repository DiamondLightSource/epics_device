/* Generic error handling framework. */

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#ifdef VX_WORKS
#include <taskLib.h>
#else
#include <execinfo.h>
#endif

#include <caerr.h>

#include "error.h"


#ifdef VX_WORKS
/* On vxWorks we are missing a lot of useful functions. */

int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    int count = vsprintf(str, format, args);
    ASSERT_OK(count < (int) size);
    return count;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = vsnprintf(str, size, format, args);
    va_end(args);
    return count;
}

char *strdup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *result = malloc(len);
    memcpy(result, str, len);
    return result;
}

int asprintf(char **result, const char *format, ...)
{
    char result_buffer[1024];   // Lazy ... hope it's long enough!

    va_list args;
    va_start(args, format);
    int count = vsnprintf(result_buffer, sizeof(result_buffer), format, args);
    va_end(args);

    *result = strdup(result_buffer);
    return count;
}

#endif


static void default_error_hook(const char *message)
{
    fprintf(stderr, "%s\n", message);
}


static error_hook_t error_hook = default_error_hook;

error_hook_t set_error_hook(error_hook_t hook)
{
    error_hook_t result = error_hook;
    error_hook = hook;
    return result;
}


/* Two mechanisms for reporting extra error information. */
char *_extra_io(void)
{
    /* This is very annoying: strerror() is not not necessarily thread safe ...
     * but not for any compelling reason, see:
     *  http://sources.redhat.com/ml/glibc-bugs/2005-11/msg00101.html
     * and the rather unhelpful reply:
     *  http://sources.redhat.com/ml/glibc-bugs/2005-11/msg00108.html
     *
     * On the other hand, the recommended routine strerror_r() is inconsistently
     * defined -- depending on the precise library and its configuration, it
     * returns either an int or a char*.  Oh dear.
     *
     * Ah well.  We go with the GNU definition, so here is a buffer to maybe use
     * for the message. */
    char str_error[256];
    int error = errno;
#ifdef VX_WORKS
    /* Of course, vxWorks has yet another and incompatible declaration of
     * strerror_r for us.  Ho hum. */
    const char *error_string = str_error;
    strerror_r(error, str_error);
#else
    const char *error_string =
        strerror_r(error, str_error, sizeof(str_error));
#endif
    char *result;
    asprintf(&result, "(%d) %s", error, error_string);
    return result;
}


void _report_error(char *extra, const char *format, ...)
{
    /* Large enough not to really worry about overflow.  If we do generate a
     * silly message that's too big, then that's just too bad. */
    const size_t MESSAGE_LENGTH = 512;
    char error_message[MESSAGE_LENGTH];

    va_list args;
    va_start(args, format);
    int count = vsnprintf(error_message, MESSAGE_LENGTH, format, args);
    va_end(args);

    if (extra)
        snprintf(error_message + count, MESSAGE_LENGTH - (size_t) count,
            ": %s", extra);

    error_hook(error_message);

    if (extra)
        free(extra);
}


void _panic_error(char *extra, const char *filename, int line)
{
    _report_error(extra, "Unrecoverable error at %s, line %d", filename, line);
    fflush(stderr);
    fflush(stdout);

#ifdef VX_WORKS
    /* On vxWorks all we can really do is hang. */
    while (true)
        taskSuspend(taskIdSelf());

#else
    /* Now try and create useable backtrace. */
    void *backtrace_buffer[128];
    int count = backtrace(backtrace_buffer, ARRAY_SIZE(backtrace_buffer));
    backtrace_symbols_fd(backtrace_buffer, count, STDERR_FILENO);
    char last_line[128];
    int char_count = snprintf(last_line, sizeof(last_line),
        "End of backtrace: %d lines written\n", count);
    write(STDERR_FILENO, last_line, (size_t) char_count);

    _exit(255);
#endif
}
