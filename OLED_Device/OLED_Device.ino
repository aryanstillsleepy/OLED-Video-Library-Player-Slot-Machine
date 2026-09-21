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