Code Examples
=============

This file contains a couple of examples of code to show many of the features
documented here.

Basic IOC
---------

The IOC in ``examples/basic_ioc`` illustrates the minimum code required to build
an IOC using this framework.  The source tree is:

======================= ========================================================
File/Directory          Description
======================= ========================================================
Db/
Db/Makefile             Needs rules for building ``.db`` from ``.py``
Db/basic_ioc.py         Minimal example building a simple database
src/
src/Makefile
src/main.c              Minimal IOC implementation
st.cmd                  Startup script for IOC initialisation
Makefile
configure/              Standard EPICS ``configure`` directory, as generated
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
needs the symbol ``PYTHON`` to identify the Python interpreter to use.


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

First comes a minimal set of headers.  Both ``stdbool.h`` and ``unistd.h`` are
required as a consequence of using ``error.h``, and our implementation will use
``time.h``.  We need ``iocsh.h`` in order to call :func:`iocsh`.

Then the EPICS Device headers ``error.h`` and ``epics_device.h`` are needed for
any use of EPICS Device.

The function ``read_timestamp`` actually implements the IOC functionality.  In
this case when the corresponding record is processed we compute a value which is
used to update the PV.

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


Internalising ``st.cmd``
~~~~~~~~~~~~~~~~~~~~~~~~

It can be more convenient to internalise the startup script to the IOC source
code, particularly if a number of template parameters need to be generated.
This can be done by replacing the line ``TEST_IO(iocsh("st.cmd") == 0)`` in the
definition of ``main`` above with a call to ``init_ioc()`` as defined here:

..  code-block:: c
    :linenos:

    extern int basic_ioc_registerRecordDeviceDriver(struct dbBase *pdb);

    static error__t init_ioc(void)
    {
        return
            TEST_IO(dbLoadDatabase("dbd/basic_ioc.dbd", NULL, NULL))  ?:
            TEST_IO(basic_ioc_registerRecordDeviceDriver(pdbbase))  ?:
            DO(database_add_macro("DEVICE", "TS-TS-TEST-99"))  ?:
            database_load_file("db/basic_ioc.db")  ?:
            TEST_OK(iocInit() == 0);
    }

..  x* (vim)

This is a bit more code, but does have the advantage of rather more thorough
error checking, and much more flexibility in macro generation.


A More Complex Example
----------------------

The source tree in ``examples/example_ioc`` illustrates a slightly fuller
functioned IOC.

Database Definition
~~~~~~~~~~~~~~~~~~~

The database code is as follows:

..  literalinclude:: ../examples/example_ioc/Db/example_ioc.py
    :language: python
    :linenos:

Here we have the following structures:

1.  Record naming and defaults are set up so that all record names are prefixed
    with the macro ``$(DEVICE):`` (the default template behaviour) and all out
    records names are suffixed with ``_S`` (a very convenient naming
    convention).

2.  An ``ao`` record ``FREQ_S`` used to set the frequency for a waveform record
    ``WF``; each time ``FREQ_S`` is written the waveform is updated and so is a
    ``SUM`` record.  In this case processing of ``WF`` and ``SUM`` is entirely
    driven by writes to ``FREQ_S``.

3.  A pair of records ``TRIFWF`` and ``COUNT`` which update on an internally
    generated IOC event.  These are controlled by a couple of settings
    ``INTERVAL_S`` and ``SCALING_S``, and ``COUNT`` can be reset by processing
    ``RESET_S``.

4.  Another action ``WRITE_S`` which forces the persistent state to be saved.

Of course here the database builder is more useful: 30 lines of source code
generates 129 lines of database.


Publishing Records
~~~~~~~~~~~~~~~~~~

For each of the records defined above an implementation needs to be defined and
published.  The following C code publishes the variables above:

..  code-block:: c
    :linenos:

    error__t initialise_example_pvs(void)
    {
        PUBLISH_WRITER_P(ao, "FREQ", set_frequency);
        PUBLISH_WF_READ_VAR(double, "WF", WF_LENGTH, waveform);
        PUBLISH_READ_VAR(ai, "SUM", sum);

        PUBLISH_ACTION("WRITE", write_persistent_state);

        PUBLISH_WRITE_VAR_P(ao, "INTERVAL", event_interval);
        PUBLISH_WRITE_VAR_P(ao, "SCALING", scaling);

        interlock = create_interlock("TRIG", false);
        PUBLISH_READ_VAR(longin, "COUNT", trigger_count);
        PUBLISH_WF_READ_VAR(int, "TRIGWF", WF_LENGTH, trigger_waveform);
        PUBLISH_ACTION("RESET", reset_trigger_count);

        ...
    }

Note that all of the out variables are published with a ``_P`` suffix,
indicating that persitence support is enabled.  Here we see a variety of
different implementations being used: calling a function, reading and writing a
variable, and finally using an interlock.


Notes on Trigger and Interlock
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The most complex structure involves :c:func:`create_interlock`.  Before updating
the variables ``trigger_count`` and ``trigger_waveform`` associated with the PVs
which will be updated when the record ``TRIG`` is processed,
:c:func:`interlock_wait` must first be called -- this blocks processing until
the library knows that the IOC is no longer reading the variables.  Once the new
state has been written then :c:func:`interlock_signal` can be called to signal
the update to EPICS and trigger processing of the associated records.

..  code-block:: c
    :linenos:

    static void process_event(void)
    {
        interlock_wait(interlock);
        trigger_count += 1;
        update_waveform();
        interlock_signal(interlock, NULL);
    }

Finally, in this example the function ``process_event`` is called internally by
the driver implementation; in this example a thread is used to call it at an
interval governed by the variable ``event_interval``.
