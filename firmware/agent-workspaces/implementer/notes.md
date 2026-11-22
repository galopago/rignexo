# Implementer notes — APRS TNC / mode LED

## D40.2 — Mode 4 packet LED tweak (+10 ms OFF)

| State | OFF / GAP | Trigger |
| --- | ---: | --- |
| Baseline | **150 / 150** | Cold, CLR, hold expiry, no FCS |
| Packet OK | **250 / 50** | FCS-OK only; hold **3 s** |

TICK 300, quiet 800, 4 pulses. Path: 220/80 → 240/60 → **250/50**. Do not
jump to 260/40 without eye check. FCS-only; no `pp_good`.

| Event | Action |
| --- | --- |
| FCS-OK | OFF **250** + hold 3000 ms |
| Hold expiry / CLR / init | baseline 150/150 |

### D38 STAT

`led_off_ms=150` idle; **250** after FCS.

## A+ RX (D31 / D33)

24 kHz A+, N=5.
