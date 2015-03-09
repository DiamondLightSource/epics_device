TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure
DIRS += src
DIRS += epics_device
DIRS += docs
include $(TOP)/configure/RULES_TOP
