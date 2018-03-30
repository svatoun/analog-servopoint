static ScheduledItem Scheduler2::work[MAX_SCHEDULED_ITEMS];
Scheduler2 scheduler;

long baseMillis = 0;

byte Scheduler2::bottom = 0;
byte Scheduler2::count = 0;

#define MILLIS_SCALE_FACTOR 50
#define MILLIS_FIXUP_THRESHOLD 15000

void Scheduler2::boot() {
  Executor::addProcessor(&scheduler);
  registerLineCommand("WAT", &waitCommand);
  registerLineCommand("WAC", &waitConditionCommand);
  Action::registerDumper(Instr::wait, &printWaitTime);
}

void printWaitTime(const Action& a, String& s) {
  a.asWaitAction().print(s);
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
void WaitActionData::print(String& s) {
  switch (waitType) {
    case wait:
      s += F("WAT:");
      s += waitTime * 50;
      break;
    default:
      s += F("WAT:err");
      break;
  }
}

bool Scheduler2::schedule(unsigned int timeout, ScheduledProcessor* callback, unsigned int data) {
  if (count >= MAX_SCHEDULED_ITEMS) {
    return false;
  }
  if (callback == NULL) {
    return false;
  }
  ScheduledItem* item = work + bottom;
  if (count == 0) {
    baseMillis = currentMillis;
  }
  long x = (currentMillis - baseMillis) / MILLIS_SCALE_FACTOR;
  x += timeout;
  if (count > 0) {
    ScheduledItem* maxItem = work + count;
    while (item < maxItem) {
      if (item->timeout > x) {
        memmove(item + 1, item, (maxItem - item - 1) * sizeof(ScheduledItem));
        break;
      }
      item++;
    }
  }
  ScheduledItem nItem(x, callback, data);
  if (x > 65535) {
    return;
  }
  int id = item - work;
  *item = nItem;
  count++;
  item = work;
  for (int i = 0; i < count; i++) {
    item++;
  }
  return true;
}

void Scheduler2::timeout(unsigned int data) {
  ExecutionState& s = (ExecutionState&)data;
  Executor::finishAction2(s);
}

void Scheduler2::schedulerTick() {
  if (count == 0) {
    return;
  }
  int delta = (currentMillis - baseMillis) / MILLIS_SCALE_FACTOR;

  ScheduledItem* item = work;
  signed char idx;
  for (idx = 0; idx < count; idx++) {
    if (item->timeout > delta) {
      break;
    }
    item++;
  }
  if (idx == 0) {
    // check if baseMillis is not TOO low
    if (delta > MILLIS_FIXUP_THRESHOLD) {
      while (item >= work) {
        item->timeout -= delta;
        item--;
      }
      baseMillis = currentMillis;
    }
    return;
  }
  bottom = idx;

  ScheduledItem *expired = work;
  for (signed char x = 0; x < idx; x++) {
    ScheduledProcessor* cb = expired->callback;
    if (cb != NULL) {
      cb->timeout(expired->data);
    }
    expired++;
  }
  count -= idx;
  memmove(work, expired, count * sizeof(ScheduledItem));
  bottom = 0;
}

void Scheduler2::cancel(ScheduledProcessor* callback, unsigned int data) {
  ScheduledItem* item = work;
  for (int id = 0; id < count; id++) {
      if ((item->callback == callback) && (item->data == data)) {
        int l = count - id - 1;
        memmove(item, item + 1, l * sizeof(ScheduledItem));
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
      schedule(this, wad.computeDelay(), &s);
      break;
  }
}

Processor::R Scheduler2::processAction(const Action& a, int handle) {
  return Processor::R::ignored;
}

void waitCommand(String& s) {
  Action action;
  action.command = Instr::wait;
  WaitActionData& wad = (WaitActionData&)action;
  char c = s.charAt(0);
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
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
        int n = nextNumber(s);
        if (n <= 0) {
          Serial.print(F("Bad timeout"));
          return;
        }
        wad.waitType = WaitActionData::wait;
        wad.waitTime = n / 50;
        break;
    }
  }
  prepareCommand();
  addCommandPart(action);
}

void waitConditionCommand(String& s) {
  
}


