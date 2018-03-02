
ModuleChain terminalModule("Terminal", 30, &terminalModuleHadler);

const int MAX_LINE = 60;
String line;
void (* charModeCallback)(char);
boolean interactive = true;
int8_t commandDef = -1;
Command definedCommand;

const int maxLineCommands = 25;

LineCommand lineCommands[maxLineCommands];

void setupTerminal() {
  line.reserve(MAX_LINE);
  registerLineCommand("HLP", &commandHelp);
  registerLineCommand("ECH", &commandInteractive);
  registerLineCommand("RST", &commandReset);
  registerLineCommand("BCK", &commandBack);
  registerLineCommand("ERR", &commandCancel);
  registerLineCommand("DEF", &commandDefine);
  registerLineCommand("FIN", &commandFinish);
  registerLineCommand("DMP", &commandDump);
  registerLineCommand("CLR", &commandClear);
  registerLineCommand("INF", &commandStatus);
  registerLineCommand("EXE", &commandExecute);
  resetTerminal();
}

void commandHelp(String&s) {
  Serial.println(F("HELP - Available commands:"));
  for (int i = 0; i < maxLineCommands; i++) {
    LineCommand &c = lineCommands[i];
    if (c.handler == NULL) {
      break;
    }
    Serial.println(c.cmd);
  }
}

void terminalModuleHadler(ModuleCmd cmd) {
  switch (cmd) {
    case initialize:
      setupTerminal();
      break;
    case reset:
      resetTerminal();
      break;
    case periodic:
      processTerminal();
      break;
  }
}

void resetTerminal() {
  line = "";
  charModeCallback = NULL;
  commandDef = -1;;
  clearNewCommand();
}


void registerLineCommand(const char* aCmd, void (*aHandler)(String&)) {
  for (int i = 0; i < maxLineCommands; i++) {
    if (lineCommands[i].handler == NULL) {
      lineCommands[i].cmd = aCmd;
      lineCommands[i].handler = aHandler;
      return;
    }
  }
}

void processLineCommand() {
  int pos = line.indexOf(':');
  if (pos == -1) {
    pos = line.length();
  }
  for (int i = 0; i < maxLineCommands; i++) {
    LineCommand &c = lineCommands[i];
    if (c.handler == NULL) {
      break;
    }
    const char *p = c.cmd;
    for (int x = 0; x < pos && ((*p) != 0); x++, p++) {
      char c = line.charAt(x);
      if (c >= 'a' && c <= 'z') {
        c -= ('a' - 'A');
      }
      if (c != *p) {
        goto end;
      }
    }
    if (*p == 0) {
      if (debugInfra) {
        Serial.print(F("Trying handler for command ")); Serial.println(c.cmd);
      }
      line.remove(0, pos + 1);
      if (debugInfra) {
        Serial.print(F("Remainder of command ")); Serial.println(line);
      }
      c.handler(line);
      return;
    }
    end:
    ;
  }
  Serial.println(F("\nBad command"));
}


void processTerminal() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (charModeCallback != NULL) {
      if (c == '`') {
        // reset from the character mode
        charModeCallback = NULL;
        resetTerminal();
        resetSetupMode();
        continue;
      }
      charModeCallback(c);
      continue;
    }
    if (c == 0x7f || c == '\b') {
      Serial.write(c);
      if (line.length() > 0) {
        line.remove(line.length() - 1, 1);
      }
      continue;
    }
    if (c == '\n' || c == '\r') {
      Serial.write("\r\n");
      processLineCommand();
      line = "";
      continue;
    }
    Serial.write(c);
    if (line.length() == MAX_LINE) {
      continue;
    }
    line.concat(c);
  }
}

int nextNumber(String& input) {
  if (input.length() == 0) {
    return -2;
  }
  int e = input.indexOf(':');
  char c = input.charAt(0);
  if (c < '0' || c > '9') {
    return -1;
  }
  const String x;
  
  if ( e == -1) {
    x = input;
    input = "";  
  } else {
    x = input.substring(0, e);
    input = input.substring(e + 1);
  }
  return x.toInt();
}

const String nextFragment(String& input) {
  int e = input.indexOf(':');
  const String x;
  if ( e == -1) {
    x = input;
    input = "";  
  } else {
    x = input.substring(0, e);
    input = input.substring(e + 1);
  }
  return x;
}

void commandInteractive(String& s) {
  char c = (s.length() == 0) ? 'N' : s.charAt(0);
  switch (c) {
    case 'y': case 'Y':
      interactive = true;
      break;
    default:
      interactive = false;
      break;
  }
  if (interactive) {
    //commandStatus();
  }
}

void commandReset(String& s) {
  void (*func)() = 0;
  func();
}

void commandBack(String& s) {
  if (commandDef <= 0) {
    return;
  }
  commandDef--;
  Serial.print(F("\n")); Serial.print(commandDef, DEC); Serial.println(F(":>>"));
}

void commandCancel(String& s) {
  resetTerminal();
  Serial.println(F("\nAborted"));
}

void prepareCommand() {
  if (commandDef == -1) {
    clearNewCommand();
  }
}

void maybeExecuteCommandPart() {
  if (!interactive) {
    return;
  }
  if (commandDef == -1) {
    executor.playNewAction();
  } else {
    executor.schedule((&newCommand[0]) + commandDef);
  }
}

void addCommandPart(const Action& ac) {
  if (commandDef == -1) {
    addNewCommand(ac);
    maybeExecuteCommandPart();
    return;
  }
  if (commandDef >= newCommandPartMax) {
    Serial.print(F("Command full."));
    return;
  }
  if (interactive) {
    Serial.print(commandDef + 1); Serial.println(F(": OK"));
  }
  if (commandDef > 0) {
    newCommand[commandDef - 1].last = false;
  }
  newCommand[commandDef] = ac;
  maybeExecuteCommandPart();
  ++commandDef;
}

void commandDump(String& s) {
  ModuleChain::invokeAll(ModuleCmd::dump);
}

void commandDefine(String& s) {
  int btn = nextNumber(s);
  if (btn == -1 || btn > MAX_INPUT_BUTTONS) {
    Serial.println(F("\nBad button"));
    return;
  }
  boolean state = true;
  
  if (s.length() > 0) {
    switch (s.charAt(0)) {
      case '+': case 'U': case 'u': case '1':
        state = true;
        break;
      case '-': case 'D': case 'd': case '0':
        state = false;
        break;
      default:
        Serial.println(F("Bad state"));
        return;
    }
    if (s.length() > 1) {
      if (s.charAt(1) != ':') {
        Serial.println(F("\nBad state"));
        return;
      }
      s.remove(0, 2);
    } else {
      s.remove(0, 1);
    }
  }
  boolean wait = true;
  if (s.length() == -1 || (s.length() > 1 && s.charAt(1) == ':')) {
    if (s.charAt(0) == 'N' || s.charAt(0) == 'n') {
      wait = false;
      s.remove(0,s.length() > 1 ? 2 : 1);
    }
  }
  Command c(btn, state, wait);
  definedCommand = c;
  commandDef = 0;
  if (interactive) {
    Serial.println(); Serial.print(commandDef + 1); Serial.println(F(":>"));
  }
}

void commandFinish(String& s) {
  if (commandDef < 0) {
    Serial.print(F("Empty command"));
    resetTerminal();
  }

  int idx = defineCommand(definedCommand);
  if (interactive) {
    Serial.print(F("Defined command #")); Serial.println(idx);
    Serial.print(F("Free RAM: ")); Serial.println(freeRam());
  }
  resetTerminal();
}

void commandStatus(String& s) {
  ModuleChain::invokeAll(ModuleCmd::status);
}

void commandClear(String& s) {
  ModuleChain::invokeAll(ModuleCmd::reset);
  saveHandler(s);
  commandReset(s);
}

void commandExecute(String& s) {
  int cmd = nextNumber(s);
  if (cmd < 1 || cmd > maxId) {
    Serial.println(F("Bad command"));
  }
  boolean state = true;
  if (s.length() > 0) {
    switch (s.charAt(0)) {
      case 'D': case 'd': case '0': case '-':
        state = false;
        break;
      case 'U': case 'u': case '1': case '+':
        state = true;
        break;
    }
  }
  Command *c = Command::find(cmd, state);
  if (c == NULL) {
    Serial.println(F("Unhandled input/state"));
    return;
  }
  c->execute();
  
}


