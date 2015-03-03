/* This is a little helper component that has somewhat grown.  All it is meant
 * to be is a thread which can simulate hardware triggers at a programmable
 * interval, but doing this properly and allowing the interval to be changed is
 * ... painful.
 *   The main issue is that if an excessively long interval is set it should be
 * possible to change to a short interval, in which case we would expect the
 * long interval to be immediately interrupted.  This means we have to build
 * things around pthread_cond_timedwait, and suddenly everything is trick and
 * fiddly and hard to get just right. */

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include "error.h"
#include "epics_device.h"
#include "epics_extra.h"

#include "trigger_thread.h"


#define NSEC    1000000000



struct trigger_thread {
    bool running;                   // Used to trigger orderly thread shutdown
    void *context;                  // Context for trigger_event
    void (*trigger_event)(void *context);    // Trigger event callback

    pthread_t thread_id;            // Thread id of process
    pthread_mutex_t mutex;          // Mutex for locked thread access
    pthread_cond_t cond;            // Condition for notifying interval change

    bool interval_set;              // If not we wait for interval to be set
    struct timespec interval;       // Interval between trigger events
    struct timespec start;          // Records start point of current interval
};


void lock_trigger_thread(struct trigger_thread *thread)
{
    ASSERT_PTHREAD(pthread_mutex_lock(&thread->mutex));
}

void unlock_trigger_thread(struct trigger_thread *thread)
{
    ASSERT_PTHREAD(pthread_mutex_unlock(&thread->mutex));
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Timespec support. */

/* Ensures .tv_nsec in range 0..NSEC-1, assuming it's not too out of range on
 * entry. */
static void normalise_timespec(struct timespec *ts)
{
    if (ts->tv_nsec >= NSEC)
    {
        ts->tv_sec += 1;
        ts->tv_nsec -= NSEC;
    }
}

/* Compute timespec from interval in seconds. */
static void double_to_timespec(double interval, struct timespec *ts)
{
    double seconds;
    double fraction = modf(interval, &seconds);
    ts->tv_sec = (time_t) seconds;
    ts->tv_nsec = (long) round(NSEC * fraction);
    normalise_timespec(ts);     // In case fraction > 1 - 0.5e-9
printf("interval: %ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
}

/* Adds two timespec values together. */
static struct timespec add_timespec(
    const struct timespec *ts1, const struct timespec *ts2)
{
    struct timespec ts = {
        .tv_sec  = ts1->tv_sec  + ts2->tv_sec,
        .tv_nsec = ts1->tv_nsec + ts2->tv_nsec };
    normalise_timespec(&ts);
    return ts;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static bool wait_for_event(struct trigger_thread *thread, bool *timeout)
{
    lock_trigger_thread(thread);

    if (thread->interval_set)
    {
        struct timespec target =
            add_timespec(&thread->start, &thread->interval);
// printf("target: %ld.%09ld\n", target.tv_sec, target.tv_nsec);

        int error = pthread_cond_timedwait(
            &thread->cond, &thread->mutex, &target);
        if (error != ETIMEDOUT)
            ASSERT_PTHREAD(error);

        *timeout = error == ETIMEDOUT;
    }
    else
    {
        /* Timeout will not happen, just wait for something to change. */
        ASSERT_PTHREAD(pthread_cond_wait(&thread->cond, &thread->mutex));
        *timeout = false;
    }

    bool running = thread->running;
    unlock_trigger_thread(thread);

    return running;
}


static void *trigger_event_thread(void *context)
{
    struct trigger_thread *thread = context;

    /* Set the reference point for the first event to be after EPICS has
     * completed startup. */
    wait_for_epics_start();
    ASSERT_IO(clock_gettime(CLOCK_REALTIME, &thread->start));

    bool call_trigger;
    while (wait_for_event(thread, &call_trigger))
        if (call_trigger)
        {
            /* There's a plus and a minus to setting the time reference here,
             * immediately before calling trigger event.
             *  +   It's simple
             *  -   The start point will drift, events will not be precisely
             *      spaced at interval seconds. */
            ASSERT_IO(clock_gettime(CLOCK_REALTIME, &thread->start));
            thread->trigger_event(thread->context);
        }

    return NULL;
}


struct trigger_thread *create_trigger_thread(
    void (*trigger_event)(void *context), void *context)
{
    struct trigger_thread *thread = malloc(sizeof(struct trigger_thread));
    thread->running = true;
    thread->context = context;
    thread->trigger_event = trigger_event;
    thread->interval_set = false;

    bool ok =
        TEST_PTHREAD(pthread_mutex_init(&thread->mutex, NULL))  &&
        TEST_PTHREAD(pthread_cond_init(&thread->cond, NULL))  &&
        TEST_PTHREAD(pthread_create(
            &thread->thread_id, NULL, trigger_event_thread, thread));

    if (ok)
        return thread;
    else
    {
        free(thread);
        return NULL;
    }
}


void destroy_trigger_thread(struct trigger_thread *thread)
{
    /* Tell the thread to stop running and wake it up. */
    lock_trigger_thread(thread);
    thread->running = false;
    ASSERT_PTHREAD(pthread_cond_signal(&thread->cond));
    unlock_trigger_thread(thread);

    /* Wait for the thread to terminate; unless it's stuck inside a trigger
     * callback this should be pretty quick. */
    ASSERT_PTHREAD(pthread_join(thread->thread_id, NULL));

    /* Now we can tear down the resources in an orderly way. */
    ASSERT_PTHREAD(pthread_mutex_destroy(&thread->mutex));
    ASSERT_PTHREAD(pthread_cond_destroy(&thread->cond));
    free(thread);
}


void set_trigger_interval(struct trigger_thread *thread, double interval)
{
printf("set_trigger_interval %p, %g\n", thread, interval);
    lock_trigger_thread(thread);

    thread->interval_set = interval > 0;
    if (thread->interval_set)
        double_to_timespec(interval, &thread->interval);

    ASSERT_PTHREAD(pthread_cond_signal(&thread->cond));
    unlock_trigger_thread(thread);
}
