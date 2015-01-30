Code Examples
=============

This file contains a number of examples of code to show many of the features
documented here.

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
