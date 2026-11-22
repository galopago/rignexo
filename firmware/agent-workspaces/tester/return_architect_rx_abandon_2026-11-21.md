# Return to architect — RX live decode ABANDONED (CLOSED FAIL)

Date: 2026-11-21  
From: tester + human  
Action: **Stop** the D24–D30 patch spiral. Human will replan with architect.

## Closed verdict

**CLOSED FAIL.** Acceptance (external fixture → `ok≥1` + KISS section A) was
**never** met on the bench across clock, debug, sync, pico_tnc port, A1/AGC,
and field-robust front-end orders.

## Why abandon (not “one more patch”)

- Same fixture, same weak ADC sticky span (~8 counts on 8-bit after A1).
- Latest D30: front-end metrics green (`agc` max, `cdt`, `disc_tr`, `dem`)
  while **`ok=bad_fcs=kiss=0`** — celebrating front-end is false progress.
- Further AGC/CDT/B1 tuning under this framing is judged **non-productive**.

## Preserved inputs for the rethink (do not discard)

| Doc | Use |
| --- | --- |
| `note_architect_rx_level_ops_2026-11-21.md` | AF level is not constant; no “set volume” product |
| `analysis_D28_stat_vs_code_2026-11-21.md` | frozen `dem` / ADC_BIT divergence |
| `analysis_D29_agc_cdt_2026-11-21.md` | AGC dead-zone + pre-AGC CDT |
| `analysis_D30_front_ok_no_frame_2026-11-21.md` | edges ≠ frames |
| `test_runs.md` | chronological FAIL/PARTIAL evidence |
| `fixtures/afsk_rx_bench_2min.*` | still the regression probe when a new plan exists |

## Explicit non-requests

- No D31 “slightly better AGC” under the old handoff chain.
- No CLOSE PASS on `disc_tr` / `agc_q8` alone.
- TX/shell prior PASS stands until a new design touches them.

## Ask

Architect + human: **new problem statement and handoff ID** before any
implementer RX work resumes (e.g. offline golden demod, hardware path audit,
alternate modem strategy — architect decides; tester waits).
