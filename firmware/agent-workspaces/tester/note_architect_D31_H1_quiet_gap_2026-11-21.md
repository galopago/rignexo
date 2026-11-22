# Note to architect — D31 A+ H1 results + quiet-gap options

Date: 2026-11-21  
From: tester (human-directed)  
Handoff: `RGNX_FW_APRS_TNC_RX_APLUS`

## Bottom line for the architect

**The live A+ modem works** at reference (~73% PC/VLC) and especially at
**loud** inject. It does **not** yet meet product H1 because **quiet (34%)
FAIL**. Do not treat the D24–D30 abandon as “RX impossible”; D31 proved
decode. Do not CLOSE until quiet is addressed or the quiet floor is
explicitly renegotiated.

## H1 evidence (same fixture, WAV loop, KISS CLR `0x0D` / STAT `0x0C`)

| Level | PC/VLC vol | Result | Highlights |
| --- | --- | --- | --- |
| Ref | ~73% | **PASS** (marginal) | `ok=1`, KISS `RGNX-H-REPEAT` (H); section A empty that window |
| Quiet | **34%** | **FAIL** | `ok=0`, 0 KISS; `dcd=1`; `flag`/`entered`/`bad_fcs`↑ (~3k / ~1k) |
| Loud | high | **PASS** (strong) | STAT `ok=64`, `kiss_ok=25`; **A01–A10**; also B/C/E/F/G/H/I; slicers ok0/ok1/ok2 |

Offline O0/O1 (implementer): already PASS including digital quiet/loud scale.
So live quiet failure is **not** “algorithm never works at low amplitude”; it
is a **live weak-AF / SNR / coupling** gap vs the digital 0.25× case.

H2 on loud: counters move; per-slicer `ok0`/`ok1`/`ok2` contributed.  
TX host smoke: no own payload echo (human PTT not re-audited every pass).

## Human product stance (unchanged)

AF level is **not** constant in the field. “Use 73%+ only” is not an
acceptable long-term product answer. See prior
`note_architect_rx_level_ops_2026-11-21.md`.

## Recommended next work (improve quiet **without** breaking loud/ref)

Prefer **additive / parallel** changes; regression matrix must re-run
**34% / ~73% / loud** on the same fixture.

### Prefer (in order)

1. **Dual-tone / per-tone normalize (B2-class, clean-room)**  
   Quiet often hurts mark/space balance; relative normalize helps weak AF
   and should not punish loud (already strong).

2. **Confidence / quality gate before committing HDLC pressure**  
   Quiet already shows heavy `flag`/`bad_fcs` with `ok=0`. Blindly lowering
   thresholds risks more garbage. Gate on soft `|m−s|` / eye quality so quiet
   gets cleaner bits; loud stays as-is or improves.

3. **Extra parallel slicer arm(s) for weak AF**  
   Keep existing geometric slicers that win on loud (`ok0`/`ok1`/`ok2`).
   Add aggressive weak path in parallel + dup suppress (already present).

### Secondary

4. Soft DCD/enable only if a future quiet run shows `dcd=0` while ADC has
   real AF — **not** the main issue at 34% (`dcd` was already 1).

### Do **not**

- Revive D29/D30 AGC-only patch spiral.  
- Retune a **single global** fixed threshold until quiet passes (likely
  damages loud false-frame rate).  
- CLOSE on loud-only or ref-only success.  
- Claim PASS from `flag`/`bits` alone (quiet already has those).

## Suggested acceptance after a quiet-gap order

| Gate | Pass |
| --- | --- |
| Quiet ~34% (or architect-defined quiet floor) | `ok≥1` and/or KISS; prefer ≥1 section-A if fixture timing allows |
| Ref ~73% | still `ok≥1` / KISS (no regress) |
| Loud | still strong (section A still OK; no collapse) |
| TX smoke | still no own-TX KISS echo |

## Ask

Architect: either (a) open a **new** implementer ID for quiet-gap work above,
or (b) explicitly redefine the quiet floor with human product buy-in. Tester
waits for that ID before more live thrash.
