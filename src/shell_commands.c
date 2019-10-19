/* This file exports a number of functions to the IOC shell. */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>

#include "error.h"
#include "epics_device.h"
#include "persistence.h"

#include <iocsh.h>
#include <epicsExport.h>


static void call_initialise_epics_device(const iocshArgBuf *args)
{
    initialise_epics_device();
}

static const iocshFuncDef def_initialise_epics_device = {
    "initialise_epics_device", 0, NULL
};


static void call_load_persistent_state(const iocshArgBuf *args)
{
    const char *file_name = args[0].sval;
    int interval = args[1].ival;

    error_report(
        TEST_OK_IO_(file_name, "Must specify a filename")  ?:
        TEST_OK_(interval > 1, "Must specify a sensible interval")  ?:
        load_persistent_state(file_name, interval, false));
}

static const iocshFuncDef def_load_persistent_state = {
    "load_persistent_state", 2, (const iocshArg *[]) {
        &(iocshArg) { "File name",      iocshArgString },
        &(iocshArg) { "Save interval",  iocshArgInt },
    }
};


static void epicsShareAPI epics_device_registrar(void)
{
    iocshRegister(&def_initialise_epics_device, &call_initialise_epics_device);
    iocshRegister(&def_load_persistent_state,   &call_load_persistent_state);
}

epicsExportRegistrar(epics_device_registrar);
