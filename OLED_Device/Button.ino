// =====================================================
// BUTTON SYSTEM
// =====================================================

// Button states
bool buttonLastState = HIGH;
bool buttonStableState = HIGH;

unsigned long buttonLastChangeTime = 0;
unsigned long buttonPressTime = 0;
unsigned long lastClickTime = 0;

// Current press
bool buttonIsPressed = false;

// Click tracking
int clickCount = 0;
bool waitingForClicks = false;

// Prevent the release after a long press from
// being interpreted as another click
bool longPressHandled = false;

// Click + hold tracking
bool clickHoldActive = false;
bool clickHoldTriggered = false;


// =====================================================
// BUTTON SETUP
// =====================================================

void setupButton() {

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  buttonLastState = HIGH;
  buttonStableState = HIGH;

  buttonLastChangeTime = millis();

  buttonPressTime = 0;
  lastClickTime = 0;

  buttonIsPressed = false;

  clickCount = 0;
  waitingForClicks = false;

  longPressHandled = false;

  clickHoldActive = false;
  clickHoldTriggered = false;
}


// =====================================================
// BUTTON UPDATE
// =====================================================

void updateButton() {

  bool reading = digitalRead(BUTTON_PIN);


  // =================================================
  // DEBOUNCE
  // =================================================

  if (reading != buttonLastState) {

    buttonLastChangeTime = millis();
    buttonLastState = reading;
  }

  if ((millis() - buttonLastChangeTime) < DEBOUNCE_MS) {
    return;
  }


  // =================================================
  // STABLE STATE CHANGED
  // =================================================

  if (reading != buttonStableState) {

    buttonStableState = reading;


    // =================================================
    // BUTTON PRESSED
    // =================================================

    if (buttonStableState == LOW) {

      buttonIsPressed = true;

      buttonPressTime = millis();

      longPressHandled = false;

      clickHoldActive = false;
      clickHoldTriggered = false;


      // ---------------------------------------------
      // CLICK + HOLD DETECTION
      // ---------------------------------------------

      if (clickCount == 1 &&
          waitingForClicks &&
          (millis() - lastClickTime <= DOUBLE_CLICK_GAP_MS)) {

        clickHoldActive = true;
      }
    }


    // =================================================
    // BUTTON RELEASED
    // =================================================

    else {

      if (!buttonIsPressed) {
        return;
      }

      buttonIsPressed = false;


      // ---------------------------------------------
      // IGNORE RELEASE AFTER LONG PRESS
      // ---------------------------------------------

      if (longPressHandled) {

        longPressHandled = false;

        waitingForClicks = false;
        clickCount = 0;

        clickHoldActive = false;
        clickHoldTriggered = false;

        return;
      }


      // ---------------------------------------------
      // IGNORE RELEASE AFTER CLICK + HOLD
      // ---------------------------------------------

      if (clickHoldTriggered) {

        clickHoldActive = false;
        clickHoldTriggered = false;

        waitingForClicks = false;
        clickCount = 0;

        return;
      }


      // ---------------------------------------------
      // MEASURE PRESS
      // ---------------------------------------------

      unsigned long pressDuration =
        millis() - buttonPressTime;


      // ---------------------------------------------
      // SHORT CLICK
      // ---------------------------------------------

      if (pressDuration < CLICK_MAX_MS) {

        clickCount++;

        lastClickTime = millis();

        waitingForClicks = true;
      }
    }
  }


  // =================================================
  // LONG PRESS / CLICK + HOLD
  // =================================================

  if (buttonIsPressed && !longPressHandled) {

    unsigned long pressDuration =
      millis() - buttonPressTime;


    if (pressDuration >= LONG_PRESS_MS) {

      // ---------------------------------------------
      // CLICK + HOLD
      // ---------------------------------------------

      if (clickHoldActive) {

        clickHoldTriggered = true;

        // IMPORTANT:
        // Prevent this same press from also becoming
        // a normal long press.
        longPressHandled = true;

        waitingForClicks = false;
        clickCount = 0;

        handleClickHold();
      }


      // ---------------------------------------------
      // NORMAL LONG PRESS
      // ---------------------------------------------

      else {

        longPressHandled = true;

        waitingForClicks = false;
        clickCount = 0;

        handleLongPress();
      }
    }
  }


  // =================================================
  // PROCESS COMPLETED CLICKS
  // =================================================

  processClicks();
}


// =====================================================
// CLICK PROCESSING
// =====================================================

void processClicks() {

  if (!waitingForClicks) {
    return;
  }


  // The button is down again: this press may become
  // another click or a click + hold, so the sequence
  // is not finished yet. Only the release restarts
  // the double-click gap timer.
  if (buttonIsPressed) {
    return;
  }


  // Still waiting for another click
  if ((millis() - lastClickTime) <= DOUBLE_CLICK_GAP_MS) {
    return;
  }


  waitingForClicks = false;


  // =================================================
  // SINGLE CLICK
  // =================================================

  if (clickCount == 1) {

    handleSingleClick();
  }


  // =================================================
  // DOUBLE CLICK
  // =================================================

  else if (clickCount == 2) {

    handleDoubleClick();
  }


  // =================================================
  // TRIPLE CLICK
  // =================================================

  else if (clickCount >= 3) {

    handleTripleClick();
  }


  clickCount = 0;
}


// =====================================================
// SINGLE CLICK
// =====================================================

void handleSingleClick() {

  switch (currentState) {

    case STATE_HOME:
      handleHomeClick();
      break;

    case STATE_VIDEOS:
      handleVideoLibraryClick();
      break;

    case STATE_PLAYER:
      handlePlayerClick();
      break;

    case STATE_MUSIC:
      handleMusicClick();
      break;

    case STATE_MESSAGES:
      handleMessagesClick();
      break;

    case STATE_SETTINGS:
      handleSettingsClick();
      break;

    case STATE_SLOT:
      handleSlotSingleClick();
      break;
  }
}


// =====================================================
// DOUBLE CLICK
// =====================================================

void handleDoubleClick() {

  switch (currentState) {

    case STATE_HOME:
      openHomeSelection();
      break;

    case STATE_VIDEOS:
      openSelectedVideo();
      break;

    case STATE_PLAYER:
      restartVideo();
      break;

    case STATE_MUSIC:
      handleMusicDoubleClick();
      break;

    case STATE_MESSAGES:
      handleMessagesDoubleClick();
      break;

    case STATE_SETTINGS:
      handleSettingsDoubleClick();
      break;

    case STATE_SLOT:
      handleSlotDoubleClick();
      break;
  }
}


// =====================================================
// TRIPLE CLICK
// =====================================================

void handleTripleClick() {

  switch (currentState) {

    case STATE_HOME:
      handleHomeTripleClick();
      break;

    case STATE_VIDEOS:
      handleVideoLibraryTripleClick();
      break;

    case STATE_PLAYER:
      handlePlayerTripleClick();
      break;

    case STATE_MUSIC:
      handleMusicTripleClick();
      break;

    case STATE_MESSAGES:
      handleMessagesTripleClick();
      break;

    case STATE_SETTINGS:
      handleSettingsTripleClick();
      break;

    case STATE_SLOT:
      handleSlotTripleClick();
      break;
  }
}


// =====================================================
// LONG PRESS
// =====================================================

void handleLongPress() {

  switch (currentState) {

    case STATE_HOME:

      // Nothing for now
      break;


    case STATE_VIDEOS:

      currentState = STATE_HOME;

      drawHome();

      break;


    case STATE_PLAYER:

      closeVideo();

      currentState = STATE_VIDEOS;

      drawLibrary();

      break;


    case STATE_MUSIC:

      currentState = STATE_HOME;

      drawHome();

      break;


    case STATE_MESSAGES:

      currentState = STATE_HOME;

      drawHome();

      break;


    case STATE_SETTINGS:

      currentState = STATE_HOME;

      drawHome();

      break;


    case STATE_SLOT:

      handleSlotLongPress();

      break;
  }
}


// =====================================================
// CLICK + HOLD
// =====================================================

void handleClickHold() {

  switch (currentState) {

    case STATE_HOME:

      // Reserved for future Home action
      break;


    case STATE_VIDEOS:

      // Reserved for future Video Library action
      break;


    case STATE_PLAYER:

      // Reserved for future Player action
      break;


    case STATE_MUSIC:

      // Reserved for future Music action
      break;


    case STATE_MESSAGES:

      // Reserved for future Messages action
      break;


    case STATE_SETTINGS:

      // Reserved for future Settings action
      break;


    case STATE_SLOT:

      handleSlotClickHold();

      break;
  }
}


// =====================================================
// PLACEHOLDER TRIPLE-CLICK ACTIONS
// =====================================================

void handleHomeTripleClick() {
  // Reserved
}


void handleMusicTripleClick() {
  // Reserved
}


void handleMessagesTripleClick() {
  // Reserved
}


void handleSettingsTripleClick() {
  // Reserved
}