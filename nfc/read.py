#!/usr/bin/env python3
"""Read the text stored on an NFC tag via the Arduino + RC522 reader."""

import argparse
import sys

from nfc_serial import DEFAULT_CONFIG_PATH, NfcConnectionError, open_connection, send_command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH, help="Path to pins.cfg")
    args = parser.parse_args()

    try:
        conn = open_connection(args.config)
    except NfcConnectionError as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)
    print("Tap a tag...")
    response = send_command(conn, "READ")

    if response.startswith("OK "):
        # "OK UID=<hex> TEXT=<text>" - split on the first space only, so a
        # TEXT value containing spaces survives intact.
        uid_part, _, text_part = response[3:].partition(" ")
        uid = uid_part.split("=", 1)[1] if "=" in uid_part else ""
        text = text_part.split("=", 1)[1] if "=" in text_part else ""
        print(f"UID:  {uid}")
        print(f"TEXT: {text}")
    elif response == "ERR TIMEOUT":
        print("No tag detected within the timeout.", file=sys.stderr)
        sys.exit(1)
    elif response == "ERR NO_READER":
        print("The RC522 isn't responding - check the wiring.", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"Unexpected response: {response!r}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
