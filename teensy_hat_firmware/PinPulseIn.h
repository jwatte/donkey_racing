#if !defined(PinPulseIn_h)
#define PinPulseIn_h

#define STRINGIZE(Pin) #Pin

template<unsigned char Pin>
class PinPulseIn {

  public:

    PinPulseIn() {
    }

    bool hasValue() {
      return present_;
    }

    uint32_t getValue() {
      long ret = 0;
      __disable_irq();
      if (present_) {
        ret = micros_;
        present_ = false;
      }
      __enable_irq();
      if (ret < 500 || ret > 2500) {
        ret = 0;
      }
      return ret;
    }

    void begin() {
      micros_ = 0;
      startTime_ = 0;
      present_ = false;
      pinMode(Pin, INPUT);
      attachInterrupt(Pin, &PinPulseIn<Pin>::on_interrupt, CHANGE);
    }

  protected:

    static uint32_t startTime_;
    static uint32_t micros_;
    static bool present_;
    
    static void on_interrupt() {
      if (digitalRead(Pin) == HIGH) {
        startTime_ = micros();
      } else {
        if (startTime_) {
          micros_ = micros() - startTime_;
          present_ = true;
          startTime_ = 0;
        }
      }
    }
};

/* todo: merge these so double linkage doesn't happen */
template<unsigned char Pin> uint32_t PinPulseIn<Pin>::startTime_;
template<unsigned char Pin> uint32_t PinPulseIn<Pin>::micros_;
template<unsigned char Pin> bool PinPulseIn<Pin>::present_;


#endif  //  PinPulseIn_h

