# Implementer handoff — `rignexo/firmware`

## Current — `RGNX_FW_MODE4_PKT_LED_TWEAK2` (**implemented** 2026-11-21)

```text
ID: RGNX_FW_MODE4_PKT_LED_TWEAK2
Date: 2026-11-21
Role: implementer only (no build/flash)
Normative: D40.2 — 250/50 + hold 3000
Status: source done — tester smoke when asked
```

### Changes

1. `board_config.h` — `OFF_PKT=250`, `GAP_PKT=50` (HOLD 3000 unchanged).
2. Comments + `notes.md`.
3. No logic / metric changes.

### Acceptance

- [x] OFF_PKT=250, GAP_PKT=50, HOLD=3000  
- [x] no logic changes  
- [x] notes.md  
- [x] no build/flash  

---

## Prior — D40.1 / D40.0 / D39 / D38
