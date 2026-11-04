# RC522 reader/writer for the Arduino Uno

The program on the Uno is written in plain C and needs no extra libraries.
It does all the RC522 work and takes one-line text commands over USB serial.
You can send those commands from the IDE's Serial Monitor, or from your PC with
[`read.py`](read.py) and [`write.py`](write.py).

| File | What it does |
|---|---|
| [`nfc.ino`](nfc.ino) | Entry point the Arduino IDE opens; calls into the C code |
| [`nfc_app.c`](nfc_app.c) | Serial commands (`PING`, `READ`, `WRITE`) |
| [`rc522.c`](rc522.c) | RC522 driver: SPI, tag selection, authentication, block read/write |

## Wiring

| RC522 | Uno |
|---|---|
| SDA (SS) | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |
| IRQ | not connected |
| GND | GND |
| RST | D9 |
| 3.3V | 3.3V (not 5V) |

The pins are set at the top of [`rc522.c`](rc522.c).

## Flashing

Open `nfc.ino` in the Arduino IDE, select **Arduino Uno** and its port, and upload.

On an Apple Silicon Mac without Rosetta, this fails with `bad CPU type in
executable`. The IDE's AVR compiler is x86_64 only, so it needs either Rosetta or
a hex file built some other way.

## Commands

115200 baud, one command per line. In the Serial Monitor, set the line ending to
**Newline**.

| Send | Reply |
|---|---|
| `PING` | `PONG` |
| `READ` | `OK UID=<hex> TEXT=<text>` |
| `WRITE <text>` | `OK WRITTEN UID=<hex>` |

The board prints `READY` when it starts. `READ` and `WRITE` wait up to 10 seconds
for a tag.

| Error | Meaning |
|---|---|
| `ERR NO_READER` | The RC522 isn't responding; check the wiring |
| `ERR TIMEOUT` | No tag was found within 10 seconds |
| `ERR TOO_LONG` | The text is over 16 bytes |
| `ERR AUTH` | Not a MIFARE Classic tag, or it no longer has the default key |
| `ERR WRITE` | The tag rejected the write |
| `ERR UNKNOWN_CMD` | Command not recognised |

`READ` still replies `OK` with an empty `TEXT=` when it finds a tag but can't read
its data, for example an NTAG sticker. You get the UID but no text.

## Using the Python scripts

```bash
pip install -r requirements.txt
```

Set `port` under `[serial]` in [`pins.cfg`](pins.cfg) to your Arduino's port, then:

```bash
python read.py
```

```bash
python write.py "hello world"
```

Close the Serial Monitor first, because it locks the port.

## Limits

Text is stored in block 8 using the MIFARE Classic factory key
(`FF FF FF FF FF FF`), so reading text and writing only work on MIFARE Classic
1K/4K tags that still have that key. Other tags still report their UID.
