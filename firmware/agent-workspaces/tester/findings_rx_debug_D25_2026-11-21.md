# Technical findings — AFSK RX debug (D25) — for architect

```text
From: tester
To: architect
Date: 2026-11-21
Related IDs: RGNX_FW_APRS_TNC_RX, RGNX_FW_APRS_TNC_RX_CLOCK (D24),
             RGNX_FW_APRS_TNC_RX_DEBUG (D25)
Purpose: Enough evidence for architect to issue specific implementer orders.
```

## 1. Executive summary (current — after D26)

Live AFSK into GPIO28 (external WAV) **reaches ADC** and **L1 tones PASS**
(`both=1`, `disc_tr`↑). D26 fixed STAT transport: **full L0–L3 readable**.

L2 under WAV: `bits`↑, **`abort` dominates**, **`align_slip`↑**, `bad_fcs`
rare, **`ok=0`**, `kiss_ok=kiss_drop=0`.

**Causal conclusion for architect:** discriminator activity is fine; the
**bit-clock / NRZI→HDLC stream is not forming byte-aligned frames**. Valid
frames are not closed because of **bit insertions/deletions / mis-timed emits**
(see §10–§11), not because of missing audio or KISS FIFO drops.

**Next implementer focus:** bit sync / SPB PLL emit path + HDLC alignment
behavior — goal `ok>0` on same fixture. Do **not** order more tone detection
or STAT TX work.

## 2. Sampling path (normative reminder)

In `APRS_TNC` (mode 4):

- Samples come from **`adc_read()` on GPIO28** inside the AFSK timer/alarm path
  (`afsk_bell202.c`), converted as `(raw - 2048) << 4`.
- **UAC2 / `audio_adc` @ 48 kHz is not used** (not started in this mode).
- FT8 success validates the composite USB audio pipe, **not** this AFSK stack.

## 3. Test setup that produced the numbers

| Item | Detail |
| --- | --- |
| Firmware | Build after D25 debug (`RGNX_FW_APRS_TNC_RX_DEBUG`); device `cafe:4101` |
| Inject | `fixtures/afsk_rx_bench_2min.wav` from **external** player → TNC audio in |
| Host | CDC open; ASCII `RGNX CLR` / `RGNX STAT` |
| Fixture | 41 HDLC/AFSK packets; manifest in same fixtures dir |

Prior controls (same 0-KISS symptom): host `aplay` S16 to another board’s USB
speaker → cable → TNC; eliminated “PipeWire routing only” as sole cause.

## 4. Observed STAT (partial — truncated)

Host typically receives **~128 bytes**, then silence. L0 and L1 appear mashed;
**L2 / L3 / Lclk / END never arrive**.

Representative values while WAV was playing (after CLR, several STAT polls):

### Layer 0 — energy / ADC

| Field | Observed | Interpretation |
| --- | --- | --- |
| `ene` | ~18 … 64 | Smoothed \|x\| **well below** `BOARD_AFSK_DCD_THRESHOLD` (400) |
| `dcd` | **0** | DCD never asserted |
| `dcd_ms` | **0** | |
| `adc_min` / `adc_max` | e.g. ≈ −980 … +1029 | **AC swing present** on ADC path |

**Architect implication:** “No DCD” ≠ “no signal”. Demod does not gate on DCD
for bit decisions; energy metric is simply low vs threshold. Optional later:
retune DCD for this inject level — **not** the first blocker for KISS silence.

### Layer 1 — tone / discriminator (answered)

| Field | Observed | Interpretation |
| --- | --- | --- |
| `disc_tr` | Rising strongly (e.g. 831 → 5k → 10k → 61k across polls) | Frequent mark↔space decisions |
| `mark_n` / `space_n` | Both large | Both polarities seen |
| `both` | **1** | Latched “saw both” |

**Architect implication:** Layer-1 acceptance from tester return is **met**.
Do **not** order more “detect tones” work as the primary fix. Order work that
explains **tones present → still 0 KISS**.

### Layer 2 / 3 — unknown (instrumentation defect)

Cannot report `bits` / `abort` / `bad_fcs` / `ok` / `align_slip` / `kiss_*`
from hardware because dump truncates before those lines.

## 5. Root cause of STAT truncation (code-level)

In `afsk_rx_debug_dump_now()` (`afsk_bell202.c`):

- Multiple `afsk_cdc_write_str()` → `tud_cdc_write()` **without** waiting for
  FIFO space or flushing between lines.
- Single `tud_cdc_write_flush()` only at the end.

TinyUSB CDC TX FIFO is small (~64–128 B class on this build). L0+L1 already
fill it; later `tud_cdc_write` calls **drop** silently → host never sees L2+.

This is an **implementer defect in D25 delivery**, separate from demod math.

## 6. Prior analysis still relevant (D24 context)

Integer timer `1000000/9600 → 104 µs` caused ~1201.9 baud vs host 1200; offline
sim showed exact 9600 PASS vs 9615 FAIL (`NONE_ALIGN`). D24 Bresenham + C-lite
was implemented; **live KISS still 0**. So clock fix alone did not close the
case; with L1 now green, architect should treat **HDLC/bit-path + readable L2**
as the next design focus.

## 7. Recommended architect → implementer orders (suggested wording)

Architect should turn these into a formal handoff ID / acceptance; tester
suggests priority:

### Order 1 — Fix RX debug STAT transport (blocking)

- Make `RGNX STAT` / KISS dump cmd deliver **all** normative lines through
  `RGNXDBG END` reliably over CDC.
- Mechanism (architect picks one): chunked write with
  `tud_cdc_write_available` wait + `tud_task` yield; flush per line; or
  shorter lines / multi-poll continuation.
- Acceptance: host receives complete L0–L3 + Lclk + END in one STAT after CLR,
  under load (WAV playing), ≥3 repeated STAT with no truncation.

### Order 2 — After Order 1: capture L2 during fixture WAV (tester)

- Tester: CLR → play WAV → STAT; record `abort`, `bad_fcs`, `ok`,
  `align_slip`, `bits`, `kiss_ok`/`kiss_drop`.
- Architect uses that branch:
  - `ok>0` but `kiss_drop` → KISS/CDC path
  - `bad_fcs`/`align_slip` dominate → bit sync / FCS / alignment
  - `abort` dominate → HDLC hunt/flag
  - `bits≈0` despite L1 → bug between disc and `hdlc_rx_bit` (should be rare
    if L1 counts disc emissions correctly)

### Order 3 — Only after L2 evidence: demod/HDLC change

- Do **not** redesign discriminator until L2 is visible.
- Candidate areas once L2 known: fractional SPB emit path, HDLC
  `bitcount!=0` → `NONE` (silent), FCS, stuff/flag SM.

### Non-orders (explicit)

- Do not “fix FT8 USB sound card”.
- Do not require same-board TX→RX loopback (analog + RX muted in TX).
- Do not treat DCD=0 alone as proof of no AFSK (L1 already proves activity).

## 8. Artifacts

| Path | Role |
| --- | --- |
| `agent-workspaces/tester/fixtures/afsk_rx_bench_2min.wav` | Inject stimulus |
| `…/afsk_rx_bench_2min_manifest.md` / `.json` | Expected payloads |
| `agent-workspaces/tester/test_runs.md` | RX / EXT_WAV / RX_CLOCK / DEBUG runs |
| `agent-workspaces/tester/handoff.md` | Latest return status |
| `docs/aprs_tnc_mode_architecture.md` | D24/D25 normative (architect-owned) |

## 9. One-line for architect decision log

**D25 L1 PASS (tones); STAT CDC truncated (block L2); 0 KISS remains; next =
fix STAT TX then branch on L2 counters — not more tone detection.**

---

## 10. Update — D26 STAT fix + L2 branch (2026-11-21)

### D26 transport

Tester: 3× full `RGNX STAT` under external WAV → **PASS** (L0–L3 + Lclk + END,
`dump_fail=0`).

### L2 numbers (after CLR, WAV playing)

| Poll | bits | abort | bad_fcs | ok | align_slip |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 598 | 15 | 1 | 0 | 7 |
| 2 | 5990 | 228 | 2 | 0 | 42 |
| 3 | 11336 | 428 | 2 | 0 | 70 |

`kiss_ok=0`, `kiss_drop=0`; L1 still `both=1`; `dcd=0`.

### Architect order suggestion (replaces “wait for L2”)

**Order 3 now concrete:** implementer focus on **bit-clock emit + HDLC
abort/align_slip** (see §11), not tone layer, not KISS FIFO. Goal: drive
`ok>0` under same fixture; then KISS DATA should appear (`kiss_drop` stayed 0).

---

## 11. Why valid frames are not closed (architect-facing causal analysis)

### 11.1 What “closing a valid frame” requires

Air HDLC: `0x7E` flag → integer number of data+FCS bits (multiple of 8) →
`0x7E`. Then CRC-16/X.25 over body matches trailing two bytes → `ok`.

Firmware behavior (`hdlc_ax25.c`):

| Counter | When it increments | Meaning |
| --- | --- | --- |
| `align_slip` | Flag close (`ones==6` then `0`) but `s_rx_bitcount != 0` | Stream length since last flag **not byte-aligned** → deliver returns `NONE` (not `bad_fcs`) |
| `abort` | e.g. `ones > 6` (seven+ HDLC `1`s), or buffer overrun | Invalid bit run / hunt reset |
| `bad_fcs` | Aligned frame assembled, CRC mismatch | Rare in our bench |
| `ok` | Aligned frame + good CRC | **Never** in live WAV bench |

NRZI reminder: HDLC `1` = **no** tone change; HDLC `0` = tone transition.
Seven HDLC `1`s ⇒ discriminator reported the **same** tone for seven emitted
bits → classic symptom of **bit clock too fast** (oversampling one tone as
many holds) or lost transitions.

### 11.2 What the live counters imply

Under WAV (STAT after CLR):

1. `bits` large → `hdlc_rx_bit` is fed continuously.
2. `abort` ≫ `bad_fcs` → failures are mostly **hunt/format/bit-run**, not
   “almost-good frames with wrong CRC”.
3. `align_slip` rising → flags are sometimes recognized but **off byte
   boundary** ⇒ **wrong bit count** between flags (insertions/deletions).
4. `ok=0` + `kiss_drop=0` → nothing good to send; CDC is not the bottleneck.
5. L1 still healthy → do not blame ADC silence or “no mark/space”.

**L1 can look good while L2 fails:** `mark_n`/`space_n`/`disc_tr` tally
**per ADC sample**, not per correct 1200-baud decision. Tone activity ≠ locked
baud.

### 11.3 Suspected mechanism (for architect to turn into implementer scope)

Pipeline after disc (`afsk_bell202.c`):

```text
disc (per sample) → fractional SPB PLL (s_pll_acc / s_spb)
                 → afsk_rx_emit_bit(disc)  // NRZI → hdlc_rx_bit
```

If emit phase/rate ≠ true 1200 baud grid of the inject:

- Extra emits on a stable tone → strings of HDLC `1` → **`abort`**
- Extra/missing bits between flags → **`align_slip`**
- Almost never a clean framed CRC candidate → **`ok=0`**, few `bad_fcs`

Observed `spb_x100≈820` (nominal 800 = 8.00 samples/bit) shows the C-lite
loop is **pushing** SPB away from nominal while still not achieving `ok>0`.

Historical note: pre-D24 integer timer `1000000/9600→104µs` caused ~1600 ppm
baud error vs host 1200; offline sim matched FAIL. D24 Bresenham+C-lite did
not clear live `ok=0`; L2 now localizes residual issue to **emit/align**, not
tone detect.

### 11.4 Explicit non-causes (with evidence)

| Hypothesis | Verdict |
| --- | --- |
| No audio on GPIO28 | Rejected — ADC min/max swing; L1 `both=1` |
| FT8 / UAC2 path broken | N/A — TNC uses direct `adc_read` |
| KISS FIFO drops OK frames | Rejected — `kiss_drop=0`, `ok=0` |
| Primary issue is FCS only | Rejected — `bad_fcs` tiny vs `abort`/`align_slip` |
| STAT unreadability | Fixed in D26 |

### 11.5 Suggested normative implementer acceptance (architect to refine)

Same external fixture (or host Bell-202 @ 1200 exact):

1. After CLR + ≥30 s WAV: `ok ≥ 1` (prefer many of section A payloads).
2. `align_slip` and `abort` rates drop materially vs baseline table in §10.
3. Host sees KISS DATA matching manifest for nominal packets.
4. TX smoke still PASS (PTT + audible AFSK); half-duplex mute unchanged.

### 11.6 One-line (replace §9 for decision log)

**L1 PASS; D26 STAT PASS; L2 shows abort+align_slip with ok=0 — bit sync /
HDLC alignment broken, not tones or KISS drops.**
