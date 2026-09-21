#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <FFat.h>

#include "Config.h"

// ===============================
// OLED
// ===============================

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,
  U8X8_PIN_NONE
);

// ===============================
// FAST I2C TRANSPORT
// ===============================
//
// Replacement for U8g2's u8x8_cad_ssd13xx_fast_i2c.
//
// U8g2 splits display data into 24-byte I2C writes so
// it fits the 32-byte AVR Wire buffer, and sends each
// command in its own write. The ESP32 Wire buffer is
// 128 bytes, so this version keeps consecutive
// commands in one write and sends a 128-byte page as
// two 64-byte writes. A full frame drops from 64 I2C
// transactions to 24, and each transaction has a fixed
// cost on top of the bytes on the wire.
//
// Batching commands is safe because the SH1106
// command sequences in U8g2 contain no delays.

#ifdef I2C_BUFFER_LENGTH
const uint8_t OLED_I2C_MAX_PAYLOAD =
  I2C_BUFFER_LENGTH > 256 ? 255 : I2C_BUFFER_LENGTH - 1;
#else
const uint8_t OLED_I2C_MAX_PAYLOAD = 31;
#endif

// Command bytes in the open I2C write, 0 = none open
uint8_t oledCommandBytes = 0;

void oledEndCommands(u8x8_t *u8x8) {

  if (oledCommandBytes > 0) {
    u8x8_byte_EndTransfer(u8x8);
    oledCommandBytes = 0;
  }
}

uint8_t oledFastI2cCad(
  u8x8_t *u8x8,
  uint8_t msg,
  uint8_t arg_int,
  void *arg_ptr
) {

  switch (msg) {

    case U8X8_MSG_CAD_SEND_CMD:
    case U8X8_MSG_CAD_SEND_ARG:

      if (oledCommandBytes >= OLED_I2C_MAX_PAYLOAD) {
        oledEndCommands(u8x8);
      }

      if (oledCommandBytes == 0) {
        u8x8_byte_StartTransfer(u8x8);
        u8x8_byte_SendByte(u8x8, 0x00);  // commands follow
      }

      u8x8_byte_SendByte(u8x8, arg_int);
      oledCommandBytes++;
      break;

    case U8X8_MSG_CAD_SEND_DATA: {

      oledEndCommands(u8x8);

      uint8_t *data = (uint8_t *)arg_ptr;

      uint8_t writes =
        (arg_int + OLED_I2C_MAX_PAYLOAD - 1) /
        OLED_I2C_MAX_PAYLOAD;

      while (arg_int > 0) {

        // Split evenly: 128 bytes -> 64 + 64, not 127 + 1
        uint8_t count = (arg_int + writes - 1) / writes;

        u8x8_byte_StartTransfer(u8x8);
        u8x8_byte_SendByte(u8x8, 0x40);  // display data follows
        u8x8_byte_SendBytes(u8x8, count, data);
        u8x8_byte_EndTransfer(u8x8);

        data += count;
        arg_int -= count;
        writes--;
      }
      break;
    }

    case U8X8_MSG_CAD_INIT:

      if (u8x8->i2c_address == 255) {
        u8x8->i2c_address = 0x78;
      }

      return u8x8->byte_cb(u8x8, msg, arg_int, arg_ptr);

    case U8X8_MSG_CAD_START_TRANSFER:

      oledCommandBytes = 0;
      break;

    case U8X8_MSG_CAD_END_TRANSFER:

      oledEndCommands(u8x8);
      break;

    default:
      return 0;
  }

  return 1;
}

// ===============================
// MAIN STATES
// ===============================

enum AppState {
  STATE_HOME,
  STATE_VIDEOS,
  STATE_PLAYER,
  STATE_MUSIC,
  STATE_MESSAGES,
  STATE_SETTINGS,
  STATE_SLOT
};

AppState currentState = STATE_HOME;

// ===============================
// SETUP
// ===============================

void setup() {

  Serial.begin(115200);
  delay(500);

  // =============================
  // OLED
  // =============================

  Wire.begin(OLED_SDA, OLED_SCL);

  // U8g2 calls Wire.setClock() before every transfer,
  // so the bus speed must be set through U8g2.
  oled.setBusClock(OLED_I2C_CLOCK_HZ);

  oled.getU8x8()->cad_cb = oledFastI2cCad;

  oled.begin();

  // =============================
  // STARTUP SCREEN
  // =============================
  //
  // Shown before the rest of the init so the screen is
  // not blank while FFat mounts (or formats on first boot,
  // which can take several seconds).

  unsigned long splashStart = millis();

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

  oled.drawStr(
    10,
    30,
    "OLED DEVICE"
  );

  oled.drawStr(
    10,
    45,
    "Starting..."
  );

  oled.sendBuffer();

  // =============================
  // BUTTON
  // =============================

  setupButton();

  // =============================
  // FFAT
  // =============================

  if (!FFat.begin(true)) {
    Serial.println("FFat mount failed!");
  } else {
    Serial.println("FFat mounted.");
  }

  // =============================
  // SLOT SETUP
  // =============================

  setupSlot();

  // Keep the splash up for at least SPLASH_MS in total
  unsigned long splashElapsed = millis() - splashStart;

  if (splashElapsed < SPLASH_MS) {
    delay(SPLASH_MS - splashElapsed);
  }

  drawHome();
}

// ===============================
// LOOP
// ===============================

void loop() {

  updateButton();

  switch (currentState) {

    case STATE_HOME:
      updateHome();
      break;

    case STATE_VIDEOS:
      updateVideos();
      break;

    case STATE_PLAYER:
      updatePlayer();
      break;

    case STATE_MUSIC:
      updateMusic();
      break;

    case STATE_MESSAGES:
      updateMessages();
      break;

    case STATE_SETTINGS:
      updateSettings();
      break;

    case STATE_SLOT:
      updateSlot();
      break;
  }
}