#pragma once
#include "Arduino.h"
#define I2C_BUFFER_LENGTH 128  // same default as the ESP32 core
struct TwoWire {
  void begin(int, int) {}
  void setClock(uint32_t) {}
};
static TwoWire Wire;
