

Action  Action::actionTable[MAX_ACTIONS];
ActionRenumberFunc Action::renumberCallbacks[5];
dumper_t Action::dumpers[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

int Action::top = 0;

Action::Action() {
  last = true;
  command = none;
  data = 0;
}

void Action::registerDumper(Instr cmd, dumper_t dumper) {
  dumpers[cmd] = dumper;
}

void Action::print(String& s) {
  dumper_t d = dumpers[command];
  if (d == NULL) {
    s.concat(F("UNK:")); s.concat(command); s.concat(F(":")); s.concat(String(data, HEX));
  } else {
    d(*this, s);
  }
}

Action* Action::get(int index) {
  if (index < 0 || index >= MAX_ACTIONS) {
    return &actionTable[0];
  }
  return &actionTable[index];
}

Action::Action(const Action& other) {
  last = other.last;
  command = other.command;
  data = other.data;
}

void Action::initialize() {
  for (int cbIndex = 0; cbIndex < sizeof(renumberCallbacks) / sizeof(renumberCallbacks[0]); cbIndex++) {
    renumberCallbacks[cbIndex] = NULL;
  }
}

const Action* Action::next() {
  if (isLast()) {
    return NULL;
  } else {
    return this + 1;
  }
}

void Action::renumberCallback(ActionRenumberFunc f) {
  for (int i = 0; i < sizeof(renumberCallbacks) / sizeof(renumberCallbacks[0]); i++) {
    if (renumberCallbacks[i] == NULL) {
      renumberCallbacks[i] = f;
      return;
    }
  }
}

int Action::copy(const Action* from, int s) {
  if (s == 0) {
    return -1;
  }
  int p = findSpace(s);
  if (debugCommands) {
    Serial.print(F("Free space at ")); Serial.println(p);
  }
  if (p == -1) {
    return -1;
  }
  int x = p;
  for (int i = 0; i < s; i++, x++) {
    actionTable[x] = *from;
    from++;
  }
  actionTable[x - 1].makeLast();
  return p;
}

int Action::findSpace(int size) {
  int index;
  int head = -1;
  for (index = 0; index < MAX_ACTIONS; index++) {
    if (!actionTable[index].isEmpty()) {
      head = -1;
      continue;
    }
    if (head == -1) {
      head = index;
    }
    if (index - head + 1 >= size) {
      return head;
    }
  }
  if (debugCommands) {
    Serial.println(F("Serial table overflow, trying to compact"));
  }
  // no room, so compact / renumber
  int compactIndex = 0;
  boolean firstNonEmpty = true;
  for (index = 0; index < MAX_ACTIONS; index++) {
    if (actionTable[index].isEmpty()) {
      if (debugCommands) {
        Serial.println(F("Free slot #")); Serial.println(index);
      }
      firstNonEmpty = true;
      continue;
    }
    if (compactIndex == index) {
      compactIndex++;
      continue;
    }

    actionTable[compactIndex] = actionTable[index];
    if (firstNonEmpty) {
      if (debugCommands) {
        Serial.print(F("Moving action #")); Serial.print(index); Serial.print(F(" to #")); Serial.println(compactIndex);
      }
      for (int cbIndex = 0; cbIndex < sizeof(renumberCallbacks) / sizeof(renumberCallbacks[0]); cbIndex++) {
        ActionRenumberFunc callback = renumberCallbacks[cbIndex];
        if (callback == NULL) {
          break;
        }
        if (debugCommands) {
          Serial.print(F("Adjusting callback #")); Serial.print(cbIndex); Serial.print(F(", addr=")); Serial.println((int)callback, HEX);
        }
        callback(index, compactIndex);
      }
    }
    compactIndex++;
  }
  if (debugCommands) {
    Serial.print(F("Compaction finished, top is ")); Serial.println(compactIndex);
  }
  top = compactIndex;
  if (compactIndex + size <= MAX_ACTIONS) {
    return compactIndex;
  } else {
    return -1;
  }
}

void Action::freeAll() {
  for (int i = 0; i < MAX_ACTIONS; i++) {
    actionTable[i].free();
  }
}

void Action::free() {
  bool l;
  Action* a = this;
  do {
    l = a->isLast();
    if (debugCommands) {
      Serial.print(F("Free action at ")); Serial.println((int)a, HEX);
    }
    a->init();
    a++;
  } while (!l && ((a - actionTable) < MAX_ACTIONS));
}


