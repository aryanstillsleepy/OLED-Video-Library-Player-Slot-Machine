// Thin stand-in for U8g2's Arduino C++ wrapper, backed by the real U8g2 C
// library. Only the transport is replaced: bytes go to the SH1106 model.
#pragma once
#include "Arduino.h"
#define U8X8_PIN_NONE 255

class U8G2 {
 protected:
  u8g2_t u8g2;
  int16_t tx = 0, ty = 0;
 public:
  u8x8_t *getU8x8() { return u8g2_GetU8x8(&u8g2); }
  void setBusClock(uint32_t c) { getU8x8()->bus_clock = c; }
  bool begin() {
    // Test-only overrides for measuring each optimisation on its own
    if (getenv("SIM_STOCK_CAD")) getU8x8()->cad_cb = u8x8_cad_ssd13xx_fast_i2c;
    if (const char *c = getenv("SIM_CLOCK")) getU8x8()->bus_clock = strtoul(c, nullptr, 10);
    // Same sequence as U8G2::begin()
    u8g2_InitDisplay(&u8g2);
    u8g2_ClearDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    return true;
  }
  void clearBuffer() { u8g2_ClearBuffer(&u8g2); }
  void sendBuffer() { u8g2_SendBuffer(&u8g2); }
  void setFont(const uint8_t *f) { u8g2_SetFont(&u8g2, f); }
  void drawStr(int x, int y, const char *s) { u8g2_DrawStr(&u8g2, x, y, s); }
  void drawXBM(int x, int y, int w, int h, const uint8_t *b) { u8g2_DrawXBM(&u8g2, x, y, w, h, b); }
  void drawXBMP(int x, int y, int w, int h, const uint8_t *b) { u8g2_DrawXBMP(&u8g2, x, y, w, h, b); }
  void drawBox(int x, int y, int w, int h) { u8g2_DrawBox(&u8g2, x, y, w, h); }
  void setCursor(int x, int y) { tx = x; ty = y; }
  void print(int v) {
    char b[16];
    snprintf(b, sizeof b, "%d", v);
    for (char *p = b; *p; p++) tx += u8g2_DrawGlyph(&u8g2, tx, ty, (uint8_t)*p);
  }
  uint8_t *getBufferPtr() { return u8g2_GetBufferPtr(&u8g2); }
  void updateDisplayArea(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { u8g2_UpdateDisplayArea(&u8g2, x, y, w, h); }
};

class U8G2_SH1106_128X64_NONAME_F_HW_I2C : public U8G2 {
 public:
  U8G2_SH1106_128X64_NONAME_F_HW_I2C(const u8g2_cb_t *rotation, uint8_t = U8X8_PIN_NONE) {
    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, rotation, sim_byte_cb, sim_gpio_cb);
  }
};
