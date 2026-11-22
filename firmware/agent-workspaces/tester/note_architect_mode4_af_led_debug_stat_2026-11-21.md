# Note to architect — Real-time debug for Mode 4 AF LED metric + OFF timer

Date: 2026-11-21  
From: tester + human  
Context: D36 OFF contrast + D37 metric (FCS-OK primary, pp 120/80/40).  
Human: raising/lowering volume while playing the packet fixture does **not**
look coherent with the blinks. Tester needs the same class of visibility we
had for RX (`RGNX STAT` / KISS `0x0C`) to judge the AF→LED path.

## Problem

Lab already proved:

- Decode quality vs PC volume is roughly coherent (≤50% fail, ~63%+ decode).  
- LED OFF widths (260 vs 50) are visible.  
- Human still sees **volume changes ↔ blink behavior** as incoherent.

Without live numbers, tester cannot tell whether:

- metric never goes GOOD,  
- GOOD is armed then cleared,  
- `set_af_good` is called but PIO OFF regs are wrong,  
- or hold masks transitions the human expects when turning volume down.

## Ask (normative for a small debug handoff)

Expose **real-time** (or on-demand dump) of the values that drive the mode-4
AF LED, in the same host channel style as existing `RGNXDBG` (ASCII after
`RGNX STAT` / KISS `0x0C` is fine; a short periodic line also OK if cheap).

### Required fields (minimum)

| Field | Meaning |
| --- | --- |
| `af_pp` | Current peak-to-peak used this update window (12-bit) |
| `af_amin` / `af_amax` | Sticky min/max for that window (optional but useful) |
| `af_active` | 0/1 — `pp >= ACTIVITY` |
| `af_clip` | 0/1 |
| `af_good` | 0/1 — boolean currently commanding GOOD |
| `af_hold` | 0/1 — hold armed |
| `af_hold_ms` | ms remaining on hold (or 0 if not armed) |
| `af_reason` | small enum/code: e.g. `fcs` / `pp_good` / `pp_bad` / `clip` / `hold_exp` / `silence` / `none` — **why** last transition |
| `led_off_ms` | **Actual** OFF duration loaded into PIO for mode 4 right now (`OFF_GOOD` or `OFF_BAD`) |
| `led_gap_ms` | GAP loaded (`TICK − OFF`) |
| `led_tick_ms` | TICK (constant OK to echo) |

Also echo configured thresholds once or each dump: `pp_act` / `pp_bad` /
`pp_good` / `hold_cfg_ms` so tester does not guess firmware constants.

### Delivery

- Prefer extending existing `RGNXDBG` dump (new `Laf` / `Lled` lines) so the
  same Python capture used for packet STAT works.  
- Dump must be complete under load (D26 lesson: no truncation).  
- Cadence: on each `STAT`, and/or every AF update (~200 ms) **only if** a
  debug flag / `RGNX AFDBG ON` is enabled (default off for field).

### Non-goals

- Changing OFF widths again in this handoff.  
- Changing demod.  
- Requiring SWD.

## Tester plan after implement

Same fixture loop at **50% / 63% / 73% / loud**:

1. Human sets volume; announces level.  
2. Tester CLR + STAT (or AFDBG stream) while WAV plays.  
3. Correlate: `af_pp` / `af_good` / `af_reason` / `led_off_ms` vs known decode
   behavior at that level.  
4. Report coherent / incoherent with evidence lines.

## Acceptance

1. Host can read `af_*` and `led_off_ms` live.  
2. When FCS-OK happens, dump shows `af_reason=fcs` (or equivalent) and
   `led_off_ms=OFF_GOOD`.  
3. When human drops to no-decode volume, dump shows path to BAD and
   `led_off_ms=OFF_BAD` within hold rules.  
4. Tester can explain blinks from numbers without guessing.

## Ask

Architect: open a debug-STAT handoff (metric + OFF timer visibility).  
Implementer: ship dump fields. Tester: volume matrix with correlation.
