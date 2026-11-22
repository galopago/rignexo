# Note to architect — D40 LED tweak (human OK + small bumps)

Date: 2026-11-21  
From: tester + human  
After: D40 STAT PASS; **human confirms long OFF is visible**

## Current (D40)

| | Value |
| --- | ---: |
| Baseline | 150 / 150 |
| Packet OK | **220 / 80** |
| HOLD | **2000** ms |
| TICK (↓→↓) | 300 ms fixed |

## Human ask

Slightly longer OFF on packet OK; slightly longer hold.

## Tester proposal (keep ↓→↓ = 300; keep 4 edges readable)

| Param | Now | **Propose** | Why |
| --- | ---: | ---: | --- |
| `OFF_PKT` | 220 | **240** | +20 ms darker; GAP **60** still >40 (avoid 260/40 merge) |
| `GAP_PKT` | 80 | **60** | = 300 − 240 |
| `HOLD_MS` | 2000 | **3000** | +1 s glance; not sticky forever |

**Do not** jump to 260/40 without eye check — earlier that collapsed the
four dips. If 240 still weak, next step 250/50, not 260.

Metric unchanged: FCS-only; idle/bad = 150/150; no `pp_good`.

## Ask

Small ID: constants only `OFF_PKT=240`, `GAP_PKT=60`, `HOLD_MS=3000`.

**Human approved 2026-11-21:** proposal accepted as written.
