#include <Arduino.h>
#include "Console.h"
#include "Power.h"

#define USE_USB 0
#if USE_USB
#define IFUSB(a,b) a
#else
#define IFUSB(a,b) b
#endif

static char inputline[32];
static uint8_t inputptr = 0;
static bool isusb = false;

static void println(char const *x = nullptr) {
  if (isusb) {
    IFUSB(SerialUSB.println(x ? x : ""),);
  } else {
    Serial1.println(x ? x : "");
  }
}

static void println(float f) {
  char msg[25];
  sprintf(msg, "%g", f);
  println(msg);
}

static void print(char const *x) {
  if (isusb) {
    IFUSB(SerialUSB.print(x),);
  } else {
    Serial1.print(x);
  }
}

static void write(char const *ptr, int size) {
  if (isusb) {
    IFUSB(SerialUSB.write(ptr, size),);
  } else {
    Serial1.write(ptr, size);
  }
}

void docommand(char const *cmd) {
  if (!strcmp(cmd, "on")) {
    force_power_on();
    goto ok;
  }
  if (!strcmp(cmd, "off")) {
    force_power_off();
    goto ok;
  }
  if (!strcmp(cmd, "volt?")) {
    println(power_voltage());
    goto ok;
  }
  print(cmd);
  println(": unknown command");
  return;
ok:
  println("OK");
}

void setup_console() {
  Serial1.begin(115200);
  SerialUSB.begin(1000000);
}

void update_console(uint32_t) {
  while (Serial1.available() || IFUSB(SerialUSB.available(), false)) {
    char ch;
    if (IFUSB(SerialUSB.available(), false)) {
      ch = SerialUSB.read();
      isusb = true;
    } else {
      ch = Serial1.read();
      isusb = false;
    }
    if (ch == 10) {
    } else if (ch == 13) {
      println();
      inputline[inputptr] = 0;
      inputptr = 0;
      docommand(inputline);
      memset(inputline, 0, sizeof(inputline));
    } else if (ch == 8) {
      if (inputptr > 0) {
        --inputptr;
        char b[3] = { 8, 32, 8 };
        write(b, 3);
      }
    } else if (inputptr < sizeof(inputline)-1) {
      inputline[inputptr] = ch;
      ++inputptr;
      write(&ch, 1);
    } else {
      write("?", 1);
    }
  }
}

