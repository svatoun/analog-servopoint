
ModuleChain outputModule("Output", 0, &outputModuleHandler);

class OutputProcessor : public Processor, public ScheduledProcessor {

  public:
    OuputProcessor() {
    }
    R processAction2(ExecutionState&) override;
    R processAction(const Action& ac, int handle) {
      return Processor::ignored;
    }
    void timeout(unsigned int data) override;
    void tick() override;
    void clear() override;
    boolean cancel(const Action& ac) override;
};

OutputProcessor outputProcessor;

void OutputProcessor::tick() {
  
}

void OutputProcessor::clear() {
  
}

#define OUTPUT_MASK 0x3f

void OutputProcessor::timeout(unsigned int data) {
  byte kind = (data >> 8) & 0xff;
  if (kind == 0) {
    // pulse, low bits is the output bit
    int b = data & OUTPUT_MASK;
    output.setBit(b, false);
  }
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
  const Action& a = *state.action.aptr();
  byte cmd = a.command;
  
  if (cmd != onOff) {
    return Processor::ignored;
  }

  const OutputActionData& data = a.asOutputAction();
  byte fn = data.fn;
  boolean newState;
  byte outN = data.outputIndex;
  
  switch (fn) {
    case OutputActionData::outPulse:   
      {
        int d = data.pulseDelay();
        if (d == 0) {
          d = 6;
        }
        scheduler.schedule(d, this, outN & OUTPUT_MASK);
      }
      // fall through
    case OutputActionData::outOn:       newState = !state.invert; break;
    case OutputActionData::outOff:      newState = state.invert; break;
    case OutputActionData::outToggle:   newState = !output.isSet(data.outputIndex); break;
  }

  output.setBit(outN, newState);
  return Processor::finished;
}

static_assert (MAX_OUTPUT <= (OUTPUT_MASK + 1), "Maximum outputs too high");

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
      case 'P': case 'p':
        outAction.pulse(n);
        if (s.charAt(1) == ':') {
          s.remove(0, 2);
          int d = nextNumber(s);
          if (d < 1 | d > 5000) {
            Serial.println(F("Bad pulse"));
            return;
          }
          d = d / 50;
          if (d == 0) {
            d++;
          }
          outAction.setDelay(d);
        }
        break;
      default:
        Serial.println(F("Bad fn"));
        return;
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


