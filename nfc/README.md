# RC522 reader/writer, driven from a PC

[`nfc.ino`](nfc.ino) runs on the Uno and does all the actual RC522 work.
[`read.py`](read.py) / [`write.py`](write.py) run on your PC and just send
one-line text commands to it over USB serial.

## Setup

1. Wire the RC522 as described at the top of [`nfc.ino`](nfc.ino) and flash it.
2. `pip install -r requirements.txt`
3. Edit [`pins.cfg`](pins.cfg)'s `[serial] port` to match your Arduino's
   serial port (see the comments in that file for how to find it per OS).

## Usage

```bash
python read.py
python write.py "hello world"
```

`read.py` prints the tag's UID and whatever text is stored in its data
block. `write.py` writes the given text (max 16 bytes) to that block.

## How it talks to the Arduino

Newline-terminated ASCII commands at 115200 baud — see the protocol
table in the comment block at the top of [`nfc.ino`](nfc.ino). Both
scripts go through [`nfc_serial.py`](nfc_serial.py) for the actual
port-open/send/receive plumbing, reading connection settings from
`pins.cfg`.

## Scope note

Read/write target block 8 using the Mifare Classic factory default key
(`FF FF FF FF FF FF`). That's the standard demo setup, but it only works
on Mifare Classic 1K/4K tags that still have default keys — not on
NTAG21x "stickers" or any card that's been re-keyed.
