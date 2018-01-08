#if !defined(serial_h)
#define serial_h

#include <stdint.h>

struct TrimInfo;
struct IBusPacket;
struct SteerControl;
struct VoltageLevels;
struct EchoEnabled;
struct TurnOff;

void serial_steer(float steer, float throttle);
bool start_serial(char const *port, int speed);
void stop_serial();
void serial_power_off();
void serial_enable_echo(bool en);

TrimInfo const *serial_trim_info(uint64_t *oTime, bool *oFresh);
IBusPacket const *serial_ibus_packet(uint64_t *oTime, bool *oFresh);
VoltageLevels const *serial_voltage_levels(uint64_t *oTime, bool *oFresh);

extern float gSteerScale;
extern float gThrottleScale;
extern float gThrottleMin;
extern float gSteer;
extern float gThrottle;

#endif  //  serial_h

