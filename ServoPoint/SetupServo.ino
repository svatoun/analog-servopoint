
byte setupServoLeft;
byte setupServoRight;
byte setupServoSpeed;
signed char setupServoOutput;

byte servoInputState = 0;
int minValue = 0;
int maxValue = 1000;
byte *valTarget = NULL;
int adjStep = 1;
int lastSelectedNumber;
int selectedNumber;
int typedNumber = -1;
long lastTypedMillis;
boolean shown;
boolean wasCancel;
byte digitCount;

char setupServoIndex = -1;


#ifdef NUMBERINPUT
void InputServoPos::finished(int value) {
  if (left) {
    Serial.print(F("LEFT: ")); Serial.println(value);
    setupServoLeft = value;
    NumberInput::set(new InputServoPos(false));
    setupState = servoSetRight;
    numberInput->display();
    Serial.print(F("Adj right: ")); Serial.println(numberInput->getValue());
  } else {
    Serial.print(F("RIGHT: ")); Serial.println(value);
    setupServoRight = value;
    setupState = servoSetSpeed;
    NumberInput::set(new InputServoSpeed());
    Serial.print(F("Adj speed: ")); Serial.println(numberInput->getValue() + 1);
  }
}

void InputServoNumber::finished(int value) {
    if (value == 0 || value > MAX_SERVO) {
      enterSetup();
      return;
    }
    startSetupServo(value - 1);
}

void InputServoPos::displayCurrent(int selectedPos) {
  Serial.print(F("Pos: ")); Serial.println(selectedPos);
  clearNewCommand();
  Action a1;
  a1.asServoAction().move(setupServoIndex, selectedPos);
  addNewCommand(a1);
  executor.playNewAction();
}

int InputServoSpeed::getValue() {
  return setupServoSpeed;
}

void InputServoSpeed::finished(int v) {
  Serial.print(F("SPEED: ")); Serial.println(v);
  setupServoSpeed = v - 1;
  overrideSpeed = -1;
  // Persist in EEPROM
  ServoConfig tmp(setupServoLeft, setupServoRight, setupServoSpeed, setupServoOutput);
  tmp.save(setupServoIndex);
  servoEEWriteSetup();
  enterSetup();
}

void InputServoSpeed::cancelled() {
  overrideSpeed = -1;
}

void InputServoSpeed::displayCurrent(int n) {
  Serial.print(F("Speed: ")); Serial.println(n);
  overrideSpeed = n - 1;

  clearNewCommand();

  Action  a;
  ServoActionData& sad = a.asServoAction();
  sad.moveLeft(setupServoIndex);
  addNewCommand(a);

  sad.moveRight(setupServoIndex);
  addNewCommand(a);
  executor.playNewAction();
}

void NumberInput::display() {
  displayCurrent(selectedNumber);
}

void NumberInput::clear() {
  selectedNumber = getValue();
  typedNumber = -1;
  lastTypedMillis = 0;
  shown = false;
  wasCancel = false;
}

void NumberInput::cancelled() {
  overrideSpeed = -1;
  enterSetup();
}

int NumberInput::acceptTypedNumber() {
  if (typedNumber == -1) {
    return 1;
  }
  if (typedNumber < minValue || typedNumber > maxValue) {
    makeLedAck(&blinkReject[0]);
    return 0;
  }
  selectedNumber = setValue(typedNumber);
  makeLedAck(&blinkShort[0]);
  typedNumber = -1;
  return 1;
}


void NumberInput::handleKeyPressed() {
  boolean prevCancel = wasCancel;
  boolean ok = true;
  
  wasCancel = currentKey == keychar_cancel;
  switch (currentKey) {
    case keychar_cancel:
      
      if (typedNumber != -1 || !prevCancel) {
        Serial.print(F("Cancel pressed\n"));
        reset();
        typedNumber = -1;
        selectedNumber = getValue();
        makeLedAck(&blinkLong[0]);
        lastTypedMillis = 0;
      } else {
        if (debugControl) {
          Serial.print(F("Cancel\n"));
        }
        numberInput = NULL;
        cancelled();
        return;
      }
      break;
    case keychar_plus: {
      if (debugControl) {
        Serial.print(F("Inc."));
      }
      if (!acceptTypedNumber()) {
        return;
      }
      int v = incValue(selectedNumber, 1);
      if (v > maxValue || v < minValue) {
        ok = false;
        break;
      }
      v = setValue(v);
      selectedNumber = v;
      lastTypedMillis = currentMillis;
      break;
    }
    case keychar_minus: {
      if (debugControl) {
        Serial.print(F("Dec."));
      }
      if (!acceptTypedNumber()) {
        return;
      }
      int v = incValue(selectedNumber, -1);
      if (v > maxValue || v < minValue) {
        ok = false;
        break;
      }
      v = setValue(v);
      selectedNumber = v;
      lastTypedMillis = currentMillis;
      break;
    }
    case keychar_ok:
      if (typedNumber != -1) {
        if (!acceptTypedNumber()) {
          return;
        }
        break;
      }
      numberInput = NULL;
      finished(selectedNumber);
      return;
    case '0': case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
      int maxDigits;
      if (maxValue < 10) {
        maxDigits = 1;
      } else if (maxValue < 100) {
        maxDigits = 2;
      } else if (maxValue < 1000) {
        maxDigits = 3;
      } else {
        maxDigits = 4;
      }
      if ((typedNumber < 0) && (currentKey == '0')) {
        ok = false;
        break;
      }
      if (typedNumber < 0) {
        typedNumber = 0;
        digitCount = 0;
      } else if (digitCount >= maxDigits) {
        typedNumber = -1;
        ok = false;
        Serial.print(F("Too many digits\n"));
        break;
      }
      typedNumber = typedNumber * 10 + (currentKey - '0');
      if (typedNumber >= maxValue) {
        typedNumber = maxValue;
      }
      digitCount++;
      lastTypedMillis = currentMillis;
      shown = false;
      break;
  }
  if (debugControl) {
    Serial.print(F("Sel: ")); Serial.println(selectedNumber);
    Serial.print(F("Typed: ")); Serial.println(typedNumber);
  }
  if (lastSelectedNumber != selectedNumber) {
    displayCurrent(selectedNumber);
    lastSelectedNumber = selectedNumber;
  }
}

#endif

void startSetupServo(int value) {
    setupServoIndex = value;
    ServoConfig tmp;
    tmp.load(value);
    setupServoLeft = tmp.left();
    setupServoRight = tmp.right();
    setupServoSpeed = tmp.speed();
    setupServoOutput = tmp.output();
//    setupServoConfig = tmp;
    if (debugServo) {      
      Serial.print(F("Configuring ")); Serial.println(setupServoIndex + 1);
    }
    setupState = servoSetLeft;
    processKeyInput = true;
    servoInputState = 0;
    handleInputFinished();
}

void displayCurrent() {
  clearNewCommand();
  Action a1;
  ServoActionData& sd = a1.asServoAction();

  switch (servoInputState) {
    case 0:
    case 1:
    case 2:
      Serial.print(F("Pos: ")); Serial.println(selectedNumber);
      sd.move(setupServoIndex, selectedNumber);
      addNewCommand(a1);
      executor.playNewAction();
      return;
    case 3:
      Serial.print(F("Speed: ")); Serial.println(selectedNumber);
      overrideSpeed = selectedNumber - 1;
      sd.moveLeft(setupServoIndex);
      addNewCommand(a1);
      sd.moveRight(setupServoIndex);
      addNewCommand(a1);
      executor.playNewAction();
      return;
    default:
      return;
  }
}

void handleInputCancelled() {
  enterSetup();
}

void handleInputReset() {
  if (valTarget != NULL) {
    selectedNumber = *valTarget;  
    if (servoInputState >= 3) {
      selectedNumber++;
    }
  }
}

void commitServoInput() {
    setupServoSpeed = selectedNumber - 1;
    overrideSpeed = -1;
    // Persist in EEPROM
    ServoConfig tmp(setupServoLeft, setupServoRight, setupServoSpeed, setupServoOutput);
    tmp.save(setupServoIndex);
    servoEEWriteSetup();
    enterSetup();
}

void handleInputFinished() {
  if (valTarget != NULL) {
    *valTarget = selectedNumber;
  }
  if (debugServo) {
    Serial.print("KIfin:"); Serial.print(selectedNumber); Serial.println(servoInputState);
  }
  switch (servoInputState) {
    case 0:
      minValue = 0;
      maxValue = 180;
      adjStep = ServoConfig::servoPwmStep;
      valTarget = &setupServoLeft;

      Serial.print(F("Adj left: ")); 
      break;
    case 1:
      Serial.print(F("LEFT: ")); Serial.println(selectedNumber);
      minValue = 0;
      maxValue = 180;
      adjStep = ServoConfig::servoPwmStep;
      valTarget = &setupServoRight;

      Serial.print(F("Adj right: ")); 
      break;
    case 2:
      Serial.print(F("RIGHT: ")); Serial.println(selectedNumber);
      minValue = 1;
      maxValue = 8;
      adjStep = 1;
      valTarget = &setupServoSpeed;
  
      Serial.print(F("Adj speed: ")); 
      break;
      
    case 3:
      Serial.print(F("SPEED: ")); Serial.println(selectedNumber);
      commitServoInput();
      return;
    default:
      handleInputCancelled();
      return;
  }
  selectedNumber = *valTarget;
  servoInputState++;
  if (servoInputState >= 3) {
    selectedNumber++;
  }
  Serial.println(selectedNumber);
  lastSelectedNumber = selectedNumber;
  displayCurrent();
}

void handleKeyInput() {
  boolean prevCancel = wasCancel;
  boolean ok = true;

  wasCancel = currentKey == keychar_cancel;
  switch (currentKey) {
    case keychar_cancel:
      if (!prevCancel) {
        Serial.print(F("\nCancel\n"));
        handleInputReset();
        break;
      } else {
        if (debugControl) {
          Serial.print(F("Abort\n"));
        }
        handleInputCancelled();
        return;
      }
    case keychar_plus:
      if (debugServo) {
        Serial.println("KIPlus");
      }
      selectedNumber += adjStep;
      break;
    case keychar_minus:
      if (debugServo) {
        Serial.println("KIMinus");
      }
      selectedNumber -= adjStep;
      break;

    case keychar_ok:
      handleInputFinished();
      return;
  }
  if (selectedNumber > maxValue) {
    selectedNumber = maxValue;
    ok = false;
  } else if (selectedNumber < minValue) {
    selectedNumber = minValue;
    ok = false;
  }
  if (debugServo) {
    Serial.print("KIfin:"); Serial.print(selectedNumber); Serial.println(lastSelectedNumber);
  }
  if (lastSelectedNumber != selectedNumber) {
    displayCurrent();
    lastSelectedNumber = selectedNumber;
  }
}
void rangeCommand() {
  int index = servoNumber();
  int left = nextNumber();
  int right = nextNumber();

  if (index < 1) {
    return;
  }
  if (left < 0 || left > 180 || right < 0 || right > 180) {
    Serial.println(F("\nBad Pos"));
    return;
  }
  index--;

  ServoConfig tmpConfig;

  tmpConfig.load(index);
  int spd = tmpConfig.speed();
  if ((*inputPos) != 0) {
    spd = nextNumber();
    if (spd < 1 || spd > 8) {
      Serial.println(F("\nBad speed"));
      return;
    }
    spd--;
  }
  tmpConfig.setLeft(left);
  tmpConfig.setRight(right);
  tmpConfig.setSpeed(spd);
  
  initPrintBuffer();
  tmpConfig.print(index, printBuffer);
  Serial.println(printBuffer);
  tmpConfig.save(index);
  Serial.println(F("\nOK"));
}

int servoNumber() {
  int index = nextNumber();
  if (index < 1 || index > MAX_SERVO) {
    Serial.print(F("Bad servo")); Serial.println(index);
    return -1;
  }
  return index;
}

void commandServoFeedback() {
  int index = servoNumber();
  int out = nextNumber();
  if (index < 0) {
    return;
  }
  ServoConfig tmp;
  tmp.load(index - 1);
  if (out == -2 || out == 0) {
    tmp.setOutput(-1);
  } else if (out < 0 || out > MAX_OUTPUT) {
    Serial.print(F("Bad output ")); Serial.println(out);
    return;
  } else {
    tmp.setOutput(out - 1);
  }
  tmp.save(index - 1);
  Serial.print(F("Servo ")); Serial.print(index); Serial.print(F(" output ")); Serial.println(out);
}


bool lastNewline;

void commandCalibrate() {
  int idx = servoNumber();
  if (idx < 1) {
    Serial.print(F("Bad servo ")); 
    if (idx >= 0) {
      Serial.println(idx);
    } else {
      Serial.println();
    }
    return;
  }

  setupActive = true;
  Serial.print(F("Calibrate ")); Serial.println(idx);
  Serial.println(F("Use Q +, A -, ENTER/* = OK, SPACE/# = cancel"));
  Serial.print(F("Set LEFT "));
  lastNewline = false;
  charModeCallback = &calibrateTerminalCallback;
  startSetupServo(idx - 1);  
}

void calibrateTerminalCallback(char c) {
  char tr;
  boolean wasNewline = lastNewline;

  lastNewline = false;
  switch (c) {
    case 'q': case 'Q': tr = '+'; break;
    case 'a': case 'A': tr = '-'; break;
    case '\r': case '\n': {
      if (!wasNewline) {
        tr = '*'; 
      }
      lastNewline = true;
      break;
    }
    case 0x7f: case 0x1b: case ' ': tr = '#'; break;

    default:
      tr = c;
      break;
  }
  currentKey = tr;
  setupHandleKey();
}


