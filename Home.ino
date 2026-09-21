// ===============================
// HOME SCREEN
// ===============================

int homeSelection = 0;

const char* homeItems[] = {
  "Videos",
  "Music",
  "Messages",
  "Settings",
  "Slot"
};

const int HOME_ITEM_COUNT = 5;


// ===============================
// DRAW HOME
// ===============================

void drawHome() {

  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);

  oled.drawStr(
    0,
    8,
    "OLED DEVICE"
  );

  for (int i = 0; i < HOME_ITEM_COUNT; i++) {

    // 5 items need tighter spacing
    int y = 18 + (i * 10);

    if (i == homeSelection) {
      oled.drawStr(
        0,
        y,
        ">"
      );
    }

    oled.drawStr(
      10,
      y,
      homeItems[i]
    );
  }

  oled.sendBuffer();
}


// ===============================
// HOME BUTTON ACTIONS
// ===============================

void handleHomeClick() {

  homeSelection++;

  if (
    homeSelection >= HOME_ITEM_COUNT
  ) {
    homeSelection = 0;
  }

  drawHome();
}


// ===============================
// OPEN SELECTED ITEM
// ===============================

void openHomeSelection() {

  switch (homeSelection) {

    case 0:
      // VIDEOS

      scanVideos();

      currentState =
        STATE_VIDEOS;

      drawLibrary();

      break;


    case 1:
      // MUSIC

      currentState =
        STATE_MUSIC;

      break;


    case 2:
      // MESSAGES

      currentState =
        STATE_MESSAGES;

      break;


    case 3:
      // SETTINGS

      currentState =
        STATE_SETTINGS;

      break;


    case 4:
     // SLOT MACHINE

  currentState =
    STATE_SLOT;

  drawMachine();

  break;
  }
}


// ===============================
// HOME UPDATE
// ===============================

void updateHome() {

  static bool firstRun = true;

  if (firstRun) {

    drawHome();

    firstRun = false;
  }

  // Button handling is connected
  // through Button.ino.
}


// ===============================
// MUSIC
// ===============================

void updateMusic() {

  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x10_tf
  );

  oled.drawStr(
    10,
    30,
    "MUSIC"
  );

  oled.drawStr(
    10,
    45,
    "Coming soon"
  );

  oled.sendBuffer();
}


// ===============================
// MESSAGES
// ===============================

void updateMessages() {

  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x10_tf
  );

  oled.drawStr(
    10,
    30,
    "MESSAGES"
  );

  oled.drawStr(
    10,
    45,
    "Coming soon"
  );

  oled.sendBuffer();
}


// ===============================
// SETTINGS
// ===============================

void updateSettings() {

  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x10_tf
  );

  oled.drawStr(
    10,
    30,
    "SETTINGS"
  );

  oled.drawStr(
    10,
    45,
    "Coming soon"
  );

  oled.sendBuffer();
}


// ===============================
// MUSIC BUTTONS
// ===============================

void handleMusicClick() {
}

void handleMusicDoubleClick() {
}


// ===============================
// MESSAGE BUTTONS
// ===============================

void handleMessagesClick() {
}

void handleMessagesDoubleClick() {
}


// ===============================
// SETTINGS BUTTONS
// ===============================

void handleSettingsClick() {
}

void handleSettingsDoubleClick() {
}