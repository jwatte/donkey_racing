#if !defined(RaspiCapture_h)
#define RaspiCapture_h

#include <stdint.h>

struct CaptureParams {
    unsigned int width;
    unsigned int height;
    char const *encodedFilename;
    void (*buffer_callback)(void *data, size_t size, void *cookie);
    void *buffer_cookie;
};

/* Capture will immediately start calling the callback. 
 * However, recording encoded video to file only happens 
 * after set_recording() is called with true.
 */
int setup_capture(CaptureParams const *cp);
int stop_capture();
void set_recording(bool rec);
uint64_t get_microseconds();
bool insert_metadata(uint16_t type, void const *data, uint16_t size);

#endif  //  RaspiCapture_h
