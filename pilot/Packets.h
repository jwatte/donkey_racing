#if !defined(Packets_h)
#define Packets_h

struct TrimInfo {
    int32_t trimSteer;
    int32_t trimThrottle;
    enum {
        Token = 'T',
        NumInts = 2
    };
};
struct IBusPacket {
    int32_t timing;
    int32_t values[10];
    enum {
        Token = 'C',
        NumInts = 11
    };
};

struct SteerControl {
    int32_t steer;
    int32_t throttle;
    enum {
        Token = 'D',
        NumInts = 2
    };
};

struct VoltageLevels {
    int32_t voltage;
    int32_t minVoltage;
    int32_t initVoltage;
    enum {
        Token = 'V',
        NumInts = 3
    };
};

struct EchoEnabled {
    int32_t echo;
    enum {
        Token = 'X',
        NumInts = 1
    };
};

struct TurnOff {
    enum {
        Token = 'O',
        NumInts = 0,
    };
};

#endif  //  Packets_h
