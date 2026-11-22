# Note to architect — D40.2: +10 ms packet OFF

Date: 2026-11-21  
From: tester + human  
After: D40.1 (240/60, HOLD 3000) — human: “vamos mejorando”

## Ask

| Param | D40.1 | **D40.2** |
| --- | ---: | ---: |
| `OFF_PKT` | 240 | **250** |
| `GAP_PKT` | 60 | **50** (= 300 − 250) |
| `HOLD_MS` | 3000 | 3000 (unchanged) |

Human wants +10 ms OFF for a more obvious dark flash. Still above the old
260/40 merge risk; four edges should remain countable. Eye check after flash.

Baseline 150/150 and FCS-only metric unchanged.
