#include "Defs.h"

const int OUTPUT_DELAY = 0;     //
const int OUTPUT_OFF = 0;       // switch the output off
const int OUTPUT_ON = 1;        // switch the output on
const int OUTPUT_ON_PULSE = 2;  // pulse the output
const int OUTPUT_TOGGLE = 3;    // toggle the output on-off

const int newCommandPartMax = 16;

bool setupActive = false;

char pressedKey;

extern byte keyState[];
extern char printBuffer[];

void bitfieldWrite(byte* bptr, byte bit, boolean state) {
  bptr += (bit / 8);
  byte v = (1 << (bit & 0x07));
  if (state) {
    *bptr |= v;
  } else {
    *bptr &= ~v;
  }
}

boolean bitfieldRead(byte* bptr, byte bit) {
  bptr += (bit / 8);
  byte v = (1 << (bit & 0x07));
  return ((*bptr) & v) > 0;
}

// Condition makers
// input keys
void primeKeyChange(byte k);
bool isKeyPressed(byte k);
char getKeyChange(byte k);
boolean setKeyPressed(byte k, boolean s);

// TTL outputs
void primeOutputChange(byte k);
bool isOutputActive(byte k);
char getOutputActive(byte k);
boolean setOutputActive(byte k, boolean s);

// commands
void primeCommandChange(byte k);
bool isCommandActive(byte k);
char getCommandActive(byte k);
boolean setCommandActive(byte k, boolean s);

// servo motion
void primeServoActive(byte k);
bool isServoActive(byte k);
char getServoActive(byte k);
boolean setServoActive(byte k, boolean s);

// servo position
void primeServoPosition(byte k);
bool isServoPosition(byte k);
char getServoPosition(byte k);
boolean setServoPosition(byte k, boolean s);

 __attribute__((always_inline)) char* append(char* &ptr, char c) {
  *(ptr++) = c;
  return ptr;
}

__attribute__((noinline)) char* initPrintBuffer() {
  printBuffer[0] = 0;
  return printBuffer;
}

__attribute__((noinline)) char *printNumber(char *out, int no, int base) {
  itoa(no, out, base);
  return out + strlen(out);
}


enum Instr {
  none = 0,
  servo,         // 1  control a servo
  wait,         // 3  wait before executing next command
  onOff,        // 4  turn the output on and off
  cancel,       // 6  cancel pending action
  control,
  continuation,
  ErrorInstruction
};
static_assert (ErrorInstruction <= 8, "Too many instruction");

const int ACTION_NONE = 255;

struct ServoActionData;
struct OutputActionData;
struct WaitActionData;

/**
   For each servo speed level, defines a number of milliseconds
   that should elapse, before the servo
*/
const byte servoSpeedTimes[] = {
//  5, 10, 15, 20,
//  30, 40, 50, 60

  5, 10, 20, 30, 
  50, 70, 100, 120
};

/**
   Configuration of a servo. Specifes PWM for the left and right position.
   Initializes at 90deg. Values are packed/remembered in 6deg steps.
   Servo speed is remembered in 100ms units
*/
struct ServoConfig {
  enum {
    noOutput = 0x3f
  };
  
  static const byte servoPwmStep = 3;
  /**
     Left/first position, in servoPwmStep = 3 deg increments
  */
  byte  pwmFirst   : 6;

  /**
     Right/second position, in servoPwmStep = 3 deg increments
  */
  byte  pwmSecond  : 6;

  /**
     Servo configurable speed
  */
  byte  servoSpeed : 3;

  /**
   * Ouptut which will signal servos' position.
   */
  byte  statusOutput : 6;

  // 6 + 6 + 3 + 6 = 21 bits
  // 24 bits limit

  // the default is chosen so that it positions the servo at neutral position. 0 would mean to position at
  // one of the edge positions, which is not usually good for servos already built into a structure.
  ServoConfig() : pwmFirst((90 / servoPwmStep)), pwmSecond((90 / servoPwmStep)), servoSpeed(0), statusOutput(noOutput) {}
  ServoConfig(byte pwmL, byte pwmR, byte speed, char output) : servoSpeed(speed), statusOutput(output < 0 ? noOutput : output) {
    setLeft(pwmL);
    setRight(pwmR);
  }

  boolean isEmpty() {
    return (pwmFirst == (90 / servoPwmStep)) && (pwmSecond == (90 / servoPwmStep)) && (servoSpeed == 0);
  }

  char output() {
    byte o = statusOutput;
    return o == noOutput ? - 1 : o;
  }

  void setOutput(char o) {
    if (o < 0 || o >= MAX_OUTPUT) {
      statusOutput = noOutput;
    } else {
      statusOutput = o;
    }
  }

  void setLeft(byte l) {
    if (l > 180) {
      return;
    }
    pwmFirst = (l / servoPwmStep);
  }
  void setRight(byte r) {
    if (r > 180) {
      return;
    }
    pwmSecond = (r / servoPwmStep);
  }
  void setSpeed(byte s) {
    if (s >= 8) {
      s = 7;
    }
    servoSpeed = s;
  }

  byte speed() {
    return servoSpeed;
  }
  byte left() {
    return pwmFirst * servoPwmStep;
  }
  byte right() {
    return pwmSecond * servoPwmStep;
  }
  byte pos(bool dir) {
    return dir ? left() : right();
  }

  static byte change(byte orig, char steps) {
    int x = orig + steps * servoPwmStep;
    if (x < 0) {
      return 0;
    } else if (x > 180) {
      return 180;
    }
    return x;
  }

  static byte value(byte set) {
    return (set / servoPwmStep) * servoPwmStep;
  }

  void clear() {
    pwmFirst = pwmSecond = (90 / servoPwmStep);
    servoSpeed = 0;
    statusOutput = noOutput;
  }

  void print(byte id, char* out);

  // load data from EEPROM
  void load(byte idx);
  // save into EEPROM
  void save(byte idx);
};

struct FlashConfig {
  unsigned int onDelay : 8;
  unsigned int offDelay : 8;
  unsigned int flashCount : 7;

  FlashConfig() : onDelay(0), offDelay(0), flashCount(0) {}

  boolean empty() {
    return onDelay == 0;
  }

  void save(byte index);
  void load(byte index);

  void print(char* out, byte index);
};

extern ServoConfig  setupServoConfig;
extern FlashConfig  flashConfig[];
extern char setupServoIndex;

typedef void (*ActionRenumberFunc)(int, int);

struct Action;
struct ActionRef;
struct ControlActionData;
/**
   Action defines the step which should be executed.
*/
extern char printBuffer[];
typedef void (*dumper_t)(const Action& , char*);

struct Action {
    byte  last    :   1;    // flag, last action
    byte  command :   3;    // instruction

    unsigned int data : 20; // action type-specific data

//    static Action actionTable[MAX_ACTIONS];
    static ActionRenumberFunc renumberCallbacks[];
    static dumper_t dumpers[];
    static byte  top;
  public:
    Action();

    static void registerDumper(Instr cmd, dumper_t dumper);

    static void initialize();
    static void renumberCallback(ActionRenumberFunc f);

    static Action& get(byte index, Action& target);
    static ActionRef getRef(byte index);

    //static boolean isPersistent(const Action* a) { return (a > actionTable) && (a < (actionTable + MAX_ACTIONS)); }
    
    void makeLast() {
      last = 1;
    }
    
    void init() {
      command = none;
      makeLast();
    }
    void free();
    static void freeAll();

    boolean isLast() {
      return last;
    }
    
    boolean isEmpty() {
      return command == none;
    }
    
    //const Action* next();

    void notLast() {
      last = 0;
    }

    static int copy(const Action* a, byte);

    static int findSpace(byte size);

    template<typename X> X& asActionData() {
      return (X&)(*this);
    }
    
    ServoActionData& asServoAction() {
      return (ServoActionData&)(*this);
    }
    OutputActionData& asOutputAction() {
      return (OutputActionData&)(*this);
    }
    WaitActionData& asWaitAction() {
      return (WaitActionData&)(*this);
    }
    ControlActionData& asControlAction() {
      return (ControlActionData&)(*this);
    }

    void save(byte idx);
    void load(byte idx);

    void print(char* out);
};

const Action noAction;

struct ActionRef {
private:
  byte    index;
  Action  current;
  const Action* ptr;

private:
public:
  enum {
    noIndex = 0xff,
    tempIndex = 0xfe
  };
  ActionRef() : index(noIndex), ptr(NULL) {
//    Serial.print("Creating action @"); Serial.print((int)&current, HEX); Serial.print(" command "); Serial.println(current.command);
  };
  
  ActionRef(byte aIndex) : index(aIndex), ptr(NULL) {
    current.load(aIndex);
  }

  ActionRef(const Action* aPtr) : 
    index(aPtr == NULL ? noIndex : tempIndex), 
    current(aPtr == NULL ? noAction : *aPtr),
    ptr(aPtr) {};
  boolean isPersistent() { return ptr == NULL; };
  void clear();
  const Action& skip();
  const Action& next();
  const Action& prev();
  const Action& a() { return current; };
  const Action* aptr() { return &current; };
  int i() { return !isPersistent() ? (int)ptr : index; };
  boolean isEmpty();
  boolean isLast() { return isEmpty() || current.isLast(); };

  void free();
  void save();

  void saveTo(byte index);

  void makeLast() { 
    current.makeLast(); 
  }
  void print(char* out);
};

struct ServoActionData {
  // must not use const-int, as it allocates memory and the structure must fit
  // into Action allocation
  enum SpecialPositions {
    servoPredefinedLeft   = 0,
    servoPredefinedRight  = 1,
    servoCustomStart = 2
  };
  
  byte  last    :   1;    // flag, last action
  byte  command :   3;    // instruction

  byte  servoIndex : 4;     // up to 32 servos
  byte  targetPos  : 6;     // 0..180, in 3deg increments

  void moveLeft(byte s) {
    command = servo;
    servoIndex = s;
    setTargetPosition(true);
  }

  void moveRight(byte s) {
    command = servo;
    servoIndex = s;
    setTargetPosition(false);
  }

  void move(byte s, byte angle) {
    command = servo;
    servoIndex = s;
    setTargetPosition(angle);
  }

  boolean isPredefinedPosition() {
    return targetPos < servoCustomStart;
  }

  boolean isLeft() {
    return targetPos == servoPredefinedLeft;
  }

  byte targetPosition() {
    return (targetPos - servoCustomStart) * 3;
  }

  void setTargetPosition(boolean left) {
    command = servo;
    targetPos = left ? servoPredefinedLeft : servoPredefinedRight;
  }

  void setTargetPosition(byte degs) {
    targetPos = (degs / 3) + servoCustomStart;
  }

  void print(char* out);
};
// sanity check
static_assert (sizeof(ServoActionData) <= sizeof(Action), "ServoActionData misaligned");

struct OutputActionData {
    byte  last    :   1;
    byte  command :   3;

    enum OutputFunction {
        outOn, 
        outOff,
        outToggle,
        outPulse,
        outFlash,

        outError
    };
    OutputFunction  fn : 3;    // output function style
    byte  outputIndex : 5;    // up to 32 outputs
    byte  pulseLen : 8;       // or flash index
    boolean nextInvert : 1;
    
    // 3 bits remain
  public:
    void  turnOn(byte output) {
      command = onOff;
      fn = outOn;
      outputIndex = output;
    }

    void turnOff(byte output) {
      command = onOff;
      fn = outOff;
      outputIndex = output;
    }

    void toggle(byte output) {
      command = onOff;
      fn = outToggle;
      outputIndex = output;
    }

    void pulse(byte output) {
      command = onOff;
      fn = outPulse;
      outputIndex = output;
      pulseLen = 0;
    }

    void flash(byte out, byte fl, boolean inv) {
      command = onOff;
      fn = outFlash;
      outputIndex = out;
      pulseLen = fl;
      nextInvert = inv;
    }

    int pulseDelay() {
      return pulseLen * 100;
    }

    void setDelay(byte d) {
      pulseLen = d;
    }

    boolean isOn();

    byte output() {
      return outputIndex;
    }

    void print(char* out);
};
static_assert (OutputActionData::outError <= 7, "Too many output functions");
static_assert (sizeof(OutputActionData) <= sizeof(Action), "Output data too large");

struct  OutputActionPWMData {
    byte  last    :   1;
    byte  command :   3;
    OutputActionData::OutputFunction  fn : 3;    // output function style
    byte  outputIndex : 5;    // up to 32 outputs
};
static_assert (sizeof(OutputActionPWMData) <= 2, "Output data too large");

struct WaitActionData {
  byte  last    : 1;
  byte  command : 3;


  enum {
    indefinitely = 0,   // wait indefinitely, until cancelled
    finishOnce = 1,     // wait for next command to finish
    alwaysFinish = 2,   // wait for all commands to finish
    noWait,             // do not wait for commands
    
    wait,               // wait a specified time

    // conditions
    inputOn,            // wait for input to become on
    inputOff,           // wait for input to become off
    commandInactive,    // wait for command to become inactive

    timerError
  };
  byte waitType : 3;
  boolean invert : 1;
  byte : 0; // padding
  union {
    byte  commandNumber : 6;
    byte  outputNumber : 6;
    unsigned int waitTime : 10;
  };

  int   computeDelay() { return waitTime * 50; }

  void print(char* s);
};

static_assert (sizeof(WaitActionData) <= sizeof(Action), "Wait action data too long");
static_assert (WaitActionData::timerError <= 8, "Too many wait types");

struct ControlActionData {
  byte  last    : 1;
  byte  command : 3;

  enum ControlType : byte {
    ctrlCancel  = 0,
    ctrlJumpIf,
    ctrlExec,
    ctrlWait,
    
    controlError
  };
  enum ConditionType : byte {
    condNone,
    condKey,
    condOutput,
    condServo,
    condCommand,

    condTop
  };

  ControlType control : 3;    // what subcommand
  ConditionType cond : 3;     // condition type
  boolean invert : 1;         // inverted condition
  byte  condIndex : 6;        // condition object number
  byte  trigger : 1;          // trigger, rather than state
  byte  cmdKeyPress : 1;       // exec inverse command
  signed char jmpTarget : 5;  // target of jump, command to cancel

  // 3 bits remain
  void cancel(byte c) {
    control = ctrlCancel;
    jmpTarget = c;
  }

  void print(char* s);
};

static_assert (sizeof(ControlActionData) <= sizeof(Action), "Control action too long");

struct ExecutionState;

class Processor {
  public:
    enum R {
      ignored,
      finished,
      blocked,
      jumped,
      full
    };
    virtual R processAction2(ExecutionState& state) = 0;
    virtual void tick() {};
    virtual void clear() {}
    virtual boolean cancel(const ExecutionState& ac) = 0;
    virtual R pending(ExecutionState& state) {
      return ignored;
    }
};

struct ExecutionState {
  byte    id;       // command ID ?
  boolean blocked;  // if the execution is blocked
  boolean wait;
  boolean waitNext;
  boolean invert;
  // 1 bits remains
  
  ActionRef  action; // the current executing action
  Processor* processor;

  byte  data[3];

  boolean isAvailable() {
    return !blocked && action.isEmpty();
  }

  ExecutionState() : id(0xff), blocked(false), processor(NULL), invert(false) {}
  
  void clear();
};
static_assert (sizeof(ExecutionState) < 20, "Execution state too large");

class Executor {
    static Processor*  processors[MAX_PROCESSORS];

    /**
       Queue of commands to be executed. Actually pointers to (immutable) action chains.
    */
    static ExecutionState queue[QUEUE_SIZE];

    void handleWait(Action* action);
    static bool isBlocked(byte index);

    static void printQ(const ExecutionState& q);
  public:
    Executor();

    static void boot();

    static signed char stateNo(const ExecutionState& s) {
      if (&s < queue) {
        return -1;
      }
      if (&s >= (queue + QUEUE_SIZE)) {
        return -1;
      }
      return (&s - queue) + 1;
    }
    static ExecutionState* state(signed char n) {
      if (n < 1 || n >= QUEUE_SIZE) {
        return NULL;
      }
      return queue + (n - 1);
    }
    static void finishAction2(ExecutionState& state);
    static void addProcessor(Processor*);
    static void process();

    static boolean isRunning(byte id);

    static void clear(boolean interactive);
    //void unblockAction(const Action**);
    static void blockAction(byte index);
    static void finishAction(const Action* action, byte index);

    static void playNewAction();
    static void schedule(const ActionRef&, byte id, boolean inverse);
    static void schedule(const ActionRef&, byte id, boolean inverse, boolean wait);
//    void schedulePtr(const Action*, int id);
    static boolean cancelCommand(byte id);
};

class ScheduledProcessor {
  public:
  virtual void  timeout(unsigned int data) = 0;
};

struct ScheduledItem {
  unsigned int timeout;
  unsigned int data;
  ScheduledProcessor* callback;

  ScheduledItem() {
    timeout = 0;
    data = 0;
    callback = NULL;
  }
  ScheduledItem(unsigned int aT, ScheduledProcessor* aP, unsigned int aD):
    timeout(aT), callback(aP), data(aD) {}
};

class Scheduler2 : public Processor, public ScheduledProcessor {
  static ScheduledItem work[];
  static byte scheduledBottom;
  static byte scheduledCount;
  static void printQ();
public:
  Scheduler2() : Processor() {}
  static void boot();
  void schedulerTick();
  virtual void timeout(unsigned int data) override;
  virtual R processAction2(ExecutionState& state) override;
  virtual bool cancel(const ExecutionState& ac) override;
  static bool schedule(unsigned int timeout, ScheduledProcessor* callback, unsigned int data);
  static void cancel(ScheduledProcessor* callback, unsigned int data);
};

extern Scheduler2 scheduler;

/**
   Handles a single servo PWM output.
*/
class ServoProcessor : public Processor {
  private:
    enum {
        noservo = 0xff,
        noaction = 0xff
    };

    /**
       Current positions of all servos, for smooth movement
    */
    Servo   ctrl;
    ServoProcessor  *linked;
    byte servoIndex;
    byte targetAngle;
    byte servoSpeed;   // how many milliseconds should the servo command last before changing the angle;
    long switchMillis;
    ExecutionState* blockedState;
    byte targetPosCounter;
    signed char servoOutput;
    bool setOutputOn;
    byte pwmPin;
    unsigned long servoMask;

    void moveFinished();
    void setupSelector(byte servo);
  public:
    ServoProcessor();

    void tick();

    /**
       Attaches the processor to a particular pin. Must be called during setup().
    */
    void attach(byte pin, unsigned long servoMask);
    void linkWith(ServoProcessor* link);

    void start(const Action& action, Action** increment); // setup data from the action
    void clear();                     // stop working
    static void clearAll();
    R processAction2(ExecutionState& state) override;
    boolean cancel(const ExecutionState& ac) override;

    boolean available() {
      return servoIndex == noservo;
    }
    boolean isCompatibleWith(byte servoIndex);
};

/**
   Represents output pins of the shift register daisy chain. During loop(), set() and clear()
   can be called. At the end of the loop, if any change has happened, Output will commit the
   new value to the daisy chain.
*/
class Output {
  public:
    static byte  lastOutputState[OUTPUT_BIT_SIZE];
    static byte  newOutputState[OUTPUT_BIT_SIZE];
    Output();

    static void boot();

    /**
       Sets the corresponding bit up
    */
    static void  set(byte outputIndex);

    /**
       Clears the appropriate bit
    */
    static void  clear(byte outputIndex);

    static void  setBit(byte outputIndex, boolean value) {
      if (value) { set(outputIndex); } else { clear(outputIndex); }
    }

    /**
       Sets all bits to zero, immediately. Resets internal state.
    */
    static void  clearAll();

    /**
       Determines if an output is set.
    */
    static bool  isSet(byte index);

    static bool  isActive(byte index);

    /**
       Commits pending changes.
    */
    static void  commit();

    static void  flush();
};


/**
   Finally command describes what input command corresponds to what actions. Each command can execute one or more actions,
   possibly delayed.
*/
struct Command {
    enum {
      noId = 0,
      noAction = 0xff
    };

    enum {
      cmdOn = 0x00,   // triggers on switch ON
      cmdOff,         // triggers on switch OFF
      cmdToggle,      // triggers on switch toggle
      cmdOnCancel,    // ON will start the action, OFF will cancel command
      cmdOffReverts,  // ON will start, OFF will revert
      cmdOnReverts,    // next ON will revert the command
      cmdOffCancel,    // ON will start the action, OFF will cancel command
    };

    byte    input : 6;        // up to 32 inputs, e.g. 8 * 4
    byte    trigger : 3;      // trigger function
    boolean wait:   1;        // wait on action to finish before next one
    byte    actionIndex :  8; // max 255 actions allowed
    byte    id : 6;

    // 5 + 3 + 1 + 8 + 6= 8 + 9 + 6 = 17 + 6 = 23 bits
  public:
    Command() : input(0x3f), actionIndex(noAction), trigger(cmdOff), wait(true) {}
    Command(byte aI, byte aT, bool aW) : input(aI), actionIndex(noAction), trigger(aT), wait(aW) {}

    boolean available() {
      return actionIndex > MAX_ACTIONS;
    }
    
    void init() {
      actionIndex = noAction;
    }

    void free();

    boolean matches(byte i, byte t) {
      return input == i && trigger == t;
    }

    static const Command* find(byte input, boolean state, const Command* from);
    static bool processAll(byte input, boolean state);

    void execute(boolean keyPressed);

    static int findFree();

    void print(char* out);

};

int packedTimeToUnits(int packed) {
  // 0..20 -> n * 50 millis8
  if (packed <= 20) {
    return packed;
  }
  // 20 = 10 * 50 = 500ms, 21 = 1000ms, up to 10 secs
  if (packed <= 40) {
    return (packed - 20) * 10;
  }
  // 50 * 20 = 1sec, up to 30sec
  if (packed <= 60) {
    return (packed - 40) * 20;
  }
  // up to 200sec = 3min 20sec, 5 seconds
  if (packed <= 120) {
    return (packed - 60) * 100;
  }
  // 10 seconds quantum
  return (packed - 100) * 200;
}

static_assert (sizeof(Command) <= 3, "Command too long");

extern int selectedNumber;
extern int lastSelectedNumber;
extern int typedNumber;
extern long lastTypedMillis;
extern boolean shown;
extern boolean wasCancel;
extern byte digitCount;

extern char inputLine[];
extern char *inputPos;

struct LineCommand {
  const char* cmd;
  void (*handler)();

  LineCommand() : cmd(NULL), handler(NULL) {}
};

void registerLineCommand(const char* cmd, void (*aHandler)());


enum ModuleCmd {
  initialize,
  eepromLoad,
  eepromSave,
  status,
  reset,
  dump,
  periodic
};

struct ModuleChain {
  static ModuleChain* head;
  
  byte priority;
  ModuleChain *next;
  void (*handler)(ModuleCmd);

  ModuleChain(const char* n, byte priority, void (*h)(ModuleCmd));

  static void invokeAll(ModuleCmd cmd);
};

//      End keypad extension
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

boolean processKeyInput = false;

const int CURRENT_DATA_VERSION = 0x06;

const int eeaddr_version = 0x00;
const int eeaddr_xbusConfig = 0x01;
const int eeaddr_servoConfig = 0x02;

 // servo positions, min 16 servos
const int eeaddr_servoPositions = 0x35;
static_assert (eeaddr_servoPositions >= eeaddr_servoConfig + 2 + MAX_SERVO * sizeof(ServoConfig), "EEPROM data overflow");
 
// configuration for max 128 actions
const int eeaddr_actionTable = 0x4e;
static_assert (eeaddr_actionTable >= eeaddr_servoPositions + 2 + MAX_SERVO * sizeof(byte), "EEPROM data overflow");
 
const int eeaddr_commandTable = (eeaddr_actionTable + MAX_ACTIONS * sizeof(Action)) + 2;
static_assert (eeaddr_commandTable >= eeaddr_actionTable + 2 + MAX_ACTIONS * sizeof(Action), "EEPROM data overflow");

const int eeaddr_flashTable = (eeaddr_commandTable + MAX_COMMANDS * sizeof(Command)) + 2;
static_assert (eeaddr_flashTable >= eeaddr_commandTable + 2 + MAX_COMMANDS * sizeof(Command), "EEPROM flash overflow");

const int eeaddr_top = (eeaddr_flashTable + MAX_FLASH * sizeof(FlashConfig)) + 2;
static_assert (eeaddr_top < 1020, "Too large data for EEPROM");


