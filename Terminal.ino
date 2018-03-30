
ModuleChain terminalModule("Terminal", 30, &terminalModuleHadler);

const int MAX_LINE = 60;
String line;
void (* charModeCallback)(char);
boolean interactive = true;
int8_t commandDef = -1;
Command definedCommand;
int commandNo = -1;

const int maxLineCommands = 40;

LineCommand lineCommands[maxLineCommands];

void setupTerminal() {
  line.reserve(MAX_LINE);
//  registerLineCommand("HLP", &commandHelp);
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
  registerLineCommand("EED", &commandDumpEEProm);
  registerLineCommand("PLA", &commandPlay);
  registerLineCommand("DEL", &commandDelete);
  resetTerminal();
}

/*
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
*/

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
  if (e == 0) {
    input.remove(0, 1);
    return -3;
  }
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
    Serial.println(F("At start"));
    return;
  }
  commandDef--;
  newCommand[commandDef - 1].makeLast();
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
    executor.schedule((&newCommand[0]) + commandDef, 0, false);
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
  int no = nextNumber(s);
  if (no == -3) {
    no = Command::findFree();
    if (no < 0) {
      Serial.println(F("\nNo free slots"));
      return;
    }
  } else {
    no--; 
  }
  if (no < 0 || no >= MAX_COMMANDS) {
    Serial.println(F("\nBad command ID"));
    return;
  }
  commandNo = no;
  int btn = nextNumber(s);
  if (btn < 1 || btn > MAX_INPUT_BUTTONS) {
    Serial.println(F("\nBad button"));
    return;
  }
  btn--;
  byte trigger = Command::cmdOn;
  
  if (s.length() > 0) {

    switch (s.charAt(0)) {
      case 'C': case 'c': case 'D': case 'd': case '1': case '+':
        trigger = Command::cmdOn;
        break;
      case 'O': case 'o': case 'U': case 'u': case '0': case '-':
        trigger = Command::cmdOff;
        break;
      case 'T': case 't': trigger = Command::cmdToggle; break;
      case 'A': case 'a': trigger = Command::cmdOnCancel; break;
      case 'B': case 'b': trigger = Command::cmdOffReverts; break;
      case 'R': case 'r': trigger = Command::cmdOnReverts; break;
      default:
        Serial.println(F("Bad trigger"));
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
  Command c(btn, trigger, wait);
  definedCommand = c;
  definedCommand.id = no;

  String e;
  c.print(e);
  Serial.println(e);
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

  int idx = defineCommand(definedCommand, commandNo);
  if (interactive) {
    Serial.print(F("Defined command #")); Serial.println(idx);
    Serial.print(F("Free RAM: ")); Serial.println(freeRam());
  }
  commandEepromSave();
  resetTerminal();
}

void commandDelete(String& s) {
  int no = nextNumber(s);
  if (no < 1 || no > MAX_COMMANDS) {
    Serial.println(F("Bad command"));
    return;
  }
  bool result = deleteCommand(no - 1);
  Serial.print(F("Delete ")); Serial.print(no);
  if (result) {
    Serial.println(": OK");
  } else {
    Serial.println(": NOP");
  }
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
  if (cmd < 1 || cmd >= MAX_INPUT_BUTTONS) {
    Serial.println(F("Bad input"));
    return;
  }
  boolean state = true;
  cmd--;
  if (s.length() > 0) {
    switch (s.charAt(0)) {
      case 'C': case 'c': case 'D': case 'd': case '1': case '+':
        state = true;
        break;
      case 'O': case 'o': case 'U': case 'u': case '0': case '-':
        state = false;
        break;
      default:
        Serial.println(F("Bad trigger"));
        return;
    }
  }
  setKeyPressed(cmd, state);
  Command::processAll(cmd, state);
}

void commandDumpEEProm(String& s) {
  Serial.println(F("EEPROM Dump:"));
  String line;
  line.reserve(8 * 3 + 4);
  int cnt = 0;
  for (int eea = 0; eea < eeaddr_top; eea++) {
    int r = EEPROM.read(eea);
    if (r < 0x10) {
      line.concat('0');
    }
    line.concat(String(r, HEX)); line.concat(' ');
    cnt++;
    if (cnt %32 == 0) {
      Serial.println(line);
      line = "";
    } else if (cnt % 8 == 0) {
      Serial.print(line);
      line = "";
      Serial.print(F("- "));
    }
  }
  Serial.println(line);
}

enum PlayMode {
    playServo = 0,
    playOutput,
    playCommand,

    playEnd
};

byte playMode;

void commandPlay(String& s) {
  playMode = playServo;
  charModeCallback = &playCharacterCallback;
}

void playCharacterCallback(char c) {
  bool shift = false;
  if (c >= 'a' && c <= 'z') {
    shift = true;
    c -= ('a' - 'A');
  }
  switch (c) {
    case '1':
      Serial.println(F("\nServo mode"));
      playMode = servo;
      return;
    case '2':
      Serial.println(F("\nOutput mode"));
      playMode = servo;
      return;
    case '3':
      Serial.println(F("\nCommand mode"));
      playMode = servo;
      return;
    case '\r':
    case ' ':
    case '.':
      Serial.println(F("\nExit"));
      charModeCallback = NULL;
      return;
  }
  if (c >= 'A' && c <= 'Z') {
    int n = c - 'A';
    Action a;
    Serial.print('\b');
    switch (playMode) {
      case playServo: {
        if (n >= MAX_SERVO) {
          return;
        }
        ServoActionData &d = a.asServoAction();
        if (shift) {
          d.moveRight(n);
        } else {
          d.moveLeft(n);
        }
        break;
      }
      case playOutput: {
        if (n >= MAX_OUTPUT) {
          return;
        }
        OutputActionData& outAction = a.asOutputAction();
        if (shift) {
          outAction.turnOff(n);
        } else {
          outAction.turnOn(n);
        }
        break;
      }
      default:
        return;
    }
    prepareCommand();
    addNewCommand(a);
    executor.playNewAction();
  }
}

