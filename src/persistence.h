/* Header file for core persistent variable support. */

/* Must be called before marking any variables as persistent. */
bool initialise_persistent_state(const char *FileName, int save_interval);

/* Loads the persistent state from file if the file is present.  Note that if no
 * file is present this is not an error.  The variables to be loaded must first
 * be defined by calling create_persistent_{variable,waveform}. */
bool load_persistent_state(void);

/* Writes out persistent state file if necessary. */
bool update_persistent_state(void);

void terminate_persistent_state(void);
