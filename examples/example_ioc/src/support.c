/* Support code for example PVs.  Separated out to try to avoid cluttering the
 * example code with unnecessary details. */

#include <stdbool.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include "error.h"

#include "support.h"


bool sleep_for(double interval)
{
    ASSERT_OK(interval > 0);

    /* Split interval into seconds and nanoseconds. */
    double seconds;
    double fraction = modf(interval, &seconds);

    struct timespec target = {
        .tv_sec = (time_t) floor(seconds),
        .tv_nsec = (long) floor(1e9 * fraction) };
    return TEST_IO(nanosleep(&target, NULL));
}


void compute_waveform(double freq, size_t count, double waveform[])
{
    for (size_t i = 0; i < count; i ++)
        waveform[i] = sin(freq * (double) i);
}


double sum_waveform(size_t count, const double waveform[])
{
    double sum = 0;
    for (size_t i = 0; i < count; i ++)
        sum += waveform[i];
    return sum;
}


void wf_double_to_int(
    size_t count, const double wf_in[], double scale, int wf_out[])
{
    for (size_t i = 0; i < count; i ++)
        wf_out[i] = (int) round(scale * wf_in[i]);
}
