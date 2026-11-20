# Implementer handoff — `rignexo/firmware`

## Current — MODE_SWITCH polarity H1 (**implemented** 2026-11-19)

```text
ID: RGNX_FW_MODE_SWITCH_POLARITY
Date: 2026-11-19
Role: implementer only (no build/flash)
Normative: docs/device_modes_architecture.md § Hardware UI Pins; architect D16
Source: tester FAIL (mode toggles on USB reconnect) → architect H1
Status: implemented 2026-11-19 — awaiting tester re-validate
```

### Changes made

1. `src/board_config.h` — `BOARD_MODE_SWITCH_ACTIVE_LEVEL` **1** (pressed = high);
   comment documents mainboard pull-down wiring.
2. `src/mode_store.c` — `gpio_pull_up` → `gpio_pull_down` on MODE_SWITCH in
   `mode_boot_resolve`.
3. No docs edits (architect already aligned H1/D16).

### MUST NOT (honored)

- No active-low rewiring of expectations
- No build/flash
- No `docs/**` edits

### Suggested tester (notes only)

```bash
./scripts/build.sh && ./scripts/flash.sh
# Replug USB without button → mode MUST stay the same
# Hold button while plugging → mode advances once
```

### Acceptance (implementer)

- [x] ACTIVE_LEVEL = 1 and pull-down in code
- [x] Notes list files changed
- [ ] Hardware PASS — **tester**

---

## Prior — `RGNX_FW_DEVICE_MODES_V1` / LED fix (**implemented** 2026-11-19)

Option A: removed `stdio_init_all` reclaim of GPIO0.

## Prior — `RGNX_FW_PINMAP_V1` (**implemented** 2026-11-19)
