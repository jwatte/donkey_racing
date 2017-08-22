#include <Arduino.h>
#include <HardwareSerial.h>
#include <stdint.h>
#include <string.h>

#include "Support.h"

static CRASH_CALLBACK gCallback;

CRASH_CALLBACK onCrash(CRASH_CALLBACK func) {
  CRASH_CALLBACK ret = gCallback;
  gCallback = func;
  return ret;
}

void CRASH_ERROR(char const *msg) {
  if (gCallback) {
    gCallback();
  }
  size_t sl = strlen(msg);
  int n = 0;
  while (true) {
    if ((n & 127) == 127) {
      if (SerialUSB.availableForWrite() >= sl+2) {
        SerialUSB.println(msg);
      }
    }
    digitalWrite(13, n & 1);
    ++n;
    delay(50);
  }
}

