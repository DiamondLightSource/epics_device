Code Examples
=============

This file contains a number of examples of code to show many of the features
documented here.

Basic IOC
---------

This section describes the minimum code required to build an IOC using this
framework.  The source tree is:

======================= ========================================================
File/Directory          Description
======================= ========================================================
Db
Db/Makefile             Needs rules for building ``.db`` from ``.py``
Db/basic_ioc.py         Minimal example building a simple database
src
src/Makefile
src/main.c              Minimal IOC implementation
st.cmd                  Startup script for IOC initialisation
Makefile
configure               Standard EPICS ``configure`` directory, as generated
                        by ``makeBaseApp.pl``.
configure/CONFIG
configure/CONFIG_APP
configure/CONFIG_SITE   Must specify ``PYTHON``
configure/Makefile
configure/RELEASE       Must specify ``EPICS_DEVICE``
configure/RULES
configure/RULES.ioc
configure/RULES_DIRS
configure/RULES_TOP
======================= ========================================================

The ``EPICS_DEVICE`` support module must be defined as usual, and the build also
needs the symbols ``PYTHON`` and ``IOCBUILDER`` to be specified.


Defining the Database
~~~~~~~~~~~~~~~~~~~~~

The database defined here is utterly minimal, consisting of a single PV with a 1
second scan which returns the current timestamp in seconds.  The EPICS Device
definition to build the database record entry is the following Python line::

    longIn('TSEC', DESC = 'Timestamp in seconds', SCAN = '1 second')

This line defines an ``longin`` record named ``TEST`` with the specified scan
interval and description.

A little bit of boilerplate is needed to set things up:

..  literalinclude:: ../examples/basic_ioc/Db/basic_ioc.py
    :language: python
    :linenos:

This code picks up the configured ``EPICS_DEVICE`` version (as configured in
``configure/RELEASE``) and configures the database to be generated with a
``$(DEVICE):`` prefix on each record name.  The last line writes out the
generated database.

Some support is also needed from the make file:

..  literalinclude:: ../examples/basic_ioc/Db/Makefile
    :language: make
    :linenos:

The important point here is that the ``.db`` file is generated from the
corresponding ``.py`` file and the ``EPICS_DEVICE`` symbol is exported to the
script above.

The result of this is the following file in ``db/basic_ioc.db``:

..  code-block:: none
    :linenos:

    record(longin, "$(DEVICE):TSEC")
    {
        field(DESC, "Timestamp in seconds")
        field(DTYP, "epics_device")
        field(INP,  "@TSEC")
        field(SCAN, "1 second")
    }


Implementing the IOC
~~~~~~~~~~~~~~~~~~~~

As this IOC does almost nothing its C implementation is pretty small:

..  literalinclude:: ../examples/basic_ioc/src/main.c
    :language: c
    :linenos:

First a minimal set of headers.  Both ``stdbool.h`` and ``unistd.h`` are
required as a consequence of using ``error.h``, and our implementation will use
``time.h``.  We need ``iocsh.h`` in order to call :func:`iocsh`.

Then the EPICS Device headers are standard.

The function ``read_timestamp`` actually implements the IOC functionality.  In
this case when the corresponding record is processed we compute a value which is
then used to update the PV.

The :func:`PUBLISH_READER` call binds our PV implementation to its definition in
the database, and we've chosen the appropriate implementation.

Finally IOC initialisation consists of a stereotyped sequence.
:func:`initialise_epics_device` must be called early, then records can be
published, then the IOC is started.  In this particular example we've put the
rest of the initialisation into an external startup script:

..  literalinclude:: ../examples/basic_ioc/st.cmd
    :language: c
    :linenos:

Note that it is possible perform complete IOC initialisation without a startup
script, and with a more complete IOC it can be more convenient to do this.


Database
--------

..  highlight:: py
    :linenothreshold: 1

This database declares three status PVs (``SE:CPU``, ``SE:ADCCLK``,
``SE:NTPSTAT``) together with a health aggregation PV::

    # A list of PVs we need to trigger and aggregate severity.
    system_alarm_pvs = [
        # CPU usage
        aIn('SE:CPU', 0, 100, '%', 1,
            DESC = 'CPU usage',
            HIGH = 80,      HSV  = 'MINOR',
            HIHI = 95,      HHSV = 'MAJOR'),

        boolIn('SE:ADCCLK', 'Clock Ok', 'Clock Dropout',
            ZSV  = 'NO_ALARM', OSV = 'MAJOR',
            DESC = 'ADC clock dropout detect'),

        # The following list must match the corresponding enum in sensors.c
        mbbIn('SE:NTPSTAT',
            ('Not monitored',   0,  'NO_ALARM'),    # Monitoring disabled
            ('No NTP server',   1,  'MAJOR'),       # Local NTP server not found
            ('Startup',         2,  'NO_ALARM'),    # No alarm during startup
            ('No Sync',         3,  'MINOR'),       # NTP server un-synchronised
            ('Synchronised',    4,  'NO_ALARM'),    # Synchronised
            DESC = 'Status of NTP server')]

    severity = AggregateSeverity(
        'SE:SYS:OK', 'System health', system_alarm_pvs)
    Trigger('SE', *trigger_pvs + [severity,])


PV Creation and Update
----------------------

..  highlight:: c
    :linenothreshold: 1

The code below publishes and updates the PVs published above::

    static struct epics_interlock *interlock;
    static double cpu_usage;
    static bool read_clock_dropout(void) { ... }
    static unsigned int NTP_status;

    static double update_cpu_usage(void) { ... }
    static unsigned int update_NTP_status(void) { ... }

    static void update_sensors(void)
    {
        interlock_wait(interlock);
        cpu_usage = update_cpu_usage();
        NTP_status = update_NTP_status();
        interlock_signal(interlock, NULL);
    }

    bool initialise_sensors(void)
    {
        interlock = create_interlock("SE", false);

        PUBLISH_READ_VAR(ai, "SE:CPU", cpu_usage);
        PUBLISH_READER(bi, "SE:ADCCLK", read_clock_dropout);
        PUBLISH_READ_VAR(mbbi, "SE:NTPSTAT", NTP_status);
    }
