# RETURN TO ARCHITECT — AFSK RX strategy (2026-11-21)

```text
From: tester
To: architect
After: D24 clock, D25 debug, D26 STAT TX, D27 mid-bit sync
Status: live ok=0 still; D27 regresses abort / disc_tr
Action: strategic design decision — stop clean-room thrash; base RX on a
        known-good libre Pico (or Pico-class) AFSK/KISS demodulator
```

## 1. Tester verdict (plain)

Hardware and inject path are **not** the blocker:

- ADC sees AC swing; L1 shows mark/space activity (`both=1`).
- Bits reach HDLC (`bits`↑).
- Full STAT works (D26).
- **`ok` remains 0** → no valid HDLC frames → no KISS.

D27 (mid-bit + hysteresis) made `align_slip=0` but **worsened** `abort`
(~3.7× vs D26) and nearly killed `disc_tr`. That is not “one more tweak”;
it shows **bit-sync experimentation without a reference modem**.

**Verdict:** Continuing iterative clean-room PLL/HDLC patches is a poor use of
implementer/tester cycles. The difficult part (robust AFSK bit sync + HDLC RX)
already exists in libre projects; RIGNEXO should **reuse or adapt** that rather
than rediscovering it under `TODO` counters.

## 2. Why “it works elsewhere” but not here yet

| Fact | Implication |
| --- | --- |
| Clean-room policy (D21/notes) forbade vendoring pico_tnc / MicroAPRS / etc. | We reimplemented the hard RX DSP from scratch |
| FT8 PASS on same analog/USB board | Proves PCM path @ 48 kHz + host DSP — **not** on-device Bell-202 RX |
| TX PASS (ear + PTT) | TX is easier to validate; RX needs locked baud |
| L2: abort + align_slip, rare bad_fcs, ok=0 | Failure is **bit timing / framing**, classic first-modem bugs |

So the pain is not mysterious hardware: it is **policy + greenfield RX** on a
problem that libre Pico TNCs already solved.

## 3. Ask of architect (strategic)

Please issue a **normative decision** that replaces open-ended clean-room RX
iteration with one of:

### Option R1 — **Preferred (tester recommendation): adopt a libre Pico TNC RX core**

- Survey and **select one libre** AFSK/KISS (or AX.25) demodulator that already
  runs on **RP2040/RP2350 / Pico SDK** (or trivially portable), e.g. community
  pico_tnc-class / similar **MIT/BSD/Apache** (license must be compatible with
  this tree — architect confirms).
- Normative rules for implementer:
  - Import **only** what is needed for RX demod + HDLC bit/frame path (or thin
    shim), with attribution and `notes.md` license section updated.
  - Keep RIGNEXO ownership of: pins/`board_config`, KISS CDC, PTT/TX SM, modes,
    USB descriptors, STAT debug.
  - Define module boundary: e.g. `afsk_rx` API still feeds `kiss_cdc_send_frame`
    on good frames; TX path may stay current or also align later.
- Acceptance unchanged in spirit: external `afsk_rx_bench_2min.wav` (or host
  Bell-202) → **`ok≥1`** and KISS DATA matching manifest section A; TX smoke
  still PASS; no own-TX echo as RX.

### Option R2 — Keep clean-room but **freeze algorithm to a published reference**

If legal/product requires no third-party code: architect must **lock** a
specific public algorithm (paper/spec + fixed constants) and forbid further
ad-hoc PLL/hyst experiments until that design is implemented as specified —
still slower than R1.

### Option R3 — Host-side softmodem (out of product intent)

Not recommended for “TNC mode” product goal; only if architect demotes
on-device AFSK RX.

**Tester recommendation: R1.**

## 4. Evidence pointers (do not rediscuss from scratch)

| Doc / run | Content |
| --- | --- |
| `findings_rx_debug_D25_2026-11-21.md` | ADC path, D25/D26, L2 causal §11 |
| `test_runs.md` | RX / EXT_WAV / RX_CLOCK / DEBUG_TX / **RX_SYNC FAIL** |
| D26 STAT3 baseline | abort=428 align_slip=70 ok=0 disc_tr≈6565 |
| D27 STAT3 | abort=1594 align_slip=0 ok=0 disc_tr≈3 |

## 5. Explicit non-requests

- Do not order more “tone detection” work (L1 already PASS).
- Do not blame FT8 interface or external WAV player.
- Do not require same-board TX→RX loopback.
- Do not ask tester for another blind WAV loop without a new architecture
  decision (R1/R2/R3).

## 6. One-line for architect decision log

**On-device AFSK RX stuck at ok=0 after D24–D27; recommend abandon clean-room
RX thrash and base demod/HDLC on a libre Pico TNC modem (Option R1).**
