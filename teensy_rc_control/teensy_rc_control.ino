#include "Calibrate.h"
#include "PinPulseIn.h"
#include "FlySkyIBus.h"

#include <Servo.h>
#include <EEPROM.h>


#define CENTER_CALIBRATION 1540

PinPulseIn<14> rcSteer;

Servo carSteer;
Servo carThrottle;

uint16_t fsValues[10];
FlySkyIBus iBus(Serial1, fsValues, 10);

Calibrate inSteer;
Calibrate inThrottle;
Calibrate outSteer(CENTER_CALIBRATION);
Calibrate outThrottle;

int16_t sendSteer = 0;
int16_t sendThrottle = 0;

bool hasRC = false;
uint32_t lastModeTime = 0;


void readTrim();
void writeTrim();
void maybeTrim(uint32_t now);


void setup() {
  rcSteer.init();
  iBus.begin();
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(13, OUTPUT);
  readTrim();
}


void loop() {

  uint32_t now = millis();

  //  I only use PinPulseIn to detect that there is a carrier
  if (rcSteer.hasValue()) {
    lastModeTime = now;
    hasRC = true;
  }

  if (now - lastModeTime > 300) {
    lastModeTime = now - 300;
    hasRC = false;
  }

  if (hasRC) {
    maybeTrim(now);
  }
  
  if (!hasRC || fsValues[9] < 1750) {
    /* turn off the servos */
    if (carSteer.attached()) {
      carSteer.detach();
    }
    if (carThrottle.attached()) {
      carThrottle.detach();
    }
    digitalWrite(13, (now & 192) == 64 );
    digitalWrite(3, (now & 192) == 128);
    digitalWrite(4, (now & 192) == 192);
  } else {
    /* send the current heading/speed */
    if (!carSteer.attached()) {
      carSteer.attach(23);
    }
    if (!carThrottle.attached()) {
      carThrottle.attach(22);
    }
    if (iBus.hasFreshFrame()) {
      sendSteer = outSteer.mapOut(inSteer.mapIn(fsValues[0]));
      sendThrottle = outThrottle.mapOut(inThrottle.mapIn(fsValues[1]));
    }
    carSteer.writeMicroseconds(sendSteer);
    carThrottle.writeMicroseconds(sendThrottle);
    digitalWrite(13, sendThrottle > outThrottle.center_ ? HIGH : LOW);
    digitalWrite(3, sendSteer < outSteer.center_ ? HIGH : LOW);
    digitalWrite(4, sendSteer > outSteer.center_ ? HIGH : LOW);
  }

  do {
    iBus.update();
  } while (millis() < now + 5);
}

uint32_t lastTrimTime;
uint32_t trimmedTime;

void maybeTrim(uint32_t now) {

  if (now - lastTrimTime >= 80) {
  
    lastTrimTime = now;

    //  trim enable switch
    if (fsValues[8] < 1700) {
      return;
    }
    
    /* trim steering */
    if (fsValues[5] > 1700) {
      if (outSteer.center_ > 1300) {
        outSteer.center_ -= 1;
        trimmedTime = now;
      }
    } else if (fsValues[5] < 1300) {
      if (outSteer.center_ < 1700) {
        outSteer.center_ += 1;
        trimmedTime = now;
      }
    }

    /* trim throttle */
    if (fsValues[4] > 1700) {
      if (outThrottle.center_ > 1300) {
        outThrottle.center_ -= 1;
        trimmedTime = now;
      }
    } else if (fsValues[4] < 1500) {
      if (outThrottle.center_ < 1700) {
        outThrottle.center_ += 1;
        trimmedTime = now;
      }
    }

    if ((trimmedTime != 0) && (now - trimmedTime > 10000)) {
      //  write to EEPROM 10 seconds after a trim has been made and no further changes
      writeTrim();
      trimmedTime = 0;
    }
  }
}

void readTrim() {
  uint16_t center = EEPROM.read(2) + ((uint16_t)EEPROM.read(3) << 8);
  if (center >= 1300 && center <= 1700) {
    outSteer.center_ = center;
  }
  center = EEPROM.read(4) + ((uint16_t)EEPROM.read(5) << 8);
  if (center >= 1300 && center <= 1700) {
    outThrottle.center_ = center;
  }
}

void writeTrim() {
  EEPROM.update(2, outSteer.center_ & 0xff);
  EEPROM.update(3, (outSteer.center_ & 0xff00) >> 8);
  EEPROM.update(4, outThrottle.center_ & 0xff);
  EEPROM.update(5, (outThrottle.center_ & 0xff00) >> 8);
}

