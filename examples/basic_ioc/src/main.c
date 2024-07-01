#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <iocsh.h>

#include "error.h"
#include "epics_device.h"

static EPICS_STRING strings[4] = {
    { "Hello" }, { "There" }, { "A longer string" }, { "THE END" }
};

static int read_timestamp(void)
{
    return (int) time(NULL);
}

static error__t publish_pvs(void)
{
    PUBLISH_READER(longin, "TSEC", read_timestamp);
    PUBLISH_WF_READ_VAR(EPICS_STRING, "STRINGS", 4, strings);
    return ERROR_OK;
}

int main(int argc, const char *argv[])
{
    bool ok =
        initialise_epics_device()  ?:
        publish_pvs()  ?:
        TEST_IO(iocsh("st.cmd") == 0)  ?:
        TEST_IO(iocsh(NULL));
    return ok ? 0 : 1;
}
