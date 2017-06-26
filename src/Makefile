TOP = ..

include $(TOP)/configure/CONFIG

include $(TOP)/src/Makefile.extra


LIBRARY_IOC += epics_device

INC += error.h
INC += epics_device.h
INC += epics_extra.h
INC += hashtable.h
INC += persistence.h
INC += pvlogging.h

DBD += epics_device.dbd


epics_device_SRCS += error.c            # General error handling support
epics_device_SRCS += epics_device.c     # EPICS device support framework
epics_device_SRCS += epics_extra.c      # Miscellanous extra EPICS support
epics_device_SRCS += hashtable.c        # Simple generic hash table
epics_device_SRCS += persistence.c      # Persistent value support for EPICS
epics_device_SRCS += pvlogging.c        # Logging for all caputs
epics_device_SRCS += shell_commands.c   # Exports selected commands to IOC shell


include $(TOP)/configure/RULES
