
Output::Output() {
  for (int i = 0; i < OUTPUT_BIT_SIZE; i++) {
    lastOutputState[i] = newOutputState[i] = 0;
  }
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
  for (int i = 0; i < OUTPUT_BIT_SIZE; i++) {
    newOutputState[i] = 0;
  }
}

void Output::flush() {
  if (debugOutput) {
    Serial.print(F("Propagating output changes: "));
  }
  digitalWrite(latchPin, LOW);
  for (int i = 0; i < MAX_OUTPUT && i < numberOfLatches; i++) {
    shiftOut(dataPin, clockPin, MSBFIRST, lastOutputState[i]); 
    if (debugOutput) {
      Serial.print(lastOutputState[i], HEX); Serial.print(F(" "));
    }
  }
  if (debugOutput) {
    Serial.println(F(""));
  }
  digitalWrite(latchPin, HIGH);
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

//
////////////////////////////////////////////////////////////////////////////
int WaitActionData::computeDelay() {
  if (waitTime <= 60) {
    return waitTime;
  } else if (waitTime <= (60 + 300)) {
    return (waitTime - 60) * 10;  
  } else {
    return (waitTime - (60 + 300)) * (60 * 10);
  }
}

//
////////////////////////////////////////////////////////////////////////////

Executor::Executor() {
  for (int i = 0; i < MAX_PROCESSORS; i++) {
    processors[i] = NULL;
  }
  clear();
}

void Executor::addProcessor(Processor* p) {
  for (int i = 0; i < MAX_PROCESSORS; i++) {
    if (processors[i] == NULL) {
      processors[i] = p;
      break;
    }
  }
}

void Executor::clear() {
  if (debugExecutor) {
    Serial.println(F("Cleaning execution queue ")); 
  }
  for (int i = 0; i < MAX_PROCESSORS; i++) {
    Processor* p = processors[i];
    if (p != NULL) {
      if (debugExecutor) {
        Serial.print(F("Clean executor ")); Serial.println((int)p, HEX);
      }
      p->clear();
    }
  }
  for (int i = 0; i < QUEUE_SIZE; i++) {
    queue[i].clear();
  }
//  for (int i = 0; i < MAX_WAIT_COUNT; i++) {
//    actionTimeout[i] = 0;
//  }
//  timeoutCount = 0;
}

void Executor::handleWait(Action* a) {
  
}

void Executor::schedule(const Action* ptr) {
  schedule(ptr, 0);  
}

void Executor::schedule(const Action* ptr, int id) {
  if (debugExecutor) {
    Serial.print(F("Scheduling action: ")); Serial.println((int)ptr, HEX);
  }
  for (int i = 0; i < QUEUE_SIZE; i++) {
    if (queue[i].action == NULL) {
      queue[i].action = ptr;
      queue[i].id = id;
      queue[i].blocked = false;
      if (debugExecutor) {
        Serial.print(F("Scheduled at: ")); Serial.print(i); Serial.print(F(" ")); Serial.println((int)&queue[i].action, HEX);
      }
      return;
    }
  }
}

bool Executor::isBlocked(int index) {
  return (index < 0) || (index >= QUEUE_SIZE) || queue[index].blocked;
}

void Executor::blockAction(int index) {
  if (index < 0 || index > QUEUE_SIZE) {
    return;
  }
  queue[index].blocked = true;
}

void Executor::finishAction(const Action* action, int index) {
  if (debugExecutor) {
    Serial.print(F("Finishing :")); Serial.println(index);
  }
  if (index < 0 || index >= QUEUE_SIZE) {
    return;
  }
  ExecutionState& q = queue[index];
  if (debugExecutor) {
    Serial.print(F("Action to finish:")); Serial.println((int)action, HEX);
    Serial.print(F("Action in state:")); Serial.println((int)q.action, HEX);
  }
  q.blocked = false;
  if (q.action == NULL || q.action != action) {
    return;
  }
  if (debugExecutor) {
    Serial.print(F("current Action: ")); Serial.println((int)q.action, HEX);
  }
  Action *a = q.action;
  if (a != NULL) {
    q.action = a->next();
  }
  if (debugExecutor) {
    Serial.print(F("Finished action: ")); Serial.print(index); Serial.print(F(", nextAction: ")); Serial.println((int)q.action, HEX);
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
  for (int px = 0; px < MAX_PROCESSORS; px++) {
    if (processors[px] == NULL) {
      continue;
    }
    processors[px]->tick();
  }
  for (int i = 0; i < QUEUE_SIZE; i++) {
    ExecutionState& q = queue[i];
    if (q.action == NULL) {
       continue;
     }
    if (q.processor != NULL) {
      Processor::R result = q.processor->pending(*q.action, &q.data);
      if (result == Processor::finished) {
        unblock(q);
        q.action = q.action->next();
      }
    }
    if (q.blocked) {
      continue;
    }

    // attempt to handle the action
    for (int px = 0; px < MAX_PROCESSORS; px++) {
      Processor* proc = processors[px];
      if (proc == NULL) {
        continue;
      }
      if (debugExecutor) {
        Serial.print("Processing action at "); Serial.print((int)queue[i].action, HEX); Serial.print(", command = "); Serial.println(queue[i].action->command);
      }
      Processor::R res;

      res = proc->processAction2(q);
      if (res == Processor::ignored) {
          res = proc->processAction(*q.action, i);
      }
      if (res == Processor::ignored) {
        continue;
      }
      if (debugExecutor) {
        Serial.print(F("Action ")); Serial.print(i); Serial.print(" : "); Serial.println(res);
      }
      if (q.action != NULL) {
        switch (res) {
          case Processor::blocked:
            block(q, proc);
            break;
          case Processor::finished:
            queue[i].action = queue[i].action->next();
            break;
        }
      }
      break;
    }
  }
}

void Executor::playNewAction() {
  schedule(&newCommand[0]);  
}

