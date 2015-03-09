/* Header file for core persistent variable support. */

/* Loads the persistent state from file if the file is present.  Note that if no
 * file is present this is not an error.  The variables to be loaded must first
 * be defined by calling create_persistent_{variable,waveform}. */
bool load_persistent_state(
    const char *FileName, bool check_parse, int save_interval);

/* Writes out persistent state file if necessary. */
bool update_persistent_state(void);

void terminate_persistent_state(void);
