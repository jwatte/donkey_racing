#if !defined(PCA9685Emulator_h)
#define PCA9685Emulator_h

#define PCA9685_I2C_ADDRESS 0x40

class PCA9685Emulator {
public:
  PCA9685Emulator();
  void begin(uint8_t address);
  //  Return true if there are new values
  bool step(uint32_t now);
  //  Return servo PWM value in microseconds
  uint16_t readChannelUs(uint16_t ch);

  void onRequest2();
  void onReceive2(int n);
  
  static void onRequest();
  static void onReceive(int n);
  static PCA9685Emulator *active; //  only one can be active at a time

  enum {
    NUM_CHANNELS = 16
  };
  uint8_t mem[6+NUM_CHANNELS*4];
  uint8_t wptr;
  bool gotwrite;
};

#endif  //  PCA9685Emulator_h
