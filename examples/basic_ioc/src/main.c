#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#include <iocsh.h>

#include "error.h"
#include "epics_device.h"

static int read_timestamp(void)
{
    return (int) time(NULL);
}

static bool publish_pvs(void)
{
    PUBLISH_READER(longin, "TSEC", read_timestamp);
    return true;
}

int main(int argc, const char *argv[])
{
    bool ok =
        initialise_epics_device()  &&
        publish_pvs()  &&
        TEST_IO(iocsh("st.cmd") == 0)  &&
        TEST_IO(iocsh(NULL));
    return ok ? 0 : 1;
}
