..  default-domain:: py
..  py:module:: epics_device

Database Builder Support
========================

The functionality documented here is designed to create EPICS databases
alongside an IOC developed using the EPICS Device C API.  The database building
process uses the EPICS database builder, which is a separate support module and
documented elsewere.

Importing :mod:`epics_device` will automatically import and initialise
:mod:`epicsdbbuilder`.  By default the version configured in
``configure/SITE_CONFIG`` of ``epics_device`` will be used, but this can be
overridden by setting the environment variable ``EPICSDBBUILDER`` before
importing :mod:`epics_device`.

A database building script will normally be invoked from the EPICS build
environment.  This will have already exported the environment variable
``EPICS_BASE``, which must be set before importing :mod:`epics_device`.  If
``EPICS_DEVICE`` has also been configured in ``configure/RELEASE`` then this can
be exported from the make file and then the following example script will work::

    # Configure path using environment
    import sys, os
    sys.path.append(os.environ['EPICS_DEVICE'])

    # Import and initialise EPICS device
    from epics_device import *

    # Configure an appropriate record naming convention
    SetTemplateRecordNames()

    # Now create records, eg
    aOut('RECORD', DESC = 'This is a record')

    # Finally write the records, here the filename is sys.argv
    WriteRecords(sys.argv[1])

Invoking a Python script of this form will write a valid EPICS database file.
With this script the following lines added to the ``Db/Makefile`` after
``include $(TOP)/configure/RULES`` will build the database as required:

..  code-block:: make

    export EPICS_DEVICE

    $(COMMON_DIR)/%.db: ../%.py
            $(PYTHON) $< $@


Creating EPICS Records
----------------------

The methods exported by :mod:`epics_device` provide helper functions for
creating EPICS records of each of the eleven basic types.  A particular coding
style is supported by these calls, if the default behaviour is not felt to be
appropriate then records can be created directly by calling the appropriate
methods of :class:`EpicsDevice`.

A number of defaults and naming conventions are applied by the functions below.

Record Address
~~~~~~~~~~~~~~

By default the name used to look up the record in IOC support, as published by
:c:func:`PUBLISH` and allies, is the name argument as passed to the appropriate
record construct documented below, before any name processing occurs.  This can
be modified in two ways:

1.  Firstly, every record creation function listed below takes an optional
    keyword argument `address`, which can be used to specify an address other
    than the record `name`.

2.  Secondly, a prefix can be prepended to every `address` and `name` by calling
    the functions :func:`push_name_prefix` and :func:`pop_name_prefix`.

..  function::
    push_name_prefix(prefix)
    pop_name_prefix()

    These two functions mirror the action of :func:`push_record_name_prefix` and
    :func:`pop_record_name_prefix`, and affect both the record name and the
    associated address.

..  function:: set_name_separator(separator)

    By default component of the prefix and the record name are separated by ':',
    but this function can be used to change the separator.

..  class:: name_prefix(prefix)

    This is a context manager for a name prefix, wrapping
    :func:`push_name_prefix` and :func:`pop_name_prefix` into a single action.
    This can be used thus::

        with name_prefix('PREFIX'):
            aIn('AIN')


In Records
~~~~~~~~~~

Input records are created by the following functions.  Processing must be
arranged for each record.  The options are:

* Setting ``SCAN`` to a delay interval and allowing EPICS to trigger processing.
* Setting ``SCAN = 'I/O Intr'`` and calling :func:`trigger_record` from within
  the driver.
* Leaving ``SCAN`` as ``'Passive'`` and ensuring that the record is processed in
  response to ``FLNK`` from some other record.  In particular see
  :func:`Trigger` for this.


..  function::
    aIn(name, LOPR=None, HOPR=None, EGU=None, PREC=None, **fields)
    longIn(name, LOPR=None, HOPR=None, EGU=None, **fields)

    For ``ai`` and ``longin`` records the ``MDEL`` field is set by default to -1
    to ensure that all ticks are generated.

..  function:: boolIn(name, ZNAM=None, ONAM=None, **fields)

    For :func:`boolIn` the two optional arguments are the strings associated
    with boolean values ``false`` and ``true`` respectively.

..  function:: mbbIn(name, *option_values, **fields)

    For ``mbbIn`` `*option_values` represents a sequence of up to 16 enumeration
    assignments.  Each assignment is either a string, in which case the default
    numbering assigns ``ZRVL=0``, ``ONVL=1``, etc; or else a tuple of two or
    three values, in which case the first value is the option name, the second
    the associated numerical value, and the third the severity.

    For example::

        status = mbbIn('STATUS',
            'Ok', ('Failing', 1, 'MINOR'), ('Failed', 2, 'MAJOR'),
            DESC = 'Status pv')

    This creates a PV with value 0, 1 or 2, and with increasing severity.

..  function:: stringIn(name, **fields)

    Creates a ``stringin`` record.

..  function:: set_MDEL_default(default)

    Normally the ``MDEL`` field for ``ai`` and ``longin`` records defaults to 0.
    This means that record updates which don't change the published value aren't
    transmitted over channel access.  In some applications this is inconvenient,
    in which case this function can be called with `default` set to -1 to ensure
    that all updates are transmitted.


Out Records
~~~~~~~~~~~

For all "out" records ``OMSL`` is set to ``supervisory`` and ``PINI`` is set to
``YES``.  As "out" records are normally used for device configuration they
should be left with ``SCAN`` set to ``'Passive'``, the default.

The name passed to each of these functions is the internal address, and
the record name is generated by adding ``_S`` to the given name.

..  function::
    aOut(name, DRVL=None, DRVH=None, EGU=None, PREC=None, **fields)
    longOut(name, DRVL=None, DRVH=None, EGU=None, **fields)

    By default for ``ao`` and ``longout`` records the ``LOPR`` and ``HOPR``
    fields are set equal to ``DRVL`` and ``DRVH`` respectively.

..  function:: boolOut(name, ZNAM=None, ONAM=None, **fields)

    See :func:`boolIn` for the optional arguments.

..  function:: mbbOut(name, *option_values, **fields)

    See :func:`mbbIn` for `option_values`.  An example control PV might be::

        mbbOut('SETUP', 'Normal', 'Unusual', 'Special',
            DESC = 'Configure setup control')

..  function:: stringOut(name, **fields)

    Creates a ``stringout`` record.

..  function:: set_out_name(function)

    This hook can be used to implement a special naming convention for out
    records: the record name for all out records is first processed by this
    function before being passed through to normal :mod:`epicsdbbuilder`
    processing.

    For example, all output records can be named with a trailing ``_S`` suffix
    with the following call::

        set_out_name(lambda name: name + '_S')


Waveform Records
~~~~~~~~~~~~~~~~

For waveform records the direction of data flow is determined by driver support
rather than by EPICS or the device layer.

..  function:: Waveform(name, length, FTVL='LONG', **fields)

    Defines a waveform record with the given `name`.  The number of points in
    the waveform must be specified as `length`, and if a field type other than
    ``'LONG'`` (which really means 32-bit integer) is wanted this must be
    explicitly specified.

..  function:: WaveformOut(name, length, FTVL='LONG', **fields)

    This is used for defining a waveform specialised for output.  Functionally
    this is identicial to :func:`Waveform` except for two differences:

    * The associated record name has ``'_S'`` appended.
    * The ``'PINI'`` field is set to ``'YES'``.


Raw Record Creation
~~~~~~~~~~~~~~~~~~~

The functions listed above are all helper functions with a number of hard-wired
defaults and actions.  A more direct approach to record creation can be taken by
invoking the record creation methods for :data:`EpicsDevice` and
:data:`epicsdbbuilder.records` directly.

..  data:: EpicsDevice

    This Python object has a method for each record type supported by EPICS
    Device, namely ``ai``, ``bi``, ``longin``, ``mbbi``, ``stringin``, ``ao``,
    ``bo``, ``longout``, ``mbbo``, ``stringout``, ``waveform``.  Each method has
    the following signature:

    ..  function:: EpicsDevice.record(name, address=None, **fields)

        ..  x** (vim)

        Creates an EPICS record  of type `record` with the given `name`.  The
        record is configured with EPICS Device support, and by default the EPICS
        Device binding address is also `name`, but a different value can be
        specified by setting `address`.  Keyword arguments can be used to
        specify any EPICS field.

        This method automatically initialises the ``DTYP`` and ``INP`` or
        ``OUT`` fields as appropriate, and is otherwise just a wrapper around
        the corresponding method of :data:`epicsdbbuilder.records`.


Helper Functions
----------------

These functions are designed to assist in the generation of databases.  The
:func:`Trigger` function is the most complex one, designed for record sets which
update on driver internal events.

..  function:: Trigger(prefix, *pvs, set_time = False)

    This function generates two records with names `prefix`\ ``:TRIG`` and
    `prefix`\ ``:DONE`` together with as many fanout records as necessary to
    ensure that all of the PVs in `pvs` are processed in turn when the ``:TRIG``
    record is processed.  This function is designed to be used with the
    :c:type:`epics_interlock` API to implement coherent updating of all the
    linked PVs.

    If the `set_time` field is set then :c:func:`create_interlock` should be
    called with its `set_time` field set to ``true`` and a valid timestamp
    should be passed to :c:func:`interlock_signal`.  In this case the driver is
    responsible for timestamping the records.

    For example, the database definition::

        Trigger('UPDATE', aIn('V1'), aIn('V2'), aIn('V3'), Waveform('WF', 1000))

    ..  highlight:: c

    can be combined with the following C code to trigger simultaneous and
    coherent updates of the three named PVs::

        static struct epics_interlock *update;
        static double v1, v2, v3;
        static int wf[1000];
        static void compute_update(
            double *v1, double *v2, double *v3, int wf[]) { ... }

        // Called in response to some internal or external action
        void trigger_update(void)
        {
            interlock_wait(update);
            compute_update(&v1, &v2, &v3, wf);
            interlock_signal(update, NULL);
        }

        // Publish the PVs
        void publish_pvs(void)
        {
            update = create_interlock("UPDATE", false);
            PUBLISH_READ_VAR(ai, "V1", v1);
            PUBLISH_READ_VAR(ai, "V2", v2);
            PUBLISH_READ_VAR(ai, "V3", v3);
            PUBLISH_WF_READ_VAR(int, "WF", 1000, wf);
        }

    ..  highlight:: py

    The key principle here is that the variables containing the values of the
    PVs are only written while the interlock (``update``) is held, so that EPICS
    see a consistent update of all PVs.  This is of particular importance when
    waveforms are involved.

..  function:: Action(name, **fields)

    Creates an "action" PV.  This is a ``bo`` record configured not to start
    during IOC initialisation.

..  function:: ForwardLink(name, desc, *pvs, **fields)

    A helper function for triggering internal processing after any PV in the
    list `pvs` is processed.  Creates an action PV by calling :func:`Action` and
    forward links each passed PV to the new action PV.  The created PV is
    returned.

    This is designed to be used to trigger common processing after any of a set
    of "out" records have been updated.

..  function:: AggregateSeverity(name, description, recs)

    For up to 12 records, passed as list `recs`, generates a ``calc`` record
    with the given name and description with severity set to the aggregated
    severity of the input records.  The value of the generated record is 1.

    The returned record must be processed, typically after processing the given
    list of records.  For example::

        pvs = [
            aIn('PV1', HIGH = 1, HSV = 'MINOR'),
            bIn('PV2', 'Ok', 'Bad', OSV = 'MAJOR')]
        pvs.append(AggregateSeverity('ALL', 'Health', pvs))
        Trigger('UPDATE', *pvs)

    Note that in this example the aggregation PV must be processed after the PVs
    it aggregates.


..  function:: concat(ll)

    A simple helper function to concatenate a list of lists.


Functions from EPICS Db Builder
-------------------------------

The following functions are reexported from :mod:`epicsdbbuilder` and are useful
for database building.  For fuller documentation see the documentation for that
module, but a selection of useful functions is listed here for reference.

..  data:: records

    This is part of the IOC builder framework, not documented here.  There is a
    method for each record type supported by EPICS, ie ``ai``, ``ao``, ``calc``,
    etc, with the following signature:

    ..  method:: records.record_type(name, **fields)

        ..  x** (vim)

        Creates an EPICS record of type `record_type` with the given `name` and
        with other `fields` as specified.  These records by default have no
        EPICS Device support configured.

    For example:

    ..  method:: records.calc(name, **fields)

        ..  x** (vim)

        This creates a ``calc`` record with the given `name` (as modified by the
        appropriate :func:`SetRecordNames` setup).

..  function::
    PP(record)
    CP(record)
    MS(record)
    NP(record)

    When generating internal record links these add the appropriate link
    annotation.

..  function::
    create_fanout(name, *records, **kargs)
    create_dfanout(name, *records, **kargs)

    Creates processing and data fanouts to an arbitrary list of records.

..  function:: WriteRecords(filename, [header])

    Writes generated database.

..  function::
    SetRecordNames(names)
    SetTemplateRecordNames([prefix] [,separator])

    These two functions are used to establish how the full record name written
    to the generated database is derived from the short form record name passed
    to the appropriate :data:`records` method.

..  function:: Parameter(name [,description] [,default])

    When generating a template database this can be used to declare template
    parameters.
