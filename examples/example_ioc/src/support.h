/* Miscellaneous helper functions for example code. */

/* Blocks for the specified interval, returns false on an IO error, aborts if
 * interval is not positive. */
bool sleep_for(double interval);

/* Generates some vaguely interesting waveform data. */
void compute_waveform(double freq, size_t count, double waveform[]);
double sum_waveform(size_t count, const double waveform[]);

/* Renders double waveform as integer, useful for alternative data format. */
void wf_double_to_int(
    size_t count, const double wf_in[], double scale, int wf_out[]);
