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
  Wire.setClock(400000);

  oled.begin();
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

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

  // =============================
  // STARTUP SCREEN
  // =============================

  oled.clearBuffer();

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

  delay(1000);
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