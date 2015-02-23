TOP = ..

include $(TOP)/configure/CONFIG

include $(EPICS_DEVICE)/src/Makefile.extra


#=============================

PROD_IOC = basic_ioc

DBD += basic_ioc.dbd
basic_ioc_DBD += base.dbd
basic_ioc_DBD += epics_device.dbd

basic_ioc_SRCS += basic_ioc_registerRecordDeviceDriver.c
basic_ioc_SRCS += main.c

basic_ioc_LIBS += epics_device
basic_ioc_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES
