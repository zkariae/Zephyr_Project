#!/usr/bin/env python3
"""Envoie l'heure/date locale au board via UART, une fois par seconde.

Usage: python send_time.py COM5

Trame envoyee : <HH:MM:SS,DD/MM/YYYY>\n
Le board (launcher.c) affiche ces valeurs et considere la liaison
"Connected" tant qu'une trame arrive au moins toutes les 3 secondes.
"""
import sys
import time
from datetime import datetime

import serial

BAUDRATE = 9600
SEND_INTERVAL_S = 1.0


def build_frame(now: datetime) -> bytes:
    return f"<{now:%H:%M:%S},{now:%d/%m/%Y}>\n".encode("ascii")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <port> (ex: COM5)", file=sys.stderr)
        return 1

    port = sys.argv[1]

    while True:
        try:
            with serial.Serial(port, BAUDRATE, timeout=1) as ser:
                print(f"Connecte sur {port} @ {BAUDRATE} bauds")
                while True:
                    frame = build_frame(datetime.now())
                    ser.write(frame)
                    print(f"-> {frame!r}")
                    time.sleep(SEND_INTERVAL_S)
        except serial.SerialException as exc:
            print(f"Port {port} indisponible ({exc}), nouvelle tentative dans 2s...",
                  file=sys.stderr)
            time.sleep(2)
        except KeyboardInterrupt:
            print("\nArret demande.")
            return 0


if __name__ == "__main__":
    raise SystemExit(main())
