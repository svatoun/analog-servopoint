
ModuleChain outputModule("Output", 0, &outputModuleHandler);

FlashConfig flashConfig[MAX_FLASH];

struct PendingFlash {
  byte  flashIdx;
  byte  execState : 5;
  byte  outputIndex : 6;
  byte  counter : 7;
  boolean active : 1;
  boolean nextInvert : 1;
    
  PendingFlash() : flashIdx(0), execState(0), outputIndex(0x1f), counter(0), active(false), nextInvert(false) {}
  PendingFlash(byte _aI, byte _aE, byte _aO, byte _aC, boolean _aN) : flashIdx(_aI), execState(_aE), outputIndex(_aO), counter(_aC),  active(false), nextInvert(_aN) {}
  void clear();
};


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
    boolean cancel(const ExecutionState& ac) override;
};

class FlashProcessor : public Processor, public ScheduledProcessor {
  static PendingFlash pendingFlashes[MAX_PENDING_FLASHES];
  static byte pendingCount;

    void clear(PendingFlash* ptr);
    void clearOutput(byte output);
  public:
    FlashProcessor() {
    }
    R processAction2(ExecutionState&) override;
    R processAction(const Action& ac, int handle) {
      return Processor::ignored;
    }
    void timeout(unsigned int data) override;
    void tick() override;
    void clear() override;
    boolean cancel(const ExecutionState& ac) override;
};

OutputProcessor outputProcessor;
FlashProcessor  flashProcessor;

PendingFlash FlashProcessor::pendingFlashes[MAX_PENDING_FLASHES];
byte FlashProcessor::pendingCount = 0;

void OutputProcessor::tick() {
  
}

void OutputProcessor::clear() {
  
}

#define OUTPUT_MASK 0x3f

void OutputProcessor::timeout(unsigned int param) {
  ExecutionState& state = (ExecutionState&)param;
  
  const Action& a = *state.action.aptr();
  const OutputActionData& data = a.asOutputAction();
  byte b = data.outputIndex;
  // pulse, low bits is the output bit
  output.setBit(b, false);
  if (state.wait) {
    Executor::finishAction2(state);
  }
}

boolean OutputProcessor::cancel(const ExecutionState& s) {
  const Action& a = s.action.a();
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
  if (fn == OutputActionData::outFlash) {
    return Processor::ignored;
  }
  
  boolean newState;
  byte outN = data.outputIndex;
  bool t = false;
  boolean invert = state.invert;
  switch (fn) {
    case OutputActionData::outPulse:   
      // invert will not toggle a pulse, but just disable the output
      if (!invert) {
        int d = data.pulseDelay();
        if (d == 0) {
          d = 6;
        }
        scheduler.schedule(d, this, &state);
        t = true;
      }
      // fall through
    case OutputActionData::outOn:       newState = !invert; break;
    case OutputActionData::outOff:      newState = state.invert; break;
    case OutputActionData::outToggle:   newState = !output.isSet(data.outputIndex);  break;
    
  }

  output.setBit(outN, newState);
  Output::setOutputActive(outN, newState);
  if (t && state.wait) {
    return Processor::blocked;
  } else {
    return Processor::finished;
  }
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
  executor.addProcessor(&flashProcessor);
  registerLineCommand("OUT", &outputCommand);
  registerLineCommand("FLS", &flashCommand);
  Action::registerDumper(servo, printOutputAction);
}

void printOutputAction(const Action& ac) {
  
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
      case '+': case '1': case 's': case 'u':
        outAction.turnOn(n);
        break;
      case '-': case '0': case 'r': case 'd':DEF:
        outAction.turnOff(n);
        break;
      case '*': case 't': 
        outAction.toggle(n);
        break;
      case 'p':
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
      case 'f': {
          s.remove(0, 2);
          int fi = nextNumber(s);
          boolean inv = false;
          if (fi < 1 || fi >= MAX_FLASH) {
            Serial.println(F("Bad index"));
            return;
          }
          if (s.length() > 0 && s.charAt(0) == 'i') {
            inv = true;
          }
          fi--;
          outAction.flash(n, fi, inv);
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

void FlashProcessor::clearOutput(byte index) {
  for (PendingFlash* ptr = pendingFlashes; ptr < pendingFlashes + MAX_PENDING_FLASHES; ptr++) {
    if (!ptr->active) {
      continue;
    }
    boolean clr = false;
    boolean next = ptr->nextInvert;
    byte i = ptr->outputIndex;
    if (i == index || (next && ((i + 1) == index))) {
      output.clear(i);
      if (next) {
        byte x = (i == index) ? i + 1 : i;
        Output::setOutputActive(x, false);
        output.clear(i + 1);  
      }
      clear(ptr);
      return;
    }
  }
}

Processor::R FlashProcessor::processAction2(ExecutionState& state) {
  const Action& a = *state.action.aptr();
  byte cmd = a.command;
  
  if (cmd != onOff) {
    return Processor::ignored;
  }

  const OutputActionData& data = a.asOutputAction();
  byte oi = data.outputIndex;
  byte fn = data.fn;
  byte nextInvert = data.nextInvert;
  if (fn != OutputActionData::outFlash) {
    // some other instruction for the output -> clear
    clearOutput(oi);
    return Processor::ignored;
  }
  Serial.println("have flash");
  if (pendingCount >= MAX_PENDING_FLASHES) {
    Serial.println("Overflow");
    return Processor::ignored;
  }
  if (state.invert) {
    // just disable:
    clearOutput(oi);
    Output::setOutputActive(oi, false);
    Serial.println("Inverted");
    return Processor::finished;
  }
  byte f = data.pulseDelay();
  Serial.println(f);
  if (f >= MAX_FLASH) {
    Serial.println("Bad Flash");
    return Processor::finished;
  }
  const FlashConfig fc = flashConfig[f];

  byte c = fc.flashCount;
  byte stIndex;
  byte d = fc.onDelay;
  if (d == 0) {
    Serial.println("0 delay");
    return Processor::finished;
  }
  if (c > 0 && state.wait) {
    stIndex = Executor::stateNo(state);
  } else {
    stIndex = 0;
  }

  if (debugFlash) {
    Serial.print("ExeFlash:"); Serial.print(oi + 1); Serial.print(':'); Serial.print(f + 1); 
    if (data.nextInvert) {
      Serial.print('I');
    }
    Serial.println();
    String s;
    fc.print(s, f + 1);
    Serial.println(s);
  }

  PendingFlash cfg;
  
  cfg.counter = fc.flashCount;
  cfg.flashIdx = f;
  cfg.execState = stIndex;
  cfg.outputIndex = oi;
  cfg.nextInvert = nextInvert;
  cfg.active = true;
  for (PendingFlash* ptr = pendingFlashes; ptr < pendingFlashes + MAX_PENDING_FLASHES; ptr++) {
    if (!ptr->active) {
      *ptr = cfg;
      if (debugFlash) {
        Serial.println("ExecFlash@"); Serial.println((int)ptr, HEX);
      }
      output.set(oi);
      if (cfg.nextInvert) {
        output.clear(oi + 1);  
      }
      Output::setOutputActive(oi, true);
      scheduler.schedule(d, this, (unsigned int)ptr);

      return stIndex == 0 ? Processor::finished : Processor::blocked;
    }
  }
  return Processor::ignored;
}

void FlashProcessor::clear(PendingFlash* ptr) {
  if (!ptr->active) {
    return;
  }
  scheduler.cancel(this, (unsigned int)ptr);
  byte oi = ptr->outputIndex;
  output.clear(oi);
  if (ptr->nextInvert) {
    output.clear(oi + 1);
  }
  Output::setOutputActive(oi, false);
  ExecutionState *st = Executor::state(ptr->execState);
  if (st != NULL) {
    Executor::finishAction2(*st);
  }
  ptr->clear();
  pendingCount--;
}

void FlashProcessor::clear() {
  for (PendingFlash* ptr = pendingFlashes; ptr < pendingFlashes + MAX_PENDING_FLASHES; ptr++) {
    clear(ptr);
  }
  pendingCount = 0;
}

void FlashProcessor::tick() {}

boolean FlashProcessor::cancel(const ExecutionState& state) {
  byte x = Executor::stateNo(state);
  for (PendingFlash* ptr = pendingFlashes; ptr < pendingFlashes + MAX_PENDING_FLASHES; ptr++) {
    if (ptr->active && ptr->execState == x) {
      clear(ptr);
      break;
    }
  }
}

void FlashProcessor::timeout(unsigned int data) {
  PendingFlash* ptr = (PendingFlash*)data;
  PendingFlash& f = *ptr;
  if (debugFlash) {
    Serial.print("Flash@"); Serial.print(data, HEX);
  }
  if (f.counter > 0) {
    if (--f.counter == 0) {
      if (debugFlash) {
        Serial.print(" End");
      }
      clear(ptr);
      return;
    }
  }
  
  byte fi = ptr->flashIdx;
  if (fi >= MAX_FLASH) {
    clear(ptr);
    return;
  }
  const FlashConfig& c = flashConfig[fi];
  byte oi = ptr->outputIndex;
  boolean ns = output.isSet(oi);
  output.setBit(oi, !ns);
  if (ptr->nextInvert) {
    output.setBit(oi + 1, ns);
  }
  int d = ns ? c.offDelay : c.onDelay;
  if (debugFlash) {
    Serial.print(':');
    Serial.print('O'); Serial.print(oi); 
    Serial.print(':'); Serial.print(ns);
    Serial.print(':'); Serial.println(d);
  }
  scheduler.schedule(d, this, (unsigned int)ptr);
}

void PendingFlash::clear() {
  outputIndex = 0xff;
  execState = 0;
  active = false;
}

void loadFlashConfig() {
  int idx = 0;
  for(FlashConfig* p = flashConfig; p < (flashConfig  + MAX_FLASH); p++, idx++) {
    p->load(idx);
  }
}

void saveFlashConfig() {
  int idx = 0;
  for(FlashConfig* p = flashConfig; p < (flashConfig  + MAX_FLASH); p++, idx++) {
    p->save(idx);
  }
}

void clearFlashConfig() {
  FlashConfig empty;
  for(FlashConfig* p = flashConfig; p < (flashConfig  + MAX_FLASH); p++) {
    *p = empty;
  }
}

void dumpFlashConfig() {
  String s;
  byte index = 1;
  for(FlashConfig* p = flashConfig; p < (flashConfig  + MAX_FLASH); p++, index++) {
    if (!p->empty()) {
      s = "";
      p->print(s, index);
      Serial.println(s);
    }
  }
}

void FlashConfig::save(int idx) {
  EEPROM.put(eeaddr_flashTable + idx * sizeof(FlashConfig), *this);
}

void FlashConfig::load(int idx) {
  EEPROM.get(eeaddr_flashTable + idx * sizeof(FlashConfig), *this);
}

void FlashConfig::print(String& s, int index) {
  s += F("FLS:"); s.concat(index); s.concat(':');
  s.concat(onDelay); s.concat(':'); s.concat(offDelay); s.concat(':');
  s.concat(flashCount);
}

void flashCommand(String& s) {
  int fl = nextNumber(s);
  if (fl < 1 || fl > MAX_FLASH) {
    Serial.println(F("Bad index"));
  }
  int onD = nextNumber(s);
  int offD = nextNumber(s);
  if (onD == 0 || offD == 0 || onD >= 200 || offD > 200) {
    Serial.println(F("Bad delay"));
    return;
  }
  int cnt = nextNumber(s);
  if (cnt > 200) {
    Serial.println(F("Bad count"));
    return;
  }
  FlashConfig& cfg = flashConfig[fl - 1];
  cfg.onDelay = onD;
  cfg.offDelay = offD;
  cfg.flashCount = cnt;

  String x;
  cfg.print(x, &cfg - flashConfig);
  Serial.println(x);
}

void outputModuleHandler(ModuleCmd cmd) {
  switch (cmd) {
    case initialize:
      outputInit();
      loadFlashConfig();
      break;
    case eepromLoad:
      loadFlashConfig();
      break;
    case eepromSave:
      saveFlashConfig();
      break;
    case dump:
      dumpFlashConfig();
      break;
    case status:
      outputStatus();
      break;
    case periodic:
      break;
    case reset:
      clearFlashConfig();
      break;
  }
}


