//
///////////////////////////// SETUP ROUTINES //////////////////////////////
enum SetupState {
  initialMenu,      // initial menu
  servoSelect,
  servoSetLeft,
  servoSetRight,
  servoSetSpeed,

  controlSelect,
  
  commandSelect,

  commandServo,
  servoDirection,

  commandConfirm
};

extern void (* charModeCallback)(char);

SetupState setupState = initialMenu;
void (*adjustCallback)(int) = NULL;

const int keyCode_plus = 3;
const int keyCode_minus = 7;
const char keychar_plus = '+';
const char keychar_minus = '-';
const char keychar_ok = '*';
const char keychar_cancel = '#';

const char keychar_resetSetup = '.';
const char char_setupServo = '1';
const char char_setupAction = '2';


void resetState() {
  executor.clear();
  adjustCallback = NULL;
//  NumberInput::set(NULL);
  processKeyInput = false;
  overrideSpeed = -1;
}

/**
 * Enters the setup routine.
 */
void enterSetup() {
  setupState = initialMenu;

  // clear servo config
  setupServoLeft = 90;
  setupServoRight = 90;
  setupServoSpeed = 2;
  setupServoOutput = -1;
  setupServoIndex = -1;
  
  if (charModeCallback != NULL) {
    // setup was invoked from terminal; reset terminal and disable setup
    setupActive = false;
    Serial.println(F("Exit setup"));
    resetTerminal();
    return;
  }
#ifdef KEYPAD_INPUT
  setupActive = 1;
  makeLedAck(&blinkSetup[0]);
  if (debugControl) {
    Serial.print(F("Entering setup"));
  }
  resetState();
#endif
}

#ifdef KEYPAD_INPUT
void handleInitialMenu() {
  if (debugControl) {
    Serial.print(F("Got key: ")); Serial.println(currentKey);
  }
  switch (currentKey) {
    case char_setupServo:
//      enterSetupServo();
      break;

    default:
      // invalid choice, signal error.
      makeLedAck(&blinkError[0]);
  }
}

void handleSetupIdle() {
  if (numberInput != NULL) {
    return;
  }
  if (processKeyInput) {
  switch (setupState) {
    case servoSelect: 
    case servoSetLeft: 
    case servoSetRight: 
      break;
  }
}

long lastHeldReaction;
const int minHoldDelay = 100;

void handleHoldAdjustment(int keycode, int step, void (*exec)(int)) {
  if (currentMillis < lastHeldReaction + minHoldDelay) {
    return;
  }
  int idx = keypad.findInList(keycode);
  if (idx == -1 || keypad.key[idx].kstate != HOLD) {
    return;
  }
  Serial.println(F("Idle number rinput"));
  if (exec == NULL) {
    Serial.println(F("No callback"));
    return;
  }
  lastHeldReaction = currentMillis;
  exec(step);
}

void handleSetupHeldKeys() {
  switch (setupState) {
    case servoSetLeft:
    case servoSetRight: {
      boolean left = setupState == servoSetLeft;
      handleHoldAdjustment(keyCode_plus, 5, adjustCallback);
      handleHoldAdjustment(keyCode_minus, -5, adjustCallback);
      break;
    }
  }
}
#endif

void resetSetupMode() {
  setupActive = false;
  setupState = initialMenu;
}

void setupLoop() {
  if (pressedKey == 0) {
#ifdef KEYPAD_INPUT
    handleSetupHeldKeys();    
    handleSetupIdle();
#endif
    return;
  }
  currentKey = keyTranslation[pressedKey];
  setupHandleKey();
}

extern void handleKeyInput();

void setupHandleKey() {
  if (currentKey == keychar_resetSetup) {
    enterSetup();
    return;
  }  
//  if (numberInput != NULL) {
//    numberInput->handleKeyPressed();
  if (processKeyInput) {
    handleKeyInput();
    return;
  }
}


