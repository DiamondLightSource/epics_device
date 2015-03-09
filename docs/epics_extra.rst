..  py:currentmodule:: epics_device

Extra EPICS Device Support
==========================

A number of extra helper routines and other facilities are provided through the
header file ``epics_extra.h``.

Initialisation Etc
------------------

..  function:: bool initialise_epics_extra(void)

    If any of the functionality documented here is to be used this function must
    be called exactly once before :func:`iocInit` and before calling any other
    function list here.

..  function:: void wait_for_epics_start(void)

    This call will block until EPICS has completed initialisation, as reported
    by the :macro:`initHookAtEnd` initialisation hook event.

..  function:: bool check_epics_ready(void)

    This call checks whether the completion of EPICS initialisation has been
    reported yet.  If ``true`` is returned the :func:`wait_for_epics_start` is
    guaranteed not to block.

..  function:: bool start_caRepeater(void)

    This will start a background caRepeater thread.  This is useful for embedded
    use where arranging for an extra caRepeater application to run is an extra
    annoyance.

..  function::
    void database_add_macro( \
        const char *macro, const char *format, ...)
    bool database_load_file(const char *filename)

    These two functions are designed to be used together to load EPICS database
    files, and are designed to be called before calling :func:`iocInit`.  If
    macro expansion is required while loading the database call
    :func:`database_add_macro` for each macro to be defined before then calling
    :func:`database_load_file`.  This call will then reset the list of macros,
    so this process can be repeated as necessary with a new list of macros.

    The following shows a simple application of this API::

        database_add_macro("DEVICE", "%s", ioc_name);
        database_add_macro("NUMBER", "%d", value);
        return database_load_file(db);

    Note that in the current implementation there is no effort to cope with
    irregular characters in the macro definitions.  In particular commas,
    backslashes and quotation marks will almost certainly cause
    :func:`database_load_file` to fail or malfunction.


Coherent Record Updates
-----------------------

When publishing a large value to EPICS, particularly when publishing waveforms,
it is important to ensure that the data seen over Channel Access is coherent.
The functions provided here are designed to support this by providing a
mechanism to ensure that data to be read by EPICS is not updated while EPICS
record processing is reading it.

These functions are designed to be used together with the :py:func:`Trigger`
function, see that link for an example.

..  type:: struct epics_interlock

    This is an opaque type representing an interlock consisting of the two EPICS
    records created by :py:func:`Trigger` and supporting the methods listed
    below.  Values are created by :func:`create_interlock`.

..  function:: struct epics_interlock *create_interlock( \
        const char *base_name, bool set_time)

    Publishes two records, one an I/O triggered ``bi`` record named `base_name`\
    ``:TRIG``, the other a ``bo`` record named `base_name`\ ``:DONE``.  The flag
    `set_time` determines that timestamps are to be specified by the IOC (rather
    than using default timestamping) if ``true``.

    It is essential that the ``:TRIG`` record is forward linked to process the
    ``:DONE`` record, as otherwise :func:`interlock_wait` will hang.  This
    linkage is automatically managed by :py:func:`Trigger`.

..  function:: void interlock_wait(struct epics_interlock *interlock)

    This function blocks until either all EPICS initialisation is complete, as
    reported by :func:`check_epics_read`, or until any previous record
    processing has complete, as signalled by the ``:DONE`` record processing.
    This should be called before updating any data that will be read by the
    record processing change processed from the ``:TRIG`` record generated as
    part of the :type:`epics_interlock`.

..  function:: void interlock_signal( \
        struct epics_interlock *interlock, struct timespec *ts)

    When data processing is complete this function should be called to trigger
    the ``:TRIG`` record and all the associated data records.
