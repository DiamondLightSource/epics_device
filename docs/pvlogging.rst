Support for PV Logging
======================

The PV logging interface is currently very simple and crude and consists of the
function :func:`hook_pv_logging`.  This can be called during initialisation to
enable logging.

The functionality described here is defined in the header file ``pvlogging.h``.

..  function:: bool hook_pv_logging(const char *access_file, int max_length)

    When called enables logging of all Channel Access PV puts.  The parameter
    `max_length` determines how many elements of waveforms are logged.  The
    `access_file` parameter must name a file readable by the IOC containing at
    least the following::

        ASG(DEFAULT) {
            RULE(1, READ)
            RULE(1, WRITE, TRAPWRITE)
        }
