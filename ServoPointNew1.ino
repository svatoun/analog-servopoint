
#undef SOFTWARE_SERVO

#define DEBUG

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

byte keyStateTriggered[(maxId + 7) / 8];
byte keyState[(maxId + 7) / 8];

Command commands[MAX_COMMANDS];

char printBuffer[50];

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
  executor.clear(true);
  for (Action* a = newCommand; a < (newCommand + newCommandPartMax); a++) {
    a->init();
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
  Action* prev = newCommand;
  for (Action* pa = newCommand; pa < (newCommand + newCommandPartMax); pa++) {
    Action& a = *pa;
    if (a.isEmpty()) {
      a = t;
      if (debugControl) {
        Serial.print(F("Added at position"));
        Serial.print((pa - newCommand));
        Serial.print(F(" = "));
        Serial.print((int)&a, HEX);
        Serial.print(F(":"));
        Serial.print(a.command);
        Serial.print(F(", data = "));
        Serial.println(a.data, HEX);
      }
      prev->notLast();
      a.makeLast();
      return (pa - newCommand);
    }
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
  if (l < *blinkPtr) {
    return;
  }
  blinkLastMillis = t;
  blinkPtr++;
  if (debugControl) {
    Serial.print(F("Next ACK time: ")); Serial.println(*blinkPtr);
  }
  if (*blinkPtr == 0) {
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

//////////////////////////////////////////////////////////////////////////

void initializeHW() {
  keypad.digitalPins(A5);
  keypad.setDebounceTime(200);
  
  //  Serial.begin(115200);
  Serial.begin(57600);
  // put your setup code here, to run once:
  pinMode(LED_ACK, OUTPUT);
  digitalWrite(LED_ACK, LOW);
  analogReference(DEFAULT);
}

void checkInitEEPROM() {
  int savedVer = EEPROM.read(eeaddr_version);
  if (savedVer == CURRENT_DATA_VERSION) {
    return;
  }
  Serial.println(F("Obsolete EEPROM, reinitializing"));
  ModuleChain::invokeAll(ModuleCmd::reset);
  ModuleChain::invokeAll(ModuleCmd::eepromSave);
  EEPROM.write(eeaddr_version, CURRENT_DATA_VERSION);
}

void setup() {
  initializeHW();
  checkInitEEPROM();

  bootProcessors();
  
  ModuleChain::invokeAll(ModuleCmd::initialize);
  
  //  initializeTables();
  for (int i = 0; i < sizeof(keyState); i++) {
    keyState[i] = 0;
    keyStateTriggered[i] = 0;
  }

  // initial load of key / switch positions
  keypad.getKeys();
  keypad.addChangeListener(&processKeyCallback);
//  enterSetup();

  Serial.print(F("Size of commands: ")); Serial.println(sizeof(commands));
  Serial.print(F("Size of executors: ")); Serial.println(sizeof(Executor) + sizeof(ServoProcessor));
  Serial.print(F("Size of actions: ")); Serial.println(sizeof(Action) * MAX_ACTIONS);

  setupTerminal();

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

void primeKeyChange(byte k) {
  if (k >= maxId) {
    return;
  }
  bitfieldWrite(keyStateTriggered, k, true); 
}

char getKeyChange(byte k) {
  if (k >= maxId) {
    return -1;
  }
  if (bitfieldRead(keyStateTriggered, k)) {
    return -1;
  }
  return bitfieldRead(keyState, k) ? 1 : 0;
}

bool isKeyPressed(byte k) {
  if (k >= maxId) {
    return false;
  }
  return bitfieldRead(keyState, k);
}

bool setKeyPressed(int k, bool val) {
  bitfieldWrite(keyState, k, val);
  bitfieldWrite(keyStateTriggered, k, false);
}

void processKey(int k, boolean pressed) {
  setKeyPressed(k, pressed);
  Command::processAll(k, pressed);
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
    Serial.print(F("Processing key: #")); Serial.print(ch);
    if (pressed) {
      Serial.println(F(" DOWN"));
    } else {
      Serial.println(F(" UP"));
    }
    processKey(ch - 1, pressed);
}

void loop() {
  currentMillis = millis();
  pressedKey = keypad.getKey();
  if (setupActive) {
    setupLoop();
  }
  scheduler.schedulerTick();
  handleAckLed();
  executor.process();
  ModuleChain::invokeAll(ModuleCmd::periodic);
  // explicit, at the end
  output.commit();
}


const int blinkSetup[] = { 500, 250, 500, 250, 500, 250, 0 };
const int blinkError[] = { 1000, 250, 1000, 250, 0 };


void saveHandler() {
  ModuleChain::invokeAll(ModuleCmd::eepromSave);
}

int defineCommand(const Command& def, int no) {
  if (no >= 0) {
    if (no >= MAX_COMMANDS) {
      return -1;
    }
  } else {
    for (const Command* ptr = commands; ptr < commands + MAX_COMMANDS; ptr++) {
      if (ptr->available()) {
        no = (ptr - commands);
        break;
      }
    }
  }
  if (no == -1) {
    if (debugCommands) {
      Serial.println(F("No free slots"));
    }
    return -1;
  }
  Command& c = commands[no];
  if (!c.available()) {
      if (debugCommands) {
        Serial.print(F("Existing command ")); Serial.println(no);
      }
      replaceCommand(c, def);
      return no;
  } else {
    return defineNewCommand(def, c);
  }  
}

int defineNewCommand(const Command& c, Command& target) {
  boolean empty = true;
  boolean ok = false;
  Action* pa;
  for (pa = newCommand; pa < (newCommand + newCommandPartMax); pa++) {
    Action& a = *pa;
    if (a.isEmpty()) {
      pa--;
      ok = true;
      break;
    }
    empty = false;
    if (a.isLast()) {
      ok = true;
      break;
    }
  }
  if (empty || !ok) {
    return -1;
  }
  pa->makeLast();
  int s = (pa - newCommand) + 1;
  if (debugCommands) {
    Serial.print(F("Action count: ")); Serial.println(s);
  }
  int aIndex = Action::copy(&newCommand[0], s);
  if (debugCommands) {
    Serial.print(F("Copied at: ")); Serial.println(aIndex);
  }
  target = c;
  target.actionIndex = aIndex;
  return &target - commands;
}

boolean deleteCommand(int i) {
  if (i < 0 || i >= MAX_COMMANDS) {
    return false;
  }
  Command &c = commands[i];
  if (c.available()) {
    return false;
  }
  c.free();
  return true;
}

void replaceCommand(Command& c, const Command& def) {
  c.free();
  defineNewCommand(def, c);
}

ModuleChain::ModuleChain(const char* name, byte aPrirority,  void (*h)(ModuleCmd)) : next(NULL), priority(aPrirority) {
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


