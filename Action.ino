
// Action  Action::actionTable[MAX_ACTIONS];
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

void Action::print(char* out) {
  dumper_t d = dumpers[command];
  if (d == NULL) {
    strcat_P(out, PSTR("UNK:"));
    out += strlen(out);
    out = printNumber(out, command, 10);
    out = printNumber(append(out, ':'), data, 16);
  } else {
    d(*this, out);
  }
}

Action& Action::get(int index, Action& target) {
  if (index < 0 || index >= MAX_ACTIONS) {
    target = noAction;
    return target;
  }
  target.load(index);
  return target;
}

void Action::initialize() {
#ifdef RENUMBER_ACTIONS
  for (ActionRenumberFunc* ptr = renumberCallbacks; ptr < (renumberCallbacks + sizeof(renumberCallbacks) / sizeof(renumberCallbacks[0])); ptr++) {
    *ptr = NULL;
  }
#endif
}

void clearActions() {
  for (int i = 0; i < MAX_ACTIONS; i++) {
    noAction.save(i);
  }
}



void Action::renumberCallback(ActionRenumberFunc f) {
#ifdef RENUMBER_ACTIONS
  for (ActionRenumberFunc* ptr = renumberCallbacks; ptr < (renumberCallbacks + sizeof(renumberCallbacks) / sizeof(renumberCallbacks[0])); ptr++) {
    if (*ptr == NULL) {
      *ptr = f;
      return;
    }
  }
#endif
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
    ActionRef ref(from);
    from++;
    if (i == s - 1) {
      ref.makeLast();
    }
    ref.saveTo(x);
  }
  return p;
}

int Action::findSpace(int size) {
  int index;
  int head = -1;
  for (index = 0; index < MAX_ACTIONS; index++) {
    ActionRef ref(index);
    if (!ref.isEmpty()) {
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
  Serial.println(F("Serial table overflow, trying to compact"));

#ifdef RENUMBER_ACTIONS
  // no room, so compact / renumber
  int compactIndex = 0;
  boolean firstNonEmpty = true;
  for (index = 0; index < MAX_ACTIONS; index++) {
    ActionRef ref(index);
    
    if (ref.isEmpty()) {
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

    ref.saveTo(compactIndex);
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
#else // !RENUMBER_ACTIONS
  return -1;
#endif
}

void Action::freeAll() {
  /*
  for (int i = 0; i < MAX_ACTIONS; i++) {
    actionTable[i].free();
  }
  */
}

ActionRef Action::getRef(byte index) {
  ActionRef x(index);
  x.a().load(index);
  return x;
}

bool ActionRef::isEmpty() {
  if (ptr != NULL) {
    return ptr->isEmpty();
  }
  if (index == noIndex) {
    return true;
  }
  return ((index >= MAX_ACTIONS) || current.isEmpty());
}

void ActionRef::clear() {
  index = noIndex;
  current = noAction;
  ptr = NULL;
}

const Action& ActionRef::skip() {
  if (!isEmpty()) {
    return current;
  }
  bool l;
  do {
    l = isLast();
    next();
  } while(!l);
  return current;
}

const Action& ActionRef::prev() {
  if (index == noIndex) {
    if (!current.isEmpty()) {
      clear();
    }
    return current;    
  } 
  if (ptr != NULL) {
    return current;
  } else if (index > 0) {
    index--;
    current.load(index);
  }
  return current;
}

const Action& ActionRef::next() {
  if (index == noIndex) {
    if (!current.isEmpty()) {
      clear();
    }
    return current;    
  } 
  
  if (isLast()) {
    clear();
    return current;
  }
  if (ptr != NULL) {
    ptr++;
    current = *ptr;
  } else {
    index++;
    current.load(index);
  }
  return current;
}

void ActionRef::free() {
  bool l;
  do {
    l = isLast();
    if (debugCommands) {
      Serial.print(F("Free action #")); Serial.println(i(), HEX);
    }
    if (index == noIndex) {
      clear();
      return;
    }
    if (ptr != NULL) {
      // use pointer
      ptr->init();
    } else {
      // save into EEPROM
      noAction.save(index);
      if (index == MAX_ACTIONS - 1) {
        break;
      }
    }
    next();
  } while (!l && !isEmpty());
  clear();
}

void ActionRef::save() {
  if (ptr != NULL) {
    return;
  }
  if (index >= MAX_ACTIONS) {
    return;
  }
  current.save(index);
}

void ActionRef::saveTo(int pos) {
  if (pos >= MAX_ACTIONS) {
    return;
  }
  current.save(pos);
  index = pos;
  ptr = NULL;
}

void ActionRef::print(char* out) {
  strcat_P(out, PSTR("i:"));
  out += strlen(out);
  out = printNumber(out, index, 10);
  strcat_P(out, PSTR("ptr:"));
  out += strlen(out);
  out = printNumber(out, (int)ptr, 16);
  append(out, ' ');
  a().print(out);
}

void Action::load(int index) {
  int eeaddr = eeaddr_actionTable + 1 + index * sizeof(Action);
  EEPROM.get(eeaddr, *this);
}

void Action::save(int index) {
  int eeaddr = eeaddr_actionTable + 1 + index * sizeof(Action);
  EEPROM.put(eeaddr, *this);
}

