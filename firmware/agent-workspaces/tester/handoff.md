# Tester handoff — `rignexo/firmware`

## Current — `RGNX_FW_MODE_SWITCH_POLARITY` (H1)

| Field | Value |
| --- | --- |
| Status | **PASS** (compile only) |
| Source | implementer H1 — `ACTIVE_LEVEL=1`, `gpio_pull_down` |
| Command | `./scripts/build.sh` |
| Artefact | `build/rp2350/rpi_pico_composite_sound_serial.uf2` (53K) |
| Source check | `board_config.h` ACTIVE_LEVEL **1**; `mode_store.c` **pull_down** |

### Pending hardware

1. Replug USB **without** button → mode must **not** change  
2. Hold button while plugging → mode advances once  
