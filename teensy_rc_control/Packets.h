#if !defined(Packets_h)
#define Packets_h

struct TrimInfo {
  enum {
    PacketCode = 'T'
  };
  uint16_t eeTrimSteer;
  uint16_t eeTrimThrottle;
  uint16_t curTrimSteer;
  uint16_t curTrimThrottle;
};

struct IBusPacket {
  enum {
    PacketCode = 'i'
  };
  uint16_t data[10];
};

struct SteerControl {
  enum {
    PacketCode = 'S'
  };
  int16_t steer;
  int16_t throttle;
};

#endif  //  Packets_h

