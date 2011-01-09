// stupid MSVC
#include <string.h>
#ifdef WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
