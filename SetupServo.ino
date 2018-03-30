
byte setupServoLeft;
byte setupServoRight;
byte setupServoSpeed;
signed char setupServoOutput;

int lastSelectedNumber;
int selectedNumber;
int typedNumber = -1;
long lastTypedMillis;
boolean shown;
boolean wasCancel;
byte digitCount;

const int showSelectedServoTime = 2000;

int setupServoIndex = -1;

const int blinkServo[] = { 250, 250, 1000, 250, 0 };

class InputServoPos : public NumberInput {
  boolean left;
  public:
  InputServoPos(boolean aLeft) : NumberInput(0, 180), left(aLeft) {
    selectedNumber = getValue();
  }

  virtual int getValue() override;
  virtual int setValue(int v) override;
  virtual int incValue(int v, int inc) override;
  virtual void finished(int value) override;
  virtual void displayCurrent(int selected) override;
};


class InputServoNumber : public NumberInput {
  public:
  InputServoNumber() : NumberInput(1, MAX_SERVO) {}

  virtual void finished(int value) override;
};

class InputServoSpeed : public NumberInput {
  public:
  InputServoSpeed() : NumberInput(1, MAX_SPEED) {}

  virtual void finished(int speed) override;
  void displayCurrent(int selected) override;
  void cancelled() override;
  int getValue() override;
};

int InputServoPos::getValue() {
  if (left) {
    return setupServoLeft;
  } else {
    return setupServoRight;
  }
}

int InputServoPos::setValue(int v) {
  return ServoConfig::value(v);
}

int InputServoPos::incValue(int v, int inc) {
  return ServoConfig::change(v, inc);
}

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

void startSetupServo(int value) {
    setupServoIndex = value;
    ServoConfig tmp;
    tmp.load(value);
    setupServoLeft = tmp.left();
    setupServoRight = tmp.right();
    setupServoSpeed = tmp.speed();
    setupServoOutput = tmp.output();
//    setupServoConfig = tmp;
    Serial.print(F("Configuring ")); Serial.println(setupServoIndex);
    setupState = servoSetLeft;
    NumberInput::set(new InputServoPos(true));
    numberInput->display();
    Serial.print(F("Adj left:")); Serial.println(numberInput->getValue());
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

void enterSetupServo0() {
    setupServoIndex = -1;
    adjustCallback = NULL;
    setupState = servoSelect;
    NumberInput::set(new InputServoNumber());
}

void enterSetupServo() {
    // read EEPROM into the servo config
//    eeBlockRead('S', eeaddr_servoConfig, &servoConfig[0], sizeof(servoConfig));
    if (debugControl) {
      Serial.print(F("Entering servo setup"));
    }
    makeLedAck(&blinkServo[0]);
    enterSetupServo0();
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
          Serial.print(F("Cancel number entry\n"));
        }
        numberInput = NULL;
        cancelled();
        return;
      }
      break;
    case keychar_plus: {
      if (debugControl) {
        Serial.print(F("Number increased"));
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
        Serial.print(F("Number decreased"));
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
    Serial.print(F("Selected number: ")); Serial.println(selectedNumber);
    Serial.print(F("Typed number: ")); Serial.println(typedNumber);
  }
  if (lastSelectedNumber != selectedNumber) {
    displayCurrent(selectedNumber);
    lastSelectedNumber = selectedNumber;
  }
}

void rangeCommand(String& l) {
  int index = servoNumber(l);
  int left = nextNumber(l);
  int right = nextNumber(l);

  if (index < 1) {
    return;
  }
  if (left < 0 || left > 180) {
    Serial.println(F("\nInvalid left pos"));
    return;
  }
  if (right < 0 || right > 180) {
    Serial.println(F("\nInvalid right pos"));
    return;
  }
  index--;

  ServoConfig tmpConfig;

  tmpConfig.load(index);
  int spd = tmpConfig.speed();
  if (l.length() > 0) {
    spd = nextNumber(l);
    if (spd < 1 || spd > 8) {
      Serial.println(F("\nInvalid speed"));
      return;
    }
    spd--;
  }
  tmpConfig.setLeft(left);
  tmpConfig.setRight(right);
  tmpConfig.setSpeed(spd);
  String s;
  tmpConfig.print(index, s);
  Serial.println(s);
  tmpConfig.save(index);
  Serial.println(F("\nOK"));
}

int servoNumber(String& s) {
  int index = nextNumber(s);
  if (index < 0 || index > MAX_SERVO) {
    Serial.print(F("Bad servo")); Serial.println(index);
    return -1;
  }
  return index;
}

void commandServoFeedback(String& s) {
  int index = servoNumber(s);
  int out = nextNumber(s);
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

void commandCalibrate(String& s) {
  int idx = servoNumber(s);
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


