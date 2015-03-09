EPICS Device
============

This is a lightweight framework for implementing record interfaces for EPICS
IOCs.  This framework is designed to be used in combination with the
epicsdbbuilder library for building the database file, but can be used
standalone.

Simple Example
--------------

For example, to interface to a simple read-write register interface, the
following two fragments of code will serve.  This interface requires one write
PV and one read PV, implemented as follows.

First the interface is implemented and published in C::

    int read_register_value(void) { ... }
    void write_register_value(int value) { ... }

    void publish_pvs(void)
    {
        PUBLISH_READER(longin, "REGISTER", read_register_value);
        PUBLISH_WRITER(longout, "REGISTER", write_register_value);
    }

The corresponding database can be declared (in Python) thus::

    read_reg = longIn('REGISTER', DESC = 'Register readback')
    longOut('REGISTER', DESC = 'Register control', FLNK = read_reg)

With a little more code to tie things together this is a complete example.  For
more details see the example IOCS in the ``examples/`` directory and the
documentation in ``docs/``.
