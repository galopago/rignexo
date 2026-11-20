#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-}"
if [[ ! -e "$PORT" ]]; then
    PORT="$(ls /dev/serial/by-id/*TinyUSB*if03* 2>/dev/null | head -1 || true)"
fi
if [[ -z "$PORT" || ! -e "$PORT" ]]; then
    PORT="$(ls /dev/serial/by-id/*headset*if03* 2>/dev/null | head -1 || true)"
fi
if [[ -z "$PORT" || ! -e "$PORT" ]]; then
    echo "Puerto no encontrado. Conecta la placa y pasa la ruta: $0 /dev/ttyACM0" >&2
    exit 1
fi

echo "Puerto: $PORT"
echo "RTS ON 2 s (GPIO 15 PTT y GPIO 29 LED deben ir a HIGH)..."
python3 - "$PORT" <<'PY'
import sys
import serial
import time

port = sys.argv[1]
with serial.Serial(port, 115200) as s:
    s.dtr = False
    s.rts = False
    time.sleep(0.2)
    s.rts = True
    time.sleep(2)
    s.rts = False
    time.sleep(0.5)
print("Listo.")
PY
