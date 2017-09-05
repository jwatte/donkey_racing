#if !defined(serial_h)
#define serial_h

#include <stdint.h>

struct TrimInfo;
struct IBusPacket;
struct SteerControl;

bool start_serial(char const *port, int speed);
void stop_serial();

TrimInfo const *serial_trim_info(uint64_t *oTime, bool *oFresh);
IBusPacket const *serial_ibus_packet(uint64_t *oTime, bool *oFresh);
SteerControl const *serial_steer_control(uint64_t *oTime, bool *oFresh);

#endif  //  serial_h

