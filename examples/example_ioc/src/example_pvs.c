#include <stdbool.h>
#include <stddef.h>
#include <math.h>

#include "error.h"
#include "epics_device.h"

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


bool initialise_example_pvs(void)
{
    PUBLISH_WRITER(ao, "FREQ", set_frequency);
    PUBLISH_WF_READ_VAR(double, "WF", WF_LENGTH, waveform);
    PUBLISH_READ_VAR(ai, "SUM", sum);

    return true;
}
