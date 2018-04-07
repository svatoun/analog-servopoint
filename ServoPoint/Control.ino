
ModuleChain controlModule("Control", 0, &controlHandler);

class ControlProcessor : public Processor {
  public:
    ControlProcessor() {
    }
    R processAction2(ExecutionState&) override;
    R pending(ExecutionState&) override;
    void tick() override;
    void clear() override;
    boolean cancel(const ExecutionState& ac) override;
};

ControlProcessor controlProcessor;

void initControlModule() {
  registerLineCommand("WAC", &waitConditionCommand);
  registerLineCommand("CAN", &cancelCommand);
  registerLineCommand("JMP", &jumpCommand);
  Action::registerDumper(Instr::control, &printControl);

  Executor::addProcessor(&controlProcessor);
}

char conditionChars[] = { 'E', 'K', 'O', 'S', 'C', 'E', 'E' };

void printControl(const Action& a, char* out) {
  const ControlActionData& cad = a.asControlAction();
  byte c = cad.control;
  switch (c) {
    case ControlActionData::ctrlCancel:
      strcat_P(out, PSTR("CAN:"));
      out += strlen(out);
      out = printNumber(out, cad.jmpTarget + 1, 10);
      break;
    case ControlActionData::ctrlWait:
      strcat_P(out, PSTR("WAC"));
      out += strlen(out);
      break;
    case ControlActionData::ctrlExec: {
      strcat_P(out, PSTR("EXE:"));
      out += strlen(out);
      out = printNumber(out, cad.jmpTarget + 1, 10);
      break;
    }
    case ControlActionData::ctrlJumpIf: {
      strcat_P(out, PSTR("JMP:"));
      out += strlen(out);
      char t = cad.jmpTarget;
      if (t < 0) {
        t = -t;
        append(out, '-');        
      }
      out = printNumber(out, t, 10);
      break;
    }
    default:
      strcat_P(out, PSTR("CTRL:x"));
      return;
  }
  if (cad.cond == ControlActionData::condNone) {
    return;
  }
  // condition spec
  append(out, ':');
  if (cad.invert) {
    append(out, '!');
  }
  append(out, conditionChars[cad.cond]);
  if (cad.trigger) {
    append(out, '*');
  }
  append(out, ':');
  out = printNumber(out, cad.condIndex + 1, 10);
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
  n--;
  const Command&c = commands[n];
  if (c.available()) {
    Serial.println(F("No cmd"));
    return;
  }
  Action ac;
  ac.command = Instr::control;
  ControlActionData& cad = ac.asControlAction();
  cad.cancel(n);
//  prepareCommand();
//  addCommandPart(ac);
  finishControlCommand(ac, true);
}

extern int8_t commandDef;


void waitConditionCommand() {
  Action ac;
  ControlActionData& cad = ac.asControlAction();
  cad.control = ControlActionData::ctrlWait;
  finishControlCommand(ac, false);
}


void finishControlCommand(Action& ac, boolean noCond) {
  ControlActionData& cad = ac.asControlAction();
  byte ct;
  boolean inv = false;
  if (*inputPos == '!') {
    inputPos++;
    inv = true;
  }
  int cond = 0;
  byte mx = 1;
  switch (*inputPos) {
    case 'k': ct = ControlActionData::condKey; mx = MAX_INPUT_BUTTONS; break;
    case 'c': ct = ControlActionData::condCommand; mx = MAX_COMMANDS; break;
    case 's': ct = ControlActionData::condServo; mx = MAX_SERVO;  break;
    case 'o': ct = ControlActionData::condOutput; mx = MAX_OUTPUT; break;
    case 0:
      if (noCond) {
        ct = ControlActionData::condNone; 
        goto noCondition;
      }
      // fall through
    default:
      Serial.print(F("Bad cond"));
      return;
  }
  ++inputPos;
  if (*inputPos == '*') {
    inputPos++;
    cad.trigger = 1;
  }
  if ((*(inputPos)) != ':') {
    Serial.print(F("No cond"));
    return;
  }
  inputPos++;
  cond = nextNumber();
  if (cond < 1 || cond > mx) {
    Serial.print(F("Bad cond"));
  }
  cond--;
  noCondition:
  ac.command = Instr::control;
  cad.invert = inv;
  cad.cond = ct;
  cad.condIndex = cond;

  prepareCommand();
  addCommandPart(ac);
}

void jumpCommand() {
  boolean minus = false;
  if (*inputPos == '-') {
    minus = true;
    inputPos++;
  }
  int sk = nextNumber();
  if (sk < 0 || sk > 14) {
    Serial.println(F("Bad pos"));
    return;
  }
  if (minus && (sk > commandDef)) {
    Serial.println(F("Inv pos"));
    return;
  }
  if (minus) {
    sk = -sk;
  }
  Action ac;
  ControlActionData& cad = ac.asControlAction();
  cad.control = ControlActionData::ctrlJumpIf;
  cad.jmpTarget = sk;
  finishControlCommand(ac, true);
}

// XXX PROGMEM !
char (*gsFuncTable[])(byte) = { NULL, &getKeyChange, &getOutputActive, &getServoActive, &getCommandActive };
boolean (*isFuncTable[])(byte) = { NULL, &isKeyPressed, &isOutputActive, &isServoActive, &isCommandActive };
void (*primeFuncTable[])(byte) = { NULL, &primeKeyChange, &primeOutputChange, &primeServoActive, &primeCommandActive };
static_assert (ControlActionData::condTop == (sizeof(isFuncTable) / sizeof(isFuncTable[0])), "Error in dispatch table");

boolean evaluateCondition(ExecutionState& state) {
  const Action& a = state.action.a();
  const ControlActionData& cad = a.asControlAction();
  byte cond = cad.cond;
  byte inv = cad.invert;
  byte index = cad.condIndex;
  byte trigger = cad.trigger;
  boolean res = !inv;

  initPrintBuffer();
  printControl(a, printBuffer);
  if (debugConditions) {
    Serial.print("CTLEval:"); Serial.print((int)&state, HEX); Serial.print(' '); Serial.println(printBuffer);
  }

  if (cond >= (sizeof(gsFuncTable) / sizeof(gsFuncTable[0]))) {
    return false;
  }
  
  char (*gsFunc)(byte) = NULL;
  boolean (*isFunc)(byte) = NULL;
  if (debugConditions) {
    Serial.print("cond:"); Serial.println(cond);
  }
  gsFunc = gsFuncTable[cond];
  isFunc = isFuncTable[cond];

  if (gsFunc != NULL) {
    if (trigger) {
      char state = gsFunc(index);
      if (debugConditions) {
        Serial.print("T"); Serial.println((int)state);
      }
      if (state == -1) {
        return false;
      } else {
        res = (state == 1);
      }
    } else {
      res = isFunc(index);
      if (debugConditions) {
        Serial.print("S"); Serial.println(res);
      }
    }
  }
  return res != inv;
}

Processor::R ControlProcessor::processAction2(ExecutionState& state) {
  const Action& a = state.action.a();
  if (a.command != control) {
    return Processor::R::ignored;
  }
  const ControlActionData& cad = a.asControlAction();
  byte ctrl = cad.control;
  if (state.invert) {
    return Processor::R::finished;
  }
  if (debugConditions) {
    Serial.print(F("CTLEx: "));
  }
  initPrintBuffer();
  printControl(a, printBuffer);
  if (debugConditions) {
    Serial.println(printBuffer);
  }
  if (!state.action.isPersistent() && (ctrl != ControlActionData::ctrlCancel)) {
    return Processor::R::finished;
  }

  if ((ctrl == ControlActionData::ctrlWait) && cad.trigger) {
    if (debugConditions) {
      Serial.println("Priming");
    }
    primeFuncTable[cad.cond](cad.condIndex);
    return Processor::R::blocked;
  }

  byte inv = cad.invert;
  char target = cad.jmpTarget;

  boolean result = evaluateCondition(state);

  if (!result) {
      if (ctrl == ControlActionData::ctrlWait) {
        if (debugConditions) {
          Serial.println("Wait");
        }
        return Processor::R::blocked;
      } else {
        return Processor::R::finished;
      }
  }
  if (debugConditions) {
    Serial.println("Exec");
  }
  // proceed with the command effect
  switch (ctrl) {
    case ControlActionData::ctrlCancel:
      executor.cancelCommand(target);
      break;
    case ControlActionData::ctrlJumpIf:
      if (target < 0) {
        do {
          state.action.prev();
          target++;
        } while (target < 0);
      } else if (target > 0) {
        while (target >= 0) {
          state.action.next();
          target--;
        }
      } else {
        state.action.skip();
      }
      return Processor::R::jumped;
    case ControlActionData::ctrlExec: {
      Command c = commands[target];
      c.execute(cad.cmdKeyPress);
      break;
    }
  }
  return Processor::R::finished;
}

Processor::R ControlProcessor::pending(ExecutionState& state) {
  boolean result = evaluateCondition(state);
  if (debugConditions) {
    Serial.print("CTLRes:"); Serial.println(result);
  }
  return result ? Processor::R::finished : Processor::R::blocked;
}

void ControlProcessor::tick() {}

void ControlProcessor::clear() {}

boolean ControlProcessor::cancel(const ExecutionState& ac) {
  return false;
}

