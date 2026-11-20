#!/usr/bin/env bash
set -euo pipefail

FIRMWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CHIP="${CHIP:-rp2350}"
case "${CHIP}" in
    rp2350|rp2040) ;;
    *)
        echo "Unsupported CHIP=${CHIP} (expected rp2350 or rp2040)" >&2
        exit 1
        ;;
esac

BUILD_DIR="${BUILD_DIR:-${FIRMWARE_DIR}/build/${CHIP}}"
UF2="${BUILD_DIR}/rpi_pico_composite_sound_serial.uf2"
PICOTOOL="${PICOTOOL:-picotool}"

if [[ ! -f "${UF2}" ]]; then
    echo "UF2 not found at ${UF2}. Run firmware/scripts/build.sh first (CHIP=${CHIP})." >&2
    exit 1
fi

if command -v "${PICOTOOL}" >/dev/null 2>&1; then
    "${PICOTOOL}" load "${UF2}" -f
    echo "Flashed with picotool."
    exit 0
fi

MOUNTPOINT=""
for candidate in /media/"${USER}"/RPI-RP2 /run/media/"${USER}"/RPI-RP2 /media/RPI-RP2 \
                 /media/"${USER}"/RP2350 /run/media/"${USER}"/RP2350 /media/RP2350; do
    if [[ -d "${candidate}" ]]; then
        MOUNTPOINT="${candidate}"
        break
    fi
done

if [[ -z "${MOUNTPOINT}" ]]; then
    echo "Hold BOOTSEL and connect the board, or install picotool." >&2
    exit 1
fi

cp "${UF2}" "${MOUNTPOINT}/"
echo "Copied UF2 to ${MOUNTPOINT}"
