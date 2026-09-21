#ifndef LEMON_SPRITE_H
#define LEMON_SPRITE_H

#include <Arduino.h>

// =====================================================
// LEMON SYMBOL
// =====================================================
//
// Size:        16 × 16 pixels
// Format:      1-bit monochrome
// Bit order:   Horizontal LSB (XBM-style)
// OLED:        SH1106 128×64
//
// Already compatible with U8g2 drawXBM().
// NO bit reversal required.
// NO inversion required.
//
// Appearance:
//   White symbol on black reel background
// =====================================================

#define LEMON_WIDTH  16
#define LEMON_HEIGHT 16

static const uint8_t PROGMEM lemon_sprite_bitmap[32] = {

  // Row  0
  0x00, 0x00,

  // Row  1
  0x00, 0x00,

  // Row  2
  0x00, 0x00,

  // Row  3
  0xC0, 0x03,

  // Row  4
  0xF0, 0x0F,

  // Row  5
  0xF8, 0x0F,

  // Row  6
  0xF8, 0x0F,

  // Row  7
  0xFC, 0x08,

  // Row  8
  0x3C, 0x08,

  // Row  9
  0x1C, 0x04,

  // Row 10
  0x1C, 0x04,

  // Row 11
  0x1C, 0x03,

  // Row 12
  0xF0, 0x00,

  // Row 13
  0x00, 0x00,

  // Row 14
  0x00, 0x00,

  // Row 15
  0x00, 0x00
};

#endif // LEMON_SPRITE_H