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
  int sl = (int)strlen(msg);
  int n = 0;
  while (true) {
    if ((n & 63) == 63) {
      if (SerialUSB.availableForWrite() >= sl+2) {
        SerialUSB.println(msg);
      }
    }
    digitalWrite(13, n & 1);
    ++n;
    delay(25);
    if (n > 500) {
      /* after 12.5 seconds, reboot */
      software_reset();
    }
  }
}

static DebugVal *gDebugVals;

void debugVal(DebugVal *dv) {
  DebugVal **dp;
  for (dp = &gDebugVals; *dp; dp = &(*dp)->_ptr) {
    if (*dp == dv) {
      //  updated!
      return;
    }
  }
  dv->_ptr = NULL;
  *dp = dv;
}

void debugVal(DebugVal *dv, char const *name, int val) {
  snprintf(dv->value, 16, "%d", val);
}

void debugVal(DebugVal *dv, char const *name, float val) {
  snprintf(dv->value, 16, "%.6e", val);
}

DebugVal const *firstDebugVal() {
  return gDebugVals;
}

DebugVal const *nextDebugVal(DebugVal const *prev) {
  return prev ? prev->_ptr : NULL;
}

DebugVal const *findDebugVal(char const *name) {
  for (DebugVal *dv = gDebugVals; dv; dv = dv->_ptr) {
    if (!strcmp(name, dv->name)) {
      return dv;
    }
  }
  return NULL;
}

void software_reset() {
  //  Flash the LED for a second, to show what's going on.
  //  Also gives user time to reset if it goes into a crash loop.
  pinMode(13, OUTPUT);
  for (int i = 0; i != 10; ++i) {
    digitalWrite(13, HIGH);
    delay(50);
    digitalWrite(13, LOW);
    delay(50);
  }
  //  reset using the control register
  ((*(volatile uint32_t *)0xE000ED0C) = (0x5FA0004));
}

