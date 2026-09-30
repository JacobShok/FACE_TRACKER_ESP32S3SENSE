#!/usr/bin/env python3
"""
TEMPORARY DEBUG TOOL.

Listens on a serial port for frames the device dumps between IMG_BEGIN/IMG_END
markers (see s_dump_frame_over_serial() in main/cam.c), base64-decodes them,
and saves each one as a .bmp file in ../captures/.

Everything else read from the port is just echoed to the console, so you can
still see the normal application log while this runs.

Usage:
    pip install pyserial
    python tools/save_serial_frame.py COM5

Reset the board (or let it boot) while this is running; it saves one file per
IMG_BEGIN/IMG_END block it sees and keeps listening until you press Ctrl+C.

Note: the XIAO ESP32S3's USB-CDC serial port disconnects and re-enumerates
whenever the board resets, which drops the OS-level serial handle. This script
detects that and automatically reopens the port, same as idf_monitor.py does.
"""
import base64
import sys
import time
from pathlib import Path

import serial

CAPTURES_DIR = Path(__file__).resolve().parent.parent / "captures"


def run(port, baud):
    in_image = False
    b64_chunks = []

    with serial.Serial(port, baud, timeout=1) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")

            if line == "IMG_BEGIN":
                in_image = True
                b64_chunks = []
                print("[capture] frame incoming...")
                continue

            if line == "IMG_END":
                in_image = False
                b64_data = "".join(b64_chunks)
                try:
                    raw_bytes = base64.b64decode(b64_data)
                except Exception as e:
                    print(f"[capture] FAILED to decode: {e}")
                    continue

                filename = CAPTURES_DIR / f"frame_{int(time.time())}.bmp"
                filename.write_bytes(raw_bytes)
                print(f"[capture] saved {filename} ({len(raw_bytes)} bytes)")
                continue

            if in_image:
                b64_chunks.append(line)
            else:
                print(line)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} COM5")
        sys.exit(1)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    CAPTURES_DIR.mkdir(exist_ok=True)

    print(f"Opening {port} @ {baud}... (Ctrl+C to stop)")
    while True:
        try:
            run(port, baud)
        except KeyboardInterrupt:
            break
        except serial.SerialException as e:
            print(f"[reconnect] lost connection to {port} ({e}); retrying...")
            time.sleep(1)
            continue


if __name__ == "__main__":
    main()
