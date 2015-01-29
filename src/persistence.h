/* Header file for core persistent variable support. */

/* The following persistence types are supported. */
enum PERSISTENCE_TYPES {
    PERSISTENT_bool,
    PERSISTENT_char,
    PERSISTENT_short,
    PERSISTENT_int,
    PERSISTENT_float,
    PERSISTENT_double,
    PERSISTENT_string,
};


/* Must be called before marking any variables as persistent. */
bool initialise_persistent_state(const char *FileName, int save_interval);

/* Loads the persistent state from file if the file is present.  Note that if no
 * file is present this is not an error.  The variables to be loaded must first
 * be defined by calling create_persisten_{variable,waveform}. */
bool load_persistent_state(void);

/* Writes out persistent state file if necessary. */
void terminate_persistent_state(void);


/* Creates new persistent variable.  Note that type is *not* checked for
 * validity, *must* be a valid enum value! */
void create_persistent_variable(const char *name, enum PERSISTENCE_TYPES type);
void create_persistent_waveform(
    const char *name, enum PERSISTENCE_TYPES type, size_t max_length);

/* Updates variable from value stored on disk, returns false if no value
 * returned.  The function load_persistent_state() must be called first. */
bool read_persistent_variable(const char *name, void *variable);
bool read_persistent_waveform(const char *name, void *variable, size_t *length);
/* Writes value to persistent variable. */
void write_persistent_variable(const char *name, const void *value);
void write_persistent_waveform(
    const char *name, const void *value, size_t length);
