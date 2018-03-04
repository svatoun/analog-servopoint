
#define SOFTWARE_SERVO


#include <EEPROM.h>
#include "Key2.h"
#include "Keypad2.h"

#ifdef SOFTWARE_SERVO
#include <SoftwareServo.h>
#define Servo SoftwareServo
#else 
#include <Servo.h>
#endif

#include "Common.h"

ModuleChain* ModuleChain::head;

byte state[(maxId + 7) / 8];

Command commands[MAX_COMMANDS];


/**
   Serves to build up a new command during setup. Takes priority over any actions
*/
Action  newCommand[newCommandPartMax];

AnalogKeypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//ModuleChain *modules;

/**
   The current number of milliseconds; read at the start of the loop().
*/
long currentMillis;

Output output;
Executor executor;

char currentKey;
uint8_t * heapptr, * stackptr;

void check_mem() {
  stackptr = (uint8_t *)malloc(4);          // use stackptr temporarily
  heapptr = stackptr;                     // save value of heap pointer
  free(stackptr);      // free up the memory again (sets stackptr to 0)
  stackptr =  (uint8_t *)(SP);           // save value of stack pointer
}

int freeRam ()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void clearNewCommand() {
  check_mem();
  if (debugControl) {
    Serial.println(F("Clearing new command"));
  }
  executor.clear();
  for (int i = 0; i < sizeof(newCommand) / sizeof(newCommand[0]); i++) {
    newCommand[i].init();
  }
}

int addNewCommand(const Action& t) {
  if (debugControl) {
    Serial.print(F("Adding action command "));
    Serial.print(F("Source: ")); Serial.print((int)&t, HEX);
    Serial.print(t.command);
    Serial.print(F(", data = "));
    Serial.println(t.data, HEX);
  }
  int lastC = -1;
  for (int i = 0; i < sizeof(newCommand) / sizeof(newCommand[0]); i++) {
    if (newCommand[i].isEmpty()) {
      newCommand[i] = t;
      if (debugControl) {
        Serial.print(F("Added at position"));
        Serial.print(i);
        Serial.print(" = ");
        Serial.print((int)&newCommand[i], HEX);
        Serial.print(":");
        Serial.print(newCommand[i].command);
        Serial.print(F(", data = "));
        Serial.println(newCommand[i].data, HEX);
      }
      if (lastC != -1) {
        newCommand[lastC].notLast();
      }
      newCommand[i].makeLast();
      return i;
    }
    lastC = i;
  }
  return -1;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ACK / status LED handling
const int* blinkPtr = NULL;
long blinkLastMillis;
byte pos = 0;
boolean ackLedState = false;
byte pulseCount = 0;

const int blinkShort[] = { 250, 250, 0 };   // blink once, short
const int blinkLong[] = { 500, 0 };
const int blinkReject[] = { 250, 250, 250, 250, 0 };
const int blinkSave[] = { 250, 250, 250, 250, 250, 250, 500, 0 };


void makeLedAck(const int *  ledSequence) {
  blinkPtr = ledSequence;
  pos = 0;
  ackLedState = 1;
  blinkLastMillis = millis();
  digitalWrite(LED_ACK, HIGH);
  if (debugControl) {
    Serial.print(F("LED ACK: ")); Serial.println(blinkPtr[pos]);
  }
}

void handleAckLed() {
  if (blinkPtr == NULL) {
    return;
  }
  long t = millis();
  long l = t - blinkLastMillis;
  if (l < blinkPtr[pos]) {
    return;
  }
  blinkLastMillis = t;
  pos++;
  if (debugControl) {
    Serial.print(F("Next ACK time: ")); Serial.println(blinkPtr[pos]);
  }
  if (blinkPtr[pos] == 0) {
    ackLedState = 0;
    digitalWrite(LED_ACK, LOW);
    if (pulseCount > 0) {
      pulseCount--;
      makeLedAck(&blinkShort[0]);
    } else {
      if (debugControl) {
        Serial.println(F("ACK done"));
      }
      blinkPtr = NULL;
      blinkLastMillis = -1;
    }
    return;
  }
  ackLedState = !ackLedState;
  digitalWrite(LED_ACK, ackLedState ? HIGH : LOW);
}

void stopAll() {
  // clear out servos
  ServoProcessor::clearAll();
}

//////////////////////////////////////////////////////////////////////////
void servoSetup();

void initializeHW() {
  keypad.digitalPins(A5);
  
  //  Serial.begin(115200);
  Serial.begin(57600);
  // put your setup code here, to run once:
  pinMode(LED_ACK, OUTPUT);
  digitalWrite(LED_ACK, LOW);
  analogReference(DEFAULT);
  //  servoSetup();
}

void checkInitEEPROM() {
  int savedVer = EEPROM.read(eeaddr_version);
  if (savedVer == CURRENT_DATA_VERSION) {
    return;
  }
  Serial.println(F("Obsolete or missing data in EEPROM, reinitializing"));
  ModuleChain::invokeAll(ModuleCmd::reset);
  ModuleChain::invokeAll(ModuleCmd::eepromSave);
  EEPROM.write(eeaddr_version, CURRENT_DATA_VERSION);
}

void setup() {
  initializeHW();
  checkInitEEPROM();

  ModuleChain::invokeAll(ModuleCmd::initialize);
  
  //  initializeTables();
  for (int i = 0; i < sizeof(state); i++) {
    state[i] = 0;
  }

  // initial load of key / switch positions
  keypad.getKeys();
  keypad.addChangeListener(&processKeyCallback);
//  enterSetup();

  int dataSize = 0; // sizeof(servoConfig) + sizeof(commands);
  dataSize += sizeof(Executor) + sizeof(ServoProcessor);
  dataSize += sizeof(Action::actionTable);

//  Serial.print(F("Size of servoConfig: ")); Serial.println(sizeof(servoConfig));
  Serial.print(F("Size of commands: ")); Serial.println(sizeof(commands));
  Serial.print(F("Size of executors: ")); Serial.println(sizeof(Executor) + sizeof(ServoProcessor));
  Serial.print(F("Size of actions: ")); Serial.println(sizeof(Action::actionTable));

  // XXX setupTerminal();

  registerLineCommand("SAV", &saveHandler);
}

/**
   Writes a block of data into the EEPROM, followed by a checksum
*/
void eeBlockWrite(byte magic, int eeaddr, const void* address, int size) {
  if (debugControl) {
    Serial.print(F("Writing EEPROM ")); Serial.print(eeaddr, HEX); Serial.print(F(":")); Serial.print(size); Serial.print(F(", source: ")); Serial.println((int)address, HEX);
  }
  const byte *ptr = (const byte*) address;
  byte hash = magic;
  EEPROM.write(eeaddr, magic);
  eeaddr++;
  for (; size > 0; size--) {
    byte b = *ptr;
    EEPROM.write(eeaddr, b);
    ptr++;
    eeaddr++;
    hash = hash ^ b;
  }
  EEPROM.write(eeaddr, hash);
}

/**
   Reads block of data from the EEPROM, but only if the checksum of
   that data is correct.
*/
boolean eeBlockRead(byte magic, int eeaddr, void* address, int size) {
  if (debugControl) {
    Serial.print(F("Reading EEPROM ")); Serial.print(eeaddr, HEX); Serial.print(F(":")); Serial.print(size); Serial.print(F(", dest: ")); Serial.println((int)address, HEX);
  }
  int a = eeaddr;
  byte hash = magic;
  byte x;
  boolean allNull = true;
  x = EEPROM.read(a);
  if (x != magic) {
    if (debugControl) {
      Serial.println(F("No magic header found"));
    }
    return false;
  }
  a++;
  for (int i = 0; i < size; i++, a++) {
    x = EEPROM.read(a);
    if (x != 0) {
      allNull = false;
    }
    hash = hash ^ x;
  }
  x = EEPROM.read(a);
  if (hash != x || allNull) {
    if (debugControl) {
      Serial.println(F("Checksum does not match"));
    }
    return false;
  }

  a = eeaddr + 1;
  byte *ptr = (byte*) address;
  for (int i = 0; i < size; i++, a++) {
    x = EEPROM.read(a);
    *ptr = x;
    ptr++;
  }
}

long lastOnePressed = 0;
int onePressedCount = 0;

void processKey(int k, boolean pressed) {
  const Command* cmd = Command::find(k, pressed);
  if (cmd == NULL) {
    if (debugInput) {
      Serial.println(F("No command found"));
    }
    return;
  }
  if (debugInput) {
    Serial.print(F("Found command:" ));  Serial.print((int)cmd, HEX);
    Serial.print(" ");
    String s;
    cmd->print(s);
    Serial.println(s);
  }
  cmd->execute();
}


void processKeys() {
  boolean haveKeys = keypad.getKeys();
  if (!haveKeys) {
    return;
  }
  for (byte i = 0; i < LIST_MAX; i++) {
    const Key2& k = keypad.key[i];
    if (!k.stateChanged) {
      continue;
    }
    if (debugInput) {
      Serial.println(F("Key state changed"));
    }
    if (k.kstate == IDLE || k.kstate == HOLD) {
      // ignore hold / idle
      continue;
    }
    boolean pressed = (k.kstate  == PRESSED);
    int ch = keypad.getChar(k);
    if (debugInput) {
      Serial.print("Processing key: #"); Serial.print(ch);
      if (pressed) {
        Serial.println(" DOWN");
      } else {
        Serial.println(" UP");
      }
    }
    processKey(ch, pressed);
  }
}

void processKeyCallback(const Key2& k, char c) {
    if (setupActive) {
      return;
    }
    if (debugInput) {
      Serial.println(F("Key state changed"));
    }
    if (k.kstate == IDLE || k.kstate == HOLD) {
      // ignore hold / idle
      return;
    }
    boolean pressed = (k.kstate  == PRESSED);
    int ch = keypad.getChar(k);
    if (debugInput) {
      Serial.print("Processing key: #"); Serial.print(ch);
      if (pressed) {
        Serial.println(" DOWN");
      } else {
        Serial.println(" UP");
      }
    }
    processKey(ch, pressed);
}

void loop() {
  currentMillis = millis();
  pressedKey = keypad.getKey();
  if (pressedKey == 1) {
    if (currentMillis - lastOnePressed < 500) {
      if (onePressedCount++ == 4) {
        enterSetup();
      }
    } else {
      lastOnePressed = currentMillis;
      onePressedCount = 0;
    }
  }
  if (setupActive) {
    setupLoop();
  }
//  processKeys();
  handleAckLed();
  executor.process();
  ModuleChain::invokeAll(ModuleCmd::periodic);
  // explicit, at the end
  output.commit();
}


const int blinkSetup[] = { 500, 250, 500, 250, 500, 250, 0 };
const int blinkError[] = { 1000, 250, 1000, 250, 0 };


void saveHandler(String& l) {
  ModuleChain::invokeAll(ModuleCmd::eepromSave);
}

int defineCommand(const Command& def) {
  for (int i = 0; i < MAX_COMMANDS; i++) {
    Command& c = commands[i];
    if (c.matches(def)) {
      if (debugCommands) {
        Serial.print(F("Found matching command ")); Serial.println(i);
      }
      replaceCommand(c, def);
      return i;
    }
    if (c.available()) {
      if (debugCommands) {
        Serial.print(F("Found free slot: ")); Serial.println(i);
      }
      return defineNewCommand(def, c);
    }
    if (debugCommands) {
      Serial.print("Skipping: ");
      String s;
      c.print(s);
      Serial.print(s);
      Serial.print(" ");
      Serial.println(c.actionIndex);
    }
  }
  if (debugCommands) {
    Serial.println(F("No free slots"));
  }
  return -1;
}

int defineNewCommand(const Command& c, Command& target) {
  int s = -1;

  for (int i = 0; i < newCommandPartMax; i++) {
    if (newCommand[i].isEmpty()) {
      if (i == 0) {
        return -1;
      }
      newCommand[i - 1].makeLast();
      s = i;
      break;
    }
    if (newCommand[i].isLast()) {
      s = i + 1;
      break;
    }
  }
  if (debugCommands) {
    Serial.print(F("Action count: ")); Serial.println(s);
  }
  if (s < 1) {
    return -1;
  }

  int aIndex = Action::copy(&newCommand[0], s);
  if (debugCommands) {
    Serial.print(F("Copied at: ")); Serial.println(aIndex);
  }
  target = c;
  target.actionIndex = aIndex;
  return &target - commands;
}

void replaceCommand(Command& c, const Command& def) {
  c.free();
  defineNewCommand(def, c);
}

ModuleChain::ModuleChain(const String& name, byte aPrirority,  void (*h)(ModuleCmd)) : next(NULL), priority(aPrirority) {
  static ModuleChain* __last = NULL;
  if (__last == NULL) {
    head = NULL;
  }
  __last = this;
  handler = h;
  if (head == NULL) {
    head = this;
    return;
  }
  if (head->priority >= aPrirority) {
    next = head;
    head = this;
    return;
  }
  ModuleChain* p = head;
  while (p->priority < aPrirority) {
    if (p->next == NULL) {
      p->next = this;
      return;
    }
    p = p->next;
  }
  if (p->next == NULL) {
    p->next = this;
    return;
  }
  next = p->next;
  p->next = this;
}

void ModuleChain::invokeAll(ModuleCmd cmd) {
  ModuleChain*p = head;
  while (p != NULL) {
    if (p->handler != NULL) {
      p->handler(cmd);
    }
    p = p->next;
  }
}


