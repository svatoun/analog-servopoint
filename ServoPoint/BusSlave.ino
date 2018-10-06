#include "RS485Frame.h"

ModuleChain busSlaveModule("Xbus", 20, &busSlaveModuleHandler);

byte ownXbusAddr = 2;

volatile int command = -1;
volatile boolean commandOn = false;
volatile CommFrame ackFrame;

void onReceiveError(byte err) {
  // Jen oznamit chybu, nema ani smysl odpovidat
  Serial.println(F("Error on XBUS"));
}

struct RemoteCommand {
  byte    operation;
  byte    commandId;
  boolean pressed;

  RemoteCommand(byte aop, byte acommand, boolean apress) : operation(aop), commandId(acommand), pressed(apress) {}
};

void busSlaveModuleHandler(ModuleCmd cmd) {
  switch (cmd) {
    case initialize:
      setupRS485Ports();
      initBusSlave();
      break;
    case eepromLoad:
      loadBusSlaveEEPROM();
      break;
    case eepromSave:
      saveBusSlaveEEPROM();
      break;
    case status:
      Serial.print(F("XBus address: ")); Serial.println(ownXbusAddr);
      break;
    case dump:
      dumpXbusAddress();
      break;
    case reset:
      ownXbusAddr = 2;
      break;
  }
}

void initBusSlave() {
  loadBusSlaveEEPROM();
  registerLineCommand("XDR", &commandXbusAddress);
}

void loadBusSlaveEEPROM() {
  byte a = EEPROM.read(eeaddr_xbusConfig);
  if (a > 0) {
    ownXbusAddr = a;
    Serial.print(F("XBus address set to: ")); Serial.println(a);
  }
}

void saveBusSlaveEEPROM() {
//  Serial.print(F("XBus address saved: ")); Serial.println(ownXbusAddr);
  EEPROM.write(eeaddr_xbusConfig, ownXbusAddr);
}

const int blinkTiny[] = { 50, 50, 0 };   // blink once, short

long recvMillis = 0;

void onReceivedMessage(const CommFrame& frame) {
  if (frame.to != ownXbusAddr) {
    makeLedAck(&blinkTiny[0]);
    return;
  }
  const RemoteCommand& rc =  *(const RemoteCommand*)(&frame.dataStart);
  if (rc.operation != 1 || command != -1) {
    return;
  }
  command = rc.commandId;
  commandOn = rc.pressed;

  ackFrame.from = ownXbusAddr;
  ackFrame.to = frame.from;
  ackFrame.len = 1;
  ackFrame.dataStart = recvChecksum;
  recvMillis = millis();
}

void processReceivedCommand() {
  if (command == -1) {
    // pripadny timeout pri receive
    periodicReceiveCheck();
    return;
  }

  if (recvMillis > 0 && currentMillis < recvMillis + 5) {
    return;
  }

  recvMillis = 0;

/*
  if (!transmitSingle(false)) {
    return;
  }
*/

  byte c = command;
  boolean o = commandOn;

  makeLedAck(&blinkLong[0]);

  // musi se ihned odeslat odpoved
  transmitFrame(&ackFrame);
  while (!transmitSingle(false));

  // odted je mozne prijmout dalsi command
  command = -1;

  // Konecne zpracujeme prijatou "klavesu"
  Serial.print(F("ProcessKey: ")); Serial.print(c); Serial.print('-'); Serial.println(o);

  boolean state = isKeyPressed(c);
  boolean now = o != 0;
  if (state != now) {
    processKey(c, o);
  }
}

void dumpXbusAddress() {
  Serial.print(F("XDR:")); Serial.println(ownXbusAddr);
}
void commandXbusAddress() {
  int a = nextNumber();
  if (a == -2) {
    dumpXbusAddress();
    return;
  }
  if (a == 1) {
    Serial.println(F("Address 1 is reserved."));
    return;
  }
  if (a < 2) {
    Serial.println(F("Bad addr"));
    return;
  }
  ownXbusAddr = a;
}

