#include "PinPulseIn.h"
#include "Calibrate.h"
#include <Servo.h>


#define CENTER_CALIBRATION 1540

PinPulseIn<14> rcSteer;
PinPulseIn<15> rcThrottle;

Servo carSteer;
Servo carThrottle;

Calibrate inSteer;
Calibrate inThrottle;
Calibrate outSteer(CENTER_CALIBRATION);
Calibrate outThrottle;

int16_t readSteer = 0;
int16_t readThrottle = 0;

int16_t sendSteer = 0;
int16_t sendThrottle = 0;

bool driving = false;
uint32_t lastModeTime = 0;


void setup() {
  rcSteer.init();
  rcThrottle.init();
  carSteer.attach(23);
  carThrottle.attach(22);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(13, OUTPUT);
}


void loop() {

  uint32_t now = millis();
  
  if (rcThrottle.hasValue()) {
    readThrottle = rcThrottle.getValue();
    sendThrottle = outThrottle.mapOut(inThrottle.mapIn(readThrottle));
    lastModeTime = now;
    driving = true;
  }
  if (rcSteer.hasValue()) {
    readSteer = rcSteer.getValue();
    sendSteer = outSteer.mapOut(inSteer.mapIn(readSteer));
    lastModeTime = now;
    driving = true;
  }

  if (now - lastModeTime > 300) {
    lastModeTime = now - 300;
    driving = false;
  }

  if (!driving) {
    /* turn off the servos */
    carSteer.writeMicroseconds(outSteer.center_);
    carThrottle.writeMicroseconds(outThrottle.center_);
    digitalWrite(13, (now & 192) == 64 );
    digitalWrite(3, (now & 192) == 128);
    digitalWrite(4, (now & 192) == 192);
  } else {
    /* send the current heading/speed */
    carSteer.writeMicroseconds(sendSteer);
    carThrottle.writeMicroseconds(sendThrottle);
    digitalWrite(13, driving ? HIGH : LOW);
    digitalWrite(3, sendSteer < outSteer.center_ ? HIGH : LOW);
    digitalWrite(4, sendSteer > outSteer.center_ ? HIGH : LOW);
  }

  do {
    // usbCommand.readSome();
    // ser3Command.readSome();
  } while (millis() < now + 5);
}


