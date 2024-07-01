#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include "error.h"
#include "epics_device.h"
#include "epics_extra.h"
#include "persistence.h"

#include "example_pvs.h"


#define WF_LENGTH   128

static double base_frequency;
static double waveform[WF_LENGTH];
static double sum;

static double event_interval = 10;
static struct epics_interlock *interlock;

static double scaling = 0.1;
static int trigger_count = 0;
static int trigger_waveform[WF_LENGTH];

static EPICS_STRING strings[4] = {
    { "1" }, { "2" }, { "3" }, { "4" }
};
static unsigned int strings_len = 4;


static void update_waveform(void)
{
    for (int i = 0; i < WF_LENGTH; i ++)
        trigger_waveform[i] = (int) round(1e3 *
            cos(scaling * base_frequency * trigger_count * i));
}

static void process_event(void)
{
    interlock_wait(interlock);

    trigger_count += 1;
    update_waveform();

    interlock_signal(interlock, NULL);
}

static void reset_trigger_count(void)
{
    /* This can fail (and do nothing) because it's not interlocked with the
     * increment in process_event above.  One solution is to use the interlock,
     * but the proper solution normally is to use a separate pthread lock.  For
     * this example we don't worry, as the failure is inconsequential. */
    trigger_count = 0;
}


static bool sleep_for(double interval)
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


/* This thread simulates asynchronous events. */
static void *event_thread(void *context)
{
    /* Do this to ensure that event_interval is up to date with saved persistent
     * value, otherwise the first sleep will use the default interval. */
    wait_for_epics_start();

    while (true)
    {
        sleep_for(event_interval);
        process_event();
    }
    return NULL;
}


static void set_frequency(double frequency)
{
    base_frequency = frequency;

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


/* We publish a group of PVs to check prefix handling. */
static void publish_group(const char *prefix)
{
    int *target = calloc(1, sizeof(int));

    WITH_NAME_PREFIX(prefix)
    {
        PUBLISH_READ_VAR(longin, "READ", *target);
        PUBLISH_WRITE_VAR(longout, "WRITE", *target);
    }
}


static bool add_one(void *context, int *value)
{
    *value += 1;
    return true;
}


error__t initialise_example_pvs(void)
{
    PUBLISH_WRITER_P(ao, "FREQ", set_frequency);
    PUBLISH_WF_READ_VAR(double, "WF", WF_LENGTH, waveform);
    PUBLISH_READ_VAR(ai, "SUM", sum);

    interlock = create_interlock("TRIG", false);
    PUBLISH_WF_READ_VAR(int, "TRIGWF", WF_LENGTH, trigger_waveform);
    PUBLISH_READ_VAR(longin, "COUNT", trigger_count);
    PUBLISH_ACTION("RESET", reset_trigger_count);

    PUBLISH_WRITE_VAR_P(ao, "INTERVAL", event_interval);
    PUBLISH_WRITE_VAR_P(ao, "SCALING", scaling);

    PUBLISH_ACTION("WRITE", write_persistent_state);

    publish_group("A");
    publish_group("B");

    PUBLISH(longout, "ADD_ONE", add_one);

    PUBLISH_WF_WRITE_VAR_LEN_P(
        EPICS_STRING, "STRINGS", 4, strings_len, strings);

    pthread_t thread_id;
    return TEST_PTHREAD(pthread_create(&thread_id, NULL, event_thread, NULL));
}
