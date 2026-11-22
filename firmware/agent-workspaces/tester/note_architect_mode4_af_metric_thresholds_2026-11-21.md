# Technical indication — Mode 4 AF quality metric thresholds

Date: 2026-11-21  
From: tester  
Audience: architect (then implementer)  
Related: `note_architect_mode4_af_metric_mismatch_2026-11-21.md`

## Goal

LED GOOD (long OFF) when inject is in the **same regime where lab decode
works**; BAD (short OFF) when it does not — without fighting FCS-OK hold.

## Bench anchors (this PC/VLC → GPIO28 path)

| Condition | Typical sticky ADC pp (12-bit) | Decode |
| --- | ---: | --- |
| ≤50% volume | pp often still ~130–150 in STAT sticky*, hunt only | **FAIL** (`ok=0`) |
| ~63% | pp ~same order | **marginal PASS** (`ok≥1`) |
| ~73% / loud | pp ≈ **130–150** (e.g. 1960…2113) | **PASS** (loud strong) |

\*Sticky min/max over long windows look similar across volumes; **do not**
trust long-run STAT min/max alone for fine volume. The **200 ms refresh
window** pp used by the LED path is the right place to measure — but the
**absolute GOOD=250 / BAD=180 ladder is far above** the pp that accompanies
successful decode on this board (~140).

Current code (`board_config.h`): `GOOD=250`, `BAD=180`, `ACTIVITY=40` — broken
here (see mismatch note).

## Recommended metric (primary + secondary)

### Primary (preferred): decode success

| Event | LED action |
| --- | --- |
| FCS-OK / frame queued to KISS (`afsk_on_rx_frame`) | **GOOD** + rearm hold (`HOLD_MS`, keep 10 s) |
| Hold running | Stay GOOD even if amplitude dips (silence OK) |
| Hold expired, no new FCS-OK | **BAD** |

This matches “quality of packet” literally. Amplitude alone on this jack is
a weak proxy (pp ceiling ~150 while decode varies with level/SNR).

### Secondary (amplitude assist — retuned for this ADC)

Use **only** as assist / cold start / no-traffic classification, with bands
below the observed decode ceiling:

| Symbol | Current | **Indicated** (12-bit pp over ~200 ms window) | Meaning |
| --- | ---: | ---: | --- |
| `AF_PP_ACTIVITY` | 40 | **30–40** (keep ~40 OK) | Below = silence; do not force BAD |
| `AF_PP_BAD` | 180 | **~70–90** | Active but too weak / useless for this front-end |
| `AF_PP_GOOD` | 250 | **~110–130** | Instant GOOD only if pp reaches “decode-class” swing |

Rationale: loud/73% decode sat at **pp≈130–150**. GOOD must be **≤ that
floor**, not 250. BAD must sit **clearly below** marginal decode (~63%), not
at 180 (which marks all real audio as BAD).

Starting lock suggestion for architect to ratify:

```text
BOARD_MODE4_AF_PP_ACTIVITY   40
BOARD_MODE4_AF_PP_BAD        80
BOARD_MODE4_AF_PP_GOOD       120
```

Tune ±10–20 after one live matrix; document in `notes.md`.

### Mandatory hold rule (fix with primary)

While `hold_armed` after FCS-OK:

- **Do not** `force_bad()` merely because `ACTIVITY ≤ pp ≤ BAD` (mid energy).  
- **May** `force_bad()` on **clip** (near-rail) if desired.  
- Silence (`pp < ACTIVITY`) → leave hold alone (already intended).

Without this, any amplitude ladder still fights packet-GOOD within 200 ms.

## Optional richer metric (if architect wants less pp dependence)

Reuse A+ STAT signals already computed:

- `eye` / `conf` rising with activity, or  
- short EMA of `|m − G·s|` soft decision  

Still: **FCS-OK remains the strongest “packet quality” bit.**

## Acceptance (tester)

1. Loud / ~73% while KISS flowing → LED **GOOD** (long OFF) and stays ~10 s
   after audio stops.  
2. ≤50% no KISS → LED **BAD** (short OFF), not stuck GOOD.  
3. After FCS-OK, mid-level pp must **not** yank GOOD→BAD in the next update.  
4. D36 OFF widths (260/50) unchanged unless human asks.

## Ask

Architect: lock primary=FCS-OK + hold fix; either adopt ~120/80/40 pp assist
or replace pp assist with A+ eye/conf; then implementer handoff.
