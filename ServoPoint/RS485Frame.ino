/**
 * Vysilac - prijimac packetu po RS485. Vysilac odesila packety se strukturou CommFrame (bez prvniho byte - retryCount).
 * Packet se vysila ve tvaru:
 * <start byte> <delka> <cil> <zdroj> <data....> <kontrolni soucet>
 * 
 * Start byte je specialni znak (`startByteChar`), ktery se nikde jinde, nez na zacatku packetu nevyskytuje. V pripade, ze je prijat startByte v prubehu prijmu jineho 
 * packetu, je tento zahozen (protoze uz mel asi davno skoncit) a prijimac zacne cist packet od zacatku.
 * 
 * Aby se start byte nevyskytl nikde jinde, je jakykoliv vyskyt hodnoty `escapeChar` - `escapeTop` preveden na dvojici <escapeChar> <data ^ escapechar>. Tim se zajisti,
 * ze se hodnoty 0x7d-0x7f nevyskytuji NIKDE nez na definovanych mistech.
 * 
 * Kontrolni soucet se pocita jako jednoduchy XOR vsech vyslanych byte, tedy i start byte, adresnich hlavicek, delky ... a pote se slozi (XOR) s kontrolnim souctem a 
 * vysledkem je, pri neporusenych datech, 0.
 * 
 * Modul registruje rutinu preruseni isrReceiveData. Z preruseni pak vola 
 *        void onReceiveData(const CommFrame&);
 * v pripade prijeti spravneho ramce (zkontrolovaly kontrolni soucet), nebo
 *        void onReceiveError(int errCode);
 * pokud dojde k chybe. POZOR - mohou se vyvolat i zcela nezavisle, kdykoliv je transceiver zapnuty na prijem a prijdou data ! Data je nutne ihned okopirovat, misto bude
 * pouzito pro prijem dalsich byte. Ve chvili kdy interrupt funkce zaregistruje data, prepne se modul do rezimu cteni (isReceiving() == true); snazi se chytit na start byte.
 * 
 * Pri vysilani je NUTNE periodicky volat 
 *  byte transmitSingle()
 *  byte periodicReceiveCheck()
 * Funkce vysle jeden byte ramce, pripadne jej obali ESC znakem (takze posila max 2 byte). Funkce vraci 0, pokud se bude i nadale vysilat, 1 pokud vysilani skoncilo, a 2, 
 * pokud se vubec nic nevysilalo (klidovy stav). Volajici se muze rozhodnout prerusit vysilani dat a provadet neco jineho. Pozor ale na nastaveni `recvDelayBetweenPacketBytes`
 * u prijimace: bude-li prodleva mezi byte prilis velka, packet zahodi. Pri vysilani rychlosti 9600 baud trva odeslani jednoho byte cca 10 * 1/9600 = 1ms, tedy vyvolani
 * transmitSingle() [spolecne s pripadnym esc] trva max 2ms. Odeslani minimalne velkeho ramce pak trva 6-10ms (velmi nepravdepodobny pripad, kde je vsude esc).
 * 
 * Prijimaci cast v preruseni strada data do prijmoveho bufferu, ktery po dokonceni byte oznami "callback" funkci `onReceiveData`. NENI ZARUCENO ze po navratu z `onReceiveData` 
 * se do bufferu opet nezacne zapisovat (dalsi prijimana data). `onReceiveData` a `onReceiveError` mohou bezet v kontextu preruseni, vsechny promenne se kterymi pracuji
 * musi byt `volatile`.
 */

const boolean debug485Frame = false;
const boolean debug485Recv = false;

#include "RS485Frame.h"

/**
 * Znacka oznamujici zacatek dat. Prijimac se podle ni synchronizuje, vypadne-li ze souladu
 * s vysilacem.
 */
const byte startByteChar = 0x7f;

/**
 * Znak escape.
 */
const int escapeChar = 0x7d;

/**
 */
const int escapeTop = 0x7f;

/**
 * Buffer pro prijem dat z ISR
 */
byte recvBuffer[recvBufferSize + 2];

/**
 * Konec bufferu pro prijem dat
 */
const byte* recvBufferLimit = recvBuffer + recvBufferSize;

#ifdef NEO
NeoSWSerial commSerial( rs485Receive, rs485Send );
#else 
SoftwareSerial commSerial( rs485Receive, rs485Send );
#endif

enum CommPhase {
  idle = 0,     // necinny, necekaji se data. Data jsou povazovana za chybu / ignorovana
  startByte,    // ceka se start byte
  length,       // ceka se delka
  payload,      // vlastni obsah packetu
  escape,       // prisel escape, dalsi znak se musi prekodovat
  checksum,     // ceka se kontrolni soucet
  discard       // analogie payload, ale data se zahazuji. Prilis dlouhy ramec.
};

/**
   The 'checksum' XOR byte accumulated during transmission
*/
checksum_t xmitXor;

/**
 * Pocitadlo znaku k odeslani
*/
len_t xmitCounter;

/**
 * Faze odesilani. V pripade escape se docasne faze ulozi do xmitPhase2.
 */
CommPhase xmitPhase = idle;
CommPhase xmitPhase2 = idle;

/**
 * Pointer na odesilana data. V klidovem stavu nastaveny na NULL
 */
const byte *xmitPtr = NULL;

void setupRS485Ports() {
  pinMode(rs485Receive, INPUT);
  digitalWrite(rs485Send, HIGH);
  pinMode(rs485Direction, OUTPUT);
  pinMode(rs485Send, OUTPUT);

#ifdef NEO
  commSerial.attachInterrupt(&isrReceiveData);
#endif
  commSerial.begin(9600);
}

void transmitFrame(const CommFrame* p) {
  if (isTransmitting()) {
    return;
  }
  xmitPtr = &(p->len);
  xmitCounter = CommFrame::frameSize(p->len);
  initChecksum(xmitXor);
  xmitPhase = startByte;

  if (debug485Frame) {
    Serial.print(F("xmit:")); Serial.println(xmitCounter);
  }
  digitalWrite(rs485Send, HIGH);
  digitalWrite(rs485Direction, HIGH);
}

boolean isTransmitting() {
  return xmitPhase != idle;
}

/**
 * Odesle jediny byte primo do RS485 transcieveru. Low-level
 */
void rs485SendRawByte(byte b) {
  // ensures that xmitXor has the checksum value
  if (xmitPhase != checksum) {
    checksumUpdate(xmitXor, b);
  }
  commSerial.write(b);
  if (debug485Frame) {
    Serial.print(F("485-send: ")); Serial.print(b, HEX); Serial.print(F(" ck:")); Serial.println(xmitXor, HEX);
    
  }
}

/**
 * Odesle jeden datovy byte, v pripade potreby provede escape.
 */
void xmitOneByte(byte b) {
  if (b >= escapeChar && b <= escapeTop) {
    rs485SendRawByte(escapeChar);
    rs485SendRawByte(b ^ escapeChar);
  } else {
    rs485SendRawByte(b);
  }
}

/**
 * Zastavi prenos, prepne transciever na prijem
 */
void stopTransmitter() {
  xmitPtr = NULL;
  xmitPhase = idle;
  delayMicroseconds(5);
  if (debug485Frame) {
    Serial.println("Stop transmitter");
  }
  digitalWrite(rs485Send, HIGH);
  digitalWrite(rs485Direction, LOW);
  while (commSerial.available()) {
    commSerial.read();
  }
}

/**
 * HLAVNI vstupni bod pro vysilani. Nutno volat ve smycce loop(). Neni-li co 
 * vysilat, neudela nic (ale vrati 2).
 */
byte transmitSingle(boolean enableImmediateRead) {
  if (xmitPtr == NULL) {
    return 2;
  }
  switch (xmitPhase) {
    case startByte:
      rs485SendRawByte(startByteChar);
      xmitPhase = payload;
      return;
    case payload:
      xmitOneByte(*xmitPtr++);
      break;
    case checksum:
      if (debug485Frame) {
        Serial.println(F("Checksum out"));
      }
      xmitOneByte(xmitXor);
      if (enableImmediateRead) {
        startReceiver();
      }
      break;
  }
  if (xmitCounter == 0) {
    stopTransmitter();
    return 1;
  }
  if (--xmitCounter == 0) {
    xmitPhase = checksum;
  }
  return 0;
}

//---------------------------- PRIJIMAC --------------------------------

/**
 * Ukazatel na ukladana data
 */
byte *recvPtr = recvBuffer;

/**
 * Pocitadlo byte k prijeti. Neobsahuje checksum, ale obsahuje delku.
 */
len_t recvCounter;

/**
 * Kontrolni soucet prijmu
*/
checksum_t recvXor;

/**
 * Misto kam se akumuje kontrolni soucet (ma-li vice nez jeden byte)
 */
checksum_t recvChecksum;

/**
 * Faze prijmu
 */
volatile CommPhase recvPhase = idle;

/**
 * Docasna faze
 */
CommPhase recvPhase2;

/**
 * je-li vetsi nez 0, vyhlasi se na konci packetu takovato chyba.
 * Pouziva se pri dlouhych packetech, neocekavanych datech.
 */
int errorAtEnd = 0;

/**
 * Cas posledniho prijmu (nebo zahajeni prijmu). Pouziva se pro detekci
 * prilis velke prodlevy mezi jednotlivymi byte nebo start byte.
 */
volatile long lastReceiveMillis = 0;

/**
 * Pomocny alias,
 */
const CommFrame& recvFrame = *((CommFrame*)recvBuffer);

void initReceiver() {
  recvPhase = idle;
}

void startReceiver() {
  if (debug485Recv) {
    Serial.println(F("Start receiver"));
  }
  delayMicroseconds(50);
  digitalWrite(rs485Direction, LOW);
  recvPhase = startByte;
  recvPtr = &recvFrame.len;
  lastReceiveMillis = currentMillis;
  errorAtEnd = 0;
}

boolean isReceiving() {
  return recvPhase != idle;
}

void stopReceiver() {
  recvPhase = idle;
  errorAtEnd = 0;
}

void initPayload() {
  initChecksum(recvXor);
  checksumUpdate(recvXor, startByteChar);
  recvPhase = length;
  recvPtr = &recvFrame.len;
}

/**
 * Nutno volat periodicky pro spravne timeoutovani pri cekani
 * na start byte, pripadne na dalsi byte packetu
 */
void periodicReceiveCheck() {
#ifndef NEO
  if (commSerial.available()) {
    isrReceiveData(commSerial.read());
  }
#endif
  if (isReceiving()) {
    long d = currentMillis - lastReceiveMillis;
    boolean tm = false;
    if (recvPhase == startByte) {
      if (d > recvDelayStartByte) {
        if (debug485Frame) {
          Serial.print(F("Start byte timeout")); Serial.println(d);
        }
        stopReceiver();
        onReceiveError(errTimeoutStart);
        return;
      }
    } else {
      if (d > recvDelayBetweenPacketBytes) {
        // just broken packet; leave it as it is, but flag error.
        stopReceiver();
        onReceiveError(errTimeout);
        return;
      }
    }
  }
}

/**
 * Interrupt routine
 */
void isrReceiveData(uint8_t data) {
  byte ph = recvPhase;
  
  lastReceiveMillis = millis();
  if ((ph == idle) || (ph == startByte)) {
    // in case of data incoming during idle, switch to startByte = active reading.
    // will skip unexpected packet on the wire.
    recvPhase = startByte;
    if (data != startByteChar) {
      if (debug485Frame) {
        Serial.print(F("Expect start, ignore ")); Serial.println(data, HEX);
      }
      // ignore, wait for the start byte.
      return;
    }
    initPayload();
    Serial.print("Start byte: "); Serial.println(lastReceiveMillis & 0xffff);
    if (debug485Recv) {
      Serial.println(F("Got start"));
    }
    return;
  }
  
  checksumUpdate(recvXor, data);
  if (debug485Recv) {
    Serial.print(F("Data: ")); Serial.print(data, HEX); Serial.print(F(" chksum:")); Serial.println(recvXor, HEX);
  }
  if (data == escapeChar) {
    if (debug485Recv) {
      Serial.println(F("Got escape"));
    }
    recvPhase2 = ph;
    recvPhase = ph = escape;
    return;
  }
  if (data == startByteChar) {
    onReceiveError(errUnexpected);
    // jump on the next packet, discard the previous.
    initPayload();
    return;
  }
  if (ph == escape) {
    data = data ^ escapeChar;
    if (debug485Recv) {
      Serial.print(F("Escaped data: ")); Serial.println(data, HEX);
    }
    recvPhase = ph = recvPhase2;
  }
  if (ph == length) {
    // frameSize obsahuje take vlastni delku packetu; bude odpoctena jeste v tomto cyklu
    recvCounter = recvFrame.frameSize(data);
    if (recvCounter > recvBufferSize) {
      recvPhase = ph = discard;
      Serial.print(F("Small recv buffer: ")); Serial.println(recvCounter);
      errorAtEnd = errLong;
      // v dalsim if-u se snizi pocitadlo
    } else {
      recvPhase = ph = payload;
      // v dalsim if-u se snizi pocitadlo
    }
  }
  if (ph == payload) {
    *(recvPtr++) = data;
    recvCounter--;
  } else if (ph == discard) {
    recvCounter--;
    Serial.print(F("Discard: ")); Serial.println(recvCounter);
  }
  
  if (ph == checksum) {
    static_assert(sizeof(checksum_t) < 2, "Nedopsal jsem podporu pro vicebytove CRC");
    recvChecksum = data;
    if (debug485Frame) {
      Serial.print(F("recv Checksum: ")); Serial.println(data, HEX);
      Serial.print(F("xor = ")); Serial.println(recvXor, HEX);
    }
    stopReceiver();
    if (errorAtEnd || !verifyChecksum(recvXor, recvChecksum)) {
      // Chyba v datech, zahodit.
      if (debug485Frame) {
        Serial.println(F("Error frame"));
      }
      errorAtEnd = false;
      if (errorAtEnd > 0) {
        onReceiveError(errorAtEnd);
      } else {
        onReceiveError(errChecksum);
      }
      errorAtEnd = 0;
    } else {
      if (debug485Frame) {
        Serial.println(F("Correct frame"));
      }
      onReceivedMessage(recvFrame);
    }
  } else if (recvCounter == 0) {
    if (debug485Frame) {
      Serial.println(F("Payload read"));
    }
    recvPhase = checksum;
    return;
  }
}

