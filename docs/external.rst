External References
===================

This file gathers together a few miscellaneous external references not
documented elsewhere.

Standard C API
--------------

..  c:function::
    int snprintf(char *str, size_t size, const char *format, ...)
    int sprintf(char *str, const char *format, ...)
    int printf(const char *format, ...)

    Standard functions for printing and formatting strings, available via
    ``#include <stdio.h>``.

..  type:: pthread_mutex_t

    Mutex type for thread synchronisation, available via ``#include
    <pthread.h>``.

..  type:: struct timespec

    Timestamp structure available via ``#include <time.h>``.  Represents time in
    seconds to nanosecond resolution.

..  type:: va_list

    Type used to manage variable arguments, available via ``#include
    <stdarg.h>``.

..  var:: int errno

    Global (thread local) variable set by IO calls to report error status,
    available via ``#include <errno.h>``.

EPICS API
---------

..  c:function:: int iocInit(void)

    EPICS function used to start the IOC.  This must be called to start the IOC
    after publishing all PVs and loading all EPICS databases.

..  c:function:: iocsh()

    Interactive IOC shell.  This can optionally be called after :func:`iocInit`
    to provide an interactive shell for the IOC.

..  c:macro:: initHookAtEnd

    EPICS event triggered at the end of IOC initialisation.
