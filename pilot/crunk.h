#if !defined(crunk_h)
#define crunk_h

#include <stdio.h>
#include <string>

bool read_crunk_block(FILE *f, std::string &okey, std::string &oinfo, float *&ovalue);

#endif  //  crunk_h

