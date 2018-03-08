//
///////////////////////////// Servo Movement //////////////////////////////
ServoProcessor  processors[2];  

/**
   Servo predefined positions.
*/

ModuleChain servoModule("Servo", 0, &servoModuleHandler);

byte  servoPositions[MAX_SERVO]; 
boolean enable = false;
int overrideSpeed = -1;

void servoSetup() {
  pinMode(servoSelectA, OUTPUT);
  pinMode(servoSelectB, OUTPUT);
//  pinMode(servo18Enable, OUTPUT);

  pinMode(servoPowerA, OUTPUT);
  pinMode(servoPowerB, OUTPUT);
  pinMode(servoPowerC, OUTPUT);
  pinMode(servoPowerD, OUTPUT);

  digitalWrite(servoPowerA, HIGH);
  digitalWrite(servoPowerB, HIGH);
  digitalWrite(servoPowerC, HIGH);
  digitalWrite(servoPowerD, HIGH);

  executor.addProcessor(&processors[0]);
  processors[0].clear();
  executor.addProcessor(&processors[1]);
  processors[1].clear();
  
  processors[0].attach(pwmPinA, 0b01010101);
  processors[1].attach(pwmPinB, 0b10101010);
  processors[0].linkWith(&processors[1]);

  for (int i = 0; i < MAX_SERVO; i++) {
    servoPositions[i] = 90;
  }
  servoEepromLoad();

  if (debugControl) {
    Serial.println(F("Servo config:"));
    servoDump();
    servoStatus();
  }
  startServos1To8();
  // register commands
  registerLineCommand("RNG", &rangeCommand);
  registerLineCommand("MOV", &moveCommand);
  registerLineCommand("CAL", &commandCalibrate);

  Action::registerDumper(servo, printServoAction);
}

void printServoAction(const Action& a, String& s) {
  a.asServoAction().print(s);
}

void servoDump() {
  String s;
  ServoConfig tmp;
  for (int i = 0; i < MAX_SERVO; i++) {
    tmp.load(i);
    if (tmp.isEmpty()) {
      continue;
    }
    s = "";
    tmp.print(i + 1, s);
    Serial.println(s);
  }
}

void servoClear() {
  for (int i = 0; i < MAX_SERVO; i++) {
    ServoConfig tmp;
    tmp.clear();
    tmp.save(i);
  }
  for (int i = 0; i < sizeof(servoPositions) / sizeof(servoPositions[0]); i++) {
    servoPositions[i] = 90;
  }
}

void servoStatus() {
  int srvCount = 0;
  for (int i = 0; i < MAX_SERVO; i++) {
    ServoConfig tmp;
    tmp.load(i);
    if (tmp.isEmpty()) {
      continue;
    }
    srvCount++;
  }
  Serial.print(F("Servos defined: ")); Serial.println(srvCount);
    Serial.print(F("Servo Positions: "));
    for (int i = 0; i < sizeof(servoPositions) / sizeof(servoPositions[0]); i++) {
      Serial.print((int)servoPositions[i]); Serial.print(F(","));
    }
    Serial.println();
}

void servoModuleHandler(ModuleCmd cmd) {
  switch (cmd) {
    case initialize:
      servoSetup();
      break;
    case eepromLoad:
      servoEepromLoad();
      break;
    case eepromSave:
      servoEEWriteSetup();
      break;
    case dump:
      servoDump();
      break;
    case status:
      servoStatus();
      break;
    case reset:
      servoClear();
      break;
    case periodic:
      handleServoMovement();
      break;
  }
}

void servoEepromLoad() {
//  eeBlockRead('S', eeaddr_servoConfig, &servoConfig[0], sizeof(servoConfig));
  eeBlockRead('P', eeaddr_servoPositions, &servoPositions[0], sizeof(servoPositions));
}

void ServoConfig::load(int idx) {
  EEPROM.get(eeaddr_servoConfig + idx * sizeof(ServoConfig), *this);
}

void ServoConfig::save(int idx) {
  EEPROM.put(eeaddr_servoConfig + idx * sizeof(ServoConfig), *this);
}


void waitAndRefresh(int l, Servo& s1, Servo& s2) {
#ifdef SOFTWARE_SERVO
  long ms = millis();
  long e = ms + l;
  while (millis() < e) {
    delay(5);
    s1.refresh();
    s2.refresh();
  }
#else
  delay(l);
#endif
}

void startServos1To8() {
  Servo s1;
  Servo s2;
  s1.attach(pwmPinA);
  s2.attach(pwmPinB);

  if (debugPower) {
    Serial.println(F("Powering up servos"));
  }

  int index = 0;
  for (int sel = 0; sel < 4; sel++, index += 2) {
    int pos1 = servoPositions[index];
    int pos2 = servoPositions[index + 1];
    
    digitalWrite(servoSelectA, (sel & 0b01) ? HIGH : LOW);
    digitalWrite(servoSelectB, (sel & 0b10) ? HIGH : LOW);

    if (debugPower) {
      Serial.print(F("Base = ")); Serial.print(index);
      Serial.print(F(", sel1 = ")); Serial.print((sel & 0b01) ? HIGH : LOW); 
      Serial.print(F(", sel2 = ")); Serial.print((sel & 0b10) ? HIGH : LOW);
      Serial.print(F(", servo = ")); Serial.print(pos1); Serial.print(F(", servo+1 = ")); Serial.println(pos2);
    }
    s1.write(pos1);
    s2.write(pos2);
    waitAndRefresh(100, s1, s2);
    // enable power
    int powerPin;
    switch (sel) {
      case 0: powerPin = servoPowerA; break;
      case 1: powerPin = servoPowerB; break;
      case 2: powerPin = servoPowerC; break;
      case 3: powerPin = servoPowerD; break;
    }
    if (debugPower) {
      Serial.print(F("Power on pin: ")); Serial.println(powerPin);
    }

/*
    int duty = 5000;
    int pwr = 700;

    for (; pwr < duty; pwr += 300) {
      for (int count = 0; count < 20; count++) {
          digitalWrite(powerPin, LOW);
          delayMicroseconds(pwr);
          digitalWrite(powerPin, HIGH);
          delayMicroseconds(duty - pwr);
      }
    }
*/
    // power up servo
    digitalWrite(powerPin, LOW);
    waitAndRefresh(200, s1, s2);
  }

  // detach from HW pins
  s1.detach();
  s2.detach();
}

void servoEEWriteSetup() {
//  eeBlockWrite('S', eeaddr_servoConfig, &servoConfig[0], sizeof(servoConfig));
  servoEEWrite();
}

void servoEEWrite() {
  eeBlockWrite('P', eeaddr_servoPositions, &servoPositions[0], sizeof(servoPositions));
}

void handleServoMovement() {
  for (int i = 0; i < sizeof(processors) / sizeof(ServoProcessor); i++) {
    processors[i].tick();
  }
}

ServoProcessor::ServoProcessor() : servoIndex(noservo), actionIndex(noaction), blockedAction(NULL), blockedState(NULL), servoMask(0)
{
}

void ServoProcessor::attach(int pin, unsigned long mask) {
  pwmPin = pin;
  servoMask = mask;
}

void ServoProcessor::linkWith(ServoProcessor *other) {
  other->linked = this;
  this->linked = other;
}

void ServoProcessor::tick() {
#ifdef SOFTWARE_SERVO
  ctrl.refresh();
#endif
  if (available()) {
    return;
  }
  
  int curAngle = servoPositions[servoIndex]; // ctrl.read();
  int diff = servoSpeedTimes[servoSpeed];
  long switchMillis = prevMillis + diff;
  if (currentMillis < switchMillis) {
    return;
  }
  prevMillis = currentMillis;
  if (curAngle == targetAngle) {
    if (targetPosCounter < (switchMillis < 50 ? 10 : 3)) {
      targetPosCounter++;
      return;
    }
    moveFinished();
    return;
  }
  
  // move the servo towards the desired angle
  if (curAngle > targetAngle) {
    curAngle --;
  } else {
    curAngle ++;
  }
  if (debugServo) {
    Serial.print(servoIndex); Serial.print(":"); Serial.print(currentMillis); Serial.print(":"); Serial.print(switchMillis); Serial.print(":"); Serial.print(prevMillis); Serial.print(F(":\t Servo moving to ")); Serial.print(curAngle); Serial.print(F(", target = ")); Serial.println(targetAngle);
  }
  ctrl.write(curAngle);
  // record the current state
  servoPositions[servoIndex] = curAngle;
}

void ServoProcessor::clear() {
  if (servoIndex == noservo) {
    return;
  }
  if (debugServo) {
    Serial.print(F("Stopping servo ")); Serial.println(servoIndex);
  }
  ctrl.detach();
  servoIndex = noservo;
  actionIndex = noaction;
  blockedAction = NULL;
  blockedState = NULL;
  enable = false;
  targetPosCounter = 0;
}

void ServoProcessor::moveFinished() {
  if (debugServo) {
    Serial.print(servoIndex); Serial.println(F(": Move finished"));
  }
  if (setupServoIndex < 0) {
    servoEEWrite();
  }
  if (blockedState != NULL) {
    executor.finishAction(*blockedState);
  }
  clear();
}

boolean ServoProcessor::isCompatibleWith(int otherIndex) {
  if (available()) {
    return true;
  }
  // pair servos 0+1, 1+2, 3+4
  int si = servoIndex & 0xfe;
  int oe = otherIndex & 0xfe;
  return si == oe;
}

boolean ServoProcessor::cancel(const Action& action) {
  if (blockedAction == &action) {
    clear();
    return true;
  } else {
    return false;
  }
}

Processor::R ServoProcessor::processAction(const Action& a, int h) {
  return ignored;
}

Processor::R ServoProcessor::processAction2(ExecutionState& state) {
  const Action& action = state.action.a();
  if (action.command != servo) {
    return ignored;
  }
  if (!available()) {
    return ignored;
  }
  ServoActionData data = action.asServoAction();
  
  int s = data.servoIndex;
  if (s >= MAX_SERVO) {
    return finished;
  }
  if ((servoMask & (1 << s)) == 0) {
    return ignored;
  }
  if (linked != NULL) {
    if (!linked->isCompatibleWith(s)) {
      return ignored;
    }
  }
  ServoConfig cfg;
  if (s == setupServoIndex) {
    cfg = setupServoConfig;  
  } else {
    cfg.load(s);
  }
  
  int target;
  if (data.isPredefinedPosition()) {
    boolean left = data.isLeft();
    if (state.invert) {
      left = !left;
    }
    target = left ? cfg.left() : cfg.right();
  } else {
    target = data.targetPosition();
  }
  int curAngle = servoPositions[s];
  if (debugServo) {
    Serial.print(s); Serial.print(F(": Action to move from ")); Serial.print(curAngle); Serial.print(F(", target = ")); Serial.println(target);
  }
  if (target == curAngle) {
    clear();
    return finished;
  }
  setupSelector(s);
  ctrl.attach(pwmPin);
  servoIndex = s;
  targetAngle = target;
  prevMillis = currentMillis;
  servoSpeed = overrideSpeed == -1 ? cfg.speed() : overrideSpeed;
  if (debugServo) {
    Serial.print(F("Speed ")); Serial.print(servoSpeed); Serial.print(F(", millis = ")); Serial.print(servoSpeedTimes[servoSpeed]);
  }
  enable = true;
  ctrl.write(curAngle);

  if (state.wait) {
    blockedAction = &action;
    blockedState = &state;
    return blocked;
  } else {
    return finished;
  }
}

void ServoProcessor::setupSelector(int index) {
  // 2 servos / selector value for 4052
  index = index >> 1;
  digitalWrite(servoSelectA, (index & 0b01) ? HIGH : LOW);
  digitalWrite(servoSelectB, (index & 0b10) ? HIGH : LOW);
}

void moveCommand(String& line) {
  int sNumber = nextNumber(line);
  if (sNumber > MAX_SERVO) {
    Serial.print(F("Invalid servo number"));
    return;
  }
  sNumber--;
  char c = line.charAt(0);
  Action a;
  
  switch (c) {
    case 'l': case 'L':
      a.asServoAction().moveLeft(sNumber);
      break;  
    case 'r': case 'R':
      a.asServoAction().moveRight(sNumber);
      break;
    default:
      int v = nextNumber(line);
      if (v < 0 || v > 180) {
        Serial.println(F("Invalid position"));
        return;
      }
      a.asServoAction().move(sNumber, v);
      break;
  }
  prepareCommand();
  addCommandPart(a);
}

