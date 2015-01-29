TOP = ..

include $(TOP)/configure/CONFIG

include $(EPICS_DEVICE)/src/Makefile.extra


#=============================

PROD_IOC = example_ioc

DBD += example_ioc.dbd
example_ioc_DBD += base.dbd
example_ioc_DBD += epics_device.dbd

example_ioc_SRCS += example_ioc_registerRecordDeviceDriver.c
example_ioc_SRCS += main.c
example_ioc_SRCS += example_pvs.c

example_ioc_LIBS += epics_device
example_ioc_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES
