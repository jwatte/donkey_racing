#include "Defines.h"
#include "Calibrate.h"
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
  pinMode(PIN_POWER_CONTROL, OUTPUT);
  digitalWrite(PIN_POWER_CONTROL, HIGH);

  rcSteer.begin();
  rcThrottle.begin();
  rcMode.begin();
  fsIbus.begin();

  RPI_SERIAL.begin(RPI_BAUD_RATE);

  pwmEmulation.begin(PCA9685_I2C_ADDRESS);
}

uint32_t lastVoltageCheck = 0;

void loop() {
  
  uint32_t now = millis();

  /* look for i2c drive commands */
  pwmEmulation.step(now);

  /* update ibus input */
  fsIbus.step(now);

  /* Turn off if under-volt */
  uint16_t v = analogRead(PIN_A_VOLTAGE_IN);
  float voltage = VOLTAGE_FACTOR * v;
  if (now-lastVoltageCheck > 100) {
    lastVoltageCheck = now;
    if (voltage < MINIMUM_VOLTAGE) {
      ++numBadVolt;
      if (numBadVolt > NUM_BAD_VOLT_TO_TURN_OFF) {
        digitalWrite(PIN_POWER_CONTROL, LOW);
      }
    } else {
      if (numBadVolt > 0) {
        --numBadVolt;
      } else {
        digitalWrite(PIN_POWER_CONTROL, HIGH);
      }
    }
  }
}


