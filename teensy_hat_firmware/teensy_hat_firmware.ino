#include "Defines.h"
#include "PinPulseIn.h"
#include "FlySkyIBus.h"
#include "PCA9685Emulator.h"

#include <Servo.h>


PinPulseIn<11> rcSteer;
PinPulseIn<12> rcThrottle;
PinPulseIn<13> rcMode;

Servo svoSteer;
Servo svoThrottle;

uint16_t iBusInput[10];
FlySkyIBus fsIbus(IBUS_SERIAL, iBusInput, 10);

PCA9685Emulator pwmEmulation;

uint16_t numBadVolt = 0;
float minimumVoltage = 6.0f;
float determinedVoltage = 0.0f;
uint16_t determinedCount = 0;

// C + CRLF + 0 == 4
// space + age <= 6
// space + value <= 6 each times 10 channels
char writeBuf[6*10+4+6];
char readBuf[64];
uint8_t readBufPtr;
uint16_t serialSteer;
uint16_t serialThrottle;

bool serialEchoEnabled = false;


void read_voltage(uint16_t &v, float &voltage) {
  v = analogRead(PIN_A_VOLTAGE_IN);
  voltage = VOLTAGE_FACTOR * v;
}

void setup() {
  analogReference(DEFAULT);
  analogReadRes(12);
  
  pinMode(PIN_STEER_IN, INPUT_PULLDOWN);
  pinMode(PIN_THROTTLE_IN, INPUT_PULLDOWN);
  pinMode(PIN_MODE_IN, INPUT_PULLDOWN);
  (void)analogRead(PIN_A_VOLTAGE_IN);

  pinMode(PIN_STEER_OUT, OUTPUT);
  svoSteer.attach(PIN_STEER_OUT);
  pinMode(PIN_THROTTLE_OUT, OUTPUT);
  svoThrottle.attach(PIN_THROTTLE_OUT);
#if defined(POWER_DEVMODE) && POWER_DEVMODE
  pinMode(PIN_POWER_CONTROL, INPUT_PULLUP);
#else
  pinMode(PIN_POWER_CONTROL, OUTPUT);
  digitalWrite(PIN_POWER_CONTROL, HIGH);
#endif

  rcSteer.begin();
  rcThrottle.begin();
  rcMode.begin();
  fsIbus.begin();

  RPI_SERIAL.begin(RPI_BAUD_RATE);

  pwmEmulation.begin(PCA9685_I2C_ADDRESS);

  //  allow things to settle
  delay(300);
}

uint32_t lastVoltageCheck = 0;

void check_voltage(uint32_t now) {
  /* Turn off if under-volt */
  uint16_t v;
  float voltage;
  if (now-lastVoltageCheck > 100) {
    read_voltage(v, voltage);
    v = (uint16_t)(voltage * 100 + 0.5f);
    if (determinedCount != 20) {
      determinedVoltage += voltage / 20;
      ++determinedCount;
      if (determinedCount == 20) {
        if (determinedVoltage > 3 * 4.3f) {
          minimumVoltage = VOLTAGE_PER_CELL * 4;
        } else if (determinedVoltage > 2 * 4.3f) {
          minimumVoltage = VOLTAGE_PER_CELL * 3;
        } else {
          minimumVoltage = VOLTAGE_PER_CELL * 2;
        }
      }
    }
    if (serialEchoEnabled && !!RPI_SERIAL) {
      char *wptr = writeBuf;
      *wptr++ = 'V';
      *wptr++ = ' ';
      itoa((int)v, wptr, 10);
      wptr += strlen(wptr);
      *wptr++ = ' ';
      itoa((int)(minimumVoltage * 100 + 0.5f), wptr, 10);
      wptr += strlen(wptr);
      *wptr++ = ' ';
      itoa((int)(determinedVoltage * 100 + 0.5f), wptr, 10);
      wptr += strlen(wptr);
      *wptr = 0;
      RPI_SERIAL.println(writeBuf);
    }
    lastVoltageCheck = now;
    if (voltage < minimumVoltage) {
      ++numBadVolt;
      if (numBadVolt > NUM_BAD_VOLT_TO_TURN_OFF) {
#if defined(POWER_DEVMODE) && POWER_DEVMODE
        pinMode(PIN_POWER_CONTROL, OUTPUT);
#endif
        digitalWrite(PIN_POWER_CONTROL, LOW);
        numBadVolt = NUM_BAD_VOLT_TO_TURN_OFF;
      }
    } else {
      if (numBadVolt > 0) {
        if (numBadVolt >= NUM_BAD_VOLT_TO_TURN_OFF) {
          //  I previously pulled power low, but am still running, so 
          //  presumably I'm being fed power through the non-switched connector.
          //  Thus, just reset things to "it's good now."
#if defined(POWER_DEVMODE) && POWER_DEVMODE
          pinMode(PIN_POWER_CONTROL, INPUT_PULLUP);
#else
          digitalWrite(PIN_POWER_CONTROL, HIGH);
#endif
        }
        --numBadVolt;
      }
    }
  }
}

uint32_t lastInputTime = 0;

void read_inputs(uint32_t now) {
  /* update PWM inputs */
  if (rcSteer.hasValue()) {
    iBusInput[0] = rcSteer.getValue();
    lastInputTime = now;
  }
  if (rcThrottle.hasValue()) {
    iBusInput[1] = rcThrottle.getValue();
    lastInputTime = now;
  }
  if (rcMode.hasValue()) {
    iBusInput[2] = rcMode.getValue();
    lastInputTime = now;
  }
  
  /* update ibus input */
  fsIbus.step(now);
  if (fsIbus.hasFreshFrame()) {
    lastInputTime = now;
  }

  /*  RC and iBus will fight, if they are both wired up.  
   *  If only one is wired, that one will take over.
   *  Either don't do that, or live with the fighting -- the 
   *  delta will be small as they are the same value (as long
   *  as they're connected in the same order to the same 
   *  receiver)
   */
}

uint32_t lastI2cTime = 0;

void read_i2c(uint32_t now) {
  if (pwmEmulation.step(now)) {
    lastI2cTime = now;
  }
}

uint32_t lastSerialTime = 0;
int servalues[4] = { 0 };

char dbg[50];

void do_serial_command(char const *cmd, uint32_t now) {
  char const *bufin = cmd;
  char cmdchar = *cmd;
  ++cmd;
  int nvals = 0;
  while (nvals < 4 && *cmd) {
    if (*cmd <= 32) {
      ++cmd;
    } else {
      servalues[nvals] = atoi(cmd);
      ++nvals;
      while (*cmd >= '0' && *cmd <= '9') {
        ++cmd;
      }
    }
  }
  if (cmdchar == 'D' && nvals == 2) {
    //  drive
    serialSteer = servalues[0];
    serialThrottle = servalues[1];
    lastSerialTime = now;
  } else if (cmdchar == 'O' && nvals == 0) {
    //  off
    pinMode(PIN_POWER_CONTROL, OUTPUT);
    digitalWrite(PIN_POWER_CONTROL, LOW);
  } else if (cmdchar == 'X' && nvals == 1) {
    //  enable/disable serial echo
    serialEchoEnabled = servalues[0] > 0;
  } else {
    //  unknown
    if (serialEchoEnabled) {
      RPI_SERIAL.println("?");
    }
    if (!!Serial) {
      int npr = sprintf(dbg, "?? %s\r\n", bufin);
      Serial.write(dbg, npr);
    }
  }
}

/*  Serial input commands are a command letter, followed by optional
 *  space separated arguments, followed by <CR><LF>.
 *  D steer throttle
 *  - D means "drive"
 *  - steer is 1000 to 2000
 *  - throttle is 1000 to 2000
 *  O
 *  - O means "turn off"
 */
void read_serial(uint32_t now) {
  while (RPI_SERIAL.available() > 0) {
    if (readBufPtr == sizeof(readBuf)) {
      readBufPtr = 0;
    }
    readBuf[readBufPtr++] = RPI_SERIAL.read();
    if (readBufPtr >= 2 && readBuf[readBufPtr-2] == 13 && readBuf[readBufPtr-1] == 10) {
      readBuf[readBufPtr-2] = 0;
      do_serial_command(readBuf, now);
      readBufPtr = 0;
    }
  }
}

void apply_control_adjustments(uint16_t &steer, uint16_t &throttle) {
  steer = (uint16_t)(((int)steer - 1500 + STEER_ADJUSTMENT) * STEER_MULTIPLY + 1500);
  if (steer < MIN_STEER) {
    steer = MIN_STEER;
  }
  if (steer > MAX_STEER) {
    steer = MAX_STEER;
  }

  throttle += THROTTLE_ADJUSTMENT;
  if (throttle < MIN_THROTTLE) {
    throttle = MIN_THROTTLE;
  }
  if (throttle > MAX_THROTTLE) {
    throttle = MAX_THROTTLE;
  }
}

void generate_output(uint32_t now) {

  uint16_t steer = 1500;
  uint16_t throttle = 1500;
  bool autoSource = false;

  if (lastInputTime && ((iBusInput[2] > 1800) || (iBusInput[9] > 1800))) {
    //  if aux channel is high, or tenth channel (right switch on FSi6) is set,
    //  just drive, without auto.
    steer = iBusInput[0];
    throttle = iBusInput[1];
  }
  else if (lastI2cTime) {
    //  I2C trumps serial
    steer = pwmEmulation.readChannelUs(0);
    throttle = pwmEmulation.readChannelUs(1);
    autoSource = true;
  } else if (lastSerialTime) {
    //  serial trumps manual
    steer = serialSteer;
    throttle = serialThrottle;
    autoSource = true;
  } else if (lastInputTime) {
    //  manual drive if available
    steer = iBusInput[0];
    throttle = iBusInput[1];
  }
  
  if (autoSource) {
    //  safety control if auto-driving
    if (iBusInput[1] > 1100 && iBusInput[1] < 1470) {
      //  back up if throttle control says so, but not too much
      //  and not if the input is the "carrier lost" 1000 value.
      throttle = min(iBusInput[1], throttle);
      if (throttle < 1350) {
        throttle = 1350;
      }
      steer = 1500;
    } else {
      if (iBusInput[1] < 1600) {
        //  don't allow driving if throttle doesn't say drive
        throttle = 1500;
      }
    }
  }
  
  apply_control_adjustments(steer, throttle);
  svoSteer.writeMicroseconds(steer);
  svoThrottle.writeMicroseconds(throttle);
}

uint32_t lastSerialWrite = 0;

void update_serial(uint32_t now) {
  if (now - lastSerialWrite > 32) {
    lastSerialWrite = now;
    if (serialEchoEnabled && !!RPI_SERIAL) {
      char *wbuf = writeBuf;
      int d = (int)(now - lastInputTime);
      if (d > 1000 || d < 0) {
        strcpy(wbuf, "C -1");
      } else {
        *wbuf++ = 'C';
        *wbuf++ = ' ';
        itoa(d, wbuf, 10);
      }
      wbuf += strlen(wbuf);
      for (int i = 0; i != 10; ++i) {
        *wbuf++ = ' ';
        itoa(iBusInput[i], wbuf, 10);
        wbuf += strlen(wbuf);
      }
      *wbuf = 0;
      RPI_SERIAL.println(writeBuf);
    }
  }
}


void loop() {
  
  uint32_t now = millis();

  read_inputs(now);
  if (lastInputTime && (now - lastInputTime > INPUT_TIMEOUT)) {
    //  Decide that we don't have any control input if there 
    //  is nothing coming in for a while.
    lastInputTime = 0;
  }
  
  /* look for i2c drive commands */
  read_i2c(now);
  if (lastI2cTime && (now - lastI2cTime > I2C_INPUT_TIMEOUT)) {
    lastI2cTime = 0;
  }

  read_serial(now);
  if (lastSerialTime && (now - lastSerialTime > INPUT_TIMEOUT)) {
    lastSerialTime = 0;
  }

  generate_output(now);

  update_serial(now);

  check_voltage(now);
}


