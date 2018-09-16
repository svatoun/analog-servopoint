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
      loadBusSlaveEEPROM();
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
      break;
    case reset:
      ownXbusAddr = 2;
      break;
  }
}

void loadBusSlaveEEPROM() {
  byte a = EEPROM.read(eeaddr_xbusConfig);
  if (a > 0) {
    ownXbusAddr = a;
    Serial.print(F("XBus address set to: ")); Serial.println(a);
  }
}

void saveBusSlaveEEPROM() {
  Serial.print(F("XBus address saved: ")); Serial.println(ownXbusAddr);
  EEPROM.write(eeaddr_xbusConfig, ownXbusAddr);
}

void onReceivedMessage(const CommFrame& frame) {
  if (frame.to != ownXbusAddr) {
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
}

void processReceivedCommand() {
  if (command == -1) {
    // pripadny timeout pri receive
    periodicReceiveCheck();
    return;
  }
  
  byte c = command;
  boolean o = commandOn;

  // musi se ihned odeslat odpoved
  transmitFrame(&ackFrame);
  while (!transmitSingle(false));
  
  // odted je mozne prijmout dalsi command
  command = -1;

  // Konecne zpracujeme prijatou "klavesu"
  processKey(c, o);
}

