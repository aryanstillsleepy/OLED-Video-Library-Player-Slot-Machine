#ifndef CONFIG_H
#define CONFIG_H

// ===============================
// OLED
// ===============================

#define OLED_SDA 8
#define OLED_SCL 9

// I2C clock for the display. The SH1106 datasheet only
// specifies 400 kHz, but most modules run fine at 800 kHz,
// which roughly halves the time to send a frame.
// Set this back to 400000 if the display glitches.
#define OLED_I2C_CLOCK_HZ 800000

// Minimum time the startup screen stays visible
#define SPLASH_MS 1000

// ===============================
// BUTTON
// ===============================

#define BUTTON_PIN 4

// ===============================
// BUTTON TIMING
// ===============================

#define DEBOUNCE_MS 35
#define CLICK_MAX_MS 350
#define DOUBLE_CLICK_GAP_MS 300
#define LONG_PRESS_MS 500

// ===============================
// VIDEO
// ===============================

#define MAX_VIDEOS 50
#define VISIBLE_VIDEOS 5

#define FRAME_WIDTH 128
#define FRAME_HEIGHT 64
#define FRAME_SIZE 1024

#define VIDEO_HEADER_SIZE 20

#endif