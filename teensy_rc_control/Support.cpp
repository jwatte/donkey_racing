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

static DebugVal *gDebugVals;

void debugVal(DebugVal *dv) {
  for (DebugVal **dp = &gDebugVals; *dp; dp = &(*dp)->_ptr) {
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

