# NFC Card/Tag → QR Code (Arduino Uno)

Tap an NFC tag on a PN532 reader; the Uno figures out what kind of tag it
is and shows a QR code of the relevant content on a small OLED.

- **Contactless payment card (EMV)** → best-effort read of the card's own
  clear-text Track2 data (same data used for magstripe fallback, no
  PIN/crypto involved), shown **masked**: `VISA **** 4242 EXP 11/28`.
  The QR only ever encodes that masked string — never the full PAN.
- **Any other NFC tag** (NTAG213/215/216 "stickers", etc.) → QR of the
  tag's own NDEF text/URL content, or its UID if it has no readable NDEF.

Sketch lives in [`nfc_card_qr/`](nfc_card_qr).

## Parts

| Part | Notes |
|---|---|
| Arduino Uno | Only 2KB SRAM — see "Memory budget" below |
| PN532 NFC/RFID breakout | Set its mode switch to **I2C** (check your board's silkscreen; this varies by vendor) |
| SSD1306 OLED, 128x64, I2C | Default address `0x3C` on most boards — some are `0x3D`, check yours |
| Breadboard + jumper wires | Everything below is I2C, so wiring is minimal |

## Wiring

Both the PN532 (I2C mode) and the OLED share the Uno's single I2C bus —
only 4 signal wires total, plus IRQ/RESET for the PN532.

```
Uno            PN532 (I2C mode)        SSD1306 OLED
----           -----------------       ------------
5V     ------> VCC                     VCC
GND    ------> GND          --------->  GND
A4 (SDA) ----> SDA          --------->  SDA
A5 (SCL) ----> SCL          --------->  SCL
D2     ------> IRQ
D3     ------> RSTO / RSTPDN
```

Both devices sit on the same SDA/SCL lines (A4/A5); make sure the PN532's
onboard DIP switches (or solder jumpers) are set to I2C mode, not SPI/HSU,
or it won't show up on the bus.

## Libraries (Arduino Library Manager)

- `Adafruit PN532` — needs the `inDataExchange()` method used by the EMV
  code; update to the latest version if your copy predates it.
- `Adafruit SSD1306` + `Adafruit GFX Library`
- `QRCode` by Richard Moore (ricmoo) — lightweight, AVR-friendly QR encoder

## Memory budget (why the code looks stingy)

The Uno has 2KB of SRAM. The SSD1306 framebuffer alone is `128*64/8 =
1024 bytes` — half the chip — before any of our own code runs. That's why
the sketch:
- caps APDU/NDEF buffers at 128 bytes instead of the PN532's max ~255,
- caps QR payloads at 106 bytes (QR version 5) instead of letting the
  encoder pick an arbitrarily large version,
- uses `char[]` + `snprintf`/`strlcpy` everywhere instead of the `String`
  class, to avoid heap fragmentation.

If you extend this and start seeing garbage on the OLED or silent resets,
it's almost always SRAM exhaustion — check `free memory` with a debug
sketch before anything else.

## EMV read: what it does and doesn't do

This is the same flow a real terminal uses, and the same thing hobby
"EMV NFC card reader" apps do: `SELECT PPSE → SELECT AID → GET PROCESSING
OPTIONS → READ RECORD`, looking for Track2-equivalent data (tag `0x57`).
That data (PAN + expiry) is exposed in the clear by design, for magstripe
fallback compatibility — no PIN, key, or cryptogram is involved or broken.

Caveats, so you're not surprised:
- **GPO uses an empty PDOL.** Cards that insist on real PDOL data (amount,
  terminal country code, etc.) will reject it. In that case the sketch
  falls back to just identifying the card network (`"VISA CARD"`) with no
  PAN/expiry — it does not try to negotiate PDOL.
- **TLV parsing is simplified**: single-byte tags, short-form lengths
  only (0–127). Real EMV data from these particular commands practically
  always fits that, but it's not a general BER-TLV parser.
- **Only the first AID / first matching record is used.** A card with
  multiple applications or unusual record layout may not be fully read.
- The full PAN only ever exists inside `parseTrack2()` in
  [`emv.cpp`](nfc_card_qr/emv.cpp) and is masked before being returned —
  it's never written to Serial, never stored in a longer-lived buffer, and
  never put in the QR code.

## NDEF read: what it does and doesn't do

Handles NTAG213/215/216-style tags with a short-record Text (`T`) or URI
(`U`) NDEF message starting at page 4 — the common case for pre-written
"NFC stickers". Does **not** handle MIFARE Classic NDEF (different
sector/key layout) or long-record / chunked NDEF messages; those fall
back to a QR of the tag's raw UID.

## Flow summary

```
tap tag
  │
  ├─ SELECT PPSE succeeds? ──yes──► EMV flow (emv.cpp)
  │                                   └─► masked summary string
  │
  └─ no ──► try NDEF read (ndef.cpp)
              ├─ Text/URI record found ─► that text
              └─ not found ─────────────► "TAG:<hex UID>"
                                              │
                                              ▼
                                   showQRCode() on the OLED (qr_display.cpp)
```
