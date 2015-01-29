/* Helper macros and declarations to simplify error handling.
 *
 * Copyright (c) 2011 Michael Abbott, Diamond Light Source Ltd.
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
 *  TEST_OK     TEST_OK_    ASSERT_OK       Fail if expression is false
 *  TEST_IO     TEST_IO_    ASSERT_IO       Fail if expression is -1
 *  TEST_NULL   TEST_NULL_  ASSERT_NULL     Fail if expression equals NULL
 *  TEST_0      TEST_0_     ASSERT_0        Fail if expression is not 0
 *
 * There are also macros for handling file I/O in a similar form (but with
 * slightly different argument lists):
 *
 *  TEST_read   TEST_read_  ASSERT_read     Fail if read not of expected size
 *  TEST_write  TEST_write_ ASSERT_write    Fail if write not of expected size
 *
 * The three patterns behave thus:
 *
 *  TEST_xx(expr)
 *      If the test fails a canned error message (defined by the macro
 *      ERROR_MESSAGE) is generated and the macro evaluates to False, otherwise
 *      evaluates to True.
 *
 *  TEST_xx_(expr, message...)
 *      If the test fails then the given error message (with sprintf formatting)
 *      is generated and the macro evaluates to False, otherwise True.
 *
 *  ASSERT_xx(expr)
 *      If the test fails then _panic_error() is called and execution does not
 *      continue from this point.
 *
 * Note that the _0 macros have the extra side effect of assigning any non-zero
 * expression to errno: these are designed to be used with the pthread functions
 * where this behaviour is appropriate.
 *
 * These macros are designed to be used as chained conjunctions of the form
 *
 *  TEST_xx(...)  &&  TEST_xx(...)  &&  ...
 *
 * To facilitate this three further macros are provided:
 *
 *  DO_(statements)             Performs statements and evaluates to True
 *  IF_(test, iftrue)                   Only checks iftrue if test succeeds
 *  IF_ELSE(test, iftrue, iffalse)      Alternative spelling of (?:)
 */

#ifdef VX_WORKS
#include "vxdefs.h"
#endif

/* Hint to compiler that x is likely to be 0. */
#define unlikely(x)   __builtin_expect((x), 0)


/* Error reporting hook: this function is called to output the error message.
 * The error hook can be modified to adjust where error reporting goes. */
typedef void (*error_hook_t)(const char *message);
error_hook_t set_error_hook(error_hook_t hook);

/* Called to report unrecoverable error.  Terminates program without return. */
void _panic_error(char *extra, const char *filename, int line)
    __attribute__((__noreturn__));
/* Performs normal error report. */
void _report_error(char *extra, const char *message, ...)
    __attribute__((format(printf, 2, 3)));

/* Two mechanisms for reporting extra error information. */
char *_extra_io(void);


/* This function performs a simple error report through the error report
 * mechanism. */
#define print_error(message...) _report_error(NULL, message)


/* Generic TEST macro: computes a boolean from expr using COND (should be a
 * macro), and prints the given error message if the boolean is false.  The
 * boolean result is the value of the entire expression. */
#define TEST_(COND, EXTRA, expr, message...) \
    ( { \
        typeof(expr) __result__ = (expr); \
        bool __ok__ = COND(__result__); \
        if (unlikely(!__ok__)) \
            _report_error(EXTRA(__result__), message); \
        __ok__; \
    } )

/* An assert for tests that really really should not fail!  This exits
 * immediately. */
#define ASSERT_(COND, EXTRA, expr)  \
    do { \
        typeof(expr) __result__ = (expr); \
        if (unlikely(!COND(__result__))) \
            _panic_error(EXTRA(__result__), __FILE__, __LINE__); \
    } while (0)


/* Default error message for unexpected errors. */
#define ERROR_MESSAGE       "Unexpected error at %s:%d", __FILE__, __LINE__

/* Tests system calls: -1 => error, pick up error data from errno. */
#define _COND_IO(expr)              ((intptr_t) (expr) != -1)
#define _MSG_IO(expr)               _extra_io()
#define TEST_IO_(expr, message...)  TEST_(_COND_IO, _MSG_IO, expr, message)
#define TEST_IO(expr)               TEST_IO_(expr, ERROR_MESSAGE)
#define ASSERT_IO(expr)             ASSERT_(_COND_IO, _MSG_IO, expr)

/* Tests an ordinary boolean: false => error. */
#define _COND_OK(expr)              ((bool) (expr))
#define _MSG_OK(expr)               NULL
#define TEST_OK_(expr, message...)  TEST_(_COND_OK, _MSG_OK, expr, message)
#define TEST_OK(expr)               TEST_OK_(expr, ERROR_MESSAGE)
#define ASSERT_OK(expr)             ASSERT_(_COND_OK, _MSG_OK, expr)

/* Tests pointers: NULL => error.  If there is extra information in errno then
 * use the NULL_IO test, otherwise just NULL. */
#define _COND_NULL(expr)            ((expr) != NULL)
#define TEST_NULL_(expr, message...) \
    TEST_(_COND_NULL, _MSG_OK, expr, message)
#define TEST_NULL(expr)             TEST_NULL_(expr, ERROR_MESSAGE)
#define ASSERT_NULL(expr)           ASSERT_(_COND_NULL, _MSG_OK, expr)

#define TEST_NULL_IO_(expr, message...) \
    TEST_(_COND_NULL, _MSG_IO, expr, message)
#define TEST_NULL_IO(expr)             TEST_NULL_IO_(expr, ERROR_MESSAGE)
#define ASSERT_NULL_IO(expr)           ASSERT_(_COND_NULL, _MSG_IO, expr)

/* Tests the return from a pthread_ call: a non zero return is the error
 * code!  We just assign this to errno. */
#define _COND_0(expr)               ((expr) == 0)
#define _MSG_0(expr)                ({ errno = (expr); _extra_io(); })
#define TEST_0_(expr, message...)   TEST_(_COND_0, _MSG_0, expr, message)
#define TEST_0(expr)                TEST_0_(expr, ERROR_MESSAGE)
#define ASSERT_0(expr)              ASSERT_(_COND_0, _MSG_0, expr)


/* For marking unreachable code.  Same as ASSERT_OK(false). */
#define ASSERT_FAIL()               _panic_error(NULL, __FILE__, __LINE__)

/* For failing immediately.  Same as TEST_OK_(false, message...) */
#define FAIL_(message...)           ({ print_error(message); false; })


/* These two macros facilitate using the macros above by creating if
 * expressions that are slightly more sensible looking than ?: in context. */
#define DO_(action)                     ({action; true;})
#define IF_(test, iftrue)               ((test) ? (iftrue) : true)
#define IF_ELSE(test, iftrue, iffalse)  ((test) ? (iftrue) : (iffalse))

/* Used to ensure that the finally action always occurs, even if action fails.
 * Returns combined success of both actions. */
#define FINALLY(action, finally) \
    ( { \
        bool __oK__ = (action); \
        (finally)  &&  __oK__; \
    } )

/* If action fails perform on_fail as a cleanup action.  Returns status of
 * action. */
#define UNLESS(action, on_fail) \
    ( { \
        bool __oK__ = (action); \
        if (!__oK__) { on_fail; } \
        __oK__; \
    } )


/* Testing read and write happens often enough to be annoying, so some
 * special case macros here. */
#define _COND_rw(rw, fd, buf, count) \
    (rw(fd, buf, count) == (ssize_t) (count))
#define TEST_read(fd, buf, count)   TEST_OK(_COND_rw(read, fd, buf, count))
#define TEST_write(fd, buf, count)  TEST_OK(_COND_rw(write, fd, buf, count))
#define TEST_read_(fd, buf, count, message...) \
    TEST_OK_(_COND_rw(read, fd, buf, count), message)
#define TEST_write_(fd, buf, count, message...) \
    TEST_OK_(_COND_rw(write, fd, buf, count), message)
#define ASSERT_read(fd, buf, count)  ASSERT_OK(_COND_rw(read, fd, buf, count))
#define ASSERT_write(fd, buf, count) ASSERT_OK(_COND_rw(write, fd, buf, count))


/* A tricksy compile time bug checking macro modified from the kernel. */
#define COMPILE_ASSERT(e)           ((void) sizeof(struct { int:-!(e); }))

/* A rather randomly placed helper routine.  This and its equivalents are
 * defined all over the place, but there doesn't appear to be a definitive
 * definition anywhere. */
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof((a)[0]))

/* An agressive cast for use when the compiler needs special reassurance. */
#define REINTERPRET_CAST(type, value) \
    ( { \
        COMPILE_ASSERT(sizeof(type) == sizeof(typeof(value))); \
        union { \
            typeof(value) __value; \
            type __cast; \
        } __union = { .__value = (value) }; \
        __union.__cast; \
    } )

/* Companion to the offsetof macro for returning a value at the given offset
 * into a structure.  Returns correctly typed pointer. */
#define USE_OFFSET(type, structure, offset) \
    ((type *) ((void *) (structure) + (offset)))

/* For ignoring return values even when warn_unused_result is in force. */
#define IGNORE(e)       do if(e) {} while (0)


/* Use this to mark functions that can be constant folded, ie depend only on
 * their arguments and global state. */
#define __pure __attribute__((pure))
/* This function is a stronger variant of pure for functions which don't even
 * look at global memory.
 *    In truth and in practice we can get away with using this on functions
 * which inspect constant global memory.  Note however that pointer arguments
 * cannot be traversed by functions with this attribute. */
#define __const_ __attribute__((const))
