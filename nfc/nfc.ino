// RC522 (MFRC522) NFC/RFID reader, driven from a PC over USB serial.
//
// The Arduino does all the actual RC522 work; a PC-side Python script
// (read.py / write.py, in this same folder) just sends one-line text
// commands over Serial and gets a one-line response back. See README.md
// for the wiring diagram, the pin numbers below must match it.
//
// Serial protocol (newline-terminated ASCII, 115200 baud):
//   PING                -> PONG
//   READ                -> OK UID=<hex> TEXT=<text read from block 8>
//                          (TEXT= empty if the block couldn't be read/
//                          authenticated - the tag still responded, just
//                          not as a Mifare Classic tag with default keys)
//                       or ERR TIMEOUT   (no tag presented within 10s)
//   WRITE <text>        -> OK WRITTEN UID=<hex>
//                       or ERR TOO_LONG  (text > 16 bytes)
//                       or ERR TIMEOUT
//                       or ERR AUTH      (authentication failed)
//                       or ERR WRITE     (write itself failed)
//
// Scope note: read/write use block 8 with the Mifare Classic factory
// default key (FF FF FF FF FF FF). That only works on Mifare Classic
// 1K/4K tags that still have default keys - not on NTAG21x "stickers"
// or re-keyed cards. Good enough for a read/write demo; see README.md.
//
// Wiring (Uno, hardware SPI):
//   RC522      Uno
//   -----      ---
//   SDA(SS) --> D10
//   SCK     --> D13
//   MOSI    --> D11
//   MISO    --> D12
//   RST     --> D9
//   GND     --> GND
//   3.3V    --> 3.3V
//
// Library needed (Arduino Library Manager): "MFRC522" by GithubCommunity.

#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN 10
#define BLOCK_ADDR 8 // a plain data block (not a sector trailer) on sector 2

MFRC522 mfrc522(SS_PIN, RST_PIN);

static bool waitForTag(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      return true;
    }
    delay(50);
  }
  return false;
}

static String uidToHex() {
  String s = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) s += "0";
    s += String(mfrc522.uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

static void defaultKey(MFRC522::MIFARE_Key &key) {
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
}

static void releaseTag() {
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

static void handleRead() {
  if (!waitForTag(10000)) {
    Serial.println(F("ERR TIMEOUT"));
    return;
  }
  String uidStr = uidToHex();

  MFRC522::MIFARE_Key key;
  defaultKey(key);
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, BLOCK_ADDR, &key, &(mfrc522.uid));

  char text[17];
  text[0] = '\0';
  if (status == MFRC522::STATUS_OK) {
    byte buffer[18];
    byte size = sizeof(buffer);
    if (mfrc522.MIFARE_Read(BLOCK_ADDR, buffer, &size) == MFRC522::STATUS_OK) {
      byte n = 0;
      for (; n < 16 && buffer[n] != 0; n++) text[n] = (char)buffer[n];
      text[n] = '\0';
    }
  }
  releaseTag();

  Serial.print(F("OK UID="));
  Serial.print(uidStr);
  Serial.print(F(" TEXT="));
  Serial.println(text);
}

static void handleWrite(const String &text) {
  if (text.length() > 16) {
    Serial.println(F("ERR TOO_LONG"));
    return;
  }
  if (!waitForTag(10000)) {
    Serial.println(F("ERR TIMEOUT"));
    return;
  }

  MFRC522::MIFARE_Key key;
  defaultKey(key);
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, BLOCK_ADDR, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    releaseTag();
    Serial.println(F("ERR AUTH"));
    return;
  }

  byte buffer[16];
  memset(buffer, 0, sizeof(buffer));
  for (byte i = 0; i < text.length() && i < 16; i++) buffer[i] = (byte)text[i];

  status = mfrc522.MIFARE_Write(BLOCK_ADDR, buffer, 16);
  String uidStr = uidToHex();
  releaseTag();

  if (status != MFRC522::STATUS_OK) {
    Serial.println(F("ERR WRITE"));
    return;
  }
  Serial.print(F("OK WRITTEN UID="));
  Serial.println(uidStr);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println(F("ERR NO_READER"));
  }
}

void loop() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "PING") {
    Serial.println(F("PONG"));
  } else if (cmd == "READ") {
    handleRead();
  } else if (cmd.startsWith("WRITE ")) {
    handleWrite(cmd.substring(6));
  } else if (cmd.length() > 0) {
    Serial.println(F("ERR UNKNOWN_CMD"));
  }
}
