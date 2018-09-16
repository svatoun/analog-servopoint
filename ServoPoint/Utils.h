extern long currentMillis;
extern unsigned int currentMillisLow;

const byte addressBroadcast = 0xff;

#if defined(__AVR_ATmega328P__)  // Arduino UNO, NANO
/**
 * Write all 4 address lines at once
 */
inline void selectDemuxLine(byte line) {
  byte pb = PORTB & 0xf0;
  pb |= (line & 0x0f);
  PORTB = pb;
}
#else
#   error "Unsupported board version"
#endif

void recordStartTime(unsigned int& store) {
  store = currentMillis & 0xffff;
}

/**
 * Quick check of the elapsed time, uses integer-based time to record delays in mills,
 * NO LONGER THAN 0xffff millis = 65 seconds.
 */
boolean elapsedTime(unsigned int &msecStart, unsigned int d) {
  unsigned int x;
  if (msecStart > currentMillisLow) {
    // currentMillisLow overflowed, so the delay
    // is from msecStart to 0x10000 (complement of msecStart) + currentMillisLow (which is
    // above 0x10000.
    x = currentMillisLow + (~msecStart);
  } else {
    x = currentMillisLow - msecStart;
  }
  boolean ret = x >= d;
  if (ret) {
    recordStartTime(msecStart);
  }
}

boolean readBit(const byte* storage, int index) {
  byte i = index >> 3;
  byte m = 1 << (index & 0x03);
  return (*(storage + index) & m) > 0;
}

boolean writeBit(const byte* storage, int index, boolean state) {
  byte i = index >> 3;
  byte m = 1 << (index & 0x03);
  byte *p = storage + index;

  if (state) {
    *p = (*p & ~m) | m;
  } else {
    *p = (*p & ~m); 
  }
}

// Describes a pending input or output change.
struct PendingChange {
  enum Phase {
    none = 0,             // the slot is free to use
    debounce,             // the change is being debounced, the counter says how long the change must be stable
    flash,                // the output should flash. Counter will count down the number of phase changes.
    
    stateEnd
  };                      
  /* 8 bit */   byte index;             // index of the output
  /* 1 bit */   boolean newState : 1;   // to-be-state on/off
  /* 2 bit */   Phase phase : 2;        // phase of the change
  /* 5 bit */   byte counter : 5;       // debounce or flash counter

  PendingChange() : phase(none), index(0) {}

  void print() {
    Serial.print("i:"); Serial.print(index); Serial.print(" s:"); Serial.print(newState);
    Serial.print(" p:"); Serial.print(phase); Serial.print(" c:"); Serial.print(counter);
  }
};

typedef boolean (*CompletedCallback)(PendingChange& ch);
class ChangeBuffer {
  const byte bufItems = sizeof(buf) / sizeof(buf[0]);
  const byte initialCounter;
  
  const byte byteSize;
  byte *stableState;
  byte *rawState;
  byte *changes;
  
  // individual recorded changes
  PendingChange   buf[20];
  byte head;
  byte tail;
  boolean empty;
  
  public:
  ChangeBuffer(byte aSize, byte* aStable, byte *aRaw, byte* aChanges, byte initCounter) : 
    byteSize(aSize), stableState(aStable), rawState(aRaw), changes(aChanges), 
    head(0), tail(0), initialCounter(initCounter), empty(true) {}
    
  boolean isEmpty() { return empty; }
  boolean isFull() { return !empty && (head == tail); }
  PendingChange *get(byte index);

  void markPending(byte index);
  void clearPending(byte index);
  PendingChange* allocate(byte index);
  void free(PendingChange& ch);
  void updateInputRow(unsigned int rowData, byte rowIndex);
  void recordChanges(const byte *rowData, byte startIndex, byte startFrom, byte count);
  virtual void recordOneChange(byte index, boolean state, byte counter);
  void compact();
  void tickPhase(PendingChange::Phase phase, CompletedCallback* callback);

  void printFull();
  void printRing();
};


#define USE_CRC

#ifdef USE_CRC
#include <util/crc16.h>
#define CRC_INIT_VAL 0xffff
typedef uint16_t checksum_t;
void initChecksum(checksum_t& c) {
  c = CRC_INIT_VAL;
}
void checksumUpdate(checksum_t& c, byte data) {
  c = _crc_xmodem_update (c, data);  
}

boolean verifyChecksum(checksum_t expected, checksum_t got) {
  return expected == got;
}
#else
typedef uint8_t checksum_t;

void initChecksum(checksum_t& c) {
  c = 0;
}
void checksumUpdate(checksum_t& c, byte data) {
  c ^= data;
}

boolean verifyChecksum(checksum_t expected, checksum_t got) {
  return expected == 0;
}
#endif

