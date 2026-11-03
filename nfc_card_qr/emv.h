#ifndef EMV_H
#define EMV_H

#include <Arduino.h>
#include <Adafruit_PN532.h>

// Best-effort EMV contactless read.
//
// Tries SELECT PPSE -> SELECT AID -> GET PROCESSING OPTIONS -> READ RECORD
// to pull Track2-equivalent data (PAN + expiry) the way a real terminal
// would, using the card's own default/empty PDOL. This is the same clear-
// text data EMV cards expose for magstripe-fallback compatibility - no
// PIN/crypto is bypassed - but GPO can fail on cards that insist on a
// non-empty PDOL (amount, terminal data, etc.), which varies by card.
//
// Returns true if the tag identified itself as an EMV payment application
// (PPSE/AID select succeeded), false otherwise (caller should fall back to
// the generic NFC-tag path).
//
// On true, summaryOut is filled with a MASKED, display-safe string such as
// "VISA **** 4242 EXP 11/28" - the full PAN is never copied out of this
// function. If the record read/parse didn't succeed, summaryOut falls back
// to a generic "<NETWORK> CARD" / "PAYMENT CARD" string.
bool emvTryReadCard(Adafruit_PN532 &nfc, char *summaryOut, size_t summaryOutLen);

#endif
