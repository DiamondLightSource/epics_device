Worked Code Example
===================

This is a worked example showing use of the features described here.  The code
here is largely cut down from TMBF and does not necessarily work as shown.

Database
--------

..  highlight:: py
    :linenothreshold: 1

This database declares three status PVs (``SE:CPU``, ``SE:ADCCLK``,
``SE:NTPSTAT``) together with health aggregation PVs::

    trigger_pvs = []        # All sensor records that need triggering, in order
    health_pvs = []         # Records for reporting aggregate health

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

    trigger_pvs.extend(system_alarm_pvs)
    health_pvs.append(
        AggregateSeverity('SE:SYS:OK', 'System health',
            system_alarm_pvs + debug_control))

    # Further code to extend both trigger_pvs and health_pvs
    ...

    # Aggregate all the alarm generating records into a single "health" record.
    # Only the alarm status of this record is meaningful.
    trigger_pvs.extend(health_pvs)
    trigger_pvs.append(
        AggregateSeverity('SE:HEALTH', 'Aggregated health', health_pvs))
    Trigger('SE', *trigger_pvs)


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

    static void *sensors_thread(void *context)
    {
        while (true)
        {
            interlock_wait(interlock);
            cpu_usage = update_cpu_usage();
            NTP_status = update_NTP_status();
            // Update other values
            ...
            interlock_signal(interlock, NULL);

            sleep(SENSORS_POLL_INTERVAL);
        }
        return NULL;
    }

    bool initialise_sensors(void)
    {
        interlock = create_interlock("SE", false);

        PUBLISH_READ_VAR(ai, "SE:CPU", cpu_usage);
        PUBLISH_READER(bi, "SE:ADCCLK", read_clock_dropout);
        PUBLISH_READ_VAR(mbbi, "SE:NTPSTAT", NTP_status);

        // Generate other pvs
        ...

        pthread_t thread_id;
        return TEST_0(pthread_create(&thread_id, NULL, sensors_thread, NULL));
    }
