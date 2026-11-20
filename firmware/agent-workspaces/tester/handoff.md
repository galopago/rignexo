# Tester handoff — `rignexo/firmware`

## Current — `RGNX_FW_CAT_IF_HAMLIB_V1` + CAT stack

| Field | Value |
| --- | --- |
| Compile | PASS |
| Linux CAT suite | PASS (prior) |
| WSJT-X Test CAT | **PASS** (human 2026-11-20) |

### Human validation

- Two different WSJT-X versions: **PASS**
- Kenwood TS-2000 emulator: **PASS**
- PTT via CAT (TX/RX): activates and releases — **PASS**

Prior FAIL (`unsupported VFO None`) resolved by D20 / 38-byte IF frame.

### Status

**CLOSED** for CAT host acceptance (human + prior automated suite).
