ModuleChain commandModule("Command", 20, &commandModuleHandler);

void commandModuleHandler(ModuleCmd cmd) {
  switch (cmd) {
    case initialize:
      initializeTables();
      commandEepromLoad();
      break;
    case eepromLoad:
      commandEepromLoad();
      break;
    case eepromSave:
      Serial.println(F("Saving actions and commands"));
//      eeBlockWrite('A', eeaddr_actionTable, &Action::actionTable, sizeof(Action::actionTable));
      commandEepromSave();
      break;
    case status:
      displayCommandStatus();
      break;
    case dump:
      dumpCommands();
      break;
    case reset:
      clearCommands();
      clearActions();
      break;
  }
}

void commandEepromSave() {
    eeBlockWrite('C', eeaddr_commandTable, &commands[0], sizeof(commands));
}

void displayCommandStatus() {
  int actionCount = 0;
  Action tmp;
  for (int i = 0; i < MAX_ACTIONS; i++) {
    if (!Action::get(i, tmp).isEmpty()) {
      actionCount++;
    }
  }
  int cmdCount = 0;
  const Command* cp;
  for (cp = commands; cp < (commands + MAX_COMMANDS); cp++) {
    if (!cp->available()) {
      cmdCount++;
    }
  }  
  Serial.print(F("Action count: ")); Serial.println(actionCount);
  Serial.print(F("Defined commands: ")); Serial.println(cmdCount);
}

void clearCommands() {
  Command* cp;
  for (cp = commands; cp < (commands + MAX_COMMANDS); cp++) {
    cp->free();
  }
  Action::freeAll();
}

void dumpCommands() {
  const Command* cp;
  for (cp = commands; cp < (commands + MAX_COMMANDS); cp++) {
    const Command&c = *cp;
    if (c.available()) {
      continue;
    }
    String s;
    c.print(s);
    Serial.println(s);
    int actionIndex = c.actionIndex;
    ActionRef ref = Action::getRef(actionIndex);
    while (!ref.isEmpty()) {
      s = "";
      ref.a().print(s);
      Serial.println(s);
      ref.next();
    }
    Serial.println("FIN");
  }
}

void commandEepromLoad() {
//    Serial.println(F("Loading actions"));
//    eeBlockRead('A', eeaddr_actionTable, &Action::actionTable, sizeof(Action::actionTable));
    Serial.println(F("Loading commands"));
    eeBlockRead('C', eeaddr_commandTable, &commands[0], sizeof(commands));
}

void initializeTables() {
  Action::initialize();
  Action::freeAll();
  const Command* cp;
  for (cp = commands; cp < (commands + MAX_COMMANDS); cp++) {
    cp->init();
  }
}  

void Command::execute(boolean keyPressed) {
  if (available()) {
    return;
  }
  int t = trigger;
  ActionRef aref = Action::getRef(actionIndex);
  boolean w = wait;
  boolean i = false;
  switch (t) {
    case cmdOn:
      if (!keyPressed) {
        return;
      }
    case cmdToggle:
      break;
    case cmdOff:
      if (keyPressed) {
        return;
      }
      break;
    case cmdOnCancel:
      if (keyPressed) {
      } else if (id != 0) {
        executor.cancelCommand(id);
        return;
      }
      break;
    case cmdOffReverts:
      if (keyPressed) {
      } else {
        i = true;
      }
      break;
    default:
      return;
  }
  executor.schedule(aref, id, i, w);
}

bool Command::processAll(int input, boolean state) {
  bool found = false;
  const Command* last = NULL;
  while (true) {
    const Command *c = Command::find(input, state, last);
    if (c == NULL) {
      if (!found) {
        Serial.println(F("Unhandled input/state"));
        return false;
      } else {
        if (debugInfra) {
          Serial.println(F("No more commands found"));
        }
      }
      return true;
    }
    found = true;
    if (debugInput) {
      Serial.print(F("Found command:" ));  Serial.print((int)c, HEX);
      Serial.print(" ");
      String s;
      c->print(s);
      Serial.println(s);
    }
    c->execute(state);
    last = c;
  }
}

int Command::findFree() {
  const Command* c = commands;
  for (int i = 0; i < MAX_COMMANDS; i++) {
    if (c->available()) {
      return i;
    }
    c++;
  }
  return -1;
}

const Command* Command::find(int input, boolean state, const Command* from) {
  if (from == NULL) {
    from = commands;
  } else {
    from++;
  }
  int i = -1;
  while (from < (commands + MAX_COMMANDS)) {
    ++i;
    const Command& c = *from;
    if (c.available()) {
      from++;
      continue;
    }
    if (c.input == input) {
        boolean ok;
        byte t = c.trigger;
        switch (t) {
            case cmdOff:
                ok = (state == false);
                break;
            case cmdOn:
            case cmdOnReverts:
                ok = state;
                break;
            case cmdToggle:
            case cmdOnCancel:
            case cmdOffReverts:
                ok = true;
                break;
            default:
                ok = false;
        }
        if (ok) {
            if (debugInput) {
              String s;
              Serial.print(F("Found: "));
              c.print(s);
              Serial.println(s);
            }
            return &c;
        }
    }
    from++;
  }
  return NULL;
}

char triggerChars[] = { 'C', 'O', 'T', 'A', 'B', 'R', 'E', 'E' };

void Command::print(String& s) {
  s += F("DEF:");
  s += (id + 1);
  s += F(":");
  s += (input + 1);
  s += F(":");
  char t = triggerChars[trigger];
  s += t;
  if (!wait) {
    s += F(":N");
  }
}

void Command::free() {
  if (available()) {
    return;  
  }
  int ai = actionIndex;
  init();
  
  if (ai < MAX_ACTIONS) {
    ActionRef ref = Action::getRef(ai);
    if (debugCommands) {
      Serial.print(F("Attempt to free action #")); Serial.print(ai); 
    }
    ref.free();
  }
}


