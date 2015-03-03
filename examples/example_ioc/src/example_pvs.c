#include <stdbool.h>
#include <unistd.h>
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

#include "support.h"

#include "example_pvs.h"


#define WF_LENGTH   128

static double base_frequency;
static double waveform[WF_LENGTH];
static double sum;

// static double event_interval = 10;
static struct epics_interlock *interlock;

static double scaling = 0.1;
static int trigger_count = 0;
static int trigger_waveform[WF_LENGTH];


// static void update_waveform(void)
// {
//     double wf[WF_LENGTH];
//     compute_waveform(
//         scaling * base_frequency * trigger_count, WF_LENGTH, wf);
//     wf_double_to_int(WF_LENGTH, wf, 1e3, trigger_waveform);
// }
// 
// static void process_event(void)
// {
//     interlock_wait(interlock);
// 
//     trigger_count += 1;
//     update_waveform();
// 
//     interlock_signal(interlock, NULL);
// }

static void reset_trigger_count(void)
{
    /* This can fail (and do nothing) because it's not interlocked with the
     * increment in process_event above.  One solution is to use the interlock,
     * but the proper solution normally is to use a separate pthread lock.  For
     * this example we don't worry, as the failure is inconsequential. */
    trigger_count = 0;
}


static void set_frequency(double frequency)
{
    base_frequency = frequency;
    compute_waveform(frequency, WF_LENGTH, waveform);
    sum = sum_waveform(WF_LENGTH, waveform);
}


static void write_persistent_state(void)
{
    update_persistent_state();
}


// /* This thread simulates asynchronous events. */
// static void *event_thread(void *context)
// {
//     /* Do this to ensure that event_interval is up to date with saved persistent
//      * value, otherwise the first sleep will use the default interval. */
//     wait_for_epics_start();
// 
//     while (true)
//     {
//         sleep_for(event_interval);
//         process_event();
//     }
//     return NULL;
// }


bool initialise_example_pvs(void)
{
    PUBLISH_WRITER_P(ao, "FREQ", set_frequency);
    PUBLISH_WF_READ_VAR(double, "WF", WF_LENGTH, waveform);
    PUBLISH_READ_VAR(ai, "SUM", sum);

    PUBLISH_ACTION("WRITE", write_persistent_state);

//     PUBLISH_WRITE_VAR_P(ao, "INTERVAL", event_interval);
    PUBLISH_WRITE_VAR_P(ao, "SCALING", scaling);

    interlock = create_interlock("TRIG", false);
    PUBLISH_READ_VAR(longin, "COUNT", trigger_count);
    PUBLISH_WF_READ_VAR(int, "TRIGWF", WF_LENGTH, trigger_waveform);
    PUBLISH_ACTION("RESET", reset_trigger_count);

    return true;
//     return publish_trigger_pvs();

//     pthread_t thread_id;
//     return TEST_PTHREAD(pthread_create(&thread_id, NULL, event_thread, NULL));
}
