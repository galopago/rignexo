#!/usr/bin/env bash
# D31 offline golden O0/O1 — same DSP sources as firmware.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/build-host/afsk_aplus_offline"
mkdir -p "${ROOT}/build-host"
gcc -O2 -std=c11 -Wall -Wextra \
  -I"${ROOT}/src" -I"${ROOT}/src/afsk_aplus" \
  -DAFSK_APLUS_N_SLICERS=5 \
  "${ROOT}/tools/afsk_aplus_offline.c" \
  "${ROOT}/src/afsk_aplus/afsk_aplus.c" \
  "${ROOT}/src/hdlc_ax25.c" \
  -lm -o "${OUT}"
WAV="${1:-${ROOT}/agent-workspaces/tester/fixtures/afsk_rx_bench_2min.wav}"
cd "${ROOT}"
exec "${OUT}" "${WAV}"
