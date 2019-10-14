/* Helper macros and declarations to simplify error handling.
 *
 * Copyright (c) 2011-2016 Michael Abbott, Diamond Light Source Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Contact:
 *      Dr. Michael Abbott,
 *      Diamond Light Source Ltd,
 *      Diamond House,
 *      Chilton,
 *      Didcot,
 *      Oxfordshire,
 *      OX11 0DE
 *      michael.abbott@diamond.ac.uk
 */

/* The following error handling macros are defined here:
 *
 *  TEST_OK      TEST_OK_      ASSERT_OK       Fail if expression is false
 *  TEST_IO      TEST_IO_      ASSERT_IO       Fail if expression is -1
 *  TEST_OK_IO   TEST_OK_IO_   ASSERT_OK_IO    Fail if false, reports errno
 *  TEST_PTHREAD TEST_PTHREAD_ ASSERT_PTHREAD  Fail if expression is not 0
 *
 * The three patterns behave thus:
 *
 *  TEST_xx(expr)
 *      If the test fails a canned error message (defined by the macro
 *      ERROR_MESSAGE) is generated and the macro evaluates to ERROR_OK,
 *      otherwise an error message is computed and returned.
 *
 *  TEST_xx_(expr, message...)
 *      If the test fails then the given error message (with sprintf formatting)
 *      is generated and returned, otherwise ERROR_OK is returned.
 *
 *  ASSERT_xx(expr)
 *      If the test fails then _error_panic() is called and execution does not
 *      continue from this point.
 *
 * Note that the _PTHREAD macros have the extra side effect of assigning any
 * non-zero expression to errno: these are designed to be used with the pthread
 * functions where this behaviour is appropriate.
 *
 * These macros are designed to be used as chained conjunctions of the form
 *
 *  TEST_xx(...)  ?:  TEST_xx(...)  ?:  ...
 *
 * To facilitate this three further macros are provided:
 *
 *  DO(statements)                  Performs statements and returns ERROR_OK
 *  IF(test, iftrue)                Only checks iftrue if test succeeds
 *  IF_ELSE(test, iftrue, iffalse)  Alternative spelling of (..?..:..)
 *
 * The following macro is designed act as a kind of exception handler: if expr
 * generates an error then the on_error statement is executed, and the error
 * code from expr is returned.
 *
 *  TRY_CATCH(expr, on_error)       Executes on_error if expr fails
 *
 * The following functions are for handling and reporting error codes.  Every
 * error code other than ERROR_OK (which is an alias for NULL) must be released
 * by calling one of the following two functions:
 *
 *  error_report(error)
 *      If error is not ERROR_OK then a formatted error message is logged (by
 *      calling log_error) and the resources handled by the error code are
 *      released.  True is returned iff there was an error to report.
 *
 *  error_discard(error)
 *      This simply releases all resources associated with error if it is not
 *      ERROR_OK.
 *
 * The following helper functions can be used to process error values:
 *
 *  error_format(error)
 *      This converts an error message into a sensible string.  The lifetime of
 *      the returned string is the same as the lifetime of the error value.
 *
 *  error_extend(error, format, ...)
 *      If error is not ERROR_OK then a new error string is formatted and added
 *      to the given error code.  If error is ERROR_OK this is a no-op, the
 *      formatting arguments are ignored.
 */


/* Hint to compiler that x is likely to be 0. */
#define unlikely(x)   __builtin_expect((x), 0)


/* Error messages are encoded as an opaque type, with NULL used to represent no
 * error.  The lifetime of error values must be managed by the methods here. */
struct error__t;
typedef struct error__t *error__t;  // Alas error_t is already spoken for!
#define ERROR_OK    ((error__t) NULL)


/* One of the following two functions must be called to release the resources
 * used by an error__t value. */

/* This reports the given error message.  If error was ERROR_OK and there was
 * nothing to report then false is returned, otherwise true is returned. */
bool error_report(error__t error);

/* A helper macro to extend the reported error with context. */
#define ERROR_REPORT(expr, format...) \
    error_report(error_extend((expr), format))

/* This function silently discards the error code. */
bool error_discard(error__t error);


/* This function extends the information associated with the given error with
 * the new message.  The original error is returned for convenience. */
error__t error_extend(error__t error, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

/* Converts an error code into a formatted string. */
const char *error_format(error__t error);


/* Called to report unrecoverable error.  Terminates program without return. */
void _error_panic(char *extra, const char *filename, int line)
    __attribute__((__noreturn__));
/* Performs normal error report. */
error__t _error_create(char *extra, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
/* This trick lets both scan-build and the optimiser know that when _TEST fails
 * a non zero error__t value is returned. */
static inline void *__attribute__((nonnull)) _nonnull(void *arg) { return arg; }

/* Mechanism for reporting extra error information from errno.  The string
 * returned must be released by the caller. */
char *_error_extra_io(void);
/* Same mechanism, but taking specific error code. */
char *_error_extra_io_errno(int error);


/* Routines to write informative message or error to stderr or syslog. */
void log_message(const char *message, ...)
    __attribute__((format(printf, 1, 2)));
void log_error(const char *message, ...)
    __attribute__((format(printf, 1, 2)));
void vlog_message(int priority, const char *format, va_list args);

/* Once this has been called all logged message will be sent to syslog. */
void start_logging(const char *ident);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* The core error handling macros. */

/* A dance for generating unique local identifiers.  This involves a number of
 * tricky C preprocessor techniques, and uses the gcc __COUNTER__ extension. */
#define _CONCATENATE(a, b)  a##b
#define CONCATENATE(a, b)   _CONCATENATE(a, b)
#define UNIQUE_ID()         CONCATENATE(_eid__, __COUNTER__)


/* Generic TEST macro: computes a boolean from expr using COND (should be a
 * macro), and generates the given error message if the boolean is false.  If
 * expr is successful then ERROR_OK is returned. */
#define _id_TEST(result, COND, EXTRA, expr, message...) \
    ( { \
        typeof(expr) result = (expr); \
        unlikely(!COND(result)) ? \
            _nonnull(_error_create(EXTRA(result), message)) : \
            ERROR_OK; \
    } )
#define _TEST(args...)  _id_TEST(UNIQUE_ID(), args)

/* An assert for tests that really really should not fail!  The program will
 * terminate immediately. */
#define _id_ASSERT(result, COND, EXTRA, expr)  \
    do { \
        typeof(expr) result = (expr); \
        if (unlikely(!COND(result))) \
            _error_panic(EXTRA(result), __FILE__, __LINE__); \
    } while (0)
#define _ASSERT(args...)    _id_ASSERT(UNIQUE_ID(), args)


/* Default error message for unexpected errors. */
#define ERROR_MESSAGE       "Unexpected error at %s:%d", __FILE__, __LINE__

/* Tests system calls: -1 => error, pick up error data from errno. */
#define _COND_IO(expr)              ((intptr_t) (expr) != -1)
#define _MSG_IO(expr)               _error_extra_io()
#define TEST_IO_(expr, message...)  _TEST(_COND_IO, _MSG_IO, expr, message)
#define TEST_IO(expr)               TEST_IO_(expr, ERROR_MESSAGE)
#define ASSERT_IO(expr)             _ASSERT(_COND_IO, _MSG_IO, expr)

/* Tests an ordinary boolean: false => error. */
#define _COND_OK(expr)              ((bool) (expr))
#define _MSG_OK(expr)               NULL
#define TEST_OK_(expr, message...)  _TEST(_COND_OK, _MSG_OK, expr, message)
#define TEST_OK(expr)               TEST_OK_(expr, ERROR_MESSAGE)
#define ASSERT_OK(expr)             _ASSERT(_COND_OK, _MSG_OK, expr)
#define TEST_OK_IO_(expr, message...) _TEST(_COND_OK, _MSG_IO, expr, message)
#define TEST_OK_IO(expr)            TEST_OK_IO_(expr, ERROR_MESSAGE)
#define ASSERT_OK_IO(expr)          _ASSERT(_COND_OK, _MSG_IO, expr)

/* Tests the return from a pthread_ call: a non zero return is the error
 * code!  We just assign this to errno. */
#define _COND_PTHREAD(expr)         ((expr) == 0)
#define _MSG_PTHREAD(expr)          (_error_extra_io_errno(expr))
#define TEST_PTHREAD_(expr, message...) \
    _TEST(_COND_PTHREAD, _MSG_PTHREAD, expr, message)
#define TEST_PTHREAD(expr)          TEST_PTHREAD_(expr, ERROR_MESSAGE)
#define ASSERT_PTHREAD(expr)        _ASSERT(_COND_PTHREAD, _MSG_PTHREAD, expr)

/* For marking unreachable code.  Same as ASSERT_OK(false). */
#define ASSERT_FAIL()               _error_panic(NULL, __FILE__, __LINE__)

/* For failing immediately.  Same as TEST_OK_(false, message...) */
#define FAIL()                      TEST_OK(false)
#define FAIL_(message...)           _nonnull(_error_create(NULL, message))


/* These two macros facilitate using the macros above by creating if
 * expressions that are slightly more sensible looking than ?: in context. */
#define DO(action...)                   ({action; ERROR_OK;})
#define IF(test, iftrue)                ((test) ? (iftrue) : ERROR_OK)
#define IF_ELSE(test, iftrue, iffalse)  ((test) ? (iftrue) : (iffalse))


/* If action fails perform on_fail as a cleanup action.  Returns status of
 * action. */
#define _id_TRY_CATCH(error, action, on_fail...) \
    ( { \
        error__t error = (action); \
        if (error) { on_fail; } \
        error; \
    } )
#define TRY_CATCH(args...) _id_TRY_CATCH(UNIQUE_ID(), args)


/* Returns result of action, but first unconditionally performs cleanup. */
#define _id_DO_FINALLY(error, action, finally...) \
    ({ \
        error__t error = (action); \
        { finally; } \
        error; \
    })
#define DO_FINALLY(args...) _id_DO_FINALLY(UNIQUE_ID(), args)



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* The following miscellaneous macros are extra to the error mechanism. */

/* For ignoring return values even when warn_unused_result is in force. */
#define IGNORE(e)       do if(e) {} while (0)

/* A tricksy compile time bug checking macro modified from the kernel.  Causes a
 * compiler error if e doesn't evaluate to true (a non-zero value). */
#define COMPILE_ASSERT(e)           ((void) sizeof(struct { int:-!(e); }))

/* For a static version we drop the COMPILE_ASSERT() expression into a
 * discardable anonymous function. */
#define _id_STATIC_COMPILE_ASSERT(f, e) \
    static inline void f(void) { COMPILE_ASSERT(e); }
#define STATIC_COMPILE_ASSERT(e)    _id_STATIC_COMPILE_ASSERT(UNIQUE_ID(), e)


/* Use this to mark functions that can be constant folded, ie depend only on
 * their arguments and global state. */
#define _pure __attribute__((pure))


/* A rather randomly placed helper routine.  This and its equivalents are
 * defined all over the place, but there doesn't appear to be a definitive
 * definition anywhere. */
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof((a)[0]))

/* Casting from one type to another with checking via a union.  Needed in
 * particular to reassure the compiler about aliasing. */
#define _id_CAST_FROM_TO(_union, from_type, to_type, value) \
    ( { \
        COMPILE_ASSERT(sizeof(from_type) == sizeof(to_type)); \
        union { \
            from_type _value; \
            to_type _cast; \
        } _union = { ._value = (value) }; \
        _union._cast; \
    } )
#define CAST_FROM_TO(args...) \
    _id_CAST_FROM_TO(UNIQUE_ID(), args)

#define CAST_TO(to_type, value) CAST_FROM_TO(typeof(value), to_type, value)

/* A macro for ensuring that a value really is assign compatible to the
 * requested type.  Note that due to restrictions on syntax this won't work if
 * type is a written out function type, as in that case the [] part needs to be
 * inside the type definition! */
#define ENSURE_TYPE(type, value)    (*(type []) { (value) })


/* A couple of handy macros: macro safe MIN and MAX functions. */
#define _MIN(tx, ty, x, y) \
    ( { typeof(x) tx = (x); typeof(y) ty = (y); tx < ty ? tx : ty; } )
#define _MAX(tx, ty, x, y) \
    ( { typeof(x) tx = (x); typeof(y) ty = (y); tx > ty ? tx : ty; } )
#define MIN(x, y)   _MIN(UNIQUE_ID(), UNIQUE_ID(), x, y)
#define MAX(x, y)   _MAX(UNIQUE_ID(), UNIQUE_ID(), x, y)


/* Casts a member of a structure out to the containing structure. */
#define _id_container_of(mptr, ptr, type, member) \
    ( { \
        typeof(((type *)0)->member) *mptr = (ptr); \
        (type *)((void *) mptr - offsetof(type, member)); \
    } )
#define container_of(args...)   _id_container_of(UNIQUE_ID(), args)


/* Tricksy code to wrap enter and leave functions around a block of code.  The
 * use of double for loops is so that the enter statement can contain a variable
 * declaration, which is then available to the leave statement and the body of
 * the block. */
#define _id_WITH_ENTER_LEAVE(loop, enter, leave) \
    for (bool loop = true; loop; ) \
        for (enter; loop; loop = false, leave)
#define _WITH_ENTER_LEAVE(enter, leave) \
    _id_WITH_ENTER_LEAVE(UNIQUE_ID(), enter, leave)


/* Debug utility for dumping binary data in ASCII format. */
void dump_binary(FILE *out, const void *buffer, size_t length);
