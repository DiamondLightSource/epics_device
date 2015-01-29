#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include "error.h"
#include "epics_device.h"
#include "epics_extra.h"
#include "persistence.h"

#include "example_pvs.h"


#define WF_LENGTH   128

static double waveform[WF_LENGTH];
static double sum;


static void set_frequency(double frequency)
{
    sum = 0;
    for (int i = 0; i < WF_LENGTH; i ++)
    {
        waveform[i] = sin(i * frequency);
        sum += waveform[i];
    }
}


static void write_persistent_state(void)
{
    update_persistent_state();
}

static void assert_fail(void)
{
    ASSERT_FAIL();
}


bool initialise_example_pvs(void)
{
    PUBLISH_WRITER_P(ao, "FREQ", set_frequency);
    PUBLISH_WF_READ_VAR(double, "WF", WF_LENGTH, waveform);
    PUBLISH_READ_VAR(ai, "SUM", sum);

    PUBLISH_ACTION("WRITE", write_persistent_state);
    PUBLISH_ACTION("FAIL", assert_fail);

    return true;
}
