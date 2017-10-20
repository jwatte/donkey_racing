#include "Calibrate.h"
#include "PinPulseIn.h"
#include "FlySkyIBus.h"
#include "Support.h"
#include "Packets.h"
#include "SerialControl.h"

#include <Servo.h>
#include <EEPROM.h>


#define CENTER_CALIBRATION 1540

PinPulseIn<14> rcSteer;

Servo carSteer;
Servo carThrottle;

SerialControl gSerialControl(/*SerialUSB,*/ true);

#define fsValues iBusPacket.data
IBusPacket iBusPacket;
FlySkyIBus iBus(Serial1, fsValues, sizeof(fsValues)/sizeof(fsValues[0]));

Calibrate inSteer;
Calibrate inThrottle;
Calibrate hostSteerCal;
Calibrate hostThrottleCal;
Calibrate outSteer(CENTER_CALIBRATION);
Calibrate outThrottle;

SteerControl steerControl;
int16_t sendSteer = 0;
int16_t sendThrottle = 0;

bool hasRC = false;
uint32_t lastModeTime = 0;

HostControl hostControl;
uint32_t lastHostControl;

void readTrim();
void writeTrim();
void maybeTrim(uint32_t now);


void detachServos() {
  if (carSteer.attached()) {
    carSteer.detach();
  }
  if (carThrottle.attached()) {
    carThrottle.detach();
  }
}

void attachServos() {
  if (!carSteer.attached()) {
    carSteer.attach(23);
  }
  if (!carThrottle.attached()) {
    carThrottle.attach(22);
  }
}

void setup() {
  rcSteer.init();
  iBus.begin();
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(13, OUTPUT);
  onCrash(detachServos);
  readTrim();
  gSerialControl.bind(IBusPacket::PacketCode, &iBusPacket, sizeof(iBusPacket), true);
  gSerialControl.bind(SteerControl::PacketCode, &steerControl, sizeof(steerControl), false);
  gSerialControl.bind(HostControl::PacketCode, &hostControl, sizeof(hostControl), false);
  SerialUSB.begin(1000000);
  gSerialControl.begin();
}


uint32_t lastDetach = 0;
float fSteer = 0;
float fThrottle = 0;

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

  if (gSerialControl.getFresh(&hostControl)) {
    lastHostControl = now;
  }
  bool steerChanged = false;
  if (!hasRC || fsValues[9] < 1750) {
    /* turn off the servos */
    detachServos();
    digitalWrite(13, (now & (64+128+256)) == 64);
    digitalWrite(3, (now & (64+128+256)) == 128);
    digitalWrite(4, (now & (64+128+256)) == 192);
    steerControl.steer = (int16_t)0x8000;
    steerControl.throttle = (int16_t)0x8000;
    if (now - lastDetach > 100) {
        steerChanged = true;
        lastDetach = now;
    }
  } else {
    /* send the current heading/speed */
    attachServos();
    if (iBus.hasFreshFrame()) {
      fSteer = inSteer.mapIn(fsValues[0]);
      steerControl.steer = intFloat(fSteer);
      sendSteer = outSteer.mapOut(fSteer);
      fThrottle = inThrottle.mapIn(fsValues[1]);
      steerControl.throttle = intFloat(fThrottle);
      sendThrottle = outThrottle.mapOut(fThrottle);
      steerChanged = true;
    }
    /* switch 7 is middle-up for recording, all-up for autonomous */
    if (fsValues[7] >= 1700) {
      if (now - lastHostControl < 500) {
        float hSteer = hostSteerCal.mapIn(hostControl.steer);
        sendSteer = outSteer.mapOut(hSteer);
        float hThrottle = hostThrottleCal.mapIn(hostControl.throttle);
        sendThrottle = outThrottle.mapOut(hThrottle * fThrottle);
      } else {
        sendSteer = outSteer.mapOut(0.0f);
        sendThrottle = outThrottle.mapOut(0.0f);
      }
    }
    carSteer.writeMicroseconds(sendSteer);
    carThrottle.writeMicroseconds(sendThrottle);
    digitalWrite(13, sendThrottle > outThrottle.center_ ? HIGH : LOW);
    digitalWrite(3, sendSteer < outSteer.center_ ? HIGH : LOW);
    digitalWrite(4, sendSteer > outSteer.center_ ? HIGH : LOW);
  }
  if (steerChanged) {
    if (!gSerialControl.sendNow(&steerControl)) {
      //  communications problem
      digitalWrite(13, HIGH);
      digitalWrite(3, HIGH);
      digitalWrite(4, HIGH);
    }
  }

  uint32_t next = now;
  do {
    iBus.update();
    gSerialControl.step(next);
  } while ((next = millis()) < (now + 5));
}


static uint32_t lastTrimTime;
static uint32_t trimmedTime;
static TrimInfo trimInfo;


void maybeTrim(uint32_t now) {

  if ((trimmedTime != 0) && (now - trimmedTime > 10000)) {
    //  write to EEPROM 10 seconds after a trim has been made and no further changes
    writeTrim();
    trimmedTime = 0;
  }

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
    } else if (fsValues[4] < 1300) {
      if (outThrottle.center_ < 1700) {
        outThrottle.center_ += 1;
        trimmedTime = now;
      }
    }

    trimInfo.curTrimSteer = outSteer.center_;
    trimInfo.curTrimThrottle = outThrottle.center_;
  }
}


void readTrim() {
  uint16_t center = EEPROM.read(2) + ((uint16_t)EEPROM.read(3) << 8);
  if (center >= 1300 && center <= 1700) {
    outSteer.center_ = center;
  }
  trimInfo.eeTrimSteer = center;
  center = EEPROM.read(4) + ((uint16_t)EEPROM.read(5) << 8);
  if (center >= 1300 && center <= 1700) {
    outThrottle.center_ = center;
  }
  trimInfo.eeTrimThrottle = center;
  trimInfo.curTrimSteer = outSteer.center_;
  trimInfo.curTrimThrottle = outThrottle.center_;
  gSerialControl.bind(TrimInfo::PacketCode, &trimInfo, sizeof(trimInfo), true);
}


void writeTrim() {
  EEPROM.update(2, outSteer.center_ & 0xff);
  EEPROM.update(3, (outSteer.center_ >> 8) & 0xff);
  EEPROM.update(4, outThrottle.center_ & 0xff);
  EEPROM.update(5, (outThrottle.center_ >> 8) & 0xff);
  trimInfo.eeTrimSteer = outSteer.center_;
  trimInfo.eeTrimThrottle = outThrottle.center_;
}

