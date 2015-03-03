/* Abstract type for trigger simulator. */
struct trigger_thread;

/* Creates a thread which will periodically call trigger_event() once an
 * interval has been specified. */
struct trigger_thread *create_trigger_thread(
    void (*trigger_event)(void *context), void *context);

/* This tears down the trigger thread, blocking until the thread returns. */
void destroy_trigger_thread(struct trigger_thread *thread);

/* Sets the trigger interval for the specified trigger thread.  If the interval
 * is set to a period shorter than the time the thread has been waiting it will
 * immediately trigger. */
void set_trigger_interval(struct trigger_thread *thread, double interval);

/* This gives us access to the thread mutex.  Note that the trigger event
 * callback is called *unlocked*. */
void lock_trigger_thread(struct trigger_thread *thread);
void unlock_trigger_thread(struct trigger_thread *thread);
