Error Handling Macros and Other Facilities
==========================================

The header file ``error.h`` is designed to support a particular style of error
handling.  The general approach is that functions which can fail should return a
boolean return code, returning ``true`` if successful, and returning ``false``
and reporting the failure reason if failed.  A sequence of such functions can
then be combined with the ``&&`` operator so that the overall result is success
only if every stage succeeds.  This can in some sense be thought of as a more
controlled variant of exception handling, or on the other hand as combining
success through the ``Maybe`` monad.

Functions can be cascaded in this style if they conform to the following
requirements:

1.  ``true`` is returned only on successful completion, otherwise all further
    processing can be aborted.

2.  When ``false`` is returned the error has been reported and handled, all that
    remains to be done is to abort further processing.

An example of this style of coding can be seen in the first half of the
implementation of :func:`ioc_main` in ``examples/example_ioc/src/main.c``::

    static bool ioc_main(void)
    {
        return
            initialise_epics_device()  &&

            initialise_example_pvs()  &&
            start_caRepeater()  &&
            hook_pv_logging("db/access.acf", 10)  &&
            load_persistent_state(
                persistence_file, false, persistence_interval)  &&

            TEST_IO(dbLoadDatabase("dbd/example_ioc.dbd", NULL, NULL))  &&
            TEST_IO(example_ioc_registerRecordDeviceDriver(pdbbase))  &&
            load_database("db/example_ioc.db")  &&
            TEST_OK(iocInit() == 0);
    }

The second half of this example uses the error handling macros.  Not all
functions are of the form described above, returning ``true`` on success,
reporting their own failure and returning ``false`` on failure, so the macros in
this file provide functions to convert functions of a more familiar form into
the format described here.

For example, the macro :func:`TEST_IO` takes as argument an expression (which
may be an assignment) which following the usual C library calling convention,
that is, which returns -1 on failure and sets :data:`errno`, otherwise returns
some other value.  The :func:`TEST_IO` macro ensures that the return code is
tested and an error message is reported if necessary.


Core Handling Macros
--------------------

The error handling macros defined here are of a uniform style.  For each class
of test three macros are defined, for example for ``IO`` we have the following:
:func:`TEST_IO`, :func:`TEST_IO_`, :func:`ASSERT_IO`.  These three patterns
behave thus:

``TEST_xx(expr)``
    If the test fails a canned error message (defined by the macro
    :macro:`ERROR_MESSAGE`) is generated and the macro evaluates to ``false``,
    otherwise evaluates to ``true``.

``TEST_xx_(expr, format...)``
    If the test fails then the given error message (with :func:`sprintf`
    formatting) is generated and the macro evaluates to ``false``, otherwise
    ``true``.

``ASSERT_xx(expr)``
    If the test fails then an error report is printing showing the calling file
    name and line number, a traceback is printed if possible, and program
    execution is terminated.  Execution does not continue from this point.

The following groups of tests are defined:

..  macro::
    TEST_IO(expr)
    TEST_IO_(expr, format...)
    ASSERT_IO(expr)

    For these macros an error is reported when `expr` evaluates to -1, in which
    case it is assumed that :data:`errno` has been set to a relevant error code,
    and it is reported as part of the error message.

..  macro::
    TEST_OK(expr)
    TEST_OK_(expr, format...)
    ASSERT_OK(expr)

    These macros all treat `expr` as a boolean, reporting an error if the result
    is ``false``.  No extra error information is included in the error message.

..  macro::
    TEST_NULL(expr)
    TEST_NULL_(expr, format...)
    ASSERT_NULL(expr)

    These all report an error if `expr` evaluates to ``NULL``, but no extra
    error information is included

..  macro::
    TEST_NULL_IO(expr)
    TEST_NULL_IO_(expr, format...)
    ASSERT_NULL_IO(expr)

    These all report an error if `expr` evaluates to ``NULL``, and it is assumed
    that :data:`errno` has been set to a valid value which is used to report
    extra error information.

..  macro::
    TEST_PTHREAD(expr)
    TEST_PTHREAD_(expr, format...)
    ASSERT_PTHREAD(expr)

    These macros are designed to be used with the ``<pthread.h>`` family of
    functions.  These all return 0 on success and a non-zero error code which is
    compatible with :data:`errno` on failure.  :data:`errno` is updated with the
    failure return code by these macros if an error is reported.


Auxilliary Error Handling Macros
--------------------------------

The following macros are used as helpers.

..  function:: void print_error(message...)

    Prints an error message through the error handling mechanism.

..  macro:: ASSERT_FAIL()

    Functionally equivalent to ``ASSERT_OK(false)``, unconditionally terminates
    execution and does not return.

..  macro:: FAIL_(message...)

    Used to return a failure error message, functionally equivalent to
    ``TEST_OK_(false, message...)``.

..  macro:: DO(action)

    Used to convert a function returning ``void``, or indeed any sequence of C
    statements, into a successful expression.  Useful for including an
    unconditionally successful call in a sequence of error tests.  To be used
    sparingly, as can be used to produce nasty looking code.

..  macro::
    IF(test, iftrue)
    IF_ELSE(test, iftrue, iffalse)

    Conditional execution of tested functions.  In both cases `test` is a
    boolean test; if it evaluates to ``true`` then the `iftrue` expression is
    evaluated, otherwise `iffalse` (if specified).  Again, should be used
    sparingly, when needed to help in the cascading of error checking functions.

..  macro:: COMPILE_ASSERT(expr)

    This macro forces a compile time error if `expr` evaluates to ``false`` at
    compile time.  Alas this cannot be used at the top declaration level.


Miscellaneous Helpers
---------------------

These macros have no other natural home and have found their place in this
header file.

..  macro:: size_t ARRAY_SIZE(type array[])

    If the number of elements of `array` is known at compile time this macro
    returns the number of elements.

..  macro:: type REINTERPRET_CAST(type, value)

    In some situations the compiler will not accept an ordinary C cast of the
    form ``(type) value`` because of anxieties about aliasing, or if a ``const``
    attribute needs to be removed, or if some other low level bit preserving
    conversion is required.  This macro performs this cast in a more compiler
    friendly manner (via a ``union`` type), and checks that `value` and `type`
    have the same size.

    For example, this macro is used to remove the ``const`` attribute from a
    hashtable key in ``hashtable.c`` thus (here :func:`release_key` takes a
    ``void *`` argument)::

        static void release_key(struct hash_table *table, const void *key)
        {
            table->key_ops->release_key(REINTERPRET_CAST(void *, key));
        }

    Another application is the following which extracts the bit pattern of a
    floating point number as an integer::

        uint32_t bit_pattern = REINTERPRET_CAST(uint32_t, 0.1F);

..  macro:: IGNORE(expr)

    Discards a return value without compiler warning even when
    ``warn_unused_result`` is in force.


Extending Error Handling
------------------------

There are two aspects to extending error handling: intercepting the generated
error message and creating new error macros.

..  type:: typedef void (*error_hook_t)(const char *message)

    A function of this type is called to output each error message.  The default
    action is to print the message followed by a newline character to
    ``stderr``.

..  function:: error_hook_t set_error_hook(error_hook_t hook)

    The default error reporting action can be modified by calling this function.
    The previous error handling function is returned.  The error hook must be
    valid.

To extend error handling two auxillary macros ``COND`` and ``EXTRA`` need to be
defined and passed to the :func:`_TEST` and :func:`_ASSERT` macros, for instance
the IO macros are defined by the following code::

    #define _COND_IO(expr)              ((intptr_t) (expr) != -1)
    #define _MSG_IO(expr)               _extra_io()
    #define TEST_IO_(expr, message...)  _TEST(_COND_IO, _MSG_IO, expr, message)
    #define TEST_IO(expr)               TEST_IO_(expr, ERROR_MESSAGE)
    #define ASSERT_IO(expr)             _ASSERT(_COND_IO, _MSG_IO, expr)

Here :func:`_extra_io` is an auxiliary function which computes a sensible error
message from the error code in :data:`errno`.  Note that the cast in
``_COND_IO`` is needed to cope with functions which can return pointers or an
error code of -1 (hang your head in shame, :func:`mmap`).  These are the helper
macros used here:

..  macro:: _TEST(COND, EXTRA, expr, message...)

    Remembers the result of evaluating `expr`, invokes `COND` on the result, if
    that returns ``false`` then invokes `EXTRA` on the result, finally performs
    a full error report using `message` as the format string.  Note that any
    non ``NULL`` value returned by `EXTRA` will be freed by a call to
    :func:`free`.

..  macro:: _ASSERT(COND, EXTRA, expr)

    As for :func:`_TEST` invokes `expr`, then `COND`, then `EXTRA` if necessary
    before reporting a fatal error and halting program execution.
