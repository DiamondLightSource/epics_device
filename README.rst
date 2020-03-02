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


Version 2.0
-----------

Version 2.0 introduces a major incompatibility: the ``mbbi`` and ``mbbo`` record
support no longer uses ``RVAL`` but instead uses ``VAL`` with conversion
disabled.  This means the following changes apply:

* ``TYPEOF(mbbi)`` and ``TYPEOF(mbbo)`` have changed from ``unsigned int`` to
  ``uint16_t``.  This means that all code declaring records of these types will
  need to be changed accordingly.

* The ``mbbIn`` and ``mbbOut`` functions no longer allow the field values to be
  assigned: these are only meaningful for ``RVAL``, which is now ignored.
