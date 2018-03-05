
ModuleChain outputModule("Output", 0, &outputModuleHandler);

class OutputProcessor : public Processor {

  public:
    OuputProcessor() {
    }
    R processAction2(ExecutionState&) override;
    R processAction(const Action& ac, int handle) {
      return Processor::ignored;
    }
    void tick() override;
    void clear() override;
    boolean cancel(const Action& ac) override;
};

OutputProcessor outputProcessor;

void OutputProcessor::tick() {
  
}

void OutputProcessor::clear() {
  
}

boolean OutputProcessor::cancel(const Action& a) {
  if (a.command == onOff) {
    OutputActionData& data = a.asOutputAction();
    output.setBit(data.output(), false);
    return true;
  }
  return false;
}

Processor::R OutputProcessor::processAction2(ExecutionState& state) {
  const Action* a = state.action.aptr();
  if (a->command != onOff) {
    return Processor::ignored;
  }

  const OutputActionData& data = a->asOutputAction();

  boolean newState;
  switch (data.fn) {
    case OutputActionData::outOn:       newState = true; break;
    case OutputActionData::outOff:      newState = false; break;
    case OutputActionData::outToggle:   newState = !output.isSet(data.outputIndex); break;
  }

  output.setBit(data.outputIndex, newState);
  return Processor::finished;
}

void printHexByte(String& buffer, int val) {
  if (val < 0x10) {
    buffer.concat('0');
  }
  buffer.concat(String(val, HEX));
}

void outputInit() {
  executor.addProcessor(&outputProcessor);
  registerLineCommand("OUT", &outputCommand);
}

void outputCommand(String& s) {
  int n = nextNumber(s) - 1;
  if (n < 0 || n >= MAX_OUTPUT) {
    Serial.print(F("Bad output number"));
    return;
  }
  Action action;
  OutputActionData& outAction = action.asOutputAction();
  
  if (s.length() == 0) {
    outAction.turnOn(n);
  } else {
    char c = s.charAt(0);
    switch (c) {
      case '+': case '1': case 'S': case 's': case 'U': case 'u':
        outAction.turnOn(n);
        break;
      case '-': case '0': case 'R': case 'r': case 'D': case 'd':DEF:
        outAction.turnOff(n);
        break;
      case '*': case 'T': case 't': 
        outAction.toggle(n);
        break;
      default:
        Serial.println(F("Bad output command"));
    }
  }
  prepareCommand();
  addCommandPart(action);
}

void outputStatus() {
  Serial.println(F("State of outputs: "));

  for (int i = 0; i < MAX_OUTPUT; i++) {
    if (i % 32 == 0) {
      Serial.print("\t");
    }
    if (i > 0) {
      if (i % 8 == 0) {
        Serial.print(' ');
      } else if (i % 4 == 0) {
        Serial.print('-');
      }
    }
    Serial.print(output.isSet(i) ? '1' : '0');
  }
  Serial.println();
}

void outputModuleHandler(ModuleCmd cmd) {
  switch (cmd) {
    case initialize:
      outputInit();
      break;
    case eepromLoad:
      break;
    case eepromSave:
      break;
    case dump:
      break;
    case status:
      outputStatus();
      break;
    case periodic:
      break;
  }
}


