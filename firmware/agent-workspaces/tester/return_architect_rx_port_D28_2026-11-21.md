# Return to architect — `RGNX_FW_APRS_TNC_RX_PORT` (D28) FAIL

Date: 2026-11-21  
Role: tester  
Handoff: external fixture WAV → `ok≥1` + KISS section A

## Verdict

**FAIL.** Pico_tnc RX path is live (energy/CDT/bit counts) but produces
**zero** `FRAME_OK` and **zero** KISS DATA under the same
`fixtures/afsk_rx_bench_2min.wav` used for D25–D27.

## Evidence (abbreviated)

| Poll | ene | dcd | bits | abort | bad_fcs | ok | kiss_ok |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| STAT1 ~8s | 2657 | 1 | 5726 | 0 | 0 | 0 | 0 |
| STAT2 ~28s | 1837 | 1 | 19191 | 0 | 0 | 0 | 0 |
| STAT3 ~48s | 763 | 0 | 32637 | 0 | 0 | 0 | 0 |
| STAT6 ~97s | 1717 | 1 | 64633 | 0 | 0 | 0 | 0 |

- `disc_tr=0` / `both=0` entire run (D28 remapped L1 — disc only if port
  increments that counter).
- ADC sticky min/max ≈ **1971…2104** (mid ≈ 2037) — ~133 peak-peak vs much
  wider swing reported under D25 clean-room path.
- CDC: STAT ASCII only; **0** KISS frames; section A **0/10**.
- TX host smoke: 3 bursts, **no** own-TX KISS echo on CDC.

Full run: `test_runs.md` → `RGNX_FW_APRS_TNC_RX_PORT`.

## What still works

- Device enum as RIGNEXO KISS TNC; `RGNX CLR` / `RGNX STAT` full dump.
- RX sample alarms advancing (`t104`/`t105`).
- Audio reaches ADC enough for varying `ene` and toggling `dcd`.

## Superseding ops note (2026-11-21, after D29 FAIL)

Read **`note_architect_rx_level_ops_2026-11-21.md`**: real AF level is
**not constant**; fixed-threshold / “set the volume” product stance rejected
by human. Next order must make adaptive front-end first-class (and fix D29
AGC dead-zone + pre-AGC CDT).

## Ask of architect

See deep dive: `analysis_D28_stat_vs_code_2026-11-21.md`.

**Root claim:** soft-decision frozen (`dem` never ±4096) ⇒ `disc_tr=0` while
`bits↑`; never flag `0x7E` ⇒ `ok=bad_fcs=0`. Contributing: ADC pp≈133 and
port `ADC_BIT 12` + raw `adc_read()` vs upstream default **`ADC_BIT 8`** +
FIFO 8-bit samples.

1. Lock sample-width: **A1** match upstream 8-bit feed, or **A2** keep 12-bit
   only with proven `dem` headroom + louder inject.
2. Implementer: apply A; optional STAT `dem_min`/`dem_max`.
3. Tester gates: `disc_tr↑` then `ok≥1` + KISS section A; TX smoke.
4. Do **not** chase KISS/CDC or revive D27 mid-bit as primary.
