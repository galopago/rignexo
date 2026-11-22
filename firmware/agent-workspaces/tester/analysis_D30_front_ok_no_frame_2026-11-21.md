# D30 debug run — front-end alive, still 0 frames

Date: 2026-11-21  
Handoff: `RGNX_FW_APRS_TNC_RX_FIELD`  
Window: ~110 s, external WAV, CLR + 8× STAT + KISS listen

## What improved vs D29

| Item | D29 | D30 this run |
| --- | --- | --- |
| `agc_q8` | stuck 256 | **8192** (GAIN_MAX) from first STAT |
| `cdt` / `dcd` | 0 | **1** entire window |
| `disc_tr` | 0 | **1725 → 35338** |
| `dem_*` | 0/0 | dem_min≈−2.3e6 dem_max≈1.3e6 `dem_env` tens of k–300k |
| `adapt` | n/a | **1** |
| `ok` / KISS | 0 | **still 0** |

D29 ops note satisfied on “AGC must move + CDT must open”. Human variable-level
front-end **engages**. Decode acceptance **not** met.

## Raw STAT arc (abbrev)

- adc sticky **123…131** (same weak 8-bit inject; AGC at ceiling, `pk=1` `vl=0`)
- `ene` post-AGC varies (1.8k–34k); CDT stays open
- `bits` ≈ 110k @ ~110 s ≈ full 1200 baud while CDT on
- `abort=bad_fcs=ok=kiss_*=0` every poll; 0 KISS DATA bytes

## Code reading (why edges ≠ frames)

1. **B1 adaptive slice** flips `bit` from `|dem|` vs `k*dem_env` — explains
   `disc_tr`↑ even with tiny raw ADC.
2. HDLC still needs NRZI bitstream to produce flag `0x7E`, then FCS. With
   `ok=bad_fcs=0`, the machine **never completes** `output_packet` with a
   candidate (never left FLAG usefully, or never hit six-1s end with
   `data_cnt≥MIN_LEN`). Sticky `abort` still only means overflow.
3. AGC max + `pk=1` means the loop treats input as near noise floor and
   **full-gains** it — possible **noise-driven edge rain** that looks like
   `disc_tr` health but is not Bell-202 structure. Architect must separate
   “edges exist” from “AFSK/HDLC lock” (e.g. flag hunt rate, eye, or
   `bad_fcs` before `ok`).

## Gates this single level

| Gate | Result |
| --- | --- |
| Front (agc↑, cdt, disc, dem) | **PASS** |
| `ok≥1` / `bad_fcs≥1` | **FAIL** |
| KISS section A | **FAIL** |
| Loud/quiet pair | only **one** level this session — do not CLOSE |
| TX host no-echo | PASS host-side |

## Ask of architect

1. Do **not** call D30 closed: front-end ≠ decoder.
2. Next order: explain/fix **live `disc_tr` + huge `dem` + `ok=0`**
   (polarity/NRZI, B1 thr vs correlator bias, noise at max AGC, flag
   instrumentation). Prefer a STAT counter for **flags seen** / time in DATA.
3. Keep field rule: no constant-volume product; but also no CLOSE on
   edge-count alone.
4. Tester will re-run Q+L after that fix — not another AGC-only spin.
