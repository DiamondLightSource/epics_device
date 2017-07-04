Core EPICS Device Functionality
===============================

The EPICS Device support module is used to create and manage a tight binding
between EPICS PVs and a "driver" implementation.  The so called "driver" layer
implements the actual functionality of the IOC, while the "device" layer
provided by this support module provides binding between the "driver" and EPICS.

The functionality described here is defined in the header file
``epics_device.h``.

The design of this module implements a one-to-one correspondence between PVs and
internally published PV names, and the builder support code is designed to help
maintain this correspondence.

Using this module involves the following steps:

1.  The support module itself must be initialised by calling the function
    :func:`initialise_epics_device` before any PVs are declared.

2.  All PVs should be internally published using the :func:`PUBLISH` macros and
    numerous allies.  This process establishes a set of valid internal PV names
    with associated processing actions: each :func:`PUBLISH` call binds one name
    to a processing action and possibly an initialisation action.

3.  If persistence is used then :func:`load_persistent_state` should now be
    called.

4.  Finally the IOC itself can be started.  This involves at least the following
    steps:

    * Load the IOC ``.dbd`` file, which must include the ``epics_device.dbd``
      file and complete database registration.

    * Load the database.  This should contain record definitions corresponding
      to each internal PV name published above.  The :doc:`builder` chapter
      documents how to create this database.

    * Finally :func:`iocInit` can be called.

In this chapter we document the core PV publishing API.


Initialisation
--------------

One function is provided for initialisation.

..  function:: error__t initialise_epics_device(void)

    This function must be called at least once before publishing any records or
    calling any other function listed here.  Repeated calls have no further
    effect.

    This function can be called from the IOC shell, but in this case the return
    code is lost.


PUBLISH Overview
----------------

The following record types are supported: ``ai``, ``bi``, ``longin``, ``mbbi``,
``stringin``, ``ao``, ``bo``, ``longout``, ``mbbo``, ``stringout``,
``waveform``, together with aliases ``ulongin`` and ``ulongout``.  For
the C datatype corresponding to each record type see :func:`TYPEOF`.

Note that ``ulongin`` and ``ulongout`` are aliases for ``longin`` and
``longout``, but supporting unsigned types internally.  The data type over
Channel Access is still signed, but internally the driver treats the values as
unsigned.

EPICS records are published with the use of a variety of ``PUBLISH...()``
macros, defined below.  Three classes of record are supported with slightly
different macros and arguments.  The table below summarises the options for
record publishing.

============================================================================== =
IN records
============================================================================== =
Record types: ``[u]longin``, ``ai``, ``bi``, ``stringin``, ``mbbi``
:func:`PUBLISH(record, name, read, .context, .io_intr, .set_time) <PUBLISH>`
:func:`PUBLISH_READ_VAR[_I](record, name, variable) <PUBLISH_READ_VAR>`
:func:`PUBLISH_READER[_I](record, name, reader) <PUBLISH_READER>`
:func:`PUBLISH_TRIGGER[_T](name) <PUBLISH_TRIGGER>`
============================================================================== =

============================================================================== =
OUT records
============================================================================== =
Record types: ``[u]longout``, ``ao``, ``bo``, ``stringout``, ``mbbo``
:func:`PUBLISH(record, name, write, .init, .context, .persist) <PUBLISH>`
:func:`PUBLISH_WRITE_VAR[_P](record, name, variable) <PUBLISH_WRITE_VAR>`
:func:`PUBLISH_WRITER[_B][_P](record, name, writer) <PUBLISH_WRITER>`
:func:`PUBLISH_ACTION(name, action) <PUBLISH_ACTION>`
============================================================================== =

=========================================================================================================================================================== =
WAVEFORM records
=========================================================================================================================================================== =
Record type: ``waveform``
Field types: ``char``, ``short``, ``int``, ``float``, ``double``
:func:`PUBLISH_WAVEFORM(field_type, name, length, process, .init, .context, .persist, .io_intr) <PUBLISH_WAVEFORM>`
:func:`PUBLISH_WF_READ_VAR[_I](field_type, name, length, waveform) <PUBLISH_WF_READ_VAR>`
:func:`PUBLISH_WF_WRITE_VAR[_P](field_type, name, length, waveform) <PUBLISH_WF_WRITE_VAR>`
:func:`PUBLISH_WF_ACTION{,_I,_P}(field_type, name, length, action) <PUBLISH_WF_ACTION>`
=========================================================================================================================================================== =

..  I really did want to do properly line wrapping above, but I can't split
    these very long markup lines over more than one line.

==========  ====================================================================
Suffixes:
==========  ====================================================================
``_I``      Sets `.io_intr` to enable ``I/O Intr`` scanning
``_P``      Sets `.persist` to enable persistent storage
``_T``      Sets `.set_time` to enable timestamp override
``_B``      Enables writer to return :type:`bool` result
==========  ====================================================================


Throughout this document the dotted arguments are optional and should be
specified using C99 named initialiser syntax, eg::

    PUBLISH(longin, "RECORD", on_read, .context = read_context).

Common Datatypes
~~~~~~~~~~~~~~~~

..  type:: EPICS_STRING

    This is a typedef::

        typedef struct { char s[40]; } EPICS_STRING;

    used for EPICS strings.  This form of declaration allows strings to be
    passed by value and thus supports a more uniform interface to the EPICS
    Driver software.

..  type:: struct epics_record

    This is an opaque structure type used to represent the return value from
    calling a ``PUBLISH...()`` macro.  The following functions can be called on
    values of this type depending on the underlying class of the defined record:

    ==================  =====================================================
    IN, WAVEFORM        :func:`trigger_record`, :func:`set_record_severity`,
                        :func:`set_record_timestamp`
    OUT                 :func:`WRITE_OUT_RECORD`
    WAVEFORM            :func:`WRITE_OUT_RECORD_WF`
    IN, OUT             :func:`READ_RECORD_VALUE`
    WAVEFORM            :func:`READ_RECORD_VALUE_WF`
    ==================  =====================================================


PUBLISH API
-----------

All the ``PUBLISH...()`` macros in this section and the `PUBLISH_WAVEFORM API`_
section return values of type ``struct epics_record*``.

..  macro:: TYPEOF(record)

    ========================================================================== =
    record class `record`
    ========================================================================== =

    Given one of the supported record type names listed in the table below, this
    macro computes the appropriate C datatype as shown:

    ==================  ==================  ====================
    In Record           Out Record          C Type
    ==================  ==================  ====================
    ai                  ao                  double
    bi                  bo                  bool
    longin              longout             int
    ulongin             ulongout            unsigned int
    mbbi                mbbo                unsigned int
    longin              longout             :type:`EPICS_STRING`
    ==================  ==================  ====================

    Thus the list of valid identifiers for "record class" `record` is:

        ``longin``, ``ulongin``, ``ai``, ``bi``, ``stringin``, ``mbbi``,
        ``longout``, ``ulongout``, ``ao``, ``bo``, ``stringout``, ``mbbo``

..  macro::
    PUBLISH(record, name, read, .context, .io_intr, .set_time)
    PUBLISH(record, name, write, .init, .context, .persist)

    ===================================================================== ======
    \                                                                     IN/OUT
    ===================================================================== ======
    record class `record`
    const char \*\ `name`
    void \*\ `context`
    bool `read`\ (void \*context, TYPEOF(`record`) \*value)               IN
    bool `io_intr`                                                        IN
    bool `set_time`                                                       IN
    bool `write`\ (void \*context, const TYPEOF(`record`) \*value)        OUT
    bool `init`\ (void \*context, TYPEOF(`record`) \*value)               OUT
    bool `persist`                                                        OUT
    ===================================================================== ======

    The PUBLISH macro is used to create a software binding for the appropriate
    record type to the given name.  The corresponding read or write method will
    be called when the record processes, and the macro ensures proper type
    checking.  Note that IN records and OUT records support different arguments,
    the first form is for IN records, the second for OUT records.

    The macros documented below provide support for more specialised variants of
    these records with hard-wired implementations of the read and write methods.

    Calling :func:`PUBLISH` returns a pointer to :type:`epics_record`.

    The arguments are as follows.

    `record`
        This identifies the record type, and must be one of ``longin``,
        ``ulongin``, ``ai``, ``bi``, ``stringin``, ``mbbi`` for IN records or
        one of ``ulongout``, ``longout``, ``ao``, ``bo``, ``stringout``,
        ``mbbo`` for OUT records.  Using any other identifier will generate a
        cryptic compiler error.

    `name`
        This is the internal name for the PV and must be passed as a C string.
        The string will be copied before :func:`PUBLISH` returns, so dynamically
        generated strings can be used here.  The same identifer should appear in
        the ``INP`` or ``OUT`` field of the record definition.

    `context`
        This is a `void*` pointer which can be used by the caller of
        :func:`PUBLISH` to bind the callbacks to any local context.  This
        pointer is passed unchanged to the `read`, `write`, and `init` methods.

    bool `read`\ (void \*context, TYPEOF(`record`) \*value)
        For IN records this method will be called when the record is
        processed.  If possible a valid value should be assigned to `*value`
        and ``true`` returned, otherwise false can be returned to indicate no
        value available, in which case the record will be marked as invalid.

    bool `write`\ (void \*context, const TYPEOF(`record`) \*value)
        For OUT records this will be called on record processing with the
        value written to the record passed by reference.  If the value is
        accepted then true should be return, otherwise if ``false`` is returned
        then value is treated as being rejected, the previous value of the
        record will be restored, and any associated Channel Access put will
        fail.

    bool `init`\ (void \*context, TYPEOF(`record`) \*value)
        For OUT records if this function is specified it will be called record
        initialisation to assign an initial value to the record unless a
        persistent initial value can be found.  ``false`` can be returned to
        indicate failure.  If `persist` is set and a value is successfully
        read from storage then this method will be ignored.

    `io_intr`
        If it is desired to operate an IN record with self generated triggering,
        i.e. with ``SCAN='I/O Intr'`` then this optional boolean flag must be
        set to ``true``.  If this is done record processing can then be
        triggered at any time by calling :func:`trigger_record`.  The ``_I``
        macro variants automatically set this flag.

        Note that ``I/O Intr`` processing of OUT records is deliberately not
        supported.

    `set_time`
        It is possible for the driver software to specify the timestamp of IN
        records.  This is done by setting ``TSE=-2`` and setting this optional
        boolean flag to ``true``.  In this case :func:`set_record_timestamp`
        must be used to explicitly set the record timestamp each time it
        processes.  The ``_T`` macro variant automatically sets this flag.

        Again, this facility is deliberately not supported for OUT records.

    `persist`
        OUT records can be marked for "persistence" by setting this optional
        boolean flag to ``true``.  If this is set then during record
        initialisation (during :func:`iocInit`) the persistence store will be
        checked for an initial value which will be loaded into the record
        instead of calling its `init` function.


The following macros provide specialisation for specific types of record.  See
the descriptions for :func:`PUBLISH` above for descriptions of arguments not
described below.

..  macro::
    PUBLISH_READ_VAR(record, name, variable)
    PUBLISH_READ_VAR_I(record, name, variable)

    ========================================================================== =
    record class `record`
    const char \*\ `name`
    TYPEOF(`record`) `variable`
    ========================================================================== =

    The given variable will be read each time the record is processed.  The
    variable must be of type ``TYPEOF(record)`` and should be passed by name to
    this macro.

..  macro::
    PUBLISH_READER(record, name, reader)
    PUBLISH_READER_I(record, name, reader)

    ========================================================================== =
    record class `record`
    const char \*\ `name`
    TYPEOF(`record`) `reader`\ (void)
    ========================================================================== =

    This will be called each time the record processes and should return the
    value to be used to update the record.

..  macro::
    PUBLISH_TRIGGER(name)
    PUBLISH_TRIGGER_T(name)

    ========================================================================== =
    const char \*\ `name`
    ========================================================================== =

    This record is useful for generating triggers into the database.  The record
    type is set to ``bi`` and the `io_intr` flag is set.  Call
    :func:`trigger_record` to make this record process, use ``FLNK`` in the
    database to build a useful processing chain.

    The ``_T`` option is available for generating triggers with time specified
    by :func:`set_record_timestamp` before calling :func:`trigger_record`.

..  macro::
    PUBLISH_WRITE_VAR(record, name, variable)
    PUBLISH_WRITE_VAR_P(record, name, variable)

    ========================================================================== =
    record class `record`
    const char \*\ `name`
    TYPEOF(`record`) `variable`
    ========================================================================== =

    The variable is written each time the record is processed and is read on
    startup to initialise the associated EPICS record.  The variable must be of
    type ``TYPEOF(record)``.

..  macro::
    PUBLISH_WRITER(record, name, writer)
    PUBLISH_WRITER_P(record, name, writer)

    ========================================================================== =
    record class `record`
    const char \*\ `name`
    void `writer`\ (TYPEOF(`record`) value)
    ========================================================================== =

    This method will be called each time the record processes with the current
    value of the record.

..  macro::
    PUBLISH_WRITER_B(record, name, writer)
    PUBLISH_WRITER_B_P(record, name, writer)

    ========================================================================== =
    record class `record`
    const char \*\ `name`
    bool `writer`\ (TYPEOF(`record`) value)
    ========================================================================== =

    This method will be called each time the record processes.  The writer can
    return a boolean to optionally reject the write, otherwise :type:`void` is
    returned and the write is unconditional.

..  macro::
    PUBLISH_ACTION(name, action)

    ========================================================================== =
    const char \*\ `name`
    void `action`\ (void)
    ========================================================================== =

    This method is called when the record processes.


PUBLISH_WAVEFORM API
--------------------

..  macro:: PUBLISH_WAVEFORM( \
        field_type, name, max_length, process, \
        .init, .context, .persist, .io_intr)

    ================================================================================= =
    type name `field_type`
    const char \*\ `name`
    size_t `max_length`
    void `process`\ (void \*context, field_type array[`max_length`], size_t \*length)
    void `init`\ (void \*context, field_type array[`max_length`], size_t \*length)
    void \*\ `context`
    bool `persist`
    bool `io_intr`
    ================================================================================= =

    This macro creates the software binding for waveform records with data of
    the specified type.  The process method will be called each time the record
    processes -- the process method can choose whether to implement reading or
    writing as the primitive operation.  Again, a pointer to :type:`record_type`
    is returned which can be used for triggering and access.

    EPICS waveform record support manages a buffer of length `max_length`.  A
    pointer to this buffer is passed into the `process` and `init` functions
    defined here during record processing and initialisation (respectively);
    it's up to the driver implementation to decide on the appropriate action to
    take.

    The arguments are as follows:

    `field_type`
        This specifies the type of values in the waveforms handled by this
        record.  One of the following identifiers must be used, otherwise a
        cryptic compiler error message will be generated, and the corresponding
        string must be written into the ``FTVL`` field:

        =============== =====================
        C type          ``FTVL`` setting
        =============== =====================
        ``char``        ``'CHAR'``
        ``short``       ``'SHORT'``
        ``int``         ``'LONG'``
        ``float``       ``'FLOAT'``
        ``double``      ``'DOUBLE'``
        =============== =====================

        Note that the ``int`` type is anomalous -- although EPICS uses the
        description ``'LONG'`` this must in fact be a 32-bit type.  The current
        implementation of EPICS Device assumes ``sizeof(int) ==
        sizeof(int32_t)`` and will fail on other targets.  Clearly this can be
        fixed if necessary.

    `max_length`
        This specifies the number of points in the waveform and must match the
        value specified in the ``NELM`` field of the record.

    `name`, `context`, `io_intr`, `persist`
        As documented above for :func:`PUBLISH`.  Note that as WAVEFORM records
        can act as either IN or OUT records, both types of functionality are
        supported.

    void `process`\ (void \*context, field_type array[`max_length`], size_t \*length)
        This is called during record processing with `*length` initialised with
        the current waveform length, as set in the ``NORD`` field of the the
        record.  The array can be read or written as required and `*length` (and
        thus ``NORD``) can be updated as appropriate if the data length changes
        (though of course `max_length` must not be exceeded).

    void `init`\ (void \*context, field_type array[`max_length`], size_t \*length)
        This optional function may be called during initialisation to initialise
        the waveform if a persistent value is not specified.


..  macro::
    PUBLISH_WF_READ_VAR(field_type, name, max_length, waveform)
    PUBLISH_WF_READ_VAR_I(field_type, name, max_length, waveform)

    ========================================================================== =
    type name `field_type`
    const char \*\ `name`
    size_t `max_length`
    `field_type` `waveform`\ [`max_length`]
    ========================================================================== =

    `waveform` will be copied into the record buffer each time this record
    processes.  This is useful for publishing internally generated waveforms.

..  macro::
    PUBLISH_WF_WRITE_VAR(field_type, name, max_length, waveform)
    PUBLISH_WF_WRITE_VAR_P(field_type, name, max_length, waveform)

    ========================================================================== =
    type name `field_type`
    const char \*\ `name`
    size_t `max_length`
    `field_type` `waveform`\ [`max_length`]
    ========================================================================== =

    `waveform` will updated from the record each time the record processes.

..  macro::
    PUBLISH_WF_ACTION(field_type, name, max_length, action)
    PUBLISH_WF_ACTION_I(field_type, name, max_length, action)
    PUBLISH_WF_ACTION_P(field_type, name, max_length, action)

    ========================================================================== =
    type name `field_type`
    const char \*\ `name`
    size_t `max_length`
    void `action`\ (`field_type` value[`max_length`])
    ========================================================================== =

    This is called each time the record processes.  It is up to the
    implementation of `action` to determine whether this is a read or a
    write action. :func:`PUBLISH_WF_ACTION`


Auxiliary API
-------------

A handful of auxiliary functions and macros allow some further processing of
records.

..  function::
    void push_record_name_prefix(const char *prefix)
    void pop_record_name_prefix(void)
    void set_record_name_separator(const char *separator)

    These two functions can be used to manage a string prefixed to the name of
    each record published by any of the :macro:`PUBLISH` macros.  The list of
    pushed prefixes is prepended to the record name generated, and prefixes are
    deleted in reverse order.  Each `prefix` is followed by the `separator`,
    which defaults to ``":"`` at startup, but can be changed.

    Note that when :func:`set_record_name_separator` is used to change the
    record name separator, the change only affects subsequent calls to
    :func:`push_record_name_prefix`, any existing prefix is unchanged.

..  type:: enum epics_alarm_severity

    This is a copy of the base EPICS severity type with the following possible
    values:

    =================== ======= ================================================
    enum name           Value   Meaning
    =================== ======= ================================================
    epics_sev_none      0       Normal status, no alarm
    epics_sev_minor     1       Minor alarm status
    epics_sev_major     2       Major alarm status
    epics_sev_invalid   3       PV value is invalid
    =================== ======= ================================================

..  function:: void set_record_severity( \
        struct epics_record *epics_record, enum epics_alarm_severity severity)

    Can be used to update the reported record severity for any IN or WAVEFORM
    `epics_record`.

..  function:: void set_record_timestamp( \
        struct epics_record *epics_record, const struct timespec *timestamp)

    If `epics_record` was published with `set_time` set then this function
    should be called before or as part of record processing to set the
    `timestamp`.

..  function:: void trigger_record(struct epics_record *epics_record)

    If `epics_record` was published with `io_intr` set then calling this
    function will trigger record processing.

..  macro::
    LOOKUP_RECORD(record, name)

    ========================================================================== =
    record class `record`
    const char \*\ `name`
    returns ``struct epics_record*``
    ========================================================================== =

    If a record of the specified `record` class has been published with the
    given `name` this function returns a pointer to the :type:`epics_record`
    structure for the record, otherwise ``NULL`` is returned.

..  macro::
    WRITE_OUT_RECORD(record, epics_record, value, process)
    WRITE_NAMED_RECORD(record, name, value)

    ========================================================================== =
    record class `record`
    struct epics_record \*\ `epics_record`
    const char \*\ `name`
    TYPEOF(`record`) `value`
    bool `process`
    ========================================================================== =

    The given `value` is written directly to the EPICS record associated with
    `epics_record`.  `process` can be set to ``false`` to suppress normal record
    processing, otherwise normal record processing will occur and the driver's
    `write` method will be called.

    The :func:`WRITE_NAMED_RECORD` variant includes an unchecked call to
    :func:`LOOKUP_RECORD` to translate a record name to the appropriate ``struct
    epics_record*`` value.

..  macro::
    WRITE_OUT_RECORD_WF(field_type, epics_record, value, length, process)
    WRITE_NAMED_RECORD_WF(field_type, name, value, length)

    ========================================================================== =
    type name `field_type`
    struct epics_record \*\ `epics_record`
    const char \*\ `name`
    const `field_type` `value`\ [`length`]
    size_t `length`
    bool `process`
    ========================================================================== =

    As for :func:`WRITE_OUT_RECORD`, and :func:`WRITE_NAMED_RECORD` but for
    waveform records.  The EPICS copy of the waveform is updated, and the record
    is processed or not as appropriate.

..  macro::
    READ_RECORD_VALUE(record, epics_record)
    READ_NAMED_RECORD(record, name)

    ========================================================================== =
    record class `record`
    struct epics_record \*\ `epics_record`
    const char \*\ `name`
    returns TYPEOF(`record`)
    ========================================================================== =

    Returns the current value of any scalar record.  Can be called with either
    `epics_record` or `name` which is subject to an unchecked lookup.

..  macro::
    READ_RECORD_VALUE_WF(field_type, epics_record, value, length)
    READ_NAMED_RECORD_WF(field_type, name, value, length)

    ========================================================================== =
    type name `field_type`
    struct epics_record \*\ `epics_record`
    const char \*\ `name`
    `field_type` `value`\ [`length`]
    size_t `length`
    ========================================================================== =

    Reads the current waveform value of a waveform record.
