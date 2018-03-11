static ScheduledItem Scheduler2::work[MAX_SCHEDULED_ITEMS];
Scheduler2 scheduler;

long baseMillis = 0;

byte Scheduler2::bottom = 0;
byte Scheduler2::count = 0;

#define MILLIS_SCALE_FACTOR 50
#define MILLIS_FIXUP_THRESHOLD 15000

void Scheduler2::boot() {
  Executor::addProcessor(&scheduler);
}

bool Scheduler2::schedule(unsigned int timeout, ScheduledProcessor* callback, unsigned int data) {
  if (count >= MAX_SCHEDULED_ITEMS) {
    return false;
  }
  if (callback == NULL) {
    return false;
  }
  Serial.print("Schedule, timeout "); Serial.print(timeout);
  ScheduledItem* item = work + bottom;
  if (count == 0) {
    baseMillis = currentMillis;
  }
  long x = (currentMillis - baseMillis) / MILLIS_SCALE_FACTOR;
  x += timeout;
  Serial.print("End time:"); Serial.println(x);
  if (count > 0) {
    ScheduledItem* maxItem = work + count;
    while (item < maxItem) {
      Serial.print(item->timeout);
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
  Serial.print("Scheduling callback "); Serial.print((int)callback, HEX); Serial.print(" at "); Serial.print(id); Serial.print(" data "); Serial.println(data, HEX); 
  *item = nItem;
  count++;
  Serial.print("Count:"); Serial.println(count); 
  item = work;
  for (int i = 0; i < count; i++) {
    Serial.println(item->timeout);
    item++;
  }
  return true;
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
//    Serial.println("No item fired");     
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

  Serial.print("Expired bottom:"); Serial.println(bottom); 

  ScheduledItem *expired = work;
  for (signed char x = 0; x < idx; x++) {
    ScheduledProcessor* cb = expired->callback;
    if (cb != NULL) {
      Serial.print("Calling callback "); Serial.print((int)cb, HEX); Serial.print(" at "); Serial.print(x);
      Serial.print(" data "); Serial.println(expired->data, HEX); 
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

bool Scheduler2::cancel(const Action& a) {
  return Processor::R::ignored;
}

Processor::R Scheduler2::processAction2(ExecutionState& s) {
  return Processor::R::ignored;
}

Processor::R Scheduler2::processAction(const Action& a, int handle) {
  return Processor::R::ignored;
}

