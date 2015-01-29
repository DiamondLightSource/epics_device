#include <stdbool.h>
#include <stdio.h>

#include <iocsh.h>
#include <dbAccess.h>
#include <iocInit.h>

#include "error.h"
#include "epics_device.h"
#include "epics_extra.h"
#include "example_pvs.h"


extern int example_ioc_registerRecordDeviceDriver(struct dbBase *pdbbase);


static bool load_database(const char *db)
{
    database_add_macro("DEVICE", "TS-TS-TEST-99");
    database_add_macro("BLAH", "%d", 33);
    return database_load_file(db);
}



int main(int argc, char *argv[])
{
    bool ok =
        initialise_epics_device()  &&
        initialise_epics_extra()  &&
        initialise_example_pvs()  &&
        start_caRepeater()  &&

        /* The following block of code could equivalently be implemented by
         * writing a startup script with the following content with a call to
         * iocsh():
         *
         *  dbLoadDatabase("dbd/example_ioc.dbd", NULL, NULL)
         *  example_ioc_registerRecordDeviceDriver(pdbbase)
         *  dbLoadRecords("db/example_ioc.db", "DEVICE=TS-TS-TEST-99")
         *  iocInit()
         */
        TEST_IO(dbLoadDatabase("dbd/example_ioc.dbd", NULL, NULL))  &&
        TEST_IO(example_ioc_registerRecordDeviceDriver(pdbbase))  &&
        load_database("db/example_ioc.db")  &&
        TEST_OK(!iocInit())  &&

        TEST_IO(iocsh(NULL));
    return ok ? 0 : 1;
}
