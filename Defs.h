#ifndef __defs_h__
#define __defs_h__

#define SERVOS

const int ROWS = 4;
const int COLS = 4;

//------------------------------- PIN ASSIGNMENT ------------------------------------
/**
 * ACK LED, will blink in response to setup keypresses
 * and status
 */
const int LED_ACK = 13;

const int latchPin  = A0;
const int clockPin  = A1;
const int dataPin   = 13;
//const int shiftOutDisable = 12;

// A0 .. A5 (6x), D2, D3, D6 = keypad, 20 klaves
// D7, D8, D9 = seriove rizeni vystupu
// D4, D5 = I2C komunikace
// D11 - PWM pro serva
// zbyva: D10, D12, D13
// zbyva: A6, A7

const int pwmPinA = 6;
const int pwmPinB = 9;

const int servoSelectA = 7;
const int servoSelectB = 8;
//const int servo18Enable = 12;

const int servoPowerA = A2;
const int servoPowerB = 5;
const int servoPowerC = A3;
const int servoPowerD = 12;

/**
 * Configuration for the keypad / control buttons. Buttons are sequentially numbered to generate orinals - indexes into the command table.
 */
const byte rowPins[ROWS] = { A4, A5, A6, A7 };
const byte colPins[COLS] = { 2, 3, 4, A1 };


#define SHIFTPWM_PHYSICAL_PINS
#define PORT_LATCH  PC
#define BIT_LATCH 0

#define PORT_DATA PB
#define BIT_DATA 5

#define PORT_CLOCK PC 
#define BIT_CLOCK 1

//------------------------------- END PIN ASSIGNMENT ------------------------------------

const int maxId = ROWS * COLS;

const int QUEUE_SIZE = 10;          // # of parallel executions
const int MAX_ACTIONS = 128;        // # of possible actions
const int MAX_COMMANDS = 32;        // # of defined commands
const int MAX_WAIT_COUNT = 10;      // # of suspended executions
const int MAX_SERVO = 16;           // # of controlled servos
const int MAX_OUTPUT = 64;          // # of on/off outputs from the shift register
const int MAX_INPUT_BUTTONS = 16;   // # of input buttons
const int MAX_SPEED = 8;            // servo speed
const int MAX_FLASH = 4;

const int MAX_SCHEDULED_ITEMS = 16;
const int MAX_PENDING_FLASHES = 8;

const int MAX_KEYS = (ROWS * COLS);
const int OUTPUT_BIT_SIZE = (MAX_OUTPUT + 7) / 8;
const int SERVO_BIT_SIZE = (MAX_SERVO + 7) / 8;
const int COMMAND_BIT_SIZE = (MAX_COMMANDS + 7) / 8;

const int defaultPulseDelay = 100;

/**
 * Maximum number of processors. Each servo processor counts as 1 !
 */
const int MAX_PROCESSORS = 20;

#define MILLIS_SCALE_FACTOR 50

/**
 * If != 0, setup control routines will print diagnostic messages over serial line.
 */

#ifdef DEBUG
const int debugControl = 0;
const int debugExecutor = 0;
const int debugServo = 0;
const int debugServoMove = 0;
const int debugOutput = 0;
const int debugCommands = 0;
const int debugInfra = 0;
const int debugInput = 0;
const int debugPower = 0;
const int debugSchedule = 0;
const int debugFlash = 0;
const int debugConditions = 0;
#else
const int debugControl = 0;
const int debugExecutor = 0;
const int debugServo = 0;
const int debugServoMove = 0;
const int debugOutput = 0;
const int debugCommands = 0;
const int debugInfra = 0;
const int debugInput = 0;
const int debugPower = 0;
const int debugSchedule = 0;
const int debugFlash = 0;
const int debugConditions = 0;
#endif

/**
 * Pins controlling the output
 */
const int numberOfLatches   = 1;
static_assert (numberOfLatches <= OUTPUT_BIT_SIZE, "Invalid number of latches");

// FIXME: no translation necessary.
char keys[ROWS][COLS]  = { { 1, 2, 3, 4, }, { 5, 6, 7, 8, }, { 9, 10, 11, 12, }, { 13, 14, 15, 16, }, };

const char keyTranslation[] = { 0, '1', '2', '3', '+', 
                                 '4', '5', '6', '-', 
                                 '7', '8', '9', 'E', 
                                 '*', '0', '#', '.', 
                                 'J', 'K', 'L', 'M', 
};

#endif

