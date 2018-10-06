#ifndef __rs485_frame_h__
#define __rs485_frame_h__
#undef USE_CRC

/**
 * Velikost prijmoveho bufferu v byte. Musi byt delsi nez nejdelsi zpracovavany packet.
 * Delsi packety se ani nezaznamenavaji a zahazuji rovnou.
 */
// const int recvBufferSize = 20;

/**
 * Kolik ms po zacatku prijmu muze prijit start byte. 
 */
const int recvDelayStartByte = 30;

/**
 * Kolik ms smi uplynout mezi jednotlivymi byte packetu.
 */
const int recvDelayBetweenPacketBytes = 8;

typedef uint8_t address_t;
typedef uint8_t len_t;

/**
 * Struktura packetu. 
 */
struct CommFrame {
  /**
   * Pouzito ve vyssi vrstve. Je to neciste, ale napsat to "spravne" je slozitejsi.
   */
  byte  retryCount;
  /**
   * Delka vlastnich dat.
   */
  len_t       len;
  /**
   * Cil packetu
   */
  address_t   to;
  /**
   * Odesilatel
   */
  address_t   from;
  /** 
   *  Prvni byte vlastnich prenasenych dat. Dalsi bezprostredne nasleduji, v bufferu musi byt prideleno dost mista pro celou zpravu.
   */
  byte  dataStart;

  CommFrame() : from(0), to(0), len(1), retryCount(0) {}
  CommFrame(address_t af, address_t at, byte l) : from(af), to(at), len(l), retryCount(0) {}

  const byte* data() const {
    return &dataStart;
  }

  /**
   * Adresa nasledujiciho ramce v bufferu.
   */
  CommFrame* next() {
    return (CommFrame*)(&dataStart + len);
  }

  /**
   * Kolik byte se ma preskocit v bufferu. Zahrnuje ta
   */
  static byte skipSize(len_t payloadLen) {
    return frameSize(payloadLen) + sizeof(byte /* retrycount */);
  }

  byte bufferSize() {
    return frameSize() + sizeof(byte /* retrycount */);
  }

  byte frameSize() {
    return frameSize(len);
  }

  /**
   * Kolik byte ma prenaseny ramec, vcetne delky. Nepocita se chcecksum. Slouzi
   * pro nastaveni pocitadel pro odesilani a prijem.
   */
  static byte frameSize(len_t payloadLen) {
    return payloadLen + (sizeof(to) + sizeof(from) + sizeof(len_t));
  }

  void printStat() {
    Serial.print(F("From:")); Serial.print(from); Serial.print(F(" to:")); Serial.print(to); Serial.print(" len:"); Serial.print(len); Serial.print(" retr:"); Serial.println(retryCount);
  }
};

boolean isBroadcast(address_t add) {
  return add == (1 << (sizeof(address_t))) - 1;
}

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

extern checksum_t recvChecksum;
const int checksumSize = sizeof(checksum_t);

enum ReceiveError {
  errNone = 0,
  errChecksum,
  errLong,
  errTimeout,
  errUnexpected,
  errTimeoutStart
};


#endif

