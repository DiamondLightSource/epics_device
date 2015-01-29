/* Configuration file parsing code.  This contains quite a bit of code and
 * concept in common with persistence.{c,h}, so some sensible refactoring would
 * be appropriate at some point. */


/* The config file is just a list of named integers. */
struct config_entry
{
    const char *name;
    int *result;
};
#define CONFIG(variable)    { #variable, &variable }

bool config_parse_file(
    const char *file_name, const struct config_entry *config_table,
    size_t config_size);
