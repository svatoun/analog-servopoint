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
      commandStatus();
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

void commandStatus() {
  int actionCount = 0;
  Action tmp;
  for (int i = 0; i < MAX_ACTIONS; i++) {
    if (!Action::get(i, tmp).isEmpty()) {
      actionCount++;
    }
  }
  int cmdCount = 0;
  for (int i = 0; i < MAX_COMMANDS; i++) {
    if (commands[i].available()) {
      break;
    }
    cmdCount++;
  }  
  Serial.print(F("Action count: ")); Serial.println(actionCount);
  Serial.print(F("Defined commands: ")); Serial.println(cmdCount);
}

void clearCommands() {
  for (int i = 0; i < MAX_COMMANDS; i++) {
    commands[i].free();
  }
  Action::freeAll();
}

void dumpCommands() {
  for (int i = 0; i < MAX_COMMANDS; i++) {
    if (commands[i].available()) {
      break;
    }
    String s;
    commands[i].print(s);
    Serial.println(s);
    int actionIndex = commands[i].actionIndex;
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
  for (int i = 0; i < MAX_COMMANDS; i++) {
    commands[i].init();
  }
}  

void Command::execute(boolean keyPressed) {
  if (available()) {
    return;
  }
  switch (trigger) {
    case cmdOn:
      if (!keyPressed) {
        break;
      }
    case cmdToggle:
      executor.schedule(Action::getRef(actionIndex), id, false);
      break;
    case cmdOff:
      if (!keyPressed) {
        executor.schedule(Action::getRef(actionIndex), id, false);
      }
      break;
    case cmdOnCancel:
      if (keyPressed) {
        executor.schedule(Action::getRef(actionIndex), id, false);
      } else if (id != 0) {
        executor.cancelCommand(id);
      }
      break;
    case cmdOffReverts:
      if (keyPressed) {
        executor.schedule(Action::getRef(actionIndex), id, false);
      } else {
        executor.schedule(Action::getRef(actionIndex), id, true);
      }
      break;
  }
//  executor.schedule(Action::getRef(actionIndex));
}

const Command* Command::find(int input, byte t) {
  for (int i = 0; i < MAX_COMMANDS; i++) {
    Command& c = commands[i];
    if (c.available()) {
      return NULL;
    }
    if (c.input == input && c.trigger == t) {
      return &c;
    }
  }
  return NULL;
}

void Command::print(String& s) {
  s += F("DEF:");
  s += input;
  char t;
  switch (trigger) {
    case cmdOn:         t = 'C'; break;
    case cmdOff:        t = 'O'; break;
    case cmdToggle:     t = 'T'; break;
    case cmdOnCancel:   t = 'A'; break;
    case cmOnReverts:   t = 'B'; break;
    case cmdOffReverts: t = 'R'; break;
  }
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
  /*
  boolean last;
  do {
    last = a->isLast();
    a->free();
  } while (!last);
  */
}

/*
Command& Command::operator =(const Command& other) {
  free();
  input = other.input;
  on = other.on;
  wait = other.wait;
  actionIndex = other.actionIndex;
}
*/

