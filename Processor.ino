
byte  Output::lastOutputState[OUTPUT_BIT_SIZE];
byte  Output::newOutputState[OUTPUT_BIT_SIZE];
byte activeOutputs[OUTPUT_BIT_SIZE];
byte outputActiveTrigger[OUTPUT_BIT_SIZE];

static Processor*  Executor::processors[MAX_PROCESSORS];

    /**
       Queue of commands to be executed. Actually pointers to (immutable) action chains.
    */
static ExecutionState Executor::queue[QUEUE_SIZE];

void bootProcessors() {
  output.boot();
  executor.boot();
  scheduler.boot();

  for (int i = 0; i < sizeof(OUTPUT_BIT_SIZE); i++) {
    activeOutputs[i] = 0;
    outputActiveTrigger[i] = 0;
  }
}

void primeOutputChange(byte k) {
  if (k >= OUTPUT_BIT_SIZE) {
    return;
  }
  bitfieldWrite(outputActiveTrigger, k, true);
}

bool isOutputActive(byte k) {
  if (k >= OUTPUT_BIT_SIZE) {
    return false;
  }
  return bitfieldRead(activeOutputs, k);
}

char getOutputActive(byte k) {
  if (k >= OUTPUT_BIT_SIZE) {
    return - 1;
  }
  if (bitfieldRead(outputActiveTrigger, k)) {
    return -1;
  }
  return bitfieldRead(activeOutputs, k) ? 1 : 0;
}

boolean setOutputActive(byte k, boolean s) {
  if (k >= OUTPUT_BIT_SIZE) {
    return;
  }
  bitfieldWrite(outputActiveTrigger, k, false);
  bitfieldWrite(activeOutputs, k, s);
}


Output::Output() {
  for (byte *os = newOutputState, *los = lastOutputState, *act = activeOutputs; los < (lastOutputState + OUTPUT_BIT_SIZE); os++, los++, act++) {
    *act = *os = *los = 0;
  }
}

void Output::boot() {
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
//  pinMode(shiftOutDisable, OUTPUT);
  
  digitalWrite(latchPin, LOW);
  digitalWrite(clockPin, LOW);
  
  flush();
//  digitalWrite(shiftOutDisable, LOW);
}



void Output::set(int index) {
  if (debugOutput) {
    Serial.print(F("Set output: ")); Serial.println(index);
  }
  newOutputState[index / 8] |= 1 << (index % 8);
}

void Output::clear(int index) {
  if (debugOutput) {
    Serial.print(F("Clear output: ")); Serial.println(index);
  }
  newOutputState[index / 8] &= ~(1 << (index % 8));
}

void Output::clearAll() {
  for (byte *os = newOutputState; os < (newOutputState + OUTPUT_BIT_SIZE); os++) {
    *os = 0;
  }
}

void Output::flush() {
  if (debugOutput) {
    Serial.print(F("Propagating output changes: "));
  }
  pinMode(clockPin, OUTPUT);
  digitalWrite(latchPin, LOW);
  for (int i = 0; i < OUTPUT_BIT_SIZE && i < numberOfLatches; i++) {
    shiftOut(dataPin, clockPin, MSBFIRST, lastOutputState[i]); 
    if (debugOutput) {
      Serial.print(lastOutputState[i], HEX); Serial.print(F(" "));
    }
  }
  if (debugOutput) {
    Serial.println(F(""));
  }
  digitalWrite(latchPin, HIGH);
  delay(1);
  digitalWrite(latchPin, LOW);
}

void Output::commit() {
  boolean change = false;
  
  for (int i = 0; i < OUTPUT_BIT_SIZE; i++) {
    if (newOutputState[i] != lastOutputState[i]) {
      lastOutputState[i] = newOutputState[i];
      change = true;
    }
  }

  if (!change) {
    return;
  }
  flush();
}

bool Output::isSet(int index) {
  return lastOutputState[index / 8] & (1 << (index % 8));
}

boolean Output::isActive(int index) {
  return activeOutputs[index / 8] & (1 << (index % 8));
}
//
////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////

Executor::Executor() {
  for (Processor **p = processors; p < (processors + MAX_PROCESSORS); p++) {
    *p = NULL;
  }
}

void Executor::boot() {
  clear(false);
}

void Executor::addProcessor(Processor* p) {
  for (Processor **x = processors; x < (processors + MAX_PROCESSORS); x++) {
    if (*x == NULL) {
      *x = p;
      break;
    }
  }
}

boolean Executor::isRunning(byte id) {
  for (int i = 0; i < QUEUE_SIZE; i++) {
    ExecutionState& q = queue[i];
    if (q.action.isEmpty()) {
       continue;
     }
     if (q.id == id) {
      return true;
     }
  }
  return false;
}

boolean Executor::cancelCommand(int id) {
  for (int i = 0; i < QUEUE_SIZE; i++) {
    ExecutionState& q = queue[i];
    if (q.action.isEmpty()) {
       continue;
     }
     if (q.id == id) {
      if (q.processor != NULL) {
        q.processor->cancel(q);
      }
      q.clear();
      return true;
     }
  }
  return false;
}

void ExecutionState::clear() {
  if (id < MAX_COMMANDS) {
    if (debugExecutor) {
      Serial.print("ExClr:"); Serial.println(id);
    }
    setCommandActive(id, false);
  }
  processor = NULL;
  blocked = false;
  action.clear();
}

void Executor::clear(boolean interactive) {
  if (debugExecutor) {
    Serial.println(F("ExecQ clean")); 
  }
  for (int i = 0; i < MAX_PROCESSORS; i++) {
    Processor* p = processors[i];
    if (p != NULL) {
      if (debugExecutor) {
        Serial.print('#'); Serial.println((int)p, HEX);
      }
      p->clear();
    }
  }
  for (int i = 0; i < QUEUE_SIZE; i++) {
    if (interactive && queue[i].id < MAX_COMMANDS) {
      continue;
    }
    queue[i].clear();
  }
}

void Executor::handleWait(Action* a) {
  
}

void Executor::schedule(const ActionRef& ref, byte id, boolean invert) {
  schedule(ref, id, invert, true);  
}

void Executor::schedule(const ActionRef& ref, byte id, boolean invert, boolean wait) {
  if (debugExecutor) {
    Serial.print(F("ExAction: ")); 
    initPrintBuffer();
    ref.print(printBuffer);
    Serial.print(F("Ref: ")); Serial.println(printBuffer);
  }  
  for (ExecutionState* st = queue; st < (queue + QUEUE_SIZE); st++) {
    ExecutionState& q = *st;
    if (q.isAvailable()) {
      q.action = ref;
      q.id = id;
      q.blocked = false;
      q.waitNext = q.wait = wait;
      q.invert = invert;

      if (id < MAX_COMMANDS) {
        setCommandActive(id, true);
      }
      if (debugExecutor) {
        Serial.print(F("ExSched: ")); printQ(q); //Serial.print((st - queue)); Serial.print(F(" ")); Serial.println((int)&q.action, HEX);
      }
      return;
    }
  }
}

bool Executor::isBlocked(int index) {
  return (index < 0) || (index >= QUEUE_SIZE) || queue[index].blocked;
}

void Executor::finishAction(const Action* action, int index) {
  if (index < 0 || index >= QUEUE_SIZE) {
    return;
  }
  finishAction2(queue[index]);
}

void Executor::printQ(const ExecutionState& q) {
  int i = &q - queue;
  const Action& a = q.action.a();
  Serial.print(i); Serial.print(F("@")); Serial.print((int)&q, HEX); 
  Serial.print(F(" W:")); Serial.print(q.wait); Serial.print(q.waitNext);
  Serial.print(F(" A:")); Serial.print((int)&a, HEX); Serial.print(F(" / ")); 
      Serial.print(q.action.i(), HEX); Serial.print(F(", C:")); Serial.print(a.command);
  Serial.print('-');
  initPrintBuffer();
  a.print(printBuffer);
  Serial.println(printBuffer);
}

void Executor::finishAction2(ExecutionState& q) {
  if ((&q < queue) || (&q >= (queue + QUEUE_SIZE))) {
    return;
  }
  if (debugExecutor) {
    Serial.print(F("Fin:")); Serial.println(q.action.i(), HEX);
  }
  q.blocked = false;
  q.wait = q.waitNext;
  q.action.next();
  if (debugExecutor) {
    Serial.print(F("FinQ: ")); printQ(q); //Serial.print((&q - queue)); Serial.print(F(" NA:")); Serial.println(q.action.i(), HEX);
  }
  if (q.action.isEmpty()) {
    q.clear();
  }
}

void unblock(ExecutionState& q) {
  q.blocked = false;
  q.processor = NULL;
}

void block(ExecutionState& q, Processor* proc) {
  q.blocked = true;
  q.processor = proc;
}

void Executor::process() {
  for (Processor** p = processors; p < (processors + MAX_PROCESSORS); p++) {
    if (*p == NULL) {
      continue;
    }
    (*p)->tick();
  }
  for (int i = 0; i < QUEUE_SIZE; i++) {
    ExecutionState& q = queue[i];
    if (q.action.isEmpty()) {
       continue;
    }
    if (q.action.a().command == continuation) {
      q.action.next();
      continue;
    }
    boolean known = false;
    if (q.processor != NULL) {
      Processor::R result = q.processor->pending(q);
      if (result == Processor::finished) {
        if (debugExecutor) {
          Serial.print(F("Fin-P#")); printQ(q); //Serial.print(q.action.i(), HEX); Serial.print(F(" C:")); Serial.println(q.action.a().command);
        }
        unblock(q);
        q.action.next();
      }
    }
    if (q.blocked) {
      continue;
    }
    if (q.action.isEmpty()) {
       q.clear();
       continue;
    }

    const Action& a = q.action.a();
    if (debugExecutor) {
      Serial.print(F("Processing:")); 
      printQ(q);
      // Serial.print(i); Serial.print(F("@")); Serial.print((int)&q, HEX); Serial.print(F(" A:")); Serial.print((int)&a, HEX); Serial.print(F(" / ")); 
      //Serial.print(q.action.i(), HEX); Serial.print(F(", C:")); Serial.println(a.command);
    }
    // attempt to handle the action
    for (int px = 0; px < MAX_PROCESSORS; px++) {
      Processor* proc = processors[px];
      if (proc == NULL) {
        continue;
      }
      Processor::R res;

      res = proc->processAction2(q);
      if (debugExecutor) {
        Serial.print(F("EXE:")); Serial.println(res);
      }
      if (res == Processor::ignored) {
        continue;
      }
      known = true;
      if (!q.action.isEmpty()) {
        switch (res) {
          case Processor::blocked:
            block(q, proc);
            if (debugExecutor) {
              Serial.print(F("BL:")); 
              printQ(q); 
            }
            break;
          case Processor::jumped:
            unblock(q);
            break;
          case Processor::finished:
            // the 'wait' flag is not reset.
            unblock(q);
            q.action.next();
            break;
          case Processor::full:
            known = true;
            continue;
        }
      }
      break;
    }
    if (!known) {
      Serial.println(F("Discard: ")); 
      initPrintBuffer();
      q.action.a().print(printBuffer);
      Serial.println(printBuffer);
      unblock(q);
      q.action.next();
    }
    if (q.action.isEmpty()) {
      q.clear();
    }
  }
}

void Executor::playNewAction() {
  schedule(&newCommand[0], 0xff, false);  
}

