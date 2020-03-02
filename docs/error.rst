Error Handling Macros and Other Facilities
==========================================

The header file ``error.h`` is designed to support a particular style of error
handling.  The general approach is that functions which can fail should return a
special :type:`error__t` return code, returning ``ERROR_OK`` if successful, and
returning a code with contains a report of the failure reason if failed.  A
sequence of such functions can then be combined with the ``?:`` operator so that
the overall result is success only if every stage succeeds.  This can in some
sense be thought of as a more controlled variant of exception handling, or on
the other hand as combining success through the ``Maybe`` monad.

Functions can be cascaded in this style if they conform to the following
requirements:

1.  ``ERROR_OK`` is returned only on successful completion, otherwise all
    further processing can be aborted.  Note that ``ERROR_OK`` tests as
    ``false`` when treated as a boolean.

2.  When any other error code is returned (testing ``true`` when treated as a
    boolean) the error code can be handed up, or reported there and then.

3.  Every error code must be subject to one of the following fates:

    a)  Passed up to the caller for further processing.
    b)  Explicitly reported using :func:`error_report` or a related function.
    c)  Explicitly discarded using :func:`error_discard`.

    In particular, values of type :type:`error__t` must not be silently dropped
    as otherwise errors can go unreported and there will be a memory leak.

An example of this style of coding can be seen in the first half of the
implementation of :func:`!ioc_main` in ``examples/example_ioc/src/main.c``::

    static error__t ioc_main(void)
    {
        return
            initialise_epics_device()  ?:

            initialise_example_pvs()  ?:
            start_caRepeater()  ?:
            hook_pv_logging("db/access.acf", 10)  ?:
            load_persistent_state(
                persistence_file, persistence_interval, false)  ?:

            TEST_IO(dbLoadDatabase("dbd/example_ioc.dbd", NULL, NULL))  ?:
            TEST_IO(example_ioc_registerRecordDeviceDriver(pdbbase))  ?:
            load_database("db/example_ioc.db")  ?:
            TEST_OK(iocInit() == 0);
    }

The second half of this example uses the error handling macros.  Not all
functions are of the form described above returning an :type:`error__t` error
code, so the macros in this file provide functions to convert functions of a
more familiar form into the format described here.

For example, the macro :func:`TEST_IO` takes as argument an expression (which
may be an assignment) which following the usual system library calling
convention, that is, which returns -1 on failure and sets :data:`errno`,
otherwise returns some other value.  The :func:`TEST_IO` macro ensures that the
return code is tested and an error message is generated if necessary.


Core Handling Macros
--------------------

The error handling macros defined here are of a uniform style.  For each class
of test three macros are defined, for example for ``IO`` we have the following:
:func:`TEST_IO`, :func:`TEST_IO_`, :func:`ASSERT_IO`.  These three patterns
behave thus:

``TEST_xx(expr)``
    If the test fails an error code representing a canned error message (defined
    by the macro :macro:`!ERROR_MESSAGE`) is returned, otherwise
    ``ERROR_OK`` is returned.

``TEST_xx_(expr, format...)``
    If the test fails then an error code representing the given error message
    (with :func:`sprintf` formatting) is returned, otherwise ``ERROR_OK``.

``ASSERT_xx(expr)``
    If the test fails then an error report is printing showing the calling file
    name and line number, a traceback is printed if possible, and program
    execution is terminated.  Execution does not continue from this point.

The following groups of tests are defined:

..  macro::
    error__t TEST_IO(expr)
    error__t TEST_IO_(expr, format...)
    ASSERT_IO(expr)

    For these macros an error is reported when `expr` evaluates to -1, in which
    case it is assumed that :data:`errno` has been set to a relevant error code,
    and it is reported as part of the error message.

..  macro::
    error__t TEST_OK(expr)
    error__t TEST_OK_(expr, format...)
    ASSERT_OK(expr)

    These macros all treat `expr` as a boolean, reporting an error if the result
    is ``false``.  No extra error information is included in the error message.

..  macro::
    error__t TEST_OK_IO(expr)
    error__t TEST_OK_IO_(expr, format...)
    ASSERT_OK_IO(expr)

    These all report an error if `expr` evaluates to ``false``, and it is
    assumed that :data:`errno` has been set to a valid value which is used to
    report extra error information.

..  macro::
    error__t TEST_PTHREAD(expr)
    error__t TEST_PTHREAD_(expr, format...)
    ASSERT_PTHREAD(expr)

    These macros are designed to be used with the ``<pthread.h>`` family of
    functions.  These functions all return 0 on success and a non-zero error
    code which is compatible with :data:`errno` on failure.  Extra information
    from this error code is included in the returned result.

All of the ``TEST_`` macros above return a value of the following type:

..  type:: error__t

    This type encapsulates an error message or success, represented by the value
    ``ERROR_OK``.  Standard C error checking can be used to test for error or
    success: testing ``ERROR_OK`` behaves as ``false``, any error code
    representing failure tests as true.

    As noted above, error handling functions are designed to be chained with the
    ``?:`` syntax.  Note also that error values **must** be handled with
    :func:`error_report` or :func:`error_discard`.


Auxilliary Error Handling Macros
--------------------------------

The following macros are used as helpers.

..  macro:: ASSERT_FAIL( )

    Functionally equivalent to ``ASSERT_OK(false)``, unconditionally terminates
    execution and does not return.

..  macro::
    error__t FAIL( )
    error__t FAIL_(message...)

    Used to return a failure error code, functionally equivalent to
    ``TEST_OK(false)`` or ``TEST_OK_(false, message...)``.

..  macro:: error__t DO(action)

    Used to convert a function returning ``void``, or indeed any sequence of C
    statements, into a successful expression.  Useful for including an
    unconditionally successful call in a sequence of error tests.

..  macro::
    error__t IF(test, iftrue)
    error__t IF_ELSE(test, iftrue, iffalse)

    Conditional execution of tested functions.  In both cases `test` is a
    boolean test; if it evaluates to ``true`` then the `iftrue` expression is
    evaluated, otherwise `iffalse` (if specified).

..  macro:: error__t TRY_CATCH(action, on_fail...)

    This macro provides a limited form of exception handling.  `action` must
    return an :type:`error__t`, which is returned by this macro.  If the error
    code is not ``ERROR_OK`` then the code `on_fail` is executed before
    returning.  Note that any value returned by `on_fail` is discarded.

..  macro:: error__t DO_FINALLY(action, finally...)

    This macro unconditionally executes `finally` after `action` has been
    evaluated, and returns the error code from `action`.  Again, any value
    returned by `finally` is discarded.

    The only difference from :macro:`TRY_CATCH` is that the `finally` code is
    unconditionally executed by :macro:`DO_FINALLY`.

For the three multi-part macros ``IF_ELSE``, ``TRY_CATCH`` and ``DO_FINALLY``,
the only separator between the key parts is a single comma character, so layout
and an extra comment should be used to structure these, as shown below::

    IF_ELSE(test,
        iftrue,
    //else
        iffalse)  ?:
    TRY_CATCH(
        action,
    //catch
        on_fail)  ?:
    DO_FINALLY(
        action,
    //finally
        finally);


Error Reporting and Management
------------------------------

The functions described here are used for reporting, discarding, or otherwise
managing error codes.

..  function:: bool error_report(error__t error)

    Converts `error` into a string and uses :func:`log_error` to report the
    error.  This is the normal destination for all error codes.  ``true`` is
    returned if `error` was an error (and an error message was reported), and
    ``false`` is returned if `error` is ``ERROR_OK``, in which case no action
    was taken.

    This function disposes of `error`, and this value is no longer valid.

..  macro:: bool ERROR_REPORT(error, format...)

    This helper macro will augment `error` with the message defined by
    `format` before reporting the error by calling :func:`error_report`.

..  function:: bool error_discard(error__t error)

    This function silently discards `error`, after which the value is invalid.
    As for :func:`error_report`, ``true`` is returned if `error` was an error,
    and ``false`` if it was ``ERROR_OK``.

..  function:: error__t error_extend(error__t error, const char *format, ...)

    If `error` is not ``ERROR_OK`` then the information associated with `error`
    is augmented with the message defined by `format`.  The lifetime of `error`
    is unaffected, and the original `error` is also returned.

    If `error` is ``ERROR_OK`` the `format` is ignored and ``ERROR_OK`` is
    returned.

..  function:: const char *error_format(error__t error)

    Returns a formatted string representing the error code.  The lifetime of the
    returned string is identical to the lifetime of `error`, which must still be
    reported or discarded at the appropriate time.


Message Logging Control
-----------------------

By default all error messages are sent to ``stderr``, but syslog can be used
instead.

..  function:: void vlog_message(int priority, const char *format, va_list args)

    Sends the given message to ``stderr`` or to syslog, depending on whether
    :func:`start_logging` has been called.

..  function::
    void log_message(const char *message, ...)
    void log_error(const char *message, ...)

    Calls :func:`vlog_message` with with `priority` set to ``LOG_INFO`` for
    :func:`log_message`, and ``LOG_ERR`` for :func:`log_error`.

    Note that :func:`error_report` uses :func:`log_error`.

..  function:: void start_logging(const char *ident)

    This invokes :func:`!openlog` (3) and sends all future messages to the
    system log with the log identifier `ident`.


Miscellaneous Helpers
---------------------

These macros have no other natural home and have found their place in this
header file.

..  macro:: size_t ARRAY_SIZE(type array[])

    If the number of elements of `array` is known at compile time this macro
    returns the number of elements.

..  macro:: to_type CAST_FROM_TO(from_type, to_type, value)

    In some situations the compiler will not accept an ordinary C cast of the
    form ``(type) value`` because of anxieties about aliasing, or if a ``const``
    attribute needs to be removed, or if some other low level bit preserving
    conversion is required.  This macro performs this cast in a more compiler
    friendly manner (via a ``union`` type), and checks that `value` has type
    `from_type` and that `to_type` and `value` have the same size.

    For example, this macro is used to remove the ``const`` attribute from a
    hashtable key in ``hashtable.c`` thus (here :func:`!release_key` takes a
    ``void *`` argument)::

        static void release_key(struct hash_table *table, const void *key)
        {
            table->key_ops->release_key(
                CAST_FROM_TO(const void *, void *, key));
        }

    Another application is the following which extracts the bit pattern of a
    floating point number as an integer::

        uint32_t bit_pattern = CAST_FROM_TO(float, uint32_t, 0.1F);

..  macro:: to_type CAST_TO(to_type, value)

    This is a short-cut wrapper for :macro:`CAST_FROM_TO` for use in the case
    when `from_type` is no more specific than ``typeof(value)``.

..  macro:: type ENSURE_TYPE(type, value)

    This is a weak cast from `value` to `type` which ensures that it is valid to
    assign `value` to this `type`.  Note that this will not work if `type` is a
    written out function type, in this case a typedef name would have to be
    used.

..  macro:: IGNORE(expr)

    Discards a return value without compiler warning even when
    ``warn_unused_result`` is in force.

..  macro:: unlikely(expr)

    Provides a hint to the compiler that ``expr`` is likely to be ``0``, can
    help in the optimisation of very rarely taken branches.

..  macro::
    COMPILE_ASSERT(expr)
    STATIC_COMPILE_ASSERT(expr)

    This macro forces a compile time error if `expr` evaluates to ``false`` at
    compile time.  :macro:`COMPILE_ASSERT` must be used inside a function
    declaration, while :macro:`STATIC_COMPILE_ASSERT` must be used at the top
    declaration level.

..  macro::
    MIN(x, y)
    MAX(x, y)

    Macro safe minimum and maximum functions.

..  macro:: container_of(ptr, type, member)

    Casts a member of a structure out to the containing structure.  For example,
    given `py` constructed thus::

        struct xy { int x, y; } xy;
        int *py = &xy.y;

    a pointer to `xy` can be reconstructed as::

        struct xy *pxy = container_of(py, struct xy, y);

..  macro:: _WITH_ENTER_LEAVE(enter, leave)

    This macro is used to construct helper macros for wrapping enter and exit
    code around a block.  Instances of this macro must be followed by a single
    statement or a block of code in braces, and the `enter` and `leave` clauses
    are used to bracket the code.  In other words, the following use of this
    macro::

        _WITH_ENTER_LEAVE(enter, leave)
        {
            statements;
        }

    is exactly equivalent to::

        {
            enter;
            statements;
            leave;
        }

    Note that the `enter` clause may include the declaration of a local
    variable, the scope of which includes the statements following and the
    `leave` clause.

    ..  warning::

        ``break`` and ``return`` must **not** be used to exit the statement
        block guarded by :macro:`_WITH_ENTER_LEAVE` or any of its derivatives.
        Using ``break`` will restart the block after reinvoking `enter`, and
        using ``return`` will bypass `leave`.

..  macro:: WITH_MUTEX(mutex)

    This macro wraps the statement or block of code that follows with calls to
    ``pthread_mutex_lock(&mutex)`` and ``pthread_mutex_unlock(&mutex)``.  The
    locking call is checked for success and an assertion failure is raised if
    an error is detected.

    ..  warning::

        Do **not** exit the guarded block with ``break`` or ``return``.

..  macro:: WITH_MUTEX_UNCHECKED(mutex)

    Similar to :macro:`WITH_MUTEX`, except that lock errors are ignored.

    ..  warning::

        Do **not** exit the guarded block with ``break`` or ``return``.

..  macro:: error__t ERROR_WITH_MUTEX(mutex, error)

    This wraps mutex locking around an error expression, and evaluates to the
    result of evaluating `error` under the lock, which must be an expression of
    type :type:`error__t`.


Pitfalls
--------

The main pitfall is that if an error code is discarded then a memory leak will
be created and errors will not be reported.  Unfortunately it is very easy to do
this by mistake.

1.  Deliberately discarding the error code.

    Example where error code is discarded, here we want to convert an
    :type:`error__t` into a boolean indicating success::

        error__t test_function(void) { ... }

        bool bad_drop_error(void) {
            return !test_function();
        }

    In this case the error code is silently dropped.  This should be rewritten
    in one of the following two forms::

        bool noisy_drop_error(void) {
            return !error_report(test_function());
        }

    if the error should be reported, or::

        bool quiet_drop_error(void) {
            return !error_discard(test_function());
        }

    if the error needs to be silently discarded.

2.  Accidentially discarding the error code.

    Inevitably there are many ways of doing this, but one way is particularly
    easy and unfortunate: consider this code::

        error__t chained(void) {
            error__t error =
                function1()  ?:
                function2();
                function3()  ?:
                function4();
            return error;
        }

    Oops.  This code has two very unfortunate behaviours.  Firstly, any error
    code returned by ``function3()`` or ``function4()`` will be silently
    discarded ... and worse, even if ``function1()`` or ``function2()`` fails,
    the last two functions will still be called.

    Do try not to do this.  I think it's impossible to persuade the compiler to
    pick this up, alas.
