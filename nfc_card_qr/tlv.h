#ifndef TLV_H
#define TLV_H

#include <Arduino.h>

// Minimal BER-TLV scanner for EMV responses.
//
// Handles multi-byte tags (e.g. 0xBF0C, used to wrap the AID list in a
// real PPSE response) well enough to skip over them correctly, and
// recurses into ANY constructed tag (bit 0x20 set in its first byte),
// not just a hardcoded list - that's what BER-TLV uses that bit for.
// Only short-form lengths (0-127) are supported, which covers every
// PPSE/AID-SELECT/GPO/READ RECORD response in practice; a long-form
// length makes this return false rather than guess.
//
// `tag` must be a single-byte tag (all tags this project searches for -
// 0x4F, 0x57, 0x80, 0x82, 0x94 - are single-byte); multi-byte tags are
// skipped, not matched.
bool tlvFind(const uint8_t *data, uint16_t len, uint8_t tag,
             const uint8_t **valueOut, uint8_t *lenOut);

#endif
