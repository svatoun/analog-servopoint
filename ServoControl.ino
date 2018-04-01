//
///////////////////////////// Servo Movement //////////////////////////////

int overrideSpeed = -1;
extern ServoConfig setupServoConfig;
extern byte setupServoLeft;
extern byte setupServoRight;
extern byte setupServoSpeed;
extern signed char setupServoOutput;

#ifdef SERVOS
ServoProcessor  processors[2];  

/**
   Servo predefined positions.
*/

ModuleChain servoModule("Servo", 0, &servoModuleHandler);

byte  servoPositions[MAX_SERVO]; 
boolean enable = false;


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
  registerLineCommand("SFB", &commandServoFeedback);

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

void initServoFeedback(int pos, const ServoConfig& cfg) {
  if (cfg.isEmpty()) {
    return;
  }
  int out = cfg.output();
  if (out < 0) {
    return;
  }
  int l = cfg.left();
  pos -= l;
  bool b = (pos > -3) && (pos < 3);
  output.setBit(out, b);
  Output::setOutputActive(out, b);
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

    // power up servo
    digitalWrite(powerPin, LOW);
    waitAndRefresh(200, s1, s2);

    ServoConfig tmp;
    tmp.load(index);
    initServoFeedback(pos1, tmp);
    tmp.load(index + 1);
    initServoFeedback(pos2, tmp);
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

ServoProcessor::ServoProcessor() : servoIndex(noservo), blockedState(NULL), servoMask(0)
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
  if (currentMillis < switchMillis) {
    return;
  }
  switchMillis = currentMillis + diff;
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
  if (debugServoMove) {
    Serial.print(servoIndex); Serial.print(":"); Serial.print(currentMillis); Serial.print(F(":\t Servo moving to ")); Serial.print(curAngle); Serial.print(F(", target = ")); Serial.println(targetAngle);
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
  if (servoOutput >= 0) {
    output.setBit(servoOutput, setOutputOn);
    Output::setOutputActive(servoOutput, setOutputOn);
  }
  if (blockedState != NULL) {
    executor.finishAction2(*blockedState);
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

boolean ServoProcessor::cancel(const ExecutionState& s) {
  if (blockedState == &s) {
    clear();
    return true;
  } else {
    return false;
  }
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
  int l, r, spd;
  
  if (s == setupServoIndex) {
    l = setupServoLeft;
    r = setupServoRight;
    spd = setupServoSpeed;
    servoOutput = setupServoOutput;
  } else {
    ServoConfig cfg;
    cfg.load(s);
    
    l = cfg.left();
    r = cfg.right();
    spd = cfg.speed();
    servoOutput = cfg.output();
  }
  
  int target;
  if (data.isPredefinedPosition()) {
    boolean left = data.isLeft();
    if (state.invert) {
      left = !left;
    }
    setOutputOn = left;
    target = left ? l : r;
  } else {
    setOutputOn = true;
    if (servoOutput >= 0) {
      output.setBit(servoOutput, false);
      Output::setOutputActive(servoOutput, false);
    }
    target = data.targetPosition();
  }
  int curAngle = servoPositions[s];
  if (debugServo) {
    Serial.print(s); Serial.print(F(": Action to move from ")); Serial.print(curAngle); Serial.print(F(", target = ")); Serial.println(target);
  }
  // must set here, otherwise moveFinished may report bad data to console.
  servoIndex = s;
  if (target == curAngle) {
    moveFinished();
    return finished;
  }
  setupSelector(s);
  ctrl.attach(pwmPin);
  targetAngle = target;
  int diff = servoSpeedTimes[servoSpeed];
  switchMillis= currentMillis + diff;
  servoSpeed = overrideSpeed == -1 ? spd : overrideSpeed;
  if (debugServo) {
    Serial.print(F("Speed ")); Serial.print(servoSpeed); Serial.print(F(", millis = ")); Serial.print(diff);
  }
  enable = true;
  ctrl.write(curAngle);

  if (state.wait) {
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

void moveCommand() {
  int sNumber = servoNumber();
  if (sNumber < 1) {
    return;
  }
  sNumber--;
  const char c = *inputPos;
  Action a;
  ServoActionData& d = a.asServoAction();  
  switch (c) {
    case 'l': 
      d.moveLeft(sNumber);
      break;  
    case 'r': 
      d.moveRight(sNumber);
      break;
    case 0:
      Serial.println(F("Bad pos"));
      return;
    default:
      int v = nextNumber();
      if (v < 0 || v > 180) {
        Serial.println(F("Bad pos"));
        return;
      }
      d.move(sNumber, v);
      break;
  }
  prepareCommand();
  addCommandPart(a);
}

#else

void servoEEWriteSetup() {}

#endif

