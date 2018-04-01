Scheduler2 scheduler;

long baseMillis = 0;

static ScheduledItem Scheduler2::work[MAX_SCHEDULED_ITEMS];
byte Scheduler2::scheduledBottom = 0;
byte Scheduler2::scheduledCount = 0;

#define MILLIS_FIXUP_THRESHOLD 15000

void Scheduler2::boot() {
  Executor::addProcessor(&scheduler);
  registerLineCommand("WAT", &waitCommand);
//  registerLineCommand("WAC", &waitConditionCommand);
  Action::registerDumper(Instr::wait, &printWaitTime);
}

void printWaitTime(const Action& a, char* out) {
  const WaitActionData& wa = a.asWaitAction();
  wa.print(out);
}

/*
  enum {
    indefinitely = 0,
    finishOnce = 1,
    alwaysFinish = 2,
    noWait,
    inputOn,
    inputOff,
    commandInactive,
    wait,
    
    timerError
  };
  */
void WaitActionData::print(char* out) {
  switch (waitType) {
    case wait:
      strcat_P(out, PSTR("WAT:"));
      out += strlen(out);
      printNumber(out, waitTime, 10);
      break;
    default:
      strcpy_P(out, PSTR("WAT:err"));
      break;
  }
}

bool Scheduler2::schedule(unsigned int timeout, ScheduledProcessor* callback, unsigned int data) {
  if (scheduledCount >= MAX_SCHEDULED_ITEMS) {
    return false;
  }
  if (callback == NULL) {
    return false;
  }
  ScheduledItem* item = work + scheduledBottom;
  if (debugSchedule) {
    Serial.print(F("Sched-T:")); Serial.print(timeout); Serial.print(F(" CB:")); Serial.print((int)callback, HEX); Serial.print(F(" D:")); Serial.println(data, HEX);
    Serial.print(F("Bot:")); Serial.println(scheduledBottom);
  }
  if (scheduledCount == 0) {
    baseMillis = currentMillis;
    if (debugSchedule) {
      Serial.print(F("BMil:")); Serial.print(baseMillis);
    }
  }
  long x = (currentMillis - baseMillis) / MILLIS_SCALE_FACTOR;
  x += timeout;
  if (debugSchedule) {
    Serial.print(F("T:")); Serial.println(x);
  }
  if (scheduledCount > 0) {
    const ScheduledItem* maxItem = work + scheduledCount;
    while (item < maxItem) {
      if (item->timeout > x) {
        memmove(item + 1, item, (maxItem - item) * sizeof(ScheduledItem));
        break;
      }
      item++;
    }
  }
  ScheduledItem nItem(x, callback, data);
  if (x > 32767) {
    return;
  }
  int id = item - work;
  *item = nItem;
  scheduledCount++;
  item = work;
  if (debugSchedule) {
    Serial.print(F("Sched@")); Serial.println(id);
  }
  printQ();
  return true;
}

void Scheduler2::timeout(unsigned int data) {
  ExecutionState& s = (ExecutionState&)*((ExecutionState*)data);
  if (debugSchedule) {
    Serial.print(F("SchFinQ:")); Serial.println(data, HEX);
    Serial.print(F("A:")); Serial.println(s.action.i());
  }
  Executor::finishAction2(s);
}

void Scheduler2::schedulerTick() {
  if (scheduledCount == 0) {
    return;
  }
  int delta = (currentMillis - baseMillis) / MILLIS_SCALE_FACTOR;

  ScheduledItem* item = work;
  signed char idx;
  for (idx = 0; idx < scheduledCount; idx++) {
    if (item->timeout > delta) {
      break;
    }
    item++;
  }
  if (idx == 0) {
    // check if baseMillis is not TOO low
    if (delta > MILLIS_FIXUP_THRESHOLD) {
      item = work + scheduledCount - 1;
      if (debugSchedule) {
        Serial.print(F("Fixup:")); Serial.println(delta);
      }
      while (item >= work) {
        item->timeout -= delta;
        item--;
      }
      baseMillis = currentMillis;
    }
    return;
  }
  
  scheduledBottom = idx;
  if (debugSchedule) {
    Serial.print(F("Exp:")); Serial.println(idx);
  }
  const ScheduledItem* top = work + MAX_SCHEDULED_ITEMS;
  ScheduledItem *expired = work;
  for (signed char x = 0; x < idx; x++) {
    if (debugSchedule) {
      Serial.print('#'); Serial.println(x);
    }
    ScheduledProcessor* cb = expired->callback;
    if (cb != NULL) {
      cb->timeout(expired->data);
    }
    expired++;
  }
  scheduledCount = 0;
  
  // collect the non-expired at the start:
  ScheduledItem* dest = work;
  for (; expired < top; expired++) {
      if (expired->callback != NULL) {
        *dest = *expired;
        dest++;
        scheduledCount++;
      }
  }
  // erase the rest
  while (dest < top) {
    dest->callback = NULL;
    dest->timeout = 0;
    dest++;
  }
  scheduledBottom = 0;
  if (debugSchedule) {
    Serial.println("EndTick");
  }
  printQ();
}

void Scheduler2::printQ() {
  if (debugSchedule) {
    for (int i = 0; i < scheduledCount; i++) {
      const ScheduledItem& s = work[i];
      Serial.print(F("I:")); Serial.print(i); Serial.print('\t');
      Serial.print(F("T:")); Serial.print(s.timeout); Serial.print(F(" D:")); Serial.println(s.data, HEX);
    }
  }
}

void Scheduler2::cancel(ScheduledProcessor* callback, unsigned int data) {
  if (debugSchedule) {
    Serial.print(F("SchCancel:")); Serial.print((int)callback, HEX); Serial.print(':'); Serial.print(data, HEX); Serial.print(':'); Serial.println(scheduledCount);
    printQ();
  }
  ScheduledItem* item = work + scheduledBottom;
  const ScheduledItem* top = work + MAX_SCHEDULED_ITEMS;
  for (int id = 0; item < top; id++) {
      if ((item->callback == callback) && (item->data == data)) {
        item->callback = NULL;
        if (debugSchedule) {
          Serial.print(F("Del:")); Serial.print(id); Serial.print(','); Serial.print(scheduledCount); Serial.print(','); // Serial.println(c);
        }
        return;
      }
      item++;
  }
}

bool Scheduler2::cancel(const ExecutionState& s) {
  const Action& a = s.action.a();
  const WaitActionData& wad = (const WaitActionData&)a;
  switch (wad.command) {
    case WaitActionData::wait:
      cancel(this, &s);
      return true;
  }
  return false;
}

Processor::R Scheduler2::processAction2(ExecutionState& s) {
  const Action& a = s.action.a();
  if (a.command != wait) {
    return Processor::R::ignored;
  }
  if (!s.action.isPersistent() || s.invert) {
    return Processor::R::finished;
  }

  const WaitActionData& wad = (const WaitActionData&)a;
  switch (wad.waitType) {
    case WaitActionData::alwaysFinish:
      s.wait = true; s.waitNext = true;
      return Processor::R::finished;  
    case WaitActionData::noWait:
      s.wait = false; s.waitNext = false;
      return Processor::R::finished;  
    case WaitActionData::finishOnce:
      s.wait = true; s.waitNext = false;
      return Processor::R::finished;  
    case WaitActionData::wait:
      schedule(wad.waitTime, this, &s);
      return Processor::R::blocked;
      break;
  }
}

void waitCommand() {
  Action action;
  action.command = Instr::wait;
  WaitActionData& wad = (WaitActionData&)action;
  const char c = *inputPos;
  switch (c) {
    case '+':
      wad.waitType = WaitActionData::alwaysFinish;
      break;
    case '-':
      wad.waitType = WaitActionData::noWait;
      break;
    case '!':
      wad.waitType = WaitActionData::finishOnce;
      break;
    default:
      if (c >= '0' && c <= '9') {
        int n = nextNumber();
        if (n <= 0) {
          Serial.print(F("Bad timeout"));
          return;
        }
        wad.waitType = WaitActionData::wait;
        wad.waitTime = n;
        break;
      }
      Serial.print(F("Bad wait"));
      return;
  }
  prepareCommand();
  addCommandPart(action);
}

void waitConditionCommand() {
  
}


