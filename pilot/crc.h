#if !defined(crc_h)
#define crc_h

#include <stdint.h>
#include <unistd.h>

extern "C" {
extern int16_t crc_kermit(void const *data, size_t size);
}

#endif  //  crc_h
