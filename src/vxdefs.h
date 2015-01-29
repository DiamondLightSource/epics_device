/* This file is only relevant on vxWorks and contains a number of definitions
 * missing from our vxWorks libraries. */

char *strdup(const char *str);
int vsnprintf(char *str, size_t size, const char *format, va_list args);
int asprintf(char **result, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);

typedef ssize_t intptr_t;
typedef size_t uintptr_t;
