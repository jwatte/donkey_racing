#include <Arduino.h>
#include "Power.h"

#define LED_PIN 13
#define POWER_CTL_PIN 2
#define POWER_SENSE_PIN A2

static float lastVoltage = 0.0f;
static uint32_t lastHigh = 0;
static bool forceOn = false;
static bool forceOff = false;

float power_voltage() {
  return lastVoltage;
}

void setup_power() {
  analogReadRes(12);
  pinMode(POWER_SENSE_PIN, INPUT);
  pinMode(POWER_CTL_PIN, OUTPUT);
  digitalWrite(POWER_CTL_PIN, HIGH);
}

void update_power(uint32_t now) {
  uint16_t v = analogRead(POWER_SENSE_PIN);
  lastVoltage = (v * 3.3 / 4096 * 704.0 / 100.0);
  if (lastVoltage > 3 * 3.3f) {
    lastHigh = now;
  }
  if (forceOn) {
    digitalWrite(POWER_CTL_PIN, HIGH);
  } else if (now - lastHigh > 2000) {
    force_power_off();
  } else if (forceOff) {
    digitalWrite(POWER_CTL_PIN, LOW);
  }
}

void force_power_on() {
  forceOn = true;
  forceOff = false;
  digitalWrite(POWER_CTL_PIN, HIGH);
}

void force_power_off() {
  forceOn = false;
  forceOff = true;
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(POWER_CTL_PIN, LOW);
}

void set_power_auto(uint32_t now) {
  forceOn = false;
  forceOff = false;
  // keep it high while calculating
  lastHigh = now;
  digitalWrite(POWER_CTL_PIN, HIGH);
}

