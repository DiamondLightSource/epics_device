TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure
DIRS += src
DIRS += docs
include $(TOP)/configure/RULES_TOP
