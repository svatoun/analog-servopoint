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
      eeBlockWrite('A', eeaddr_actionTable, &Action::actionTable, sizeof(Action::actionTable));
      eeBlockWrite('C', eeaddr_commandTable, &commands[0], sizeof(commands));
      break;
    case status:
      commandStatus();
      break;
    case dump:
      dumpCommands();
      break;
    case reset:
      clearCommands();
      break;
  }
}

void commandStatus() {
  int actionCount = 0;
  for (int i = 0; i < MAX_ACTIONS; i++) {
    if (Action::get(i)->isEmpty()) {
      break;
    }
    actionCount++;
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
    Action* a = Action::get(a);
    while (a != NULL) {
      s = "";
      a->print(s);
      Serial.println(s);
      a = a->next();
    }
    Serial.println("FIN");
  }
}

void commandEepromLoad() {
    Serial.println(F("Loading actions"));
    eeBlockRead('A', eeaddr_actionTable, &Action::actionTable, sizeof(Action::actionTable));
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

void Command::execute() {
  if (available()) {
    return;
  }
  executor.schedule(Action::get(actionIndex));
}

const Command* Command::find(int input, boolean state) {
  for (int i = 0; i < MAX_COMMANDS; i++) {
    Command& c = commands[i];
    if (c.available()) {
      return NULL;
    }
    if (c.input == input && c.on == state) {
      return &c;
    }
  }
  return NULL;
}

void Command::print(String& s) {
  s += F("DEF:");
  s += input;
  s += on ? F(":U") : F(":D");
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
    Action* a = Action::get(ai);
    if (debugCommands) {
      Serial.print(F("Attempt to free action #")); Serial.print(ai); Serial.print(F(" at ")); Serial.println((int)a);
    }
    a->free();
  }
  /*
  boolean last;
  do {
    last = a->isLast();
    a->free();
  } while (!last);
  */
}

Command& Command::operator =(const Command& other) {
  free();
  input = other.input;
  on = other.on;
  wait = other.wait;
  actionIndex = other.actionIndex;
}

