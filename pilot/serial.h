#if !defined(serial_h)
#define serial_h

#include <stdint.h>

struct TrimInfo;
struct IBusPacket;
struct SteerControl;

void serial_steer(float steer, float throttle);
bool start_serial(char const *port, int speed);
void stop_serial();

TrimInfo const *serial_trim_info(uint64_t *oTime, bool *oFresh);
IBusPacket const *serial_ibus_packet(uint64_t *oTime, bool *oFresh);
SteerControl const *serial_steer_control(uint64_t *oTime, bool *oFresh);

extern float gSteerScale;
extern float gThrottleScale;
extern float gThrottleMin;
extern float gSteer;
extern float gThrottle;

#endif  //  serial_h

