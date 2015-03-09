#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <iocsh.h>
#include <dbAccess.h>
#include <iocInit.h>

#include "error.h"
#include "epics_device.h"
#include "epics_extra.h"
#include "persistence.h"
#include "pvlogging.h"

#include "example_pvs.h"


extern int example_ioc_registerRecordDeviceDriver(struct dbBase *pdb);

static const char *persistence_file;
static int persistence_interval;


static bool load_database(const char *db)
{
    database_add_macro("DEVICE", "TS-TS-TEST-99");
    return database_load_file(db);
}


static bool ioc_main(void)
{
    return
        initialise_epics_device()  &&
        initialise_epics_extra()  &&
        initialise_persistent_state(persistence_interval) &&

        initialise_example_pvs()  &&
        start_caRepeater()  &&
        hook_pv_logging("db/access.acf", 10)  &&
        load_persistent_state(persistence_file, true)  &&

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
        TEST_OK(iocInit() == 0);
}



#ifdef VX_WORKS

void vxWorksMain(const char *persist, int interval);
void vxWorksMain(const char *persist, int interval)
{
    persistence_file = persist;
    persistence_interval = interval;
    ioc_main();
}

#else


static bool parse_args(int argc, const char *argv[])
{
    if (TEST_OK_(argc == 3, "Wrong number of arguments"))
    {
        persistence_file = argv[1];
        persistence_interval = atoi(argv[2]);
        return true;
    }
    else
        return false;
}


int main(int argc, const char *argv[])
{
    bool ok =
        parse_args(argc, argv)  &&
        ioc_main()  &&
        TEST_IO(iocsh(NULL))  &&
        DO(terminate_persistent_state());
    return ok ? 0 : 1;
}
#endif
