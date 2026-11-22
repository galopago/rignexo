# Tester handoff — `rignexo/firmware`

## D40.2 tweak — pending implementer / flash

Expect: FCS → `led_off_ms=250`, gap 50, hold ~3 s
(`RGNX_FW_MODE4_PKT_LED_TWEAK2`).

## D40.1 — landed (240/60); superseded by D40.2

## D40 `RGNX_FW_MODE4_PKT_LED_LONG` — STAT PASS

| Check | Result |
| --- | --- |
| Idle 150/150 | PASS |
| FCS → off=220 gap=80 | PASS (20/31 polls) |
| hold_exp → 150/150 | PASS |
| Decode | ok=90 kiss=49 |
| No D39-80 / no 260/50 | PASS |

**Human:** long OFF visible. Approved tweak → `240/60` + hold 3000
(`note_architect_D40_led_tweak_2026-11-21.md`).
