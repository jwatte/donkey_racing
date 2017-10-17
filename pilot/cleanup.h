#if !defined(cleanup_h)
#define cleanup_h

#include <time.h>

extern bool cleanup_temp_folder(char const *f, time_t before);

#endif  //  cleanup_h
