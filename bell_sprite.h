#ifndef BELL_SPRITE_H
#define BELL_SPRITE_H

#include <Arduino.h>

// =====================================================
// BELL SYMBOL
// =====================================================
//
// Size:        16 × 16 pixels
// Format:      1-bit monochrome
// Bit order:   Horizontal LSB (XBM-style)
// OLED:        SH1106 128×64
//
// This bitmap is already compatible with U8g2
// drawXBM(), so NO bit reversal is required.
//
// Appearance:
//   White bell on the black reel background
// =====================================================

#define BELL_WIDTH  16
#define BELL_HEIGHT 16

static const uint8_t PROGMEM bell_sprite_bitmap[32] = {

  // Row  0
  0x00, 0x00,

  // Row  1
  0x00, 0x00,

  // Row  2
  0x80, 0x01,

  // Row  3
  0xE0, 0x07,

  // Row  4
  0x10, 0x08,

  // Row  5
  0x10, 0x08,

  // Row  6
  0x10, 0x08,

  // Row  7
  0x10, 0x08,

  // Row  8
  0xD0, 0x0B,

  // Row  9
  0x08, 0x10,

  // Row 10
  0xC4, 0x23,

  // Row 11
  0x78, 0x1E,

  // Row 12
  0x80, 0x01,

  // Row 13
  0x00, 0x00,

  // Row 14
  0x00, 0x00,

  // Row 15
  0x00, 0x00
};

#endif // BELL_SPRITE_H