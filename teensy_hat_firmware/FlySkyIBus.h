#if !defined(FlySkyIBus_h)
#define FlySkyIBus_h

#define IBUS_BAUDRATE 115200


class FlySkyIBus {
  public:
    FlySkyIBus(HardwareSerial &hws, uint16_t *values, uint8_t cnt)
      : serial_(hws)
      , bufPtr_(0)
      , wanted_(cnt)
      , freshFrame_(0)
      , values_(values)
    {
      if (wanted_ > 14) {
        wanted_ = 14;
      }
    }

    void begin()
    {
      serial_.begin(IBUS_BAUDRATE);
    }

    void step(uint32_t now)
    {
      while (serial_.available()) {
        int i = serial_.read();
        if (bufPtr_ == 0) {
          if (i != 0x20) {
            continue;
          }
        }
        if (bufPtr_ == 1) {
          if (i != 0x40) {
            if (i == 0x20) {
              continue;
            }
            bufPtr_ = 0;
            continue;
          }
        }
        inBuf_[bufPtr_++] = i;
        if (bufPtr_ == sizeof(inBuf_)) {
          parseFrame();
        }
      }
    }

    bool hasFreshFrame() {
      bool ret = freshFrame_ != 0;
      freshFrame_ = 0;
      return ret;
    }

  private:
  
    uint16_t le(unsigned char const *src) {
      return *src + ((uint16_t)src[1] << 8);
    }

    void parseFrame() {
      uint16_t cks = 0xffff;
      for (int i = 0; i < 30; ++i) {
        cks -= inBuf_[i]; //  checksum doesn't detect transposition!
      }
      uint16_t received = le(&inBuf_[30]);
      if (received == cks) {
        freshFrame_ = true;
        for (int ch = 0; ch != wanted_; ch++) {
          values_[ch] = le(&inBuf_[ch*2+2]);
        }
      }
      bufPtr_ = 0;
    }

    HardwareSerial &serial_;
    unsigned char bufPtr_;
    unsigned char wanted_;
    unsigned char freshFrame_;
    uint16_t *values_;
    unsigned char inBuf_[32];
};

#endif

