#ifndef NDEF_H
#define NDEF_H

#include <Arduino.h>
#include <Adafruit_PN532.h>

// Reads the NDEF message off an NTAG213/215/216-style tag (the common
// "NFC sticker" family) and decodes the first Text or URI record into
// textOut. Returns false if the tag has no NDEF message, or the first
// record isn't a Text/URI record we handle - caller should fall back to
// QR-encoding the raw UID instead.
//
// Does not handle MIFARE Classic NDEF (different key/sector layout) - out
// of scope for this sketch, see README.
bool ndefTryReadTag(Adafruit_PN532 &nfc, char *textOut, size_t textOutCap);

#endif
