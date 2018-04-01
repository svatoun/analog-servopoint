
ModuleChain controlModule("Control", 0, &controlHandler);

void initControlModule() {
  registerLineCommand("CAN", &cancelCommand);
  registerLineCommand("JMP", &jumpCommand);
  Action::registerDumper(Instr::control, &printControl);
}

char conditionChars[] = { 'O', 'S', 'C', 'E' };

void printControl(const Action& a) {
  const ControlActionData& cad = a.asControlAction();
  byte c = cad.control;
  switch (c) {
    case ControlActionData::ctrlCancel:
      Serial.print(F("CAN:")); Serial.println(cad.condIndex);
      break;
    case ControlActionData::ctrlJumpIf: {
      Serial.print(F("JMP:")); Serial.println(cad.jmpTarget);

      // condition spec
      Serial.print(':');
      if (cad.invert) {
        Serial.print('!');
      }
      Serial.print(conditionChars[cad.cond]);
      Serial.print(':');
      Serial.println(cad.condIndex);
      break;
    }
    default:
      Serial.println(F("CTRL:x"));
      return;
  }
}

void controlHandler(ModuleCmd cmd) {
  switch (cmd) {
    case initialize:
      initControlModule();
      break;
    case eepromLoad:
      break;
    case eepromSave:
      break;
    case dump:
      break;
    case status:
      break;
    case reset:
      break;
    case periodic:
      break;
  }
}

void cancelCommand() {
  int n = nextNumber();
  if (n < 1 || n > MAX_COMMANDS) {
    Serial.println(F("Bad cmd"));
    return;
  }
  const Command&c = commands[n];
  if (c.available()) {
    Serial.println(F("No cmd"));
    return;
  }
  Action ac;
  ac.command = Instr::control;
  ControlActionData& cad = ac.asControlAction();
  cad.cancel(n - 1);
  prepareCommand();
  addCommandPart(ac);
}

extern int8_t commandDef;

void jumpCommand() {
  boolean minus = false;
  if (*inputPos == '-') {
    minus = true;
    inputPos++;
  }
  int sk = nextNumber();
  if (sk < 0 || sk > 14) {
    Serial.print(F("Bad pos"));
    return;
  }
  if ((!minus) && (sk > commandDef)) {
    Serial.print(F("Inv pos"));
    return;
  }
  if (minus) {
    sk = -sk;
  }
  Action ac;
  ControlActionData cad = ac.asControlAction();
  cad.control = ControlActionData::ctrlJumpIf;
  cad.jmpTarget = sk;

  byte ct;
  boolean inv = false;
  if (*inputPos == '!') {
    inputPos++;
    inv = true;
  }
  switch (*inputPos) {
    case 'c': ct = ControlActionData::condCommand; break;
    case 's': ct = ControlActionData::condServo; break;
    case 'o': ct = ControlActionData::condOutput; break;
    default:
      Serial.print(F("Bad cond"));
      return;
  }
  if ((*(++inputPos)) != ':') {
    Serial.print(F("No cond"));
    return;
  }
  inputPos++;
  int cond = nextNumber();
  if (cond < 1 || cond > 31) {
    Serial.print(F("Bad cond"));
  }

  ac.command = Instr::control;
  cad.invert = inv;
  cad.cond = ct;
  cad.condIndex = cond;
  prepareCommand();
  addCommandPart(ac);
}

