#!/usr/bin/env python3
"""Write a short text string to an NFC tag via the Arduino + RC522 reader."""

import argparse
import sys

from nfc_serial import DEFAULT_CONFIG_PATH, NfcConnectionError, open_connection, send_command

MAX_TEXT_BYTES = 16  # one Mifare Classic data block


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("text", help=f"Text to write (max {MAX_TEXT_BYTES} bytes)")
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH, help="Path to pins.cfg")
    args = parser.parse_args()

    if len(args.text.encode("utf-8")) > MAX_TEXT_BYTES:
        print(f"Text too long: max {MAX_TEXT_BYTES} bytes.", file=sys.stderr)
        sys.exit(1)
    if "\n" in args.text or "\r" in args.text:
        print("Text can't contain newlines.", file=sys.stderr)
        sys.exit(1)

    try:
        conn = open_connection(args.config)
    except NfcConnectionError as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)
    print("Tap a tag...")
    response = send_command(conn, f"WRITE {args.text}")

    if response.startswith("OK WRITTEN"):
        uid = response.split("UID=", 1)[1] if "UID=" in response else ""
        print(f"Written. UID: {uid}")
    elif response == "ERR TIMEOUT":
        print("No tag detected within the timeout.", file=sys.stderr)
        sys.exit(1)
    elif response == "ERR NO_READER":
        print("The RC522 isn't responding - check the wiring.", file=sys.stderr)
        sys.exit(1)
    elif response == "ERR TOO_LONG":
        print("Arduino rejected the text as too long.", file=sys.stderr)
        sys.exit(1)
    elif response == "ERR AUTH":
        print("Authentication failed - not a default-key Mifare Classic tag?", file=sys.stderr)
        sys.exit(1)
    elif response == "ERR WRITE":
        print("Write failed.", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"Unexpected response: {response!r}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
