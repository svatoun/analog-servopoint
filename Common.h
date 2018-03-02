#include "Defs.h"

const int OUTPUT_DELAY = 0;     //
const int OUTPUT_OFF = 0;       // switch the output off
const int OUTPUT_ON = 1;        // switch the output on
const int OUTPUT_ON_PULSE = 2;  // pulse the output
const int OUTPUT_TOGGLE = 3;    // toggle the output on-off

const int newCommandPartMax = 10;

bool setupActive = false;

char pressedKey;

enum Instr {
  none = 0,
  servoPredef,  // 1  control a servo
  servoPos,     // 2  specific position of a servo
  wait,         // 3  wait before executing next command
  onOff,        // 4  turn the output on and off
  pulse,        // 5  pulse the output
  cancel,       // 6  cancel pending action
  flash,        // 7  flash in some frequency,
  skipIf        // skip if condition (not) met
};

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
const int servoPwmStep = 3;

struct ServoConfig {
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

  ServoConfig() : pwmFirst(90), pwmSecond(90), servoSpeed(0) {
  }

  boolean isEmpty() {
    return (pwmFirst == 90) && (pwmSecond == 90) && (servoSpeed == 0);
  }

  void setLeft(int l) {
    if (l < 0 || l > 180) {
      return;
    }
    pwmFirst = (l / servoPwmStep);
  }
  void setRight(int r) {
    if (r < 0 || r > 180) {
      return;
    }
    pwmSecond = (r / servoPwmStep);
  }
  void setSpeed(int s) {
    if (s < 0) {
      s = 0;
    } else if (s >= 8) {
      s = 7;
    }
    servoSpeed = s;
  }

  int speed() {
    if (isEmpty()) {
      return 2;
    }
    return servoSpeed;
  }
  int left() {
    if (isEmpty()) {
      return 90;
    }
    return pwmFirst * servoPwmStep;
  }
  int right() {
    if (isEmpty()) {
      return 90;
    }
    return pwmSecond * servoPwmStep;
  }
  int pos(bool dir) {
    return dir ? left() : right();
  }

  static int change(int orig, int steps) {
    int x = orig + steps * servoPwmStep;
    if (x < 0) {
      return 0;
    } else if (x > 180) {
      return 180;
    }
    return x;
  }

  static int value(int set) {
    return (set / servoPwmStep) * servoPwmStep;
  }

  void clear() {
    pwmFirst = pwmSecond = 0;
    servoSpeed = 0;
  }

  void print(int id, String& s) {
    s.concat("RNG:");
    s.concat(id);
    s.concat(':');
    s.concat(left());
    s.concat(':');
    s.concat(right());
    s.concat(':');
    s.concat(servoSpeed + 1);
  }
};

typedef void (*ActionRenumberFunc)(int, int);

struct Action;
/**
   Action defines the step which should be executed.
*/
typedef void (*dumper_t)(const Action& , String&);

struct Action {
    byte  last    :   1;    // flag, last action
    byte  command :   4;    // instruction

    unsigned int data : 11; // action type-specific data

    static Action actionTable[MAX_ACTIONS];
    static ActionRenumberFunc renumberCallbacks[];
    static dumper_t dumpers[];
    static int  top;
  public:
    Action();
    Action(const Action& data);

    static void registerDumper(Instr cmd, dumper_t dumper);

    static void initialize();
    static void renumberCallback(ActionRenumberFunc f);

    static Action* get(int index);
    
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
    const Action* next();

    void notLast() {
      last = 0;
    }

    static int copy(const Action* a, int);

    static int findSpace(int size);

    ServoActionData& asServoAction() {
      return (ServoActionData&)(*this);
    }
    OutputActionData& asOutputAction() {
      return (OutputActionData&)(*this);
    }
    WaitActionData& asWaitAction() {
      return (WaitActionData&)(*this);
    }

    void print(String& s);
};

const int servoPredefinedLeft   = 0;
const int servoPredefinedRight  = 1;

struct ServoActionData {
  byte  last    :   1;    // flag, last action
  byte  command :   4;    // instruction

  byte  servoIndex : 5;     // up to 32 servos
  byte  targetPos  : 6;     // 0..180, in 3deg increments

  void moveLeft(int s) {
    command = servoPredef;
    servoIndex = s;
    setTargetPosition(true);
  }

  void moveRight(int s) {
    command = servoPredef;
    servoIndex = s;
    setTargetPosition(false);
  }

  void move(int s, int angle) {
    command = servoPos;
    servoIndex = s;
    setTargetPosition(angle);
  }

  boolean isPredefinedPosition() {
    return command == servoPredef;
  }

  boolean isLeft() {
    return targetPos == servoPredefinedLeft;
  }

  int targetPosition() {
    return (targetPos) * 3;
  }

  void setTargetPosition(boolean left) {
    command = servoPredef;
    targetPos = left ? servoPredefinedLeft : servoPredefinedRight;
  }

  void setTargetPosition(int degs) {
    targetPos = (degs / 3);
  }

  void print(String& out) {
    if (isPredefinedPosition()) {
      out.concat("MOV:"); out.concat(servoIndex + 1); out.concat(":");
      out.concat(isLeft() ? "L" : "R");
    } else {
      out.concat("MOV:"); out.concat(servoIndex + 1); out.concat(":");
      out.concat(targetPosition());
    }
  }
};

struct OutputActionData {
    byte  last    :   1;
    byte  command :   4;

    byte  makeOn    : 2;      // true - turn on. False - turn off.
    byte  outputIndex : 5;    // up to 32 outputs

  public:
    void  turnOn(int output) {
      command = onOff;
      makeOn = 1;
      outputIndex = output;
    }

    void turnOff(int output) {
      command = onOff;
      makeOn = 0;
      outputIndex = output;
    }

    void toggle(int output) {
      command = onOff;
      makeOn = 2;
      outputIndex = output;
    }

    boolean isOn();

    void print(String& s) {
      s.concat("OUT:"); s.concat(outputIndex); s.concat(":");
      switch (makeOn) {
        case 0: s.concat("H"); break;
        case 1: s.concat("L"); break;
        case 2: s.concat("T"); break;
        default: s.concat("E"); break;
      }
    }
};


struct PulseActionData {
  byte  last    :   1;
  byte  command :   4;

  byte  outputIndex : 5;
  byte  units      : 2;
  byte  pulseDelay : 4;

  void pulse(int t) {
    command = ::pulse;
    if (t < 16) {
      units = 0;
      pulseDelay = t;
    } else if (t < 80) {
      units = 1;
      pulseDelay = t / 5;
    } else if (t < 160) {
      units = 2;
      pulseDelay = t / 10;
    } else {
      return;
    }
  }

  int getPulseDelay() {
    switch (units) {
      case 0:
        return pulseDelay;
      case 1:
        return pulseDelay * 5;
      case 2:
        return pulseDelay * 10;
    }
  }

  void print(String& s) {
    s.concat("PLS:"); s.concat(outputIndex); s.concat(":"); s.concat(getPulseDelay() * 10);
  }
};

struct WaitActionData {
  byte  last    : 1;
  byte  command : 4;
  
  int   waitTime : 11;

  int   computeDelay();
};

class Processor {
  public:
    enum R {
      ignored,
      finished,
      blocked
    };
    virtual R processAction(const Action& ac, int handle) = 0;
    virtual void tick() {};
    virtual void clear() {}
    virtual boolean cancel(const Action& ac) = 0;
};

struct ExecutionState {
  int id : 5;
  boolean blocked : 1;
  const Action* action;

  boolean isAvailable() {
    return !blocked && action == NULL;
  }

  ExecutionState() : id(0), blocked(false), action(NULL) {}
  void clear() {
    blocked = false;
    action = NULL;
  }
};

class Executor {
    Processor*  processors[MAX_PROCESSORS];

    /**
       Queue of commands to be executed. Actually pointers to (immutable) action chains.
    */
    ExecutionState queue[QUEUE_SIZE];
    byte  timeoutCount;

    long lastMillisTick;
    int waitIndex;

    void handleWait(Action* action);
    bool isBlocked(int index);

  public:
    Executor();

    void addProcessor(Processor*);
    void process();

    void clear();
    //void unblockAction(const Action**);
    void blockAction(int index);
    void finishAction(const Action* action, int index);

    void playNewAction();
    void schedule(const Action*, int id);
    void schedule(const Action*);
};

/**
   Handles a single servo PWM output.
*/
class ServoProcessor : public Processor {
  private:
    /**
       Current positions of all servos, for smooth movement
    */
    Servo   ctrl;
    ServoProcessor  *linked;
    int  servoIndex;
    int  targetAngle;
    byte servoSpeed;   // how many milliseconds should the servo command last before changing the angle;
    long prevMillis;
    int actionIndex;
    const Action* blockedAction;
    byte targetPosCounter;

    byte pwmPin;
    unsigned long servoMask;

    void moveFinished();
    void setupSelector(int servo);
  public:
    ServoProcessor();

    void tick();

    /**
       Attaches the processor to a particular pin. Must be called during setup().
    */
    void attach(int pin, unsigned long servoMask);
    void linkWith(ServoProcessor* link);

    void start(const Action& action, Action** increment); // setup data from the action
    void clear();                     // stop working
    static void clearAll();

    R processAction(const Action& ac, int handle) override;
    boolean cancel(const Action& ac) override;

    boolean available() {
      return servoIndex == -1;
    }
    boolean isCompatibleWith(int servoIndex);
};

/**
   Represents output pins of the shift register daisy chain. During loop(), set() and clear()
   can be called. At the end of the loop, if any change has happened, Output will commit the
   new value to the daisy chain.
*/
class Output {
  private:
    byte  lastOutputState[OUTPUT_BIT_SIZE];
    byte  newOutputState[OUTPUT_BIT_SIZE];

  public:
    Output();

    /**
       Sets the corresponding bit up
    */
    void  set(int outputIndex);

    /**
       Clears the appropriate bit
    */
    void  clear(int outputIndex);

    /**
       Sets all bits to zero, immediately. Resets internal state.
    */
    void  clearAll();

    /**
       Determines if an output is set.
    */
    bool  isSet(int index);

    /**
       Commits pending changes.
    */
    void  commit();

    void  flush();
};

/**
   Finally command describes what input command corresponds to what actions. Each command can execute one or more actions,
   possibly delayed.
*/
struct Command {
    byte    input : 5;        // up to 32 inputs, e.g. 8 * 4
    boolean on :    1;        // flag: act on 1 = pressed, 0 = released
    boolean wait:   1;        // wait on action to finish before next one
    byte    actionIndex :  8; // max 255 actions allowed
  public:
    Command() : input(0), actionIndex(0xff), on(false), wait(true) {}
    Command(int aI, boolean aO, boolean aW, int aN) : input(aI), on(aO), wait(aW), actionIndex(aN) {}
    Command(int aI, boolean aO, boolean aW) : input(aI), on(aO), wait(aW), actionIndex(0xff) {}
    Command(const Command& copy) : input(copy.input), on(copy.on), wait(copy.wait), actionIndex(copy.actionIndex) {}

    boolean available() {
      return actionIndex > MAX_ACTIONS;
    }
    
    void init() {
      actionIndex = 0xff;
    }

    void free();

    boolean matches(int i, boolean o) {
      return input == i && on == o;
    }

    static const Command* find(int input, boolean state);

    void execute();

    boolean matches(const Command& c) {
      return matches(c.input, c.on);
    }

    void print(String& s);

    Command& operator =(const Command& other);
};

class NumberInput {
  public:
    const int minValue;
    const int maxValue;

    int selectedNumber;
    int typedNumber = -1;
    long lastTypedMillis;
    boolean shown;
    boolean wasCancel;
    byte digitCount;
  public:
    NumberInput(int aMin, int aMax) : minValue(aMin), maxValue(aMax), selectedNumber(aMin), lastTypedMillis(0), shown(false), wasCancel(false), digitCount(0) {}

    void handleKeyPressed();
    void handleIdle();

    virtual void clear();
    virtual void reset() {};
    virtual int getValue() {
      return selectedNumber;
    }
    virtual int setValue(int a) {
      return a;
    }
    virtual int incValue(int v, int step) {
      return v + step;
    }
    virtual void  finished(int) {}
    virtual void  cancelled();
    virtual void  showIdle(int) {}
    virtual void displayCurrent(int, int) {}
    void display();

    static void set(NumberInput* input);

  protected:
    int acceptTypedNumber();
};

struct LineCommand {
  const char* cmd;
  void (*handler)(String& );

  LineCommand() : cmd(NULL), handler(NULL) {}
};

void registerLineCommand(const char* cmd, void (*aHandler)(String& ));


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

  ModuleChain(const String& n, byte priority, void (*h)(ModuleCmd));

  static void invokeAll(ModuleCmd cmd);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Analog input keypad extension
class KeypadAnalog : public Keypad {
  public:
    KeypadAnalog(char *userKeymap, byte *row, byte *col, byte numRows, byte numCols) : Keypad(userKeymap, row, col, numRows, numCols) {}

    virtual void pin_mode(byte pinNum, byte mode);
    virtual void pin_write(byte pinNum, boolean level);
    virtual int  pin_read(byte pinNum);
};

void KeypadAnalog::pin_mode(byte pinNum, byte mode) {
  if (pinNum >= A6) {
    // ignore pinMode for analog inputs
    return;
  }
  Keypad::pin_mode(pinNum, mode);
}

void KeypadAnalog::pin_write(byte pinNum, boolean level) {
  if (pinNum >= A6) {
    return;
  }
  Keypad::pin_write(pinNum, level);
}

int KeypadAnalog::pin_read(byte pinNum) {
  if (pinNum < A6) {
    return Keypad::pin_read(pinNum);
  }

  // attempt to read analog
  return (analogRead(pinNum - A0) > 300) ? 1 : 0;
}

//      End keypad extension
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

class NumberInput;
NumberInput *numberInput = NULL;

void NumberInput::set(NumberInput * input) {
  if (numberInput != NULL) {
    delete numberInput;
  }
  numberInput = input;
}

// configuration for 16 servos, padded to 16byte boundary
const int eeaddr_servoConfig = 0x00;

// servo positions, min 16 servos
const int eeaddr_servoPositions = 0x28;

// configuration for max 200 actions
const int eeaddr_actionTable = 0x40;

const int eeaddr_commandTable = (eeaddr_actionTable + sizeof(Action::actionTable)) + 2;

const int eeaddr_top = (eeaddr_commandTable + MAX_COMMANDS * 2) + 2;

