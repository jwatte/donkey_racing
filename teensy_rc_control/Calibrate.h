#if !defined(Calibrate_h)
#define Calibrate_h

class Calibrate {
  public:
    Calibrate(int16_t center = 1500, int16_t low = 1000, int16_t high = 2000, int16_t deadband = 10, int16_t tolerance = 100)
      : center_(center)
      , low_(low)
      , high_(high)
      , deadband_(deadband)
      , tolerance_(tolerance)
    {
    }

    float mapIn(int16_t value) {
      if (value < low_) {
        if (value < low_ - tolerance_) {
          return 0;
        }
        value = low_;
      }
      if (value > high_) {
        if (value > high_ + tolerance_) {
          return 0;
        }
        value = high_;
      }
      int16_t lowDeadband = center_ - deadband_;
      if (value < lowDeadband) {
        return float(value - lowDeadband) / float(lowDeadband - low_);
      }
      int16_t highDeadband = center_ + deadband_;
      if (value > highDeadband) {
        return float(value - highDeadband) / float(high_ - highDeadband);
      }
      return 0;
    }

    int16_t mapOut(float value) {
      if (value < -1.0f) {
        value = -1.0f;
      }
      if (value > 1.0f) {
        value = 1.0f;
      }
      if (value < 0.0f) {
        value = -value;
        return (int16_t)(low_ * value + (center_ - deadband_) * (1.0f - value));
      }
      if (value > 0.0f) {
        return (int16_t)(high_ * value + (center_ + deadband_) * (1.0f - value));
      }
      return center_;
    }

    int16_t center_;
    int16_t low_;
    int16_t high_;
    int16_t deadband_;
    int16_t tolerance_;
};

#endif  //  Calibrate_h

