const int showSelectedServoTime = 2000;

int servoIndex = -1;

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
  virtual void displayCurrent(int typed, int selected) override;
};


class InputServoNumber : public NumberInput {
  public:
  InputServoNumber() : NumberInput(1, MAX_SERVO) {}

  virtual void finished(int value) override;
  
  virtual void showIdle(int value) override {
    waveSelectedServo(value - 1);
    shown = true;
  }
};

class InputServoSpeed : public NumberInput {
  public:
  InputServoSpeed() : NumberInput(1, MAX_SPEED) {}

  virtual void finished(int speed) override;
  virtual void showIdle(int speed) override;
  void displayCurrent(int typed, int selected) override;
  void cancelled() override;
  int getValue() override;
};

int InputServoPos::getValue() {
  if (left) {
    return servoConfig[servoIndex].left();
  } else {
    return servoConfig[servoIndex].right();
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
    Serial.print(F("Set left position to: ")); Serial.println(value);
    servoConfig[servoIndex].setLeft(value);
    NumberInput::set(new InputServoPos(false));
    setupState = servoSetRight;
    numberInput->display();
    Serial.print(F("Right pos = ")); Serial.println(numberInput->getValue());
  } else {
    Serial.print(F("Set right position to: ")); Serial.println(value);
    servoConfig[servoIndex].setRight(value);
    setupState = servoSetSpeed;
    NumberInput::set(new InputServoSpeed());
    Serial.print(F("Speed = ")); Serial.println(numberInput->getValue());
  }
}

void InputServoNumber::finished(int value) {
    if (value > MAX_SERVO) {
      enterSetup();
      return;
    }
    startSetupServo(value - 1);
}

void startSetupServo(int value) {
    servoIndex = value;
    Serial.print(F("Configuring servo ")); Serial.println(servoIndex);
    setupState = servoSetLeft;
    NumberInput::set(new InputServoPos(true));
    numberInput->display();
    Serial.print(F("Left pos = ")); Serial.println(numberInput->getValue());
}

void InputServoPos::displayCurrent(int typed, int selectedPos) {
  if (debugControl) {
    Serial.print(F("Moving servo to: ")); Serial.println(selectedPos);
  }
  clearNewCommand();
  Action a1;
  a1.asServoAction().move(servoIndex, selectedPos);
  addNewCommand(a1);
  executor.playNewAction();
}

int InputServoSpeed::getValue() {
  return servoConfig[servoIndex].speed();
}

void InputServoSpeed::finished(int v) {
  Serial.print(F("Setting speed ")); Serial.println(v);
  servoConfig[servoIndex].setSpeed(v - 1);
  overrideSpeed = -1;
  // Persist in EEPROM
  servoEEWriteSetup();
  enterSetup();
}

void InputServoSpeed::cancelled() {
  overrideSpeed = -1;
}

void InputServoSpeed::displayCurrent(int typed, int n) {
  overrideSpeed = n - 1;

  clearNewCommand();

  Action  a;
  ServoActionData& sad = a.asServoAction();
  sad.moveLeft(servoIndex);
  addNewCommand(a);

  sad.moveRight(servoIndex);
  addNewCommand(a);
  executor.playNewAction();
}

void InputServoSpeed::showIdle(int n) {
  displayCurrent(-1, n);
}

void enterSetupServo0() {
    servoIndex = -1;
    adjustCallback = NULL;
    setupState = servoSelect;
    NumberInput::set(new InputServoNumber());
}

void enterSetupServo() {
    // read EEPROM into the servo config
    eeBlockRead('S', eeaddr_servoConfig, &servoConfig[0], sizeof(servoConfig));
    if (debugControl) {
      Serial.print(F("Entering servo setup"));
    }
    makeLedAck(&blinkServo[0]);
    enterSetupServo0();
}

void NumberInput::display() {
  displayCurrent(-1, selectedNumber);
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
  boolean ok;
  switch (currentKey) {
    case keychar_cancel:
      
      if (typedNumber != -1 || !prevCancel) {
        Serial.print(F("Cancel pressed"));
        reset();
        typedNumber = 1;
        selectedNumber = getValue();
        makeLedAck(&blinkLong[0]);
        lastTypedMillis = 0;
      } else {
        if (debugControl) {
          Serial.print(F("Cancel number entry"));
        }
        numberInput = NULL;
        cancelled();
        return;
      }
      wasCancel = true;
      return;
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
      }
      typedNumber = -1;
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
        break;
      }
      if (typedNumber < 0 || digitCount >= maxDigits) {
        typedNumber = 0;
        digitCount = 0;
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
  displayCurrent(typedNumber, selectedNumber);
}

void NumberInput::handleIdle() {
  if ((lastTypedMillis == 0) || shown) {
    return;
  }
  if (lastTypedMillis + 2000 > currentMillis) {
    return;
  }
  int v = typedNumber == -1 ? selectedNumber : typedNumber;
  showIdle(v);
  shown = true;
}

void waveSelectedServo() {
  waveSelectedServo(servoIndex);
}

void waveSelectedServo(int servoId) {
    if (servoId >= MAX_SERVO) {
      return;
    }
    clearNewCommand();

    String out;

    Action a1;
    a1.asServoAction().move(servoId, 70);
    addNewCommand(a1);
    if (debugControl) {
      a1.asServoAction().print(out);
      out.concat("\n");
    }

    a1.asServoAction().move(servoId, 110);
    addNewCommand(a1);
    if (debugControl) {
      a1.asServoAction().print(out);
      out.concat("\n");
    }

    a1.asServoAction().moveLeft(servoId);
    addNewCommand(a1);
    if (debugControl) {
      a1.asServoAction().print(out);
      out.concat("\n");

      Serial.print(out);
    }
    executor.playNewAction();
}

void rangeCommand(String& l) {
  int index = nextNumber(l);
  int left = nextNumber(l);
  int right = nextNumber(l);

  if (index < 1 || index > MAX_SERVO) {
    Serial.println(F("\nInvalid servo index"));
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

  int spd = servoConfig[index].speed();
  if (l.length() > 0) {
    spd = nextNumber(l);
    if (spd < 1 || spd > 8) {
      Serial.println(F("\nInvalid speed"));
      return;
    }
    spd--;
  }
  servoConfig[index].setLeft(left);
  servoConfig[index].setRight(right);
  servoConfig[index].setSpeed(spd);
  String s;
  servoConfig[index].print(index, s);
  Serial.println(s);

  Serial.println(F("\nOK"));
}


void commandCalibrate(String& s) {
  int idx = nextNumber(s);
  if (idx < 1 || idx > MAX_SERVO) {
    Serial.print(F("Bad servo")); Serial.println(idx);
    return;
  }

  setupActive = true;
  Serial.print(F("Calibrate servo ")); Serial.println(idx);
  Serial.println(F("Use Q +, A -, ENTER/* = OK, SPACE/# = cancel"));
  Serial.print(F("Set LEFT position"));
  charModeCallback = &calibrateTerminalCallback;
  startSetupServo(idx - 1);  
}

void calibrateTerminalCallback(char c) {
  char tr;

  switch (c) {
    case 'q': case 'Q': tr = '+'; break;
    case 'a': case 'A': tr = '-'; break;
    case '\n': case '\r': tr = '*'; break;
    case ' ': tr = '#'; break;

    default:
      tr = c;
      break;
  }
  currentKey = tr;
  setupHandleKey();
}


