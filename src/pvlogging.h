/* Interface to EPICS PV logging interface. */

bool HookLogging(int max_length);
bool hook_pv_logging(const char *access_file, int max_length);
