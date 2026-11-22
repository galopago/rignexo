# Note to architect — real RX level / threshold policy (human + tester)

Date: 2026-11-21  
From: tester (human-directed)  
Re: post-D29; supersedes any design that assumes a “set and forget” inject level

## Human / product constraint (normative for next RX order)

**It is not possible to keep a constant audio volume** into the TNC in real
operation.

Field conditions include, without calibration ritual:

- Different radios, squelch, and AF gains  
- Different cables, attenuators, and bias/coupling  
- Host/player volume and OS mixer changes between runs  
- Packet-to-packet and session-to-session level swings  

Bench fixture `afsk_rx_bench_2min.wav` is only a **repeatable probe**, not a
promise that production AF will sit at one peak-to-peak forever. Designs that
only work after the human “sets the right volume” are **not acceptable** as
the primary RX strategy for mode 4.

## What is ridiculous (and must not be the product answer)

Relying on a **fixed soft-decision threshold** (pico_tnc `±4096`) and/or
**fixed CDT energy gates** as if the operator will present a constant,
well-scaled waveform is **not credible** for this product.

- Fixed `LPF_THRESHOLD` / CDT constants copied from pico_tnc assume the
  **front-end already delivers** a usable amplitude band.  
- Telling the operator (or tester) to “raise volume until it decodes” is a
  lab workaround, **not** an architecture.  
- D29’s AGC was ordered exactly because level varies — then the shipped
  adapt loop **did not move** (`agc_q8` stuck at 256) and CDT still judged
  **pre-AGC** energy, so the TNC refused to demodulate the same weak inject
  that still had measurable ADC activity. That combination is a failed
  response to the variability requirement.

## Required design stance for the next handoff

1. **Assume level is unknown and time-varying** within a wide practical range
   (from near-noise-floor AF up to near clipping on the 8-bit feed).  
2. **Front-end adaptation is mandatory**, not optional polish:
   - AGC (or equivalent) must **actually change gain** for small errors (no
     integer dead-zone that freezes at 1.0×).  
   - Carrier/energy detect used to enable the demod must see **post-gain**
     (or otherwise level-robust) energy — not a pre-AGC gate that stays dark
     forever on quiet-but-real AF.  
3. Fixed pico_tnc slice `±4096` may remain **only if** the adapted input is
   driven into the band those constants expect. If that cannot be guaranteed,
   the architect must own an **explicit** level-robust alternative (e.g.
   threshold as a fraction of recent `|dem|`, or documented AGC locks that
   keep `|dem|` near that scale) — **not** “human constant volume”.  
4. Acceptance must include **at least two inject levels** on the same fixture
   (quiet and loud), not a single “sweet spot” volume. Tester will refuse to
   CLOSE on one lucky level alone.

## Bench evidence (pointer)

- D28: energy/CDT moved; `disc_tr=0`; `ok=0` (frozen `dem` / scale).  
- D29: 8-bit feed OK (`adc` 123…131 ≡ prior `>>4`); **`agc_q8=256` stuck**;
  `ene=bits=disc=dem=0`.  
  Detail: `analysis_D29_agc_cdt_2026-11-21.md`.

## Ask

Rewrite the next RX order so **variable AF level is a first-class constraint**,
and drop any acceptance or narrative that depends on a constant operator
volume or on fixed thresholds alone without a working, level-tracking
front-end.
