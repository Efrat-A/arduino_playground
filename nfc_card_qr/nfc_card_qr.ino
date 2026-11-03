// NFC tag reader -> QR code display, for an Arduino Uno.
//
// Hardware (see README.md for the full write-up and wiring diagram):
//   - PN532 NFC module in I2C mode (SDA/SCL shared with the OLED)
//   - SSD1306 128x64 I2C OLED
//
// Behavior:
//   - Tap a contactless payment card -> best-effort EMV read, display a
//     MASKED summary (network + last 4 digits + expiry) as a QR code.
//     Full PAN is never stored in a variable longer than the single
//     function call that parses Track2 - see emv.cpp.
//   - Tap any other NFC tag (NTAG21x "sticker" with NDEF text/URL) -> QR
//     code of that tag's own content. Tags without readable NDEF fall
//     back to a QR of their UID.
//
// Libraries needed (Library Manager): Adafruit PN532, Adafruit SSD1306,
// Adafruit GFX Library, QRCode (by Richard Moore / ricmoo).

#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "emv.h"
#include "ndef.h"
#include "qr_display.h"

#define PN532_IRQ 2
#define PN532_RESET 3

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static void uidToHexText(const uint8_t *uid, uint8_t uidLength, char *out, size_t outCap) {
  snprintf(out, outCap, "TAG:");
  size_t pos = strlen(out);
  for (uint8_t i = 0; i < uidLength && pos + 2 < outCap; i++) {
    snprintf(out + pos, outCap - pos, "%02X", uid[i]);
    pos += 2;
  }
}

static void showMessage(const char *line1, const char *line2 = nullptr) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2) display.println(line2);
  display.display();
}

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 not found - check wiring/address"));
    while (true) delay(1000);
  }
  showMessage("Starting PN532...");

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println(F("PN532 not found - check wiring"));
    showMessage("PN532 not found", "check wiring");
    while (true) delay(1000);
  }
  Serial.print(F("Found PN5"));
  Serial.println((versiondata >> 24) & 0xFF, HEX);

  nfc.SAMConfig();
  showMessage("Ready.", "Tap a card/tag");
}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 1000)) {
    return; // no tag in range this poll
  }

  char qrText[107];

  char summary[48];
  if (emvTryReadCard(nfc, summary, sizeof(summary))) {
    Serial.print(F("Payment card: "));
    Serial.println(summary);
    snprintf(qrText, sizeof(qrText), "%s", summary);
  } else if (ndefTryReadTag(nfc, qrText, sizeof(qrText))) {
    Serial.print(F("NDEF content: "));
    Serial.println(qrText);
  } else {
    uidToHexText(uid, uidLength, qrText, sizeof(qrText));
    Serial.print(F("Raw tag, UID: "));
    Serial.println(qrText);
  }

  showQRCode(display, qrText);

  // Simple debounce: hold the QR on screen instead of re-reading the same
  // tag on every poll. Lift the tag and wait for this delay to scan another.
  delay(5000);
  showMessage("Ready.", "Tap a card/tag");
}
