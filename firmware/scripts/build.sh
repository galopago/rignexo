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

PICO_SDK_DIR="${PICO_SDK_DIR:-/home/libertad/pico/pico-sdk}"
TINYUSB_ROOT="${TINYUSB_ROOT:-${FIRMWARE_DIR}/external/tinyusb}"
# Pin TinyUSB so builds match the known-good cdc_uac2 revision (not floating master).
TINYUSB_GIT_REF="${TINYUSB_GIT_REF:-375e20090a0a60fd2d47ba6553d189e05482dfea}"
TINYUSB_GIT_URL="${TINYUSB_GIT_URL:-https://github.com/hathach/tinyusb.git}"
CMAKE="${CMAKE:-/usr/bin/cmake}"
NINJA="${NINJA:-/usr/bin/ninja}"
if [[ -z "${PICO_BOARD:-}" ]]; then
    PICO_BOARD="waveshare_${CHIP}_zero"
fi

if [[ ! -d "${PICO_SDK_DIR}" ]]; then
    echo "Pico SDK not found at ${PICO_SDK_DIR}" >&2
    exit 1
fi

tinyusb_at_pinned_ref() {
    [[ -f "${TINYUSB_ROOT}/hw/bsp/family_support.cmake" ]] || return 1
    [[ -d "${TINYUSB_ROOT}/.git" ]] || return 1
    local head desired
    head="$(git -C "${TINYUSB_ROOT}" rev-parse HEAD)"
    desired="$(git -C "${TINYUSB_ROOT}" rev-parse "${TINYUSB_GIT_REF}^{commit}" 2>/dev/null || true)"
    [[ -n "${desired}" && "${head}" == "${desired}" ]]
}

ensure_tinyusb() {
    if tinyusb_at_pinned_ref; then
        echo "Using TinyUSB at ${TINYUSB_ROOT} ($(git -C "${TINYUSB_ROOT}" rev-parse --short HEAD))"
        return 0
    fi

    echo "Cloning TinyUSB ${TINYUSB_GIT_REF} into ${TINYUSB_ROOT}..."
    rm -rf "${TINYUSB_ROOT}"
    mkdir -p "$(dirname "${TINYUSB_ROOT}")"
    git clone "${TINYUSB_GIT_URL}" "${TINYUSB_ROOT}"
    git -C "${TINYUSB_ROOT}" checkout --detach "${TINYUSB_GIT_REF}"
    python3 "${TINYUSB_ROOT}/tools/get_deps.py" rp2040
    echo "TinyUSB ready: $(git -C "${TINYUSB_ROOT}" rev-parse HEAD)"
}

ensure_tinyusb

mkdir -p /tmp/fakebin
ln -sf "$(command -v python3)" /tmp/fakebin/python
export PATH="/tmp/fakebin:${PATH}"

"${CMAKE}" -S "${FIRMWARE_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DFAMILY=rp2040 \
    -DBOARD=pico_sdk \
    -DPICO_SDK_PATH="${PICO_SDK_DIR}" \
    -DPICO_BOARD="${PICO_BOARD}" \
    -DTINYUSB_ROOT="${TINYUSB_ROOT}" \
    -DCMAKE_MAKE_PROGRAM="${NINJA}"

"${CMAKE}" --build "${BUILD_DIR}"

echo "Build complete: ${BUILD_DIR}/rpi_pico_composite_sound_serial.uf2"
