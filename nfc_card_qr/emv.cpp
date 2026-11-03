#include "emv.h"
#include "tlv.h"
#include <string.h>

// Keep this small - SRAM on the Uno is only 2KB and the SSD1306
// framebuffer alone eats half of it.
#define MAX_APDU 128

static const uint8_t PPSE_AID[] = {0x32, 0x50, 0x41, 0x59, 0x2E, 0x53,
                                    0x59, 0x53, 0x2E, 0x44, 0x44, 0x46,
                                    0x30, 0x31}; // "2PAY.SYS.DDF01"

// Send one APDU over ISO14443-4, strip the trailing SW1SW2 on success.
// *inOutLen is the response buffer capacity on the way in, the payload
// length (status bytes stripped) on the way out.
static bool sendAPDU(Adafruit_PN532 &nfc, const uint8_t *apdu, uint8_t apduLen,
                      uint8_t *resp, uint8_t *inOutLen) {
  // *inOutLen is the response buffer's capacity on entry; inDataExchange
  // overwrites it with the actual response length.
  if (!nfc.inDataExchange((uint8_t *)apdu, apduLen, resp, inOutLen)) {
    return false;
  }
  if (*inOutLen < 2) return false;
  uint8_t sw1 = resp[*inOutLen - 2];
  uint8_t sw2 = resp[*inOutLen - 1];
  *inOutLen -= 2;
  return (sw1 == 0x90 && sw2 == 0x00);
}

static const uint8_t AID_PREFIX_VISA[]       = {0xA0, 0x00, 0x00, 0x00, 0x03};
static const uint8_t AID_PREFIX_MASTERCARD[] = {0xA0, 0x00, 0x00, 0x00, 0x04};
static const uint8_t AID_PREFIX_AMEX[]       = {0xA0, 0x00, 0x00, 0x00, 0x25};
static const uint8_t AID_PREFIX_DISCOVER[]   = {0xA0, 0x00, 0x00, 0x01, 0x52};

static const char *networkFromAID(const uint8_t *aid, uint8_t aidLen) {
  if (aidLen >= 5) {
    if (memcmp(aid, AID_PREFIX_VISA, 5) == 0) return "VISA";
    if (memcmp(aid, AID_PREFIX_MASTERCARD, 5) == 0) return "MASTERCARD";
    if (memcmp(aid, AID_PREFIX_AMEX, 5) == 0) return "AMEX";
    if (memcmp(aid, AID_PREFIX_DISCOVER, 5) == 0) return "DISCOVER";
  }
  return "EMV";
}

// Track2-equivalent data (tag 0x57): BCD-ish nibble stream of
// <PAN><sep 0xD><expiry YYMM><service code><discretionary data>, padded
// with 0xF nibbles. We only need the PAN digits and the 4-digit expiry.
static void parseTrack2(const uint8_t *data, uint8_t len, char *panOut,
                         uint8_t panOutCap, char *expOut) {
  uint8_t panLen = 0;
  bool sawSep = false;
  uint8_t expIdx = 0;
  expOut[0] = '\0';

  for (uint8_t b = 0; b < len; b++) {
    uint8_t hi = (data[b] >> 4) & 0x0F;
    uint8_t lo = data[b] & 0x0F;
    uint8_t nibbles[2] = {hi, lo};
    for (uint8_t n = 0; n < 2; n++) {
      uint8_t v = nibbles[n];
      if (!sawSep) {
        if (v == 0x0D) {
          sawSep = true;
        } else if (v <= 9 && panLen < panOutCap - 1) {
          panOut[panLen++] = '0' + v;
        }
      } else if (expIdx < 4) {
        if (v <= 9) expOut[expIdx++] = '0' + v;
      }
    }
  }
  panOut[panLen] = '\0';
  expOut[expIdx] = '\0';
}

static void maskPAN(const char *pan, char *out, size_t outCap) {
  uint8_t panLen = strlen(pan);
  if (panLen < 4) {
    snprintf(out, outCap, "****");
    return;
  }
  snprintf(out, outCap, "**** %s", pan + panLen - 4);
}

bool emvTryReadCard(Adafruit_PN532 &nfc, char *summaryOut, size_t summaryOutLen) {
  uint8_t resp[MAX_APDU];
  uint8_t respLen;

  // 1. SELECT PPSE
  uint8_t selectPPSEcmd[5 + sizeof(PPSE_AID)];
  selectPPSEcmd[0] = 0x00;
  selectPPSEcmd[1] = 0xA4;
  selectPPSEcmd[2] = 0x04;
  selectPPSEcmd[3] = 0x00;
  selectPPSEcmd[4] = sizeof(PPSE_AID);
  memcpy(&selectPPSEcmd[5], PPSE_AID, sizeof(PPSE_AID));

  respLen = sizeof(resp);
  if (!sendAPDU(nfc, selectPPSEcmd, sizeof(selectPPSEcmd), resp, &respLen)) {
    return false; // not a payment card (or no PPSE) -> caller falls back to generic tag path
  }

  // 2. Pull the first AID (tag 0x4F) out of the PPSE FCI.
  const uint8_t *aid = nullptr;
  uint8_t aidLen = 0;
  if (!tlvFind(resp, respLen, 0x4F, &aid, &aidLen) || aidLen == 0) {
    snprintf(summaryOut, summaryOutLen, "PAYMENT CARD");
    return true; // identified as EMV-capable, but no AID to select further
  }
  uint8_t aidBuf[16];
  if (aidLen > sizeof(aidBuf)) aidLen = sizeof(aidBuf);
  memcpy(aidBuf, aid, aidLen);
  const char *network = networkFromAID(aidBuf, aidLen);

  // 3. SELECT that AID.
  uint8_t selectAIDcmd[5 + sizeof(aidBuf)];
  selectAIDcmd[0] = 0x00;
  selectAIDcmd[1] = 0xA4;
  selectAIDcmd[2] = 0x04;
  selectAIDcmd[3] = 0x00;
  selectAIDcmd[4] = aidLen;
  memcpy(&selectAIDcmd[5], aidBuf, aidLen);

  respLen = sizeof(resp);
  if (!sendAPDU(nfc, selectAIDcmd, 5 + aidLen, resp, &respLen)) {
    snprintf(summaryOut, summaryOutLen, "%s CARD", network);
    return true;
  }

  // 4. GET PROCESSING OPTIONS with an empty PDOL. Works on cards happy with
  // a minimal/no-amount contactless read; cards that demand real PDOL data
  // (amount, terminal country, etc.) will reject this with an error, and we
  // just fall back to the generic summary below.
  static const uint8_t gpoCmd[] = {0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00};
  respLen = sizeof(resp);
  if (!sendAPDU(nfc, gpoCmd, sizeof(gpoCmd), resp, &respLen)) {
    snprintf(summaryOut, summaryOutLen, "%s CARD", network);
    return true;
  }

  // AFL (tag 0x94) lives directly in a format-1 (tag 0x80) response, or
  // nested in a format-2 (tag 0x77) response.
  const uint8_t *afl = nullptr;
  uint8_t aflLen = 0;
  const uint8_t *fmt1 = nullptr;
  uint8_t fmt1Len = 0;
  if (tlvFind(resp, respLen, 0x80, &fmt1, &fmt1Len) && fmt1Len > 2) {
    afl = fmt1 + 2; // skip 2-byte AIP
    aflLen = fmt1Len - 2;
  } else {
    tlvFind(resp, respLen, 0x94, &afl, &aflLen);
  }

  if (!afl || aflLen == 0) {
    snprintf(summaryOut, summaryOutLen, "%s CARD", network);
    return true;
  }

  // 5. Walk the AFL, READ RECORD on each entry, stop at the first Track2.
  for (uint8_t e = 0; e + 3 < aflLen; e += 4) {
    uint8_t sfi = afl[e] >> 3;
    uint8_t firstRec = afl[e + 1];
    uint8_t lastRec = afl[e + 2];
    for (uint8_t rec = firstRec; rec <= lastRec; rec++) {
      uint8_t readCmd[5] = {0x00, 0xB2, rec, (uint8_t)((sfi << 3) | 4), 0x00};
      respLen = sizeof(resp);
      if (!sendAPDU(nfc, readCmd, sizeof(readCmd), resp, &respLen)) continue;

      const uint8_t *track2 = nullptr;
      uint8_t track2Len = 0;
      if (tlvFind(resp, respLen, 0x57, &track2, &track2Len) && track2Len > 0) {
        char pan[20];
        char exp[5];
        parseTrack2(track2, track2Len, pan, sizeof(pan), exp);
        char masked[16];
        maskPAN(pan, masked, sizeof(masked));
        if (strlen(exp) == 4) {
          // Track2 stores expiry as YYMM; show it as MM/YY.
          snprintf(summaryOut, summaryOutLen, "%s %s EXP %c%c/%c%c", network,
                   masked, exp[2], exp[3], exp[0], exp[1]);
        } else {
          snprintf(summaryOut, summaryOutLen, "%s %s", network, masked);
        }
        return true;
      }
    }
  }

  snprintf(summaryOut, summaryOutLen, "%s CARD", network);
  return true;
}
