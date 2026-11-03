#include "ndef.h"
#include <string.h>

// Pages 4.. are user memory on NTAG213/215/216. 32 pages * 4 bytes = 128
// bytes is enough for most short text/URL tags and keeps SRAM usage sane.
#define NDEF_MAX_PAGES 32
#define NDEF_BUF_LEN (NDEF_MAX_PAGES * 4)

static const char *uriPrefix(uint8_t code) {
  switch (code) {
    case 0x01: return "http://www.";
    case 0x02: return "https://www.";
    case 0x03: return "http://";
    case 0x04: return "https://";
    case 0x05: return "tel:";
    case 0x06: return "mailto:";
    default: return ""; // 0x00 = literal, no prefix; unknown codes: best effort
  }
}

bool ndefTryReadTag(Adafruit_PN532 &nfc, char *textOut, size_t textOutCap) {
  uint8_t buf[NDEF_BUF_LEN];
  uint8_t filled = 0;

  for (uint8_t p = 0; p < NDEF_MAX_PAGES; p++) {
    if (!nfc.ntag2xx_ReadPage(4 + p, &buf[p * 4])) break;
    filled += 4;
  }
  if (filled < 2) return false;

  // Find the NDEF Message TLV (type 0x03). 0x00 = NULL TLV (skip, len 0),
  // 0xFE = Terminator TLV (end of data).
  uint16_t i = 0;
  while (i < filled) {
    uint8_t t = buf[i];
    if (t == 0x00) { i++; continue; }
    if (t == 0xFE) return false;
    if (i + 1 >= filled) return false;
    uint8_t len = buf[i + 1];
    if (t == 0x03) {
      i += 2;
      if (i + len > filled) len = filled - i; // truncated read, best effort
      break;
    }
    i += 2 + len; // skip unknown TLV
  }
  if (i >= filled) return false;

  // --- Parse the first NDEF record header ---
  uint8_t header = buf[i++];
  bool shortRecord = header & 0x10;
  bool hasIdLen = header & 0x08;
  uint8_t tnf = header & 0x07;

  if (i >= filled) return false;
  uint8_t typeLen = buf[i++];

  uint32_t payloadLen;
  if (shortRecord) {
    if (i >= filled) return false;
    payloadLen = buf[i++];
  } else {
    if (i + 4 > filled) return false;
    payloadLen = ((uint32_t)buf[i] << 24) | ((uint32_t)buf[i + 1] << 16) |
                 ((uint32_t)buf[i + 2] << 8) | buf[i + 3];
    i += 4;
  }

  uint8_t idLen = 0;
  if (hasIdLen) {
    if (i >= filled) return false;
    idLen = buf[i++];
  }

  if (i + typeLen > filled) return false;
  char type = (typeLen == 1) ? (char)buf[i] : '\0';
  i += typeLen;
  i += idLen; // skip ID field, unused here
  if (i > filled) return false; // idLen ran past what we actually read

  if (i + payloadLen > filled) payloadLen = filled - i; // truncated, best effort

  if (tnf != 0x01 || payloadLen == 0) return false; // only "well-known" Text/URI handled

  if (type == 'U') {
    uint8_t code = buf[i];
    const char *prefix = uriPrefix(code);
    uint32_t uriLen = payloadLen - 1;
    snprintf(textOut, textOutCap, "%s%.*s", prefix, (int)min((uint32_t)textOutCap, uriLen),
             (const char *)&buf[i + 1]);
    return true;
  }

  if (type == 'T') {
    uint8_t statusByte = buf[i];
    uint8_t langLen = statusByte & 0x3F;
    uint32_t textStart = 1 + langLen;
    if (textStart >= payloadLen) return false;
    uint32_t textLen = payloadLen - textStart;
    uint32_t n = min((uint32_t)(textOutCap - 1), textLen);
    memcpy(textOut, &buf[i + textStart], n);
    textOut[n] = '\0';
    return true;
  }

  return false;
}
