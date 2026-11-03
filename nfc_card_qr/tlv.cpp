#include "tlv.h"

bool tlvFind(const uint8_t *data, uint16_t len, uint8_t tag,
             const uint8_t **valueOut, uint8_t *lenOut) {
  uint16_t i = 0;
  while (i < len) {
    uint16_t tagStart = i;
    uint8_t firstTagByte = data[i];
    uint16_t tagBytes = 1;

    // Multi-byte tag: low 5 bits of the first byte all set, then each
    // following byte continues the tag while its high bit is set.
    if ((firstTagByte & 0x1F) == 0x1F) {
      do {
        if (tagStart + tagBytes >= len) return false; // truncated
        tagBytes++;
      } while (data[tagStart + tagBytes - 1] & 0x80);
    }
    i += tagBytes;
    if (i >= len) return false;

    uint8_t lenByte = data[i];
    if (lenByte & 0x80) return false; // long-form length - not supported, bail rather than guess
    uint8_t valLen = lenByte;
    i += 1;
    if (i + valLen > len) return false; // truncated / malformed

    if (tagBytes == 1 && firstTagByte == tag) {
      *valueOut = &data[i];
      *lenOut = valLen;
      return true;
    }

    bool constructed = firstTagByte & 0x20;
    if (constructed) {
      if (tlvFind(&data[i], valLen, tag, valueOut, lenOut)) return true;
    }

    i += valLen;
  }
  return false;
}
