/* An example of triggered PVs. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>

#include "error.h"
#include "epics_device.h"
#include "epics_extra.h"

#include "support.h"
#include "trigger_thread.h"

#include "triggered_pvs.h"


struct triggered_pvs {
    struct trigger_thread *thread;
};


static void on_trigger(void *context)
{
    struct triggered_pvs *pvs = context;
printf("tick: %p\n", pvs);
}


static bool set_interval(void *context, const double *interval)
{
    struct triggered_pvs *pvs = context;
printf("set interval %g\n", *interval);
    set_trigger_interval(pvs->thread, *interval);
    return true;
}


static struct triggered_pvs *create_triggered_pvs(void)
{
    struct triggered_pvs *pvs = malloc(sizeof(struct triggered_pvs));
    pvs->thread = create_trigger_thread(on_trigger, pvs);
    PUBLISH(ao, "INTERVAL", set_interval, .context = pvs, .persist = true);
    return pvs;
}


static struct triggered_pvs *triggered_pvs;

bool initialise_triggered_pvs(void)
{
    triggered_pvs = create_triggered_pvs();
    return triggered_pvs != NULL;
}
