
ModuleChain terminalModule("Terminal", 30, &terminalModuleHadler);

const int MAX_LINE = 60;
void (* charModeCallback)(char);
boolean interactive = true;
int8_t commandDef = -1;
Command definedCommand;
int commandNo = -1;
const int maxLineCommands = 40;

const int maxInputLine = MAX_LINE;
char inputLine[maxInputLine + 1];
char *inputPos = inputLine;
char *inputEnd = inputLine;

LineCommand lineCommands[maxLineCommands];

void clearInputLine() {
  inputLine[0] = 0;
  inputEnd = inputPos = inputLine;  
}

void setupTerminal() {
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
void commandHelp() {
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
  clearInputLine();
  charModeCallback = NULL;
  commandDef = -1;;
  clearNewCommand();
}


void registerLineCommand(const char* aCmd, void (*aHandler)()) {
  for (int i = 0; i < maxLineCommands; i++) {
    if (lineCommands[i].handler == NULL) {
      lineCommands[i].cmd = aCmd;
      lineCommands[i].handler = aHandler;
      return;
    }
  }
}

void processLineCommand() {
  inputEnd = inputPos;
  char *pos = strchr(inputLine, ':');
  if (pos == NULL) {
    pos = inputEnd;
    inputPos = pos;
  } else {
    *pos = 0;
    inputPos = pos + 1;
  }
  if (debugInfra) {
    Serial.print("Command: "); Serial.println(inputLine);
  }
  for (int i = 0; i < maxLineCommands; i++) {
    LineCommand &c = lineCommands[i];
    if (c.handler == NULL) {
      break;
    }
    const char *p = c.cmd;
    const char *x = inputLine;
    for (; (*p != 0) && (*x != 0); x++, p++) {
      char e = *x;
      if ((e >= 'a') && (e <= 'z')) {
        e -= ('a' - 'A');
      }
      if (e != (*p)) {
        goto end;
      }
    }
    if (*p != *x) {
      goto end;
    }
    if (debugInfra) {
      Serial.print(F("Trying handler for command ")); Serial.println(c.cmd);
    }
    if (debugInfra) {
      Serial.print(F("Remainder of command ")); Serial.println(inputPos);
    }
    c.handler();
    return;

    end:
    ;
  }
  Serial.println(F("\nBad command"));
}

void printPrompt() {
  if (!interactive) {
    return;
  }
  if (commandDef < 0) {
    Serial.print("@ > ");
  } else {
    Serial.print((int)(commandDef + 1)); Serial.print(":> ");
  }
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
      if (inputPos > inputLine) {
        inputPos--;
        *inputPos = 0;
      }
      continue;
    }
    if (c == '\n' || c == '\r') {
      Serial.write("\r\n");
      processLineCommand();
      clearInputLine();
      printPrompt();
      continue;
    }
    Serial.write(c);
    if ((inputPos - inputLine) >= MAX_LINE) {
      continue;
    }
    *inputPos = tolower(c);
    inputPos++;
    *inputPos = 0;
  }
}


int nextNumber() {
  if ((*inputPos) == 0) {
    return -2;
  }
  if ((*inputPos) == ':') {
    inputPos++;
    return -3;
  }
  char c = *inputPos;
  if (c < '0' || c > '9') {
    return -1;
  }
  
  char *p = strchr(inputPos, ':');
  if (p == NULL) {
    p = inputEnd - 1;  
  } else {
    *p = 0;
  }
  char *ne;
  int val = atoi(inputPos);
  inputPos = p + 1;
  return val;
}

void commandInteractive() {
  char c = *inputPos;
  switch (c) {
    case 'y': 
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

void commandReset() {
  void (*func)() = 0;
  func();
}

void commandBack() {
  if (commandDef <= 0) {
    Serial.println(F("At start"));
    return;
  }
  commandDef--;
  newCommand[commandDef - 1].makeLast();
  Serial.print(F("\n"));
}

void commandCancel() {
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
    executor.schedule((&newCommand[0]) + commandDef, 0xff, false);
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

void commandDump() {
  ModuleChain::invokeAll(ModuleCmd::dump);
}

void commandDefine() {
  int no = nextNumber();
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
  int btn = nextNumber();
  if (btn < 0 || btn > MAX_INPUT_BUTTONS) {
    Serial.println(F("\nBad button"));
    return;
  }
  if (btn == 0) {
    btn = 0x3f;
  } else {
    btn--;
  }
  byte trigger = Command::cmdOn;
  const char ch = *inputPos;
  switch (ch) {
    case 'c': case 'd': case '1': case '+':
      trigger = Command::cmdOn;
      break;
    case 'o': case 'u': case '0': case '-':
      trigger = Command::cmdOff;
      break;
    case 't': trigger = Command::cmdToggle; break;
    case 'a': trigger = Command::cmdOnCancel; break;
    case 'b': trigger = Command::cmdOffReverts; break;
    case 'r': trigger = Command::cmdOnReverts; break;
    default:
      Serial.println(F("Bad trigger"));
      return;
  }
  boolean wait = true;
  inputPos++;
  if (*inputPos != 0) {
    if (*inputPos != ':') {
      Serial.println(F("\nBad state"));
      return;
    }
    inputPos++;

    if ((*inputPos != 0) || (*inputPos == 'n')) {
      wait = false;
    }
  }
  Command c(btn, trigger, wait);
  definedCommand = c;
  definedCommand.id = no;

  initPrintBuffer();
  definedCommand.print(printBuffer);
  Serial.println(printBuffer);
  commandDef = 0;
}

void commandFinish() {
  if (commandDef < 0) {
    Serial.println(F("Empty command"));
    resetTerminal();
    return;
  }

  int idx = defineCommand(definedCommand, commandNo);
  if (interactive) {
    if (idx < 0) {
      Serial.println(F("Error"));
    } else {
      Serial.print(F("Defined command #")); Serial.println(idx);
      Serial.print(F("Free RAM: ")); Serial.println(freeRam());
    }
  }
  commandEepromSave();
  resetTerminal();
}

void commandDelete() {
  int no = nextNumber();
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
void commandStatus() {
  ModuleChain::invokeAll(ModuleCmd::status);
}

void commandClear() {
  ModuleChain::invokeAll(ModuleCmd::reset);
  saveHandler();
  commandReset();
}

void commandExecute() {
  int cmd = nextNumber();
  if (cmd < 1 || cmd >= MAX_INPUT_BUTTONS) {
    Serial.println(F("Bad input"));
    return;
  }
  boolean state = true;
  cmd--;
  const char ch = *inputPos;
  switch (ch) {
    case 'c': case 'd': case '1': case '+':
      state = true;
      break;
    case 'o': case 'u': case '0': case '-':
      state = false;
      break;
    default:
      Serial.println(F("Bad trigger"));
      return;
  }
  setKeyPressed(cmd, state);
  Command::processAll(cmd, state);
}

void commandDumpEEProm() {
  Serial.println(F("EEPROM Dump:"));
  char buffer[24 + 4 + 1];
  int cnt = 0;
  char *ptr = buffer;
  *ptr = 0;
  for (int eea = 0; eea < eeaddr_top; eea++) {
    int r = EEPROM.read(eea);
    if (r < 0x10) {
      *(ptr++) = '0';
    }
    ptr = printNumber(ptr, r, 16);
    append(ptr, ' ');
    cnt++;
    if (cnt %32 == 0) {
      Serial.println(buffer);
      *(ptr = buffer) = 0;
    } else if (cnt % 8 == 0) {
      Serial.print(buffer);
      Serial.print(F("- "));
      *(ptr = buffer) = 0;
    }
  }
  Serial.println(buffer);
}

enum PlayMode {
    playServo = 0,
    playOutput,
    playCommand,

    playEnd
};

byte playMode;

void commandPlay() {
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

