#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <iocsh.h>
#include <dbAccess.h>
#include <iocInit.h>

#include "error.h"
#include "epics_device.h"
#include "epics_extra.h"


extern int basic_ioc_registerRecordDeviceDriver(struct dbBase *pdb);


static double read_timestamp(void)
{
    return (double) time(NULL);
}


static bool initialise_pvs(void)
{
    PUBLISH_READER(ai, "TSEC", read_timestamp);
    return true;
}


static bool load_database(const char *db)
{
    database_add_macro("DEVICE", "TS-TS-TEST-99");
    return database_load_file(db);
}


int main(int argc, const char *argv[])
{
    bool ok =
        initialise_epics_device()  &&

        initialise_pvs()  &&

        TEST_IO(dbLoadDatabase("dbd/basic_ioc.dbd", NULL, NULL))  &&
        TEST_IO(basic_ioc_registerRecordDeviceDriver(pdbbase))  &&
        load_database("db/basic_ioc.db")  &&
        TEST_OK(iocInit() == 0)  &&
        TEST_IO(iocsh(NULL));

    return ok ? 0 : 1;
}
