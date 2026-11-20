#!/usr/bin/env bash
set -euo pipefail

echo "=== USB (TinyUSB cdc_uac2) ==="
lsusb -d cafe: 2>/dev/null || lsusb | grep -i tinyusb || echo "Dispositivo TinyUSB no encontrado"

echo
echo "=== Puertos serial (CDC = interfaz if03) ==="
ls -l /dev/ttyACM* 2>/dev/null || echo "No hay /dev/ttyACM*"
ls -l /dev/serial/by-id/*TinyUSB*if03* 2>/dev/null || \
ls -l /dev/serial/by-id/*headset*if03* 2>/dev/null || \
echo "No hay by-id CDC (if03)"

echo
echo "=== Procesos usando ttyACM ==="
for dev in /dev/ttyACM*; do
    [[ -e "$dev" ]] || continue
    echo -n "$dev: "
    fuser -v "$dev" 2>&1 || echo "libre"
done

PORT="${1:-}"
if [[ -z "$PORT" ]]; then
    PORT="$(ls /dev/serial/by-id/*TinyUSB*if03* 2>/dev/null | head -1 || true)"
fi
if [[ -z "$PORT" ]]; then
    PORT="$(ls /dev/serial/by-id/*headset*if03* 2>/dev/null | head -1 || true)"
fi
if [[ -z "$PORT" ]]; then
    PORT="/dev/ttyACM0"
fi

if [[ ! -e "$PORT" ]]; then
    echo
    echo "Puerto $PORT no existe. ¿Placa conectada?"
    exit 1
fi

echo
echo "=== Abrir $PORT ==="
python3 - "$PORT" <<'PY'
import sys
import serial

port = sys.argv[1]
try:
    s = serial.Serial(port, 115200, timeout=1)
    s.dtr = False
    s.rts = False
    s.write(b"ping\r\n")
    s.flush()
    reply = s.read(64)
    s.close()
    print(f"OK: {port} — eco: {reply!r}")
except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)
PY
