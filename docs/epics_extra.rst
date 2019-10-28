..  py:currentmodule:: epics_device

Extra EPICS Device Support
==========================

A number of extra helper routines and other facilities are provided through the
header file ``epics_extra.h``.

Initialisation Etc
------------------

..  function:: void wait_for_epics_start(void)

    This call will block until EPICS has completed initialisation, as reported
    by the :macro:`initHookAtEnd` initialisation hook event.

..  function:: bool check_epics_ready(void)

    This call checks whether the completion of EPICS initialisation has been
    reported yet.  If ``true`` is returned the :func:`wait_for_epics_start` is
    guaranteed not to block.

..  function:: error__t start_caRepeater(void)

    This will start a background caRepeater thread.  This is useful for embedded
    use where arranging for an extra caRepeater application to run is an extra
    annoyance.

..  function::
    void database_add_macro( \
        const char *macro, const char *format, ...)
    error__t database_load_file(const char *filename)

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
    reported by :c:func:`check_epics_ready`, or until any previous record
    processing has complete, as signalled by the ``:DONE`` record processing.
    This should be called before updating any data that will be read by the
    record processing change processed from the ``:TRIG`` record generated as
    part of the :type:`epics_interlock`.

..  function:: void interlock_signal( \
        struct epics_interlock *interlock, struct timespec *ts)

    When data processing is complete this function should be called to trigger
    the ``:TRIG`` record and all the associated data records.


Records With Data
-----------------

..  note::

    The API described in this section is *not* intended as core functionality
    (despite its complexity), instead it is an experimental extension designed
    to be used sparingly.

Although in most cases it is enough to bind a published "in" record to a single
value, there are a couple of cases where both the published value and the
associated record need to be managed together:

1.  When a single value is being updated and triggered separately from any other
    records.  The standard implementation for this is::

        struct epics_record *record1;
        double value1;
        ...
        // Publish with this code
        record1 = PUBLISH_READ_VAR_I(ai, "NAME", value1);
        ...
        // Update with this code
        value1 = update_value();
        trigger_record(record1);

2.  When both the value and severity associated with the record need to be
    maintained.  The standard implementation is::

        struct epics_record *record2;
        double value2;
        ...
        // Publish with this code
        record2 = PUBLISH_READ_VAR(ai, "NAME", value2);
        ...
        // Update with this code
        value2 = update_value();
        set_record_severity(record2, severity);

It is a little irritating to have to carry two values around to perform a single
function, so the "records with data" API provides support so that for example
the first code example above can be replaced by::

    struct in_epics_record_ai *record1;
    ...
    record1 = PUBLISH_IN_VALUE_I(ai, "NAME");
    ...
    WRITE_IN_RECORD(ai, record1, update_value());

The API consists of the following definitions.

..  type::
    struct in_epics_record_longin
    struct in_epics_record_ulongin
    struct in_epics_record_ai
    struct in_epics_record_bi
    struct in_epics_record_stringin
    struct in_epics_record_mbbi

    Each EPICS "in" record type has an associated record wrapper type.  These
    are created by the appropriate :func:`PUBLISH_IN_VALUE` call and can be
    passed to any of the other functions documented in this section.

..  macro::
    struct in_epics_record_##record *PUBLISH_IN_VALUE( \
        record, name, .set_time, .merge_update)
    struct in_epics_record_##record *PUBLISH_IN_VALUE_I( \
        record, name, .set_time, .merge_update)

    ========================================================================== =
    record class `record`
    const char \*\ `name`
    bool `set_time`
    bool `merge_update`
    Returns in_epics_record\_\ `record`\*
    ========================================================================== =

    Returns a pointer to the appropriate in_epics_record\_\ `record`
    structure.  A record of the given type and name is published and storage for
    the associated value is created and initialised to zero.  `set_time` has the
    same meaning as for :macro:`PUBLISH`.  Unless `merge_update` is set to true
    every update to the returned value will generate an EPICS value update.

    If the ``_I`` suffix is used then the record will be created with ``I/O
    Intr`` processing support, and the records ``SCAN`` field must be set to
    this.

..  macro:: WRITE_IN_RECORD(record, in_record, value, \
        .severity, .timestamp, .force_update)

    ========================================================================== =
    record class `record`
    in_epics_record\_\ `record` \*\ `in_record`
    TYPEOF(`record`) `value`
    bool `severity`
    const struct timespec \*\ `timestamp`
    bool `force_update`
    ========================================================================== =

    This call will update the value associated with `in_record` with `value` and
    if the record was created with ``I/O Intr`` support then record processing
    will be triggered.  The optional argument `severity` can be set to specify
    record severity, otherwise severity 0 will be written.

    If the record was created with `set_time` set then a timestamp should be
    passed using the `timestamp` parameter.

    If the record was created with `merge_update` set then `force_update` can be
    used to force an update.

..  macro:: WRITE_IN_RECORD_SEV(record, in_record, severity, .timestamp)

    ========================================================================== =
    record class `record`
    in_epics_record\_\ `record` \*\ `in_record`
    bool `severity`
    const struct timespec \*\ `timestamp`
    ========================================================================== =

    This updates the `severity` associated with `in_record` without changing the
    value and triggers a record update if appropriate.  If the record was
    declared with `set_time` then `timestamp` must be specified.  Note that
    updating is always forced for this call.

..  macro:: TYPEOF(record) READ_IN_RECORD(record, in_record)

    ========================================================================== =
    record class `record`
    in_epics_record\_\ `record` \*\ `in_record`
    Returns TYPEOF(`record`)
    ========================================================================== =

    Returns the current value associated with `in_record`.
