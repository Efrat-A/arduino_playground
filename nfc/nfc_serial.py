"""Shared serial-connection helper for read.py and write.py.

Talks to the nfc.ino sketch's line-based protocol: write one command line,
read one response line back.
"""

import configparser
import time
from pathlib import Path

import serial
import serial.tools.list_ports

DEFAULT_CONFIG_PATH = Path(__file__).parent / "pins.cfg"


class NfcConnectionError(Exception):
    """Raised when the configured serial port can't be opened."""


def load_config(config_path=DEFAULT_CONFIG_PATH):
    parser = configparser.ConfigParser()
    parser.read(config_path)
    return parser


def _available_ports():
    return [p.device for p in serial.tools.list_ports.comports()]


def open_connection(config_path=DEFAULT_CONFIG_PATH):
    config = load_config(config_path)
    port = config.get("serial", "port")
    baud = config.getint("serial", "baud", fallback=115200)
    timeout = config.getfloat("serial", "timeout", fallback=2.0)
    boot_delay = config.getfloat("serial", "boot_delay", fallback=2.0)

    try:
        conn = serial.Serial(port, baud, timeout=timeout)
    except serial.SerialException as exc:
        ports = _available_ports()
        if ports:
            hint = "Ports currently available: " + ", ".join(ports)
        else:
            hint = "No serial ports detected at all - is the Arduino plugged in?"
        raise NfcConnectionError(
            f"Couldn't open '{port}' (from {config_path}): {exc}\n{hint}\n"
            f"Update the [serial] port in {config_path} to match."
        ) from exc

    time.sleep(boot_delay)  # let the Arduino finish its reset-on-connect
    conn.reset_input_buffer()
    return conn


def send_command(conn, command):
    conn.write((command + "\n").encode("utf-8"))
    line = conn.readline().decode("utf-8", errors="replace").strip()
    return line
