# APRS TNC Mode Architecture (Mode 4 — Software Modem)

Normative architecture for a fourth boot-time device mode: an on-device
**1200 baud Bell 202 AFSK modem** exposed to the host as a **KISS TNC** over
USB CDC. This is the “make a modem” personality for APRS / packet apps.

Source handoff: `RGNX_FW_APRS_TNC_V1`  
Related: [`device_modes_architecture.md`](device_modes_architecture.md),
[`architecture.md`](architecture.md), [`test_strategy.md`](test_strategy.md).

All agentic development methodology documentation in this firmware tree is
maintained in English.

## Normative Language

- `MUST` / `MUST NOT` / `SHOULD` / `MAY` as in other architecture docs.

## Purpose

Give hosts that speak **KISS** (YAAC, Xastir, APRSIS32, APRSdroid via USB OTG
adapters, Linux AX.25 `kissattach`, etc.) a serial TNC that:

1. Accepts AX.25 frames from the host over USB CDC (KISS framing).
2. Modulates them to **Bell 202 AFSK** audio out the existing radio TX path.
3. Demodulates AFSK from the existing radio RX path back to KISS frames.
4. Keys **PTT** from the modem TX state (TXDELAY / TXTAIL), not RTS or CAT.

The MCU **is** the modem. The host builds APRS/AX.25; the device does audio ↔
bits. This is intentionally different from mode 1 (host soundcard + RTS) and
mode 3 (host CAT + USB audio).

## Survey of Similar Projects (research input)

| Project | Platform | What it is | Fit for RIGNEXO |
| --- | --- | --- | --- |
| [amedes/pico_tnc](https://github.com/amedes/pico_tnc) | RP2040 Pico | Soft Bell-202 encode/decode, digipeat, beacon, **KISS**, USB+UART | Closest full softmodem reference (BSD-3) |
| [pfiliberti/pico_tnc](https://github.com/pfiliberti/pico_tnc) (Pocket Pico) | RP2040 | Softmodem + Z80 HK-21 TNC emu + KISS switch | Too heavy (PBBS/connected mode); modem core still useful as reference |
| [eleccoder/raspi-pico-aprs-tnc](https://github.com/eleccoder/raspi-pico-aprs-tnc) | RP2040/RP2350 | **TX-only** APRS AFSK PWM beacon | Good TX tone example; **not** a bidirectional TNC |
| [markqvist/MicroAPRS](https://github.com/markqvist/MicroAPRS) / MicroModem | AVR-class | Soft AFSK + KISS + APRS helpers | Proven demod/mod algorithms; license/portability check before reuse |
| [markqvist/OpenModem](https://github.com/markqvist/OpenModem) | OpenModem HW | Successor softmodem (300/1200/2400) | Reference for modern stack; not Pico-native |
| [wb2osz/direwolf](https://github.com/wb2osz/direwolf) | Host PC | Soundcard software TNC | Already covered by mode 1 (UAC2 + host Direwolf) — **not** mode 4 |
| [mcapdeville/esp32s3aprs](https://github.com/mcapdeville/esp32s3aprs) | ESP32-S3 | KISS CDC + UAC2 + tracker | Parallel product idea; different MCU/stack |
| [cgorski/yodel](https://github.com/cgorski/yodel) | Rust crate | Bell-202 + HDLC + KISS + APRS codecs | Excellent protocol reference; not drop-in C firmware |

**Product insight:** Mode 1 already lets a **host** TNC (Direwolf) use USB
audio. Mode 4 exists so the **device** is the TNC — lower host CPU, works with
apps that only open a COM/KISS port, and keeps radio audio timing on the MCU.

## Options Considered for Mode 4

| ID | Option | Pros | Cons | Verdict |
| --- | --- | --- | --- | --- |
| **A** | **KISS softmodem** (Bell-202 TX+RX, HDLC, KISS CDC, PTT from modem) | Matches “modem/TNC”; works with mainstream APRS apps; reuses pins 28/2/14/27 | Hardest DSP; CPU load on RX | **Chosen for v1** |
| B | TX-only APRS beacon (fixed/callsign in flash) | Small; easy PWM tones | Not a general TNC; no host app path | Deferred / optional later |
| C | “APRS via USB audio” only (no new mode) | Already mode 1 | Not a TNC; user must run Direwolf | Out of scope for mode 4 |
| D | Full AX.25 connected TNC + PBBS (Pocket Pico style) | Standalone packet station | Huge scope; not APRS-focused | Non-goal v1 |
| E | MicroAPRS shorthand + GPS tracker | Nice for mobile digi | Needs GPS UART pin; APRS stack on device | Non-goal v1 (MAY later) |

**Decision (D21):** Mode 4 = **`APRS_TNC`**: software Bell-202 **1200 baud**
modem + HDLC bit-(un)stuffing + CRC-16/X.25 check + **KISS over USB CDC**.
No UAC2 in this mode. No GPS, digipeater, IGate, or connected-mode AX.25 in
v1.

## Mode Identity

| Field | Value |
| --- | --- |
| Enum name | `APRS_TNC` (mode index **4**) |
| Host USB | **CDC only** (KISS). **No** UAC2 speaker/mic in this mode |
| PTT source | Modem TX state (`TXDELAY` … frame … `TXTAIL`) |
| RTS / CAT / VOX | **MUST NOT** key PTT |
| MODE_LED | **4** OFF pulses per cycle (PIO). Quiet + inter-blink period **fixed**;
  OFF **duty** may track AF good/bad in mode 4 only — see
  `device_modes_architecture.md` § Mode 4 AF level overlay |
| Cycle position | After `COMPOSITE_CAT`: `1→2→3→4→1→…` |
| Suggested PID bit | Distinct from modes 1–3 so hosts do not reuse COM bindings |

### Why no UAC2 in mode 4

In KISS-TNC mode the host does **not** stream audio. The MCU synthesizes
1200/2200 Hz on `RADIO_AUDIO_OUT` and samples RX on `RADIO_AUDIO_IN`. Exposing
UAC2 would invite users to treat it like mode 1 and fight the modem for the
PWM/ADC path. CDC-only matches Pocket Pico / classic KISS TNC USB dongles.

## Signal Stack (normative)

```text
Host APRS app
    │  KISS over USB CDC (FEND 0xC0, FESC 0xDB, …)
    ▼
kiss_cdc  ───────────  deframe / escape / TXDELAY params
    │
    ▼
hdlc_ax25 ───────────  flags 0x7E, bit-stuff, FCS CRC-16/X.25, NRZI
    │
    ▼
afsk_bell202 ────────  mark 1200 Hz / space 2200 Hz @ 1200 baud
    │
    ├─ TX → audio_pwm path → GPIO2 → radio mic/acc
    └─ RX ← ADC GPIO28 ← radio speaker/disc
    │
ptt ← modem TX gate → GPIO14 + GPIO27
```

### Physical / modem constants (v1 locked)

| Parameter | Value |
| --- | --- |
| Baud | **1200** |
| Mark | **1200 Hz** |
| Space | **2200 Hz** |
| Line coding | **NRZI** (0 = tone transition, 1 = hold) — Bell-202 packet convention |
| Framing | HDLC flags `0x7E`, bit-stuff after five `1`s |
| FCS | CRC-16/X.25 (poly 0x1021 reflected as used by AX.25), little-endian on air |
| Sample rate | **9600 Hz** locked (`BOARD_AFSK_SAMPLE_RATE_HZ`) — TX and RX MUST share it |
| Max frame | **512** bytes max AX.25 frame body after KISS unescape (before FCS);
  reject larger with silent drop (no USB crash) |

HF 300 baud / 9600 G3RUH are **non-goals** for v1.

### KISS over CDC (v1)

Normative host link: [KISS](https://www.ax25.net/kiss.aspx) (KA9Q).

| Item | Requirement |
| --- | --- |
| Port | Single USB CDC data interface (same physical role as today’s `…-if03`) |
| Encoding | Binary 8-bit; FEND `0xC0`; escape FEND/FESC per KISS |
| Data frames | Command nibble `0` = data; port nibble `0` only in v1 |
| Host→TNC commands | MUST accept and store at least: **TXDELAY**, **P**, **SlotTime**,
  **TXTail**, **FullDuplex** (ignore FullDuplex effects if always half-duplex) |
| Defaults | TXDELAY ≈ 30 (×10 ms → 300 ms), P=63, SlotTime=10, TXTail=5 — tunable in
  `board_config.h` / soft state |
| Return from KISS | Optional; MAY ignore `0xFF` exit command (device stays KISS forever
  in this mode) |
| RTS/DTR | MUST NOT key PTT; MAY be ignored entirely |

Host apps build AX.25 UI frames (APRS). Firmware MUST treat KISS data payload
as an HDLC I-frame body **including** address/control/PID/info/**FCS** as the
host sent them, **or** document clearly if the TNC appends FCS itself.

**Locked policy (v1):** Host KISS data payload is the AX.25 frame **without**
FCS; firmware **MUST** append CRC-16/X.25 on TX and strip/check FCS on RX
before KISS-encapsulating to the host (common KISS TNC behavior). If a host
sends FCS anyway, that is a host misconfig — do not double-append.

## Hardware Mapping (reuse existing board)

| Signal | GPIO | Mode 4 use |
| --- | --- | --- |
| `RADIO_AUDIO_IN` | **28** ADC | AFSK demod input (radio RX audio) |
| `RADIO_AUDIO_OUT` | **2** PWM | AFSK modulate output (radio TX audio) |
| `RADIO_PTT_OUT` | **14** | Asserted while modem transmitting (incl. TXDELAY/TXTAIL) |
| `TX_STATUS_INDICATOR` | **27** | Follows PTT |
| `MODE_LED` | **0** | 4-pulse pattern; **MUST NOT** be used as AFSK PWM (unlike some
  pico-aprs demos that PWM on GP0) |
| `MODE_SWITCH` | **15** | Boot-time mode cycle only |

No new pins for v1. GPS UART / second radio port = future handoff + pin
escalation to architect.

## USB / Boot Integration

1. Mode resolved at boot like modes 1–3 (`mode_store`, anti-glitch, one flash
   write).
2. Unknown stored mode still heals to `COMPOSITE` (not to `APRS_TNC`).
3. Before `tusb_init`, select **CDC-only** descriptor set for `APRS_TNC`.
4. Do **not** start UAC2 / `audio_task` USB paths; start AFSK sample engine
   instead (may still reuse PWM/ADC drivers behind a modem-facing API).
5. MODE_LED PIO: **4** OFF pulses; extend `mode_led_pio` pattern table.

## Module Boundaries

| Module | Responsibility | MUST NOT |
| --- | --- | --- |
| `kiss_cdc` (new) | Parse/emit KISS on CDC; hold TNC params | Drive PWM/ADC/PTT directly |
| `hdlc_ax25` (new) | Bit-stuff, FCS, NRZI ↔ bytes | Know USB |
| `afsk_bell202` (new) | Modulate/demodulate tones; sample ISR or DMA | Speak KISS |
| `tnc_ptt` or modem hook | TXDELAY/TXTAIL → `ptt_set_active` | Use RTS/CAT/VOX |
| `cdc_app` | In `APRS_TNC`, feed bytes to `kiss_cdc` (not echo, not CAT) | Run CAT parser |
| `cat_kenwood` | Inactive | Run in mode 4 |
| `uac2_app` | Inactive | Own ADC/PWM in mode 4 |
| `mode_store` / LED | Enum value 4; 4-pulse LED | |

`main.c` stays thin: mode resolve → LED → modem init → `tusb_init` → loop
(`tud_task` + modem tick if needed).

## CPU / Timing Constraints

- Soft demod is continuous DSP on RX samples — prefer **ADC IRQ / DMA** feeding
  a tight demod; consider core1 on RP2040/RP2350 if USB+demod contend (open
  implementer choice; document in notes).
- TX path: phase-accumulator sine (or filtered PWM duty) at mark/space; must
  keep continuous phase across bit boundaries (click-free).
- CSMA: with FullDuplex=0, before TX MUST wait for channel clear using a simple
  DCD (energy or correlator metric) + persistence/SlotTime (KISS P / SlotTime).
  v1 MAY use a conservative “energy above threshold” DCD; document limits.

## Explicit Non-Goals (v1)

- Digipeater / WIDEn-N / IGate / APRS-IS.
- GPS / Mic-E tracker / weather station.
- Connected-mode AX.25, PBBS, converse TNC command set.
- 300 baud HF, 9600 G3RUH.
- Dual KISS ports, AGWPE TCP on-device.
- Replacing mode 1 Direwolf-on-host workflow.
- Moving MODE_LED or reclaiming GPIO0 for audio.
- Editing `../hardware/` without a separate human request.

## License / IP Note for Implementer

Reference projects use BSD-3 (pico_tnc), GPL (Direwolf, some ESP projects),
and various MicroModem licenses. Implementer MUST either:

1. Clean-room implement from this architecture + public KISS/AX.25/Bell-202
   specs, or  
2. Vendoring only with compatible license and attribution recorded in
   `implementer/notes.md`.

Do **not** copy GPL sources into this tree without an explicit human license
decision.

## Acceptance Criteria (Tester)

1. Mode 4 selected (4 LED OFF pulses); USB shows **CDC only** (no TinyUSB
   headset / UAC2).
2. RTS toggling MUST NOT key PTT.
3. Host KISS TX of a known UI frame → PTT asserts for TXDELAY+air+TXTAIL;
   GPIO2 shows 1200/2200 Hz AFSK (scope or audio monitor).
4. Inject / air a known APRS packet into GPIO28 → host receives KISS data
   frame with matching payload (after FCS strip).
5. Unknown CDC bytes that are not valid KISS MUST NOT crash; framing recovers
   at next FEND.
6. Mode cycle `…→CAT→APRS_TNC→COMPOSITE…`; persistence across unplug.
7. Builds `CHIP=rp2350` and `CHIP=rp2040`.

## Suggested Implementer Phases

1. **`RGNX_FW_APRS_TNC_MODE_SHELL`:** (**DONE** — tester PASS)
2. **`RGNX_FW_APRS_TNC_TX`:** (**DONE** — tester PASS 2026-11-20)
3. **`RGNX_FW_APRS_TNC_RX` … `_FIELD`:** D24–D30 — **CLOSED FAIL** (abandon);
   do not extend that patch spiral
4. **`RGNX_FW_APRS_TNC_RX_APLUS`:** D31 landed — live LOUD strong / REF marginal /
   **QUIET FAIL** (H1 not CLOSED)
5. **`RGNX_FW_APRS_TNC_RX_WEAK`:** D32 — **CLOSED FAIL / REGRESS** (live
   `dcd=0` at quiet/ref/loud)
6. **`RGNX_FW_APRS_TNC_RX_REVERT_D31`:** (**NEXT** — D33) full revert to D31 A+
7. Future quiet work only after live 73%+loud restored

Phases MUST stay separate handoffs and tester close-outs.

## Implementer rules (binding for TX / RX code)

The implementer already knows **role** rules (`ROLE.md`, `AGENTS.md`: no
build/flash, no redefining docs, thin `main.c`, pins only via
`board_config.h`). Those are **not** enough alone for the modem. The
following are **architecture locks** for `RGNX_FW_APRS_TNC_TX` and
`_RX`. If a detail below is missing, the implementer MUST stop and ask the
architect — **MUST NOT** invent protocol behavior.

### Already decided (do not reopen)

| Item | Lock |
| --- | --- |
| Sample rate | **D31:** RX primary **24000 Hz** (20 samples/bit); TX may stay 9600 until aligned — see § RX rethink A+ |
| Mark / space / baud | 1200 / 2200 / 1200 |
| IP approach | **D31 supersedes D28 as primary RX:** clean-room **A+-class** dual-tone + multi-slicer (ideas from WB2OSZ papers; **no GPL code**). pico_tnc BSD-3 may remain only as optional legacy until A+ lands. TX/KISS shell RIGNEXO-owned. |
| USB product | CDC-only; product string may say “KISS TNC”; distinct PID bit (shell already did) |
| FCS policy | Firmware appends FCS on TX; strips/checks on RX; host KISS payload has **no** FCS |
| PTT | Only modem TX gate in mode 4; never RTS/CAT/VOX |
| GPIO0 | MODE_LED only — never AFSK PWM |

### Module file rules

| Module | Files (expected) | Public surface (minimal) |
| --- | --- | --- |
| `kiss_cdc` | already exists | Feed CDC bytes; on complete data frame → callback/queue to TX path; emit RX frames to CDC; keep params |
| `hdlc_ax25` | `hdlc_ax25.c/.h` (new) | `bytes → NRZI bit stream + stuff + FCS` (TX); `NRZI bits → bytes + unstuff + FCS check` (RX) |
| `afsk_bell202` | `afsk_bell202.c/.h` (new) | Start/stop TX of bit stream at 9600 SPS to PWM; feed ADC samples into demod → bits |
| PTT hook | inside modem TX SM | `ptt_set_active(true/false)` only |

- MUST NOT put AFSK math inside `kiss_cdc` or USB descriptors.
- MUST NOT put KISS escaping inside `afsk_bell202`.
- MUST NOT grow `main.c` beyond init + `tud_task` + one `tnc_task()`/`afsk_task()` call.
- New sources MUST be added to root `CMakeLists.txt` in the same handoff.

### TX state machine (normative order)

When a valid KISS **data** frame is accepted (port 0, cmd 0):

```text
IDLE
  → (optional CSMA: if FullDuplex==0 and DCD busy → persist/SlotTime wait;
     shell/TX-v1 MAY defer full CSMA to RX handoff and TX immediately if DCD
     API not ready — document in notes)
  → PTT on
  → TXDELAY (param × 10 ms) of flags/preamble tones (HDLC flags `0x7E` NRZI)
  → send stuffed frame bits + FCS
  → TXTAIL (param × 10 ms) of flags/idle
  → PTT off
  → IDLE
```

Rules:

1. Only **one** TX at a time. If another KISS data frame arrives during TX,
   MUST queue **at least one** pending frame (depth ≥ 1); if queue full, drop
   newest or oldest (pick one, document in notes) — MUST NOT nest TX or block
   USB forever.
2. During TXDELAY/frame/TXTAIL, PTT MUST stay asserted; PTT LED follows.
3. Continuous-phase AFSK across bit boundaries (no hard tone jumps that click).
4. TX sample path drives **GPIO2 PWM** at 9600 SPS (reuse/adapt `audio_pwm`
   internals behind modem API; UAC2 remains off).
5. Silent on KISS param commands (TXDELAY etc. already stored in shell).

### RX rules (summary — full contract in § RX implementer contract)

1. ADC GPIO28 @ 9600 SPS → demod → NRZI bits → HDLC hunt flags → unstuff →
   FCS check → on OK, KISS-wrap payload **without** FCS to host.
2. Bad FCS → drop, no KISS frame.
3. DCD + CSMA MUST land in this RX handoff (TX deferred them).
4. RX MUST keep working while USB `tud_task` runs.

### Buffer / sizing locks

| Buffer | Minimum |
| --- | --- |
| KISS RX assemble | 512 + escape overhead |
| TX frame after unescape | 512 + 2 FCS |
| Pending TX queue | ≥ 1 frame |
| HDLC bit / sample FIFOs | sized so 9600 SPS does not overrun at USB load; document actual sizes |

### Coding / project rules (modem-specific)

1. All new tunables in `board_config.h` (sample rate, tones, KISS defaults,
   DCD threshold). No magic numbers scattered unmarked.
2. Mark provisional RF thresholds with `TODO(rgnx-fw):` — do not treat as final
   product calibration.
3. No `stdio_init_all` / UART on GPIO0/1 (MODE_LED law still applies).
4. No flash persistence of KISS params in v1 (RAM only; reset on reboot to
   `BOARD_KISS_*` defaults) unless a later handoff says otherwise.
5. Do not edit `../hardware/`.
6. Do not “finish” TX and RX in one un-handoffed mega-PR if the handoff is
   TX-only — stop at TX acceptance and leave RX pending.

### What the implementer MUST escalate

Stop and return to architect if:

- Need a new GPIO / second CDC / UAC2 in mode 4.
- Want to vendor GPL or copy another repo’s sources.
- Want connected-mode AX.25, digi, GPS, or APRS encode on-device.
- Sample rate change away from 9600.
- FCS policy change (host includes FCS).

## Closed questions (2026-11-20)

1. Sample rate → **9600 Hz** (shell + `board_config.h`).
2. License → **clean-room** for v1.
3. Product string / PID → shell already advertises KISS TNC + distinct PID.

## RX baud-clock fix (`RGNX_FW_APRS_TNC_RX_CLOCK` — D24)

Tester return 2026-11-21: host/external Bell-202 inject → **0 KISS** on CDC.
Hardware / FT8 audio path **ruled out**. Offline clone of firmware demod:

- exact **9600 Hz** sample clock → `FRAME_OK`
- firmware timer rate `1e6/104` µs (= **9615.3846 Hz**) → 0 OK (`ABORT` /
  `NONE_ALIGN`)

### Root cause (normative)

```c
delay_us = 1000000 / BOARD_AFSK_SAMPLE_RATE_HZ;  // integer → 104
```

| Quantity | Value |
| --- | --- |
| Intended | 9600 Hz sample → 1200 baud @ SPB=8 |
| Actual | 104 µs → **9615.3846 Hz** → bit clock **~1201.92 Hz** |
| Host / fixture AFSK | **1200.000 Hz** |
| Offset | ~1600 ppm → ~2 bit slips / second |

Soft bit sync is **phase-only** (±1 sample on transitions) — **no frequency
lock**. Same-chip TX+RX share the skewed clock so skew **cancels** on-device;
**cross-source** audio (host `aplay`, external WAV, air) fails. Silent CDC
matches `hdlc_rx_deliver` returning `NONE` when bit alignment is wrong at
flag close (not always `BAD_FCS`).

Same-board GPIO2→GPIO28 loopback is **not** an acceptance path (analog
intermediate; RX muted in `AFSK_TX`).

### Architect decision: **D (hybrid)**

1. **A — true mean 9600 Hz** shared TX/RX sample clock (fix integer truncation).
2. **C-lite — RX frequency assist** so residual host/crystal/tape error still
   locks (~±2000 ppm class).

**Not chosen alone:** B (document 9615 Hz) — leaves TX/RX off exact 1200 vs
the world. **Not chosen alone:** C without A — papers over a known bug.

### Implementer rules (binding — replace prior “9600 timer” assumption)

#### 1) Sample timer MUST average exactly 9600 Hz

- **Forbidden:** `1000000 / 9600` as a single fixed `delay_us` of **104** for
  every tick (current bug).
- **Required:** a period schedule whose **long-term average** is
  `1/9600` s. Preferred clean approach on Pico
  `add_repeating_timer_us` / alarm pool:

  - Target period = `1e6/9600` ≈ **104.1667 µs**.
  - Use an **error accumulator** (Bresenham / fractional): e.g. mostly **104**,
    occasionally **105**, so over 6 ticks: `5×104 + 1×105 = 625` µs →
    `6 / 625e-6 = 9600` Hz exactly.
  - Equivalent absolute-time scheduling from a free-running µs clock is also
    OK if average rate is 9600 ± a few ppm of the MCU crystal.

- TX PWM and RX ADC MUST share this corrected clock (one `afsk_ensure_timer`
  path).
- After the change, TX baud becomes true 1200 (was accidentally ~1201.9).
  Tester MUST re-smoke TX (ear / PTT) — expect still PASS; do not retune mark
  frequencies.

Document the schedule in `notes.md` (e.g. “Bresenham 104/105 → 9600.0 Hz”).

#### 2) RX bit sync MUST track frequency (C-lite)

Phase nudge alone is **insufficient** for host/air inject.

Minimum viable:

- Keep SPB nominal **8**.
- Maintain a **fractional samples-per-bit** (Q-format or float in the RX ISR),
  e.g. init `8.0`, clamp to about **7.7 … 8.3** (~±4% — wider than 1600 ppm
  so host skew and residual timer error are covered).
- On tone/bit **transitions**, adjust:
  - **phase** (as today), and
  - **slowly** `samples_per_bit` (integral term / leaky average of early-late
    error) so steady baud offset is driven toward zero.
- Emit one NRZI bit when the phase accumulator crosses the bit boundary using
  the **fractional** SPB, not a hard `bitcount == 8` only.

Do **not** require a full Gardner/resampler rewrite unless the light PLL fails
bench; start C-lite, escalate to architect only if still 0 frames after (1)+(2).

#### 3) HDLC close / diagnostics (small hardening)

- When a closing flag arrives with `bitcount != 0` (misaligned), treat as
  abort/reset (already ABORT/NONE) — OK to stay silent on CDC.
- Optionally count `rx_align_slip` / `rx_frame_ok` in RAM for notes; **MUST
  NOT** spam CDC with debug text in v1 product mode.
- Do **not** change FCS policy or KISS framing.

#### 4) Non-goals for this fix handoff

- Blaming or “fixing” the FT8 analog interface.
- Same-board digital TX→RX loopback as the pass criterion.
- Vendoring Direwolf/multimon demod.
- Changing mark/space Hz or SPB away from 8 nominal.

#### 5) Acceptance (tester) — closes `RGNX_FW_APRS_TNC_RX_CLOCK`

1. Host or external Bell-202 AFSK @ exact 1200 baud into GPIO28 (fixtures
   such as `fixtures/afsk_rx_bench_2min.wav` or prior `aplay` S16 path) →
   **≥1** matching KISS DATA frame on CDC (payload without FCS).
2. Prefer: multiple frames from the 2‑min fixture (target: most of the
   expected YES packets; document count).
3. TX regression: KISS→AFSK still keys PTT; tones still audible/valid.
4. Own TX still MUST NOT echo as RX KISS (half-duplex mute unchanged).
5. Hardware narrative remains: interface OK; this was firmware baud clock.

Authorized files: `src/afsk_bell202.c/.h` (timer + RX PLL), possibly tiny
`board_config.h` clamps for SPB min/max; `notes.md`. Touch `hdlc_ax25` only
if align/abort hardening needs it.

## RX implementer contract (`RGNX_FW_APRS_TNC_RX`)

Binding for the receive handoff after TX PASS. Extend existing
`afsk_bell202` / `hdlc_ax25` / `kiss_cdc` — do **not** invent a parallel stack.

### Goal

Audio on **GPIO28** → valid AX.25 UI frames → **KISS data** on USB CDC to the
host; enable **DCD** and wire **CSMA** into the existing TX start path.

### Pipeline (normative)

```text
ADC GPIO28 @ 9600 SPS
    → AFSK demod (soft discriminator) → raw tone bit stream
    → NRZI decode (transition=0, hold=1) → HDLC bit stream
    → HDLC RX SM: hunt 0x7E → unstuff → assemble bytes
    → FCS CRC-16/X.25 check (same `hdlc_crc16` as TX)
    → on OK: strip FCS (last 2 bytes) → kiss_cdc_send_frame(payload)
    → on fail: drop silently
```

### Demod algorithm (locked choice)

**Delay discriminator** (clean-room, MicroModem-class idea — do not copy
sources):

1. High-pass / remove DC from ADC samples (simple IIR or DC subtract).
2. Multiply sample by sample delayed ~½ bit (**4 samples** at 9600/1200).
3. Low-pass the product; sign (or threshold) → raw mark/space decision stream
   at sample rate.
4. Soft bit sync: on transitions, nudge **phase** and (per D24) slowly adapt
   **fractional samples-per-bit**; emit **one** NRZI raw bit per baud using
   that fractional SPB (nominal 8 at true 9600 Hz).

Implementer MAY use a compact integer correlator instead **only if** notes
justify it and timing still meets 9600 SPS without starving USB — default
remains delay discriminator. **Timer truncation fix + RX freq assist are
mandatory** after the 2026-11-21 FAIL (see § RX baud-clock fix).

Mark/space polarity MUST match TX NRZI mapping already used
(`0` = tone transition, `1` = hold). If loopback TX→RX fails polarity, fix RX
polarity in one place and document — do not break TX that already PASSed.

### HDLC RX API (add to `hdlc_ax25`)

Add a small streaming decoder, e.g.:

| API | Behavior |
| --- | --- |
| `hdlc_rx_reset()` | Clear hunt/unstuff/byte assembly |
| `hdlc_rx_bit(bit)` | Feed one HDLC bit (post-NRZI); return event |
| Events | `NONE` / `ABORT` / `FRAME_OK` (payload ptr+len **without** FCS) / `FRAME_BAD_FCS` |

Rules:

- Hunt consecutive flag `0x7E` (LSB-first bit order **identical** to TX builder).
- After flag, unstuff: after five `1`s, delete the next `0`; if next is `1`,
  treat as abort/flag per HDLC.
- Accumulate bytes LSB-first; when flag closes a frame with ≥ 2 bytes, verify
  FCS over all bytes except the last two; if OK deliver body without FCS.
- Max body **512**; overrun → abort/reset, no deliver.
- MUST reuse `hdlc_crc16()` — do not implement a second CRC.

### AFSK RX / ADC ownership

| Rule | Requirement |
| --- | --- |
| Pin | GPIO **28** / ADC2 only |
| Rate | **True mean 9600 Hz** sample clock (see § RX baud-clock fix — not
  `1000000/9600` truncated to 104 µs) |
| UAC2 / `audio_adc` | MUST NOT start UAC2 path in mode 4. MAY reuse init helpers from
  `audio_adc` **or** configure ADC inside `afsk_*` — pick one owner; do not run
  two 9600/48k timers on the same pin |
| Half-duplex | While `afsk_tx_busy()`, RX demod MUST **ignore** samples (or hold HDLC
  in reset) so the unit does not “hear” its own TX as a host frame |
| `afsk_task` | MAY drain a short sample FIFO / push completed RX frames to KISS;
  heavy DSP SHOULD stay in the sample callback |

Public additions (names MAY vary slightly; behavior MUST match):

- `bool afsk_dcd(void);` — true when channel looks busy
- RX path runs after `afsk_init()` in `APRS_TNC` (already called)

### DCD (required in this handoff)

- Metric: smoothed energy **or** correlator magnitude / discriminator
  confidence over a few bits — implementer chooses; document in `notes.md`.
- Threshold + hang time in `board_config.h`, e.g.
  `BOARD_AFSK_DCD_THRESHOLD`, `BOARD_AFSK_DCD_HANG_MS`, marked
  `TODO(rgnx-fw):` for bench tune.
- `afsk_dcd()` used by CSMA; MAY also gate spurious HDLC if useful (optional).

### CSMA (wire into existing TX)

TX currently starts immediately (CSMA deferred). In this handoff:

1. Before PTT on / starting TXDELAY, if `kiss_cdc` FullDuplex == 0 (default):
   - While `afsk_dcd()` busy → wait
   - Then persistence: random 0..255 ≤ P (KISS persist) else wait SlotTime
     (×10 ms) and retry
2. If FullDuplex ≠ 0: MAY TX without waiting (still half-duplex radio
   physically — document).
3. MUST NOT block `tud_task` for long; implement waits in TX state machine /
   `afsk_task` (state `WAIT_CSMA`), not a tight spin in CDC callback.
4. Add `kiss_cdc_fullduplex()` getter if missing.

### KISS to host (RX → CDC)

Add `kiss_cdc_send_frame(const uint8_t *payload, size_t len)`:

- Emit `FEND`, `0x00` (port0/data), escaped payload, `FEND`
- Escape `0xC0` / `0xDB` per KISS
- If CDC not connected or FIFO full: drop frame (MUST NOT stall demod forever);
  MAY count drops in notes only
- Payload is AX.25 **without** FCS (matches TX host convention)

### Half-duplex / concurrency

| Situation | Behavior |
| --- | --- |
| RX frame completes during TX | Impossible if RX muted while TX busy — required |
| KISS TX queued while RX active | Allowed; CSMA waits for clear |
| USB storm | Demod continues; KISS TX to host may drop |

### Files authorized (`RGNX_FW_APRS_TNC_RX`)

- `src/afsk_bell202.c/.h` — ADC RX, demod, DCD, CSMA wait state
- `src/hdlc_ax25.c/.h` — RX bit streaming API
- `src/kiss_cdc.c/.h` — `kiss_cdc_send_frame`, fullduplex getter
- `src/board_config.h` — DCD / RX tunables only
- `src/main.c` — only if needed to keep `afsk_task` / init order (stay thin)
- `CMakeLists.txt` — only if new files (prefer extending existing)
- `agent-workspaces/implementer/notes.md` — algorithm, buffer sizes, DCD method

MUST NOT change MODE_LED, USB descriptors, pin map, or TX tone constants
unless a bug blocks RX loopback — then note and keep TX PASS behavior.

### Acceptance (tester) for RX handoff

1. Build rp2350 (and rp2040 if exercised).
2. Mode 4: feed AFSK into GPIO28 (audio from another TNC, recorded WAV via
   coupler, or TX→RX cable loopback with attenuation) containing a known UI
   frame → host CDC receives KISS data with matching payload (no FCS).
3. Bad CRC / noise → no bogus KISS frame (or rare false accept documented).
4. Own TX does **not** echo a KISS RX frame to the host when transmitting.
5. With FullDuplex=0 and a busy tone / DCD forced, TX waits (CSMA); with clear
   channel, TX still works as before.
6. RTS still does not key PTT.

### Non-goals (still)

- Digipeater, APRS parse on-device, dual port KISS, 9600 baud G3RUH, AGC UI.

## RX debug observability (`RGNX_FW_APRS_TNC_RX_DEBUG` — D25)

Tester return after `RGNX_FW_APRS_TNC_RX_CLOCK` still **0 CDC** on external
WAV: “0 KISS” is too coarse. Need **bottom-up** bench visibility — **tone
detection first** — without SWD. **Instrumentation only** in this handoff;
no demod redesign until Layer 0/1 localize the fault.

### Channel (locked)

| Item | Rule |
| --- | --- |
| Transport | Same mode-4 USB CDC port |
| Trigger A (ASCII) | Host sends exactly `RGNX STAT\n` or `RGNX STAT\r\n` (ASCII, **not** inside a KISS FEND frame). Case-sensitive. |
| Trigger B (KISS) | Host sends `C0 0C C0` (KISS cmd nibble **0x0C**, empty body) — proprietary debug dump |
| Clear | `RGNX CLR\n` / `RGNX CLR\r\n` **or** `C0 0D C0` — zeroes **since-clear** counters |
| Reply | **ASCII lines only** (not KISS-wrapped), so tester can `cat` / log the port |
| After reply | Resume normal KISS RX/TX behavior |
| Product default | Debug always compiled in mode 4 for now (no `#ifdef` maze). If size hurts later, architect may gate it |

ASCII trigger: implement a tiny line buffer in `kiss_cdc` / `cdc_app` that
detects `RGNX …` when **not** mid-KISS-frame (if `0xC0` seen, use KISS path).
Do not treat `RGNX` as AX.25.

### Reply format (normative)

One dump = several lines ending with `RGNXDBG END\n`. Field names fixed so
scripts can parse `key=value`.

```text
RGNXDBG L0 ene=<u> dcd=<0|1> dcd_ms=<u> adc_min=<i> adc_max=<i> adc_mid=<i>
RGNXDBG L1 disc_tr=<u> mark_n=<u> space_n=<u> both=<0|1>
RGNXDBG L2 bits=<u> abort=<u> bad_fcs=<u> ok=<u> align_slip=<u>
RGNXDBG L3 kiss_ok=<u> kiss_drop=<u>
RGNXDBG Lclk spb_x100=<u> t104=<u> t105=<u> tx_busy=<0|1>
RGNXDBG END
```

| Field | Layer | Meaning |
| --- | --- | --- |
| `ene` | L0 | Smoothed post-DC energy (same family as DCD metric), last sample or short EMA |
| `dcd` | L0 | Current `afsk_dcd()` |
| `dcd_ms` | L0 | ms DCD was true **since clear** (or since last STAT if easier — document) |
| `adc_min` / `adc_max` / `adc_mid` | L0 | Raw or DC-removed ADC extrema / mid over a recent window (~50–200 ms latch) |
| `disc_tr` | L1 | Discriminator **transitions** (mark↔space decisions) **since clear** — **priority** |
| `mark_n` / `space_n` | L1 | Samples (or decisions) classified mark vs space since clear |
| `both` | L1 | Latched: saw mark **and** space in the window / since clear |
| `bits` | L2 | HDLC bits fed since clear |
| `abort` / `bad_fcs` / `ok` | L2 | HDLC event counts |
| `align_slip` | L2 | Misaligned flag closes (if counted) |
| `kiss_ok` / `kiss_drop` | L3 | Frames sent to CDC vs dropped (FIFO/not connected) |
| `spb_x100` | Lclk | Current fractional SPB ×100 (e.g. 800 = 8.00) |
| `t104` / `t105` | Lclk | Bresenham tick counts (proves timer schedule alive) |
| `tx_busy` | Lclk | RX muted? |

**Clear-on-read:** `STAT` dump does **not** auto-clear (so two STATs can
compare). `CLR` zeroes since-clear counters and latches; extrema window MAY
reset.

Counters are `uint32_t` saturating or free-run wrap — document in notes.

### Layer priority for tester

1. **Silence:** low `ene`, `dcd=0`, `disc_tr≈0`.
2. **Play WAV / AFSK:** `ene` up, `dcd` often 1, **`disc_tr` clearly rising**
   and `both=1` → Layer 1 PASS (tones seen).
3. Only then interpret L2/L3 (`ok` vs `bad_fcs` vs `kiss_drop`).

If L0 dead with loud inject → ADC/path/timer, not HDLC.  
If L1 dead but L0 alive → discriminator/gain/polarity.  
If L1 alive, L2 `ok=0` → bit sync/HDLC/FCS.  
If `ok>0` but `kiss_ok=0` → CDC send path.

### Implementer rules

1. Update counters in the **RX sample path** (ISR/alarm) with minimal work;
   format ASCII in `afsk_task` / on-demand STAT handler (not in ISR).
2. MUST NOT spam STAT unsolicited.
3. MUST NOT redesign demod in this handoff.
4. Authorized: `afsk_bell202.*`, `hdlc_ax25.*` (event tallies), `kiss_cdc.*`
   (triggers + `kiss_ok`/`drop`), thin `cdc_app` if needed, `notes.md`.
5. Document exact STAT/CLR examples for tester in `notes.md`.

### Acceptance (tester) for debug handoff

1. Idle → STAT shows quiet L0/L1.
2. During `fixtures/afsk_rx_bench_2min.wav` (or steady AFSK) → STAT shows
   **non-zero `disc_tr`** and elevated `ene` (tone detection visible).
3. `CLR` then STAT → counters near zero again.
4. Normal KISS TX smoke still works; debug ASCII does not permanently break
   KISS framing.

### Bench result after D25 landed (2026-11-21) — L1 PASS, STAT broken

Tester findings (`agent-workspaces/tester/findings_rx_debug_D25_2026-11-21.md`):

- **L1 PASS:** `disc_tr` rising, `both=1`, ADC swing ~±1k — tones **are** seen.
- **`dcd=0`** with `ene` ~18–64 vs threshold 400 — not “no signal”; do not
  chase DCD as the KISS-silence root.
- **STAT truncated ~128 B** — L2/L3/Lclk/END never reach host → cannot branch
  demod vs KISS.
- **0 KISS DATA** remains; next work is **not** more tone detection.

## RX debug STAT transport fix (`RGNX_FW_APRS_TNC_RX_DEBUG_TX` — D26)

### Goal

Make one `RGNX STAT` (or KISS `0x0C`) deliver the **entire** normative dump
through `RGNXDBG END` every time, including while WAV inject is running.

### Root cause (normative)

`afsk_cdc_write_str()` calls `tud_cdc_write()` for each line **without**
waiting for FIFO space; only one `tud_cdc_write_flush()` at the end. TinyUSB
CDC TX FIFO (~64–128 B) fills on L0+L1; later writes are **dropped** → host
never sees L2+.

### Implementer rules (binding)

1. **Replace fire-and-forget CDC writes** for debug dumps with a reliable
   writer, e.g.:

   ```text
   while bytes remain:
     avail = tud_cdc_write_available()
     if avail == 0:
       tud_cdc_write_flush()
       tud_task()          // or yield; MUST run USB
       brief wait / retry
     else:
       write min(avail, remaining)
   flush at end of each line OR end of dump
   ```

2. Dump MUST run from **task context** (`afsk_task` / main loop) — already
   required; MUST NOT call the blocking wait loop from the AFSK alarm ISR.
3. Cap wait (e.g. a few ms total or N retries) so a stuck host cannot freeze
   the modem forever; if still incomplete, set a sticky `dump_fail` flag
   visible on the *next* successful STAT as `dump_fail=1` (optional field on
   Lclk) — prefer completing the dump under normal USB load.
4. **Preferred:** flush after **each** `RGNXDBG` line + `tud_task` between
   lines. Acceptable: single buffer (~400 B) built then chunked with
   available-wait.
5. **MUST NOT** redesign demod, change DCD threshold, or “fix tones” in this
   handoff.
6. **MUST NOT** shorten the normative field set to dodge the FIFO — fix the
   writer instead (optional: slightly shorter lines OK if all keys remain).

### Authorized files

- `src/afsk_bell202.c` (`afsk_cdc_write_str` / `afsk_rx_debug_dump_now`) —
  primary
- `notes.md` — note reliable TX; tester re-STAT procedure
- Touch `kiss_cdc` / TinyUSB config **only** if CDC TX buf size must grow;
  prefer wait/flush first (do not raise FIFO as the sole “fix”)

### Acceptance (tester)

1. With WAV playing: `RGNX CLR` then `RGNX STAT` ≥ **3** times → each reply
   contains **L0, L1, L2, L3, Lclk, END** with no truncation.
2. L2 fields (`bits`, `abort`, `bad_fcs`, `ok`, `align_slip`) are numeric and
   readable for architect branch (Order 2 in findings).
3. KISS TX smoke still PASS; L1 still shows activity under inject.

### Explicit non-orders (until L2 captured)

- No discriminator rewrite.
- No FT8/UAC2 “fix”.
- No same-board TX→RX loopback requirement.
- No DCD threshold retune as the primary fix (optional later).

## RX bit-sync robustness (`RGNX_FW_APRS_TNC_RX_SYNC` — D27)

### Evidence lock (tester 2026-11-21 — read carefully)

Sources: `tester/handoff.md`, `findings_rx_debug_D25_2026-11-21.md` §10–§11,
`test_runs.md` `RGNX_FW_APRS_TNC_RX_DEBUG_TX`.

| Fact | Implication |
| --- | --- |
| D26 STAT full under WAV | L2 numbers are trustworthy |
| L1 `both=1`, `disc_tr`↑, ADC swing | **Do not redesign delay discriminator first** |
| `bits`↑ into HDLC | Path disc→NRZI→`hdlc_rx_bit` is alive |
| **`ok=0`**, `kiss_ok=kiss_drop=0` | No good frames; **not** KISS FIFO |
| **`abort` ≫ `bad_fcs`** | Hunt / seven-`1`s / format — not “almost CRC” |
| **`align_slip`↑** | Flag seen but **not byte-aligned** → bit insertions/deletions |
| `spb_x100≈820` (nom 800) | C-lite is shoving SPB off nominal **without** achieving `ok` |
| L1 tallies are **per sample** | Tone activity ≠ correct 1200-baud grid |

**Causal model (normative for this handoff):** emit rate / sample phase of
NRZI bits does not match the inject’s 1200 baud grid → extra HDLC `1`s
(hold runs → `abort`) and wrong bit counts between flags (`align_slip`) →
never `FRAME_OK`.

Baseline to beat (CLR then STAT while WAV):

| bits | abort | bad_fcs | ok | align_slip |
| ---: | ---: | ---: | ---: | ---: |
| ~11k | ~428 | ~2 | **0** | ~70 |

### Goal

Same external fixture (`fixtures/afsk_rx_bench_2min.wav` or equivalent exact
1200 Bell-202): produce **`ok ≥ 1`** (prefer many section-A payloads) and
materially **lower** `abort` + `align_slip` vs baseline; host then sees KISS
DATA (`kiss_drop` was already 0).

### Scope (what to change)

**In scope:** bit timing / sample phase / NRZI emit path in `afsk_bell202.c`
(and only HDLC if a clear bug in flag/unstuff is found — unlikely primary).

**Out of scope:** tone discriminator topology rewrite; DCD retune as primary;
STAT transport; UAC2/FT8; digipeater; TX tone constants (TX smoke must remain).

### Normative algorithm (replace current C-lite emit)

Current code (approx.): per sample, on disc edge adjust `s_spb` aggressively
(`err >> 5`), then `pll_acc += 1.0` and **`while (acc >= spb) emit(disc)`**
using the **edge-time** disc value. That can emit off mid-bit and walk SPB to
~8.20 without locking → matches live L2.

Implementer MUST move to a **mid-bit sample + slow baud track** scheme:

1. **Discriminator output** may keep delay-disc + LPF, but add **hysteresis**
   before calling a level a “transition” (e.g. require LPF cross 0 and stay, or
   ±deadband) so sample-rate chatter does not spam PLL.

2. **One baud decision per bit period**, not “emit whenever acc wraps with
   whatever disc is now”:
   - Maintain phase 0 … `spb` (fractional OK).
   - Advance phase by 1 sample (Q-format) each ADC tick.
   - When phase crosses **mid-bit** (`spb/2`), **sample** disc (or majority of
     last 3 raw disc samples) → that is the tone bit for NRZI →
     `afsk_rx_emit_bit` **once**.
   - When phase wraps past `spb`, wrap phase; do **not** emit a second time in
     the same sample unless using a proven dual-edge design (default: **max
     one emit per sample**).

3. **PLL / SPB adaptation — slow and bounded:**
   - On **validated** transitions only, measure early/late vs mid-bit; nudge
     **phase** strongly, **`spb` weakly** (much slower than today’s `>> 5`).
   - Prefer pull toward nominal **8.00** (`AFSK_SPB_NOM`) with a weak leak
     (e.g. each bit: `spb += (NOM - spb) >> k`) so live `spb_x100` stays near
     **800 ± ~20**, not wandering to 820+ while still failing.
   - Keep clamp ~7.7–8.3 unless notes justify a tighter band (e.g. 7.85–8.15).

4. **NRZI** unchanged in meaning: HDLC `1` = same tone as previous **emitted**
   bit; `0` = change. First emit after reset still primes without HDLC output.

5. **Half-duplex:** still mute/reset RX while `AFSK_TX`.

6. Optional (if mid-bit alone insufficient after first try): 3-sample majority
   inside the bit (MicroModem-class) — document in `notes.md`. Do **not** copy
   GPL sources.

### Debug / acceptance instrumentation

Keep D25/D26 STAT. After the sync change, under same CLR+WAV procedure,
tester expects:

1. `ok ≥ 1` (target: many YES packets from fixture section A).
2. `abort` and `align_slip` **clearly down** vs § baseline (~428 / ~70 at
   similar `bits` scale — compare rates `abort/bits`, `align_slip/bits`).
3. `spb_x100` nearer **800** when locked (informative, not sole pass).
4. KISS DATA on CDC for at least one nominal payload; `kiss_drop` stays ~0.
5. TX smoke: PTT + AFSK still OK.

### Authorized files

- `src/afsk_bell202.c/.h` — primary (RX sync / emit)
- `src/board_config.h` — PLL gains, hysteresis, SPB clamp if needed
- `src/hdlc_ax25.*` — only if a real flag/unstuff bug is proven
- `notes.md` — describe mid-bit sync + gains; baseline vs new STAT example

### Non-orders

- Do not “fix” FT8 / UAC2.
- Do not treat DCD=0 as blocker (`ene` low vs 400; L1 already proves activity).
- Do not require same-board TX→RX loopback.
- Do not remove STAT fields.

## Proven Pico RX port (`RGNX_FW_APRS_TNC_RX_PORT` — D28 / Option R1)

### Strategic verdict (tester 2026-11-21)

After D24–D27, live fixture still **`ok=0`**. D27 cleared `align_slip` but
**regressed** `abort` (~3.7×) and nearly killed `disc_tr`. L1 tones and D26
STAT are fine; clean-room bit-sync thrash is not.

Tester return: `return_architect_rx_strategy_2026-11-21.md` — **Option R1**.

**Architect decision (D28):** Stop inventing RX PLL/demod. **Port a known-good
libre Pico AFSK RX core** into RIGNEXO, keep our USB/KISS/PTT/modes shell.

### Selected reference (locked)

| Field | Value |
| --- | --- |
| Project | [amedes/pico_tnc](https://github.com/amedes/pico_tnc) |
| Platform proof | Raspberry Pi Pico / **RP2040**, Pico SDK, soft Bell-202 + KISS |
| License | **BSD-3-Clause** (compatible; retain copyright + LICENSE text) |
| Why not Direwolf / multimon-ng | **GPL** — forbidden without explicit human license decision |
| Why not MicroAPRS-only | AVR-oriented; pico_tnc is the Pico-native proven stack |

Public description of that demod (must be followed, not re-derived):

| Stage | pico_tnc behavior |
| --- | --- |
| Sample rate | **13200 Hz** = 1200 baud × **11** samples/bit |
| Band-pass | ~900–2500 Hz on ADC samples |
| Delay correlator | `m = val[t] * val[t − 6 samples]` (~446 µs @ 13200) |
| Low-pass | ~1200 Hz on `m` → `sum` |
| Slice | `sum < −4096` → mark/bit1; `sum ≥ +4096` → space/bit0 (use **their** polarity/thresholds verbatim when porting) |
| Digital PLL | **11** steps per bit; on bit transition adjust PLL counter **±25%**; decode one bit per domain |

Do **not** “approximate” this with our old 9600/SPB=8 C-lite path.

### Architecture after port

```text
ADC GPIO28 @ 13200 Hz (Bresenham / exact mean — same discipline as D24)
    → [ported] BPF → delay correlator → LPF → slicer
    → [ported] digital PLL → NRZI / bit stream
    → HDLC RX (ported pico_tnc HDLC **or** keep hdlc_ax25 if bit-compatible)
    → kiss_cdc_send_frame (RIGNEXO) → CDC
```

| Keep (RIGNEXO) | Replace / port |
| --- | --- |
| Mode 4 USB CDC-only, KISS framing, STAT/CLR | Soft RX demod + bit PLL |
| PTT TXDELAY/TXTAIL SM, half-duplex mute | Sample rate for **RX** → **13200** |
| `board_config` pins 28/2/14/27/0 | Delay-disc @ 9600 + D27 mid-bit sync |
| Product strings / PID / modes | — |
| TX AFSK path (for this handoff) | MAY stay; see clock note below |

### Sample clock (normative)

1. **RX path MUST run at mean 13200 Hz** (not 9600). Use fractional/Bresenham
   period (`1e6/13200 ≈ 75.757 µs`) — **forbid** truncating to a single integer
   µs that skews baud the way `1000000/9600→104` did.
2. **TX** (this handoff): MAY keep the existing 9600 TX PWM timer **if** TX and
   RX use **separate** timers/alarms. Prefer documenting dual clocks in notes.
   A follow-up MAY unify TX to 11 samples/bit @ 13200; **not required** to close
   RX decode.
3. Do **not** drive RX from the old `BOARD_AFSK_SAMPLE_RATE_HZ=9600` define
   without updating it: introduce e.g. `BOARD_AFSK_RX_SAMPLE_RATE_HZ 13200` and
   keep TX define if dual-rate.

### Module layout (implementer)

Suggested (names MAY vary):

| Module | Role |
| --- | --- |
| `pico_tnc_rx/` or `vendor/pico_tnc_demod/` | Ported BPF/correlator/LPF/PLL (+ HDLC if taken) with upstream copyright |
| `afsk_bell202.c` | Owns ADC timer(s), feeds samples into ported RX; TX SM unchanged; mute RX in TX |
| `kiss_cdc` / STAT | Unchanged contracts; map new internal metrics into L0–L2 if possible |
| `LICENSE.pico_tnc` / `notes.md` | BSD-3 notice + git tag/commit of upstream |

**Import only** demod/PLL/(optional) HDLC RX sources. **MUST NOT** import
digipeater, beacon, converse, GPS, multi-port TNC UI, or their USB stack.

### License / attribution (overrides D21 clean-room **for RX only**)

- D21 clean-room remains for code we still own; **D28 explicitly allows**
  BSD-3 pico_tnc **RX modem** files with attribution.
- Still **MUST NOT** vendor GPL (Direwolf, multimon-ng, etc.).
- Record upstream URL + commit/tag in `implementer/notes.md`.

### Integration rules

1. Ported RX outputs bits or completed AX.25 payloads **without FCS** into
   existing `kiss_cdc_send_frame` (same host contract).
2. If using pico_tnc HDLC, ensure FCS policy matches (strip before KISS).
3. Half-duplex: while `afsk_tx_busy()`, do not feed ADC into RX (or reset RX
   state) — no own-TX echo.
4. Keep `RGNX STAT` working; L1/L2 fields MAY be remapped to ported internals
   (document mapping). Goal of STAT remains localization, not blocking decode.
5. No change to MODE_LED / USB descriptors / pin map.

### Acceptance (tester)

Same external `fixtures/afsk_rx_bench_2min.wav` (or host Bell-202 @ 1200):

1. After CLR + play: **`ok ≥ 1`** (prefer many section-A YES packets).
2. Host CDC receives KISS DATA matching manifest payloads (no FCS).
3. `abort`/`align_slip` no longer dominate with `ok=0` (material improvement).
4. TX smoke still PASS; RTS still does not PTT; no own-TX KISS echo.
5. License files present; notes cite pico_tnc commit.

### Explicit non-goals

- Another clean-room PLL experiment “inspired by” pico_tnc without porting.
- Porting Pocket Pico Z80/HK-21 stack.
- Demoting mode 4 to host-only softmodem (Option R3 rejected).
- Same-board TX→RX loopback as acceptance.

## RX level / scale fix (`RGNX_FW_APRS_TNC_RX_LEVEL` — D29)

### Tester verdict after D28 port (2026-11-21)

Sources: `return_architect_rx_port_D28_2026-11-21.md`,
`analysis_D28_stat_vs_code_2026-11-21.md`.

| Observation | Meaning |
| --- | --- |
| `bits`↑, timers OK, `ene`/`dcd` move | RX path alive |
| **`disc_tr=0`**, `both=0` | Soft bit **never flips** |
| `ok=bad_fcs=abort=0`, 0 KISS | Never leaves HDLC FLAG (no `0x7E`) |
| ADC sticky pp ≈ **133** (1971…2104) | Very small AC into 12-bit ADC |
| Port `ADC_BIT 12` + raw `adc_read()` | Diverges from pico_tnc **default `ADC_BIT 8`** |

Root claim: `dem` never crosses **±4096** → frozen mark/space → PLL counts
`bits` without edges → no frames. Audible AFSK ≠ scaled soft-decision.

### Architect locks

#### A1 (required) — match upstream sample width

| Item | Requirement |
| --- | --- |
| `ADC_BIT` | **8** (pico_tnc default; not 12) |
| Feed | From 12-bit `adc_read()`, pass **`(adc >> 4)`** as 0…255 into the
  ported demod (or equivalent FIFO shift-to-8). **MUST NOT** feed raw 0…4095
  into an `ADC_BIT 8` path |
| Thresholds | Keep pico_tnc `±4096` / CDT constants **verbatim** after A1 |

#### AGC / headroom (required — radio volume varies)

Fixed thresholds assume a usable amplitude after A1. Bench pp≈133 on 12-bit ≈
**~8 counts pp on 8-bit** — still too small for `dem` to reach ±4096 even
with correct `ADC_BIT`.

Implementer MUST add a **digital front-end** before BPF (or immediately after
8-bit conversion), clean-room OK:

1. Track recent peak (or RMS) of AC-coupled 8-bit samples.
2. Apply a gain so that, under normal inject, peak AC sits in a target band
   (guidance: roughly **40–100** counts pp on the 8-bit scale — tune and
   document; clamp gain min/max to avoid noise blow-up / clipping).
3. Slow AGC attack/decay (not instant per sample) so mark/space balance stays
   stable within a packet.
4. Do **not** change LPF_THRESHOLD as the primary “fix”; scale the **input**.

Optional later: automatic threshold as fraction of `dem` EMA — only if A1+AGC
still fails with `dem_max` clearly &lt; 4096 at max safe gain.

#### STAT (required)

Add sticky **`dem_min` / `dem_max`** (since CLR) on an existing L-line or
`Lclk` so tester can see threshold gap without SWD. Keep `disc_tr` as edge
counter.

### Scope / non-scope

| Do | Do not |
| --- | --- |
| A1 wiring + AGC + `dem_min`/`dem_max` | New clean-room PLL / revive D27 |
| Keep 13200 Hz pico_tnc demod/PLL | Blame KISS/CDC (`kiss_drop` already 0) |
| Keep half-duplex mute / TX smoke | Treat `bits↑` alone as health |
| Document gain clamps in `notes.md` | Require same-board TX→RX loopback |

### Acceptance (tester gates)

Same external `afsk_rx_bench_2min.wav`:

| Gate | Pass if |
| --- | --- |
| G1 | While WAV plays: **`disc_tr` rises**; `dem_min`/`dem_max` show `|dem|`
  reaching near/over 4096 at times |
| G2 | `ok ≥ 1` and/or `bad_fcs ≥ 1` (left FLAG) |
| G3 | ≥1 KISS DATA matching section A |
| G4 | *(superseded by D30)* — do **not** CLOSE by “raise player”; see FIELD |
| G5 | TX smoke still PASS; no own-TX KISS echo |

**Status:** D29 **FAIL** live (`agc_q8` stuck, CDT pre-AGC). Superseded by
**D30** (`RGNX_FW_APRS_TNC_RX_FIELD`). Keep A1 (`ADC_BIT 8` + `>>4`).

### Authorized files

- `src/pico_tnc_rx/**` (`ADC_BIT`, feed types)
- `src/afsk_bell202.c` (8-bit feed + AGC)
- `board_config.h` (AGC target/min/max gain)
- STAT dump formatting; `notes.md`

## Field-robust RX front-end (`RGNX_FW_APRS_TNC_RX_FIELD` — D30)

### Product constraint (immutable)

Mode 4 is a **field product**. Audio level into GPIO28 is **never** constant:

- Different radios, AF gain, squelch, cables, bias  
- Different stations (pre-/de-emphasis imbalance mark vs space)  
- Host/player/OS mixer changes on the bench  

**Forbidden product answers:** “raise the volume”, “set a sweet-spot gain”,
or any acceptance that CLOSES on **one** inject level only.

Sources: `note_architect_rx_level_ops_2026-11-21.md`,
`analysis_D29_agc_cdt_2026-11-21.md` (D29 FAIL: `agc_q8` stuck, CDT pre-AGC,
`dem=0`).

### Research → normative intent (clean-room; no GPL)

Do **not** vendor GPL Direwolf / multimon. Steal **ideas**, implement
clean-room. Cite in `notes.md`.

| Practice | Source (idea only) | RIGNEXO rule |
| --- | --- | --- |
| Fast-attack / slow-decay **peak+valley envelope AGC**; normalize each tone to ~±0.5 before compare | Direwolf `demod_afsk` + WB2OSZ *Better APRS Packet Demodulator* Part 1 | Per-sample / per-tone normalize; never absolute volts |
| Relative mark vs space (voice radios + pre/de-emphasis make 2200 Hz often weaker; WA8LMF TNC Test CD Track 2 median M/S ≈ 2.5) | Same paper | Prefer relative / dual AGC over fixed slice alone |
| AGC on **decimated / in-band** block; gate on tone-band RMS with hysteresis; **hold gain only on converter noise**, still raise real weak AF | ESP32-IDF APRS DSP chain (`rx_agc`, receive gate) | CDT must not permanently block weak-but-real AF |
| Soft confidence ≈ `\|soft\| / running_avg(\|soft\|)` | esp32_libmodem / similar | Adaptive slice / quality metric |
| pico_tnc fixed `±4096` | amedes/pico_tnc | Valid **only** after working AGC drives `|dem|` into band; **B1 adaptive slice is primary** |

Guidance numbers (starting points — tune + document; not sacred):

| Param | Starting guidance |
| --- | --- |
| Input AGC target peak `|ac|` on 8-bit | ~40–80 counts (mid headroom) |
| Input gain clamp | ~0.25× … 32× (Q8: 64…8192 if 256=1.0) |
| Attack vs decay | Attack ≫ decay (Direwolf-class asymmetry; e.g. attack ~0.25–0.7 of error toward peak, decay ~0.001–0.01) |
| ESP32-like hold | Freeze gain only if **pre-gain** `|ac|` below converter noise (~few LSB); never freeze when quiet-but-real AF is present |
| Gate hysteresis | Open above floor for N samples; hold until below floor/2 |
| B1 `k` | ~0.25–0.40 of `dem_env` |

### D29 defects that MUST be fixed (code-level)

1. **AGC dead-zone:** `gain += (target - peak) >> 7` with `peak≈4` → add **0**
   forever (`agc_q8=256`). Illegal.  
2. **CDT on pre-AGC** 8-bit AC → energy ÷256 vs 12-bit era → never enters
   BPF → `dem_min=dem_max=0`. Illegal.

### Normative RX chain (field)

```text
adc_read() 12-bit
  → u8 = adc >> 4                    (A1 keep)
  → DC remove → ac
  → peak/valley envelope of |ac| (fast attack / slow decay)
  → multiplicative AGC (no dead-zone) → x_g = sat_feed(ac * gain)
  → CDT / energy on x_g (post-AGC) with hysteresis  [or soft enable]
  → pico_tnc BPF → delay correlator → LPF → dem
  → B1 adaptive slice on |dem| envelope → bit
  → pico_tnc digital PLL → HDLC → KISS
  [optional B2 dual-tone normalize later]
```

### Locked algorithms (implementation-ready)

#### A) Input AGC — mandatory, must move

Integer-safe intent (Q8 gain where `256 = 1.0`; Q16 OK if clearer):

```text
# Envelope of |ac| (fast attack / slow decay)
if |ac| > peak:  peak = mix(peak, |ac|, ATTACK)
else:            peak = mix(peak, |ac|, DECAY)

# Optional valley for DC-ish floor (Direwolf peak/valley idea):
if |ac| < valley: valley = mix(valley, |ac|, ATTACK)
else:             valley = mix(valley, |ac|, DECAY)
span = max(peak - valley, 1)

# Gain law — NEVER rely on (target-peak)>>N that can be 0
# Prefer multiplicative correction with minimum step:
err_q = TARGET_SPAN - span          # or TARGET_PEAK - peak
if pre_gain_|ac| < NOISE_FLOOR_LSB for HOLD_N samples:
    hold gain                                    # converter noise only
else:
    if err_q > 0:
        # climb: at least +1 Q8 when below half target
        step = max(1, (gain * err_q) / (TARGET_SPAN * GAIN_DIV))
        gain = min(GAIN_MAX, gain + step)
    else:
        step = max(1, (gain * (-err_q)) / (TARGET_SPAN * GAIN_DIV_SLOW))
        gain = max(GAIN_MIN, gain - step)

x_g = clamp(ac * gain / 256, feed_min, feed_max)   # feed domain for BPF
```

**Hard rules:**

- After quiet inject with measurable `adc` pp (D29: ~8 counts), `agc_gain`
  MUST leave unity within **&lt;1 s** of AF.  
- Loud inject MUST reduce gain from a high start; no permanent hard clip.  
- Document every clamp/shift in `notes.md`. Ban any update that rounds small
  positive error to zero.

#### B) CDT / enable — mandatory post-gain

```text
e = energy_iir(x_g)     # same style as pico_tnc CDT but on post-AGC samples
if e > THR_OPEN:  cdt = 1
if e < THR_CLOSE: cdt = 0   # THR_CLOSE = THR_OPEN/2 (hysteresis)
if !cdt: skip BPF/slice this sample (or run BPF but don't advance HDLC — pick one; document)
```

Retune `THR_*` for **post-AGC 8-bit** domain. Do **not** use D29 pre-AGC
energy that stays dark at pp≈8. Cold start: allow demod once post-AGC peak
exceeds a **low** documented floor while gain is still climbing.

#### C) Adaptive soft decision B1 — mandatory (volume-invariant slice)

Fixed `dem ≷ ±4096` alone is **not** the field product answer.

```text
if |dem| > dem_env: dem_env = mix(dem_env, |dem|, ATTACK)
else:               dem_env = mix(dem_env, |dem|, DECAY)

thr = (k * dem_env) >> K_SHIFT     # k≈0.25..0.40
if dem_env < DEM_ENV_FLOOR:
    hold last_bit                  # true silence / no correlator output
elif dem > +thr:  bit = SPACE      # polarity: match pico_tnc convention
elif dem < -thr:  bit = MARK
else:             hold last_bit
```

- Primary field path = **B1**.  
- pico_tnc `±4096` MAY remain as a **debug parallel** or OR-path only when
  `dem_env` is already large; do not require it for CLOSE.  
- Keep polarity consistent with existing pico_tnc NRZI/PLL expectations.

#### D) Dual-tone normalize B2 — SHOULD (field quality, phase 2 OK)

Clean-room (idea from Direwolf, **no code copy**):

1. Obtain mark and space amplitudes (reuse delay-correlator internals, or
   separate 1200/2200 BPF energies).  
2. Peak/valley AGC **independently** on mark amp and space amp.  
3. `demod_out = m_norm - s_norm`; bit = sign(`demod_out`).  
4. Feeds same pico_tnc PLL.

May ship after B1+Q/L PASS if schedule requires; architect prefers B2 in the
same handoff if size allows.

#### E) Mark/space imbalance

Pre-/de-emphasis often weakens 2200 Hz. Prefer B2 dual AGC. Fixed
`SPACE_BOOST` only as last-resort constant in `board_config.h` with a note.

#### F) Keep unchanged

- 13200 Hz pico_tnc PLL / HDLC / KISS / half-duplex mute / A1 `>>4`.  
- No D27 revival; no KISS FIFO blame.

#### G) STAT (required)

Expose at least: `agc_gain` (or `agc_q8`), `agc_peak` (and valley if used),
`cdt`, `dem_min`/`dem_max`, `dem_env`, `disc_tr`, slice mode flag
(`adapt=1`). Tester must prove gain **moves** on Q and L.

### Explicit non-goals / non-answers

- “Operator sets volume.”  
- Closing RX on a single sweet-spot inject.  
- Reviving D27 mid-bit clean-room as primary.  
- GPL Direwolf/multimon code drop.  
- Blaming KISS FIFO.  
- D29 G4 “raise player” as CLOSE path.

### Acceptance (tester) — product-grade

Same `afsk_rx_bench_2min.wav` (and/or host Bell-202), **two levels minimum**:

| ID | Inject | Pass |
| --- | --- | --- |
| Q | Quiet (current weak bench — adc 8-bit pp small, ~D29) | `agc` **climbs** from unity; `cdt` opens; `disc_tr`↑; `|dem|` usable; **`ok≥1` and/or KISS ≥1** |
| L | Loud (higher player/mixer — strong AF, avoid hard ADC clip) | gain **drops or stable** mid-range; still **`ok≥1` / KISS**; no stuck clip forever |
| T | TX smoke | PTT + AFSK; no own-TX KISS echo |

CLOSE only if **both Q and L** pass. One lucky level = **FAIL product**.

### Authorized files

- `src/afsk_bell202.c` (AGC/CDT/adaptive slice glue)
- `src/pico_tnc_rx/**` (slice hook; keep PLL/HDLC)
- `src/board_config.h` (AGC/attack/decay/k/clamps/CDT thr)
- STAT dump formatting; `agent-workspaces/implementer/notes.md`
  (cite Direwolf/ESP32 **ideas** + final constants)

**Status:** D30 **FAIL** live (front green, `ok=kiss=0`). Human/tester
**abandoned** D24–D30 spiral. Superseded by **D31** § RX rethink A+.

---

## RX rethink — Direwolf-class A+ (`RGNX_FW_APRS_TNC_RX_APLUS` — D31)

### Why the previous spiral is closed

Sources: `return_architect_rx_abandon_2026-11-21.md`,
`analysis_D30_front_ok_no_frame_2026-11-21.md`.

| Fact | Implication |
| --- | --- |
| D24–D30 never produced `ok≥1` / KISS on `afsk_rx_bench_2min.wav` | Patch spiral on pico_tnc delay-correlator + AGC/slice is **non-productive** |
| D30: `agc=max`, `cdt=1`, `disc_tr`↑, `dem` huge, `adapt=1`, still `ok=bad_fcs=kiss=0` | **Edges ≠ frames**; noise-driven slice at GAIN_MAX is false progress |
| Same weak ADC sticky (~8 counts 8-bit) across runs | Front-end patches cannot invent Bell-202 structure the correlator does not extract |
| Product AF level never constant | Correct constraint — but **relative multi-slicer** is the industry answer, not more absolute thresholds |

**Hard stop:** no further D30-style AGC/CDT/B1-only orders. New ID only.

### What “Direwolf-robust / no manual thresholds” actually means

Dire Wolf (WB2OSZ) does **not** ask the operator to set a decision voltage.
For VHF 1200 AFSK the proven idea (papers + `demod_afsk` Profile **A / A+**)
is:

1. Measure **mark amplitude** and **space amplitude** separately (IQ mix →
   envelope, or equivalent correlators at 1200 / 2200 Hz).  
2. Decide the bit by **which tone is stronger**, not by absolute ADC counts.  
3. Because radios pre-/de-emphasize and stations differ, the mark/space ratio
   varies (WA8LMF TNC Test CD Track 2: median M/S ≈ 2.5). Instead of one
   magic AGC setpoint for the slicer, run **many parallel slicers** with a
   geometric ladder of space gains (A+: **9** gains from **0.5 … 4.0**):

   `demod_out[i] = m_amp − space_gain[i] × s_amp`

4. Each slicer has its own DPLL + HDLC path; **duplicate frames** collapse to
   one emission. Whichever gain matches this packet’s twist wins — **no pot,
   no “set volume until it works.”**  
5. Peak/valley AGC remains useful for **confidence / audio-level display**,
   not as the primary decision threshold (A+ lesson: multi-slice replaced
   “single AGC then one threshold”).

That is the product behavior users know from Direwolf: plug AF in, decode.

**License:** Direwolf and AetherSDR’s Direwolf-derived path are **GPL**.
RIGNEXO **must not** vendor that source. Implement **clean-room** from the
public algorithm description in WB2OSZ *Building a Better Demodulator for
APRS — Part 1 (1200 baud)* and the normative sketch below. Cite papers in
`notes.md`; do not paste GPL files.

### Research map (libre / Pico-feasible)

| Source | License | Use for RIGNEXO |
| --- | --- | --- |
| WB2OSZ Better APRS Demodulator Part 1 | Documentation / ideas | **Normative algorithm** (A / A+) |
| Direwolf `demod_afsk` Profile A+ | GPL-2+ | **Read for understanding only** — no copy |
| AetherSDR Vhf1200 / AetherAFSKDemod | GPL (Direwolf-derived) | Same — ideas only |
| iontodirel/libmodem | **MIT** | Desktop C++ reference; WA8LMF-oriented; not a drop-in Pico C port (WIP, host-oriented). May inform offline golden tests |
| esp32_libmodem (hiperiondev) | **GPL-3** (upstream MIT ideas) | Do **not** vendor; pipeline shape (BPF → IQ → sinc → Gardner) is a useful **idea** checklist |
| cgorski/yodel | MIT/Apache | Rust `no_std` quadrature correlator + adaptive PLL — algorithm ideas; not C drop-in |
| amedes/pico_tnc | BSD-3 | Already ported (D28); **failed live** as glued; demote from primary demod |
| AE6EO OpenTNC | GPL-3 | Out |
| Goertzel dual-tone (µArt / PacketRF articles) | Educational | Lighter MCU tone detector; optional Phase-0 prototype, not as robust as A+ multi-slice |

**MCU fit:** Primary board **RP2350** has usable float DSP (ARM FPU). Target
**24000 Hz** RX (20 samples/bit) — same class as Aether Vhf1200. Keep
fixed-point Q15 path for **RP2040** if float budget hurts. 5 slicers first
is OK; scale to 9 if CPU allows. Dual-core: optional Core1 RX later; not
required for first land.

### Normative RX architecture (A+-class)

```text
adc_read() 12-bit
  → x = (adc - mid) / scale     # normalize to ~[-1,1] or Q15; DC remove
  → optional light bandpass around 900–2500 Hz (prefilter)
  → IQ (or correlator) at 1200 Hz → m_amp = |m|
  → IQ (or correlator) at 2200 Hz → s_amp = |s|
  → for i in 0..N-1:            # N=5 first, goal N=9
        soft[i] = m_amp - G[i]*s_amp
        DPLL[i] → bit[i] → NRZI → HDLC[i]
  → first FCS-OK frame wins; suppress duplicates (same FCS/payload)
  → KISS emit (existing path)
```

**Geometric gains (lock intent):**

```text
G[0] = 0.5
G[k] = G[k-1] * 10^(log10(4.0/0.5)/(N-1))   # N=9 → Direwolf A+ ladder
```

**What is deliberately NOT the product answer**

- Operator volume / sweet-spot  
- Single fixed `±4096` (pico_tnc) as primary  
- Single adaptive `|dem|` threshold alone (D30 proved edges without flags)  
- “AGC louder until correlator looks busy”  
- GPL code drop  

**Relative compare is self-normalizing:** if both tones scale with volume,
`m − G·s` sign is stable across a wide AF range. That is why Direwolf does
not need a manual decision threshold for twist — the **bank of G** covers
it.

Optional: soft input AGC only to avoid ADC idle-noise domination when AF is
absent (gate / DCD), **not** to invent a single slice level.

### Sample-rate / module plan

| Item | Lock |
| --- | --- |
| RX sample rate | **24000 Hz** primary (`BOARD_AFSK_RX_SAMPLE_RATE_HZ` or unify) |
| TX | Keep working 9600 TX until a dedicated align handoff; do not break TX PASS |
| New module | `src/afsk_aplus/**` (or similar) — correlator, slicer bank, DPLLs |
| HDLC/KISS | Reuse RIGNEXO HDLC/CRC/KISS; **or** thin per-slicer HDLC state copied from existing logic — no pico_tnc delay-correlator as bit source |
| Legacy | `pico_tnc_rx` may compile behind `#if 0` / unused until removed in a cleanup handoff |
| STAT | Per-slicer: `flag_hunt`, `entered_data`, `ok`, `bad_fcs`; global `dup_drop`; **forbid** CLOSE on `disc_tr` alone |

### Validation strategy (mandatory — stops false progress)

Tester abandon asked for a **new framing**. Gate order:

| Gate | Who | Pass |
| --- | --- | --- |
| **O0 Offline golden** | Implementer (+ host tool / same C DSP sources) | Feed `fixtures/afsk_rx_bench_2min.wav` (PCM) into A+ demod → **≥1 FCS-OK frame** matching manifest. **No flash required.** |
| **O1 Offline level sweep** | Implementer | Same WAV at ≤0.25× and ≥2× amplitude (digital scale) still yields ≥1 frame — proves “no manual threshold” |
| **H1 Live** | Tester | Flash; quiet + loud inject; `ok≥1` and/or KISS section A; TX smoke |
| **H2** | Tester | STAT shows which slicer(s) contributed; `flag`/`bad_fcs` counters move before celebrating edges |

**CLOSE PASS only after O0+O1 then H1.** Live-only edge metrics are insufficient.

### Implementer phases inside this handoff ID

1. **Offline-first DSP** in `src/afsk_aplus/` (+ optional small host harness under
   `scripts/` or `tools/` if architect-authorized — prefer reuse of fixture WAV
   with a documented host command). Prove O0/O1 in `notes.md` with counts.  
2. **Wire to mode 4** ADC timer @ 24 kHz → A+ → existing KISS callback.  
3. STAT + half-duplex mute + TX smoke preserved.  
4. Tester H1/H2.

If O0 fails on the fixture, **stop** and return to architect (fixture bad or
algorithm bug) — do not flash thrash.

### Explicit non-goals

- Extending D29/D30 AGC patches  
- Vendoring Direwolf / AetherSDR / esp32_libmodem GPL trees  
- Claiming PASS from `disc_tr` / `agc_q8`  
- Reviving D27 mid-bit as primary  

### Acceptance (closes `RGNX_FW_APRS_TNC_RX_APLUS`)

- [x] O0: offline fixture → ≥1 good frame (documented)  
- [x] O1: amplitude sweep offline still decodes  
- [ ] H1: live quiet + loud → `ok≥1` and/or KISS; TX smoke  
      (**PARTIAL 2026-11-21:** LOUD strong, REF marginal, **QUIET FAIL** —
      see D32; do not CLOSE D31 alone)
- [x] Attribution: papers + clean-room note; **no** GPL sources in tree  
- [x] Builds `CHIP=rp2350` (required); `rp2040` best-effort / fixed-point  

### Authorized files

- New: `src/afsk_aplus/**` (demod core)  
- `src/afsk_bell202.c` / `.h` (timer rate, feed, mute glue)  
- `src/board_config.h` (RX rate, N slicers, gain table)  
- Existing KISS/HDLC modules as needed  
- Optional: `tools/` or `scripts/` offline replay (document in notes)  
- `agent-workspaces/implementer/notes.md`  
- Demote/isolate `src/pico_tnc_rx/**` (no further feature work)

---

## A+ weak-AF / yield improvement (`RGNX_FW_APRS_TNC_RX_WEAK` — D32)

**Status: CLOSED FAIL / REGRESS (2026-11-21).** Live matrix quiet/ref/loud all
`dcd=0` — worse than D31. **Do not ship or extend.** See § D32 revert (D33).
Section kept for history / what not to repeat.

### (Historical order — superseded by D33 revert)

### Tester verdict (keep working; raise yield)

Sources: `note_architect_D31_H1_quiet_gap_2026-11-21.md`, `test_runs.md`
`H1-LOUD` / `H1-QUIET`, implementer `notes.md` O0/O1.

| Level | Live | Notes |
| --- | --- | --- |
| LOUD | **PASS strong** | `ok=64`, `kiss_ok=25`, **A01–A10** + B/C/E/F/G/H/I; slicers 0/1/2 win |
| REF ~73% | **PASS marginal** | often `ok=1`, e.g. `RGNX-H-REPEAT`; section A may miss in short window |
| QUIET ~34% | **FAIL** | `ok=0`, 0 KISS; `dcd=1`; `flag`/`entered`↑; **`bad_fcs`≈1k** |
| Offline 0.25× / 1× / 2× | **PASS** | Same algorithm; digital scale ≠ live SNR |

**Product read:** modem **works**. Gap is **live weak-AF SNR / bit quality**,
not “A+ is wrong.” Offline quiet PASS means amplitude alone is handled;
live 34% adds coupling/ADC/ambient noise → many almost-frames (`bad_fcs`)
and zero FCS-OK.

**Quiet floor:** keep **~34% PC/VLC** (or equivalent ADC sticky span
~1970…2100) as the H1 quiet gate. Do **not** redefine to “73% only.”

### Diagnosis vs current `src/afsk_aplus` (architect reading)

| Current behavior | Why it hurts quiet |
| --- | --- |
| No bandpass **prefilter** before IQ | Wideband noise into correlators; Direwolf Profile A uses narrow BP (~0.155×baud outside tones, ~8 symbol taps) |
| Soft decision uses **peak/valley-normalized** `m,s` then `m−G·s` | At low SNR envelopes track noise → normalized noise looks “strong”; A+ papers use **raw** amps + gain ladder for the decision |
| `AFSK_APLUS_N_SLICERS=5` | Loud already uses ok0/1/2; more gains (goal **9**) cover twist under noise |
| DCD computed but **does not gate** slicer/HDLC pressure | Quiet: perpetual flag hunt + `bad_fcs` rain |
| Single PLL nudge (`pll -= pll>>2`) | No search vs locked inertia (Direwolf-class ~0.50 / ~0.74) |
| No soft **eye / confidence** gate | Bits commit even when `|m−G·s|` is tiny vs noise |

### Normative improvements (ordered — additive; protect loud)

Implement in order; after each meaningful chunk re-run **offline O0/O1** and
document. Live matrix is tester’s: **34% / ~73% / loud**.

#### W1 — Narrow bandpass prefilter (required)

Clean-room FIR (or cheap biquad cascade) before IQ, targeting roughly
900–2500 Hz (mark−margin … space+margin). Intent matches Direwolf A
“narrower is better” — **ideas only, no GPL taps copy**.

- Document tap count / cutoff in `notes.md`.  
- Must not regress offline O0 or live LOUD.

#### W2 — Decision on raw mark/space amps (required)

Change soft path to Direwolf A+ intent:

```text
m_raw, s_raw = |IQ| envelopes (optional light LPF)
soft[i] = m_raw - G[i] * s_raw          # primary
```

Peak/valley normalize may remain for **STAT / confidence / DCD**, not as the
only decision input. Optionally keep a **parallel** normalized bank only if
dup-suppress merges winners — default = raw bank.

#### W3 — Eye / confidence gate (required)

Quiet shows `flag`/`bad_fcs`↑ with `ok=0`. Do **not** “lower thresholds.”

```text
eye = ema(|soft_best|) or ema(|m_raw - s_raw|)
conf = |soft[i]| / max(eye, eps)
if conf < CONF_MIN: hold last symbol / skip PLL nudge / skip HDLC bit
```

Tune `CONF_MIN` so offline O0 still ≥ prior ok count (±10%); live quiet
should raise `ok` and/or cut `bad_fcs` rate. STAT: expose `conf` or
`skip_bits`.

#### W4 — More slicers (required)

Raise `AFSK_APLUS_N_SLICERS` to **9** (geometric 0.5…4.0). Loud already
benefits from 0/1/2; denser ladder helps weak twist. CPU: RP2350 float OK;
if tight, keep 7 as interim and note.

#### W5 — PLL search vs locked inertia (SHOULD)

When transitions consistent near bit center → higher inertia (less noise
tug); while searching → lower. Clean-room constants in ballpark of
0.50 search / 0.74 locked (ideas, not copied code).

#### W6 — Optional weak parallel arm (SHOULD if W1–W4 insufficient)

- Slightly longer IQ τ (more averaging) **or**  
- Soft input scale when pre-IQ RMS is low (clamp max gain; **not** D29
  dead-zone AGC as primary demod)  
Feed as extra slicer(s) into existing dup suppress. Must not increase live
LOUD false `ok` garbage.

### Explicit non-goals

- Redefining quiet floor to 73% without human product sign-off  
- D29/D30 AGC-only spiral / pico_tnc revival  
- GPL Direwolf source  
- CLOSE on LOUD-only or `flag`/`bits` alone  
- Single global fixed threshold retune as the quiet “fix”

### Regression matrix (closes `RGNX_FW_APRS_TNC_RX_WEAK`)

| Gate | Pass |
| --- | --- |
| Offline O0/O1 | Still PASS (ok count not collapsed) |
| Live QUIET ~34% | **`ok≥1` and/or KISS**; prefer ≥1 section-A if window allows |
| Live REF ~73% | still `ok≥1` / KISS (no regress vs D31) |
| Live LOUD | still strong (section A still OK; no collapse) |
| TX smoke | no own-TX KISS echo |

### Authorized files

- `src/afsk_aplus/**` (prefilter, soft path, conf gate, N, PLL)
- `src/board_config.h` (N, CONF_MIN, DCD, filter knobs)
- Offline script if constants need re-proof
- `agent-workspaces/implementer/notes.md` (before/after offline counts +
  which W# landed)

---

## D32 revert — restore D31 A+ (`RGNX_FW_APRS_TNC_RX_REVERT_D31` — D33)

### Tester verdict

Source: `return_architect_D32_regress_2026-11-21.md`.

D32 WEAK (W1–W5) live: **quiet / ~73% / loud 95%** all `dcd=0`,
`flag=bits=ok=kiss=0`. ADC still shows inject. **Regresses** D31 ref+loud
PASS. Offline O0/O1 PASS did not predict live gate failure.

### Architect lock

**Full revert** of D32 to the D31 A+ baseline that decoded live. Prefer git
restore of pre-D32 `afsk_aplus` + related `board_config` knobs.

| Undo | Restore (D31) |
| --- | --- |
| Hard DCD gate blocking slicer/HDLC | Demod runs as D31 (DCD may be STAT-only) |
| W1 RBJ prefilter in path | DC → IQ directly |
| Raw-only soft + conf skip | Peak/valley norm soft + `m_n−G·s_n` |
| N=9 | **N=5** |
| W5 dual PLL inertia | D31 single nudge |

**Forbidden in this handoff:** partial “just retune DCD”; new weak-AF stack;
keeping D32 features that never opened live.

### Acceptance

- Offline O0/O1 PASS  
- Live **~73%** and **loud** decode at least as well as D31 (ok/KISS)  
- Quiet may still fail (known D31 gap) — must not be total `dcd=0` silence if
  that is worse than D31’s flag hunt  
- notes.md: **D32 REVERTED**

### Authorized files

- `src/afsk_aplus/**`
- `src/board_config.h` (D32 symbols only)
- `agent-workspaces/implementer/notes.md`
