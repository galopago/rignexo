# Architect decisions — `rignexo/firmware`

## RGNX_FW_APRS_TNC_V1 — 2026-11-20

### D21 — Fourth mode `APRS_TNC`: KISS softmodem (Bell 202)

**Decision:** Add mode 4 as an on-device **1200 baud Bell 202 AFSK** modem
exposed as **KISS over USB CDC** (no UAC2). PTT from modem TXDELAY/TXTAIL.
Reuse GPIO 28/2/14/27; MODE_LED stays GPIO0 with **4** OFF pulses. Cycle:
COMPOSITE → AUDIO_VOX → COMPOSITE_CAT → **APRS_TNC** → COMPOSITE.

**Chosen over:** TX-only beacon; host-only Direwolf via mode 1; full HK-21 /
PBBS TNC; GPS tracker MicroAPRS.

**Why:** Apps that open a COM/KISS port need a real modem on the dongle; mode 1
already covers soundcard TNCs on the host.

**Normative doc:** `docs/aprs_tnc_mode_architecture.md`.

### D40 — Mode 4 packet OK = long OFF (human vision)

**Decision:** Human/tester revise D39: brief dark dips (OFF=80) do not survive
retinal persistence; **packet OK must be a longer OFF**. Idle/bad stay
**150/150** (modes 1–3). Metric remains **FCS-only** (no `pp_good`).

| State | OFF/GAP | Trigger |
| --- | --- | --- |
| Baseline | **150/150** | Cold, CLR, hold expiry, no recent FCS |
| Packet OK | **250/50** (TICK 300) | FCS-OK only; hold **3 s** then baseline |

**D40.0** **220/80** + 2 s → **D40.1** **240/60** + 3 s → **D40.2** **250/50**
(+10 ms OFF; human “más notorio”). Do not jump to 260/40 without eye check.

TICK/count/quiet unchanged. D38 STAT stays (`led_off_ms` 150 vs 250).

**Handoff:** `RGNX_FW_MODE4_PKT_LED_TWEAK2`.  
**Evidence:** `note_architect_D40_led_tweak2_2026-11-21.md`.

### D39 — Mode 4 packet LED: baseline 150/150; short OFF only on FCS

**Decision (superseded by D40 for polarity):** bad/idle **150/150**; D39 used
**short OFF 80** after FCS. FCS-only metric and delete `pp_good` **remain**.
D40 flips packet timing to long OFF **220**.

**Handoff:** `RGNX_FW_MODE4_PKT_LED` (landed; polarity amended by D40).  
**Evidence:** code-review note; D38 matrix.

### D38 — Mode 4 AF LED debug via RGNX STAT

**Decision:** Human/tester cannot correlate volume vs blinks after D36/D37
without live numbers. Add **debug-only** visibility of AF metric + PIO OFF
timer on the existing `RGNX STAT` / KISS `0x0C` path (`RGNXDBG` lines), same
class as RX STAT. Default field behavior unchanged (optional `AFDBG`
stream off by default).

Minimum dump fields: `af_pp`, `af_good`, `af_hold`, `af_hold_ms`, `af_reason`,
`led_off_ms`, `led_gap_ms`, thresholds echo. No OFF-width or demod change in
this handoff.

**Handoff:** `RGNX_FW_MODE4_AF_LED_DEBUG`.  
**Evidence:** `note_architect_mode4_af_led_debug_stat_2026-11-21.md`.

### D37 — Mode 4 AF LED metric: packet quality, not VU

**Decision:** Tester proved D34/D36 pp ladder (**GOOD=250 / BAD=180**) never
matches live decode (**pp≈130–150** on PASS) and **cancels FCS-OK hold**.

Lock metric as **packet quality**:

1. **Primary GOOD** = FCS-OK (+ 10 s hold).  
2. While hold armed: silence OK; mid-pp MUST NOT `force_bad`.  
3. **Secondary** pp assist only: ACTIVITY=40, BAD=80, GOOD=120 (12-bit / 200 ms
   window).  
4. Noise/activity alone ≠ GOOD. Clip may force BAD.  
5. Keep D36 OFF 260/50.

**Handoff:** `RGNX_FW_MODE4_AF_LED_METRIC`.  
**Evidence:** `note_architect_mode4_af_metric_mismatch_2026-11-21.md`,
`note_architect_mode4_af_metric_thresholds_2026-11-21.md`.

### D36 — Mode 4 AF LED OFF widths: maximize GOOD↔BAD contrast

**Decision:** Human report: D34 **GOOD (220) and BAD (80) looked the same** —
could not tell the two states apart. That is a **between-state contrast**
failure. (Earlier draft that lengthened BAD toward GOOD was **wrong** and is
withdrawn.)

Keep **4** pulses and **TICK=300 ms**. Push duties to opposite extremes:

| | D34 (failed) | D36 |
| --- | ---: | ---: |
| OFF_GOOD | 220 | **260** ms (GAP 40) |
| OFF_BAD | 80 | **50** ms (GAP 250) |
| \|ΔOFF\| | 140 | **210** ms |

Hold (D35) unchanged. If still indistinguishable, reopen TICK (new decision).

**Handoff:** `RGNX_FW_MODE4_AF_LED_DUTY`.  
**Evidence:** human glance report (GOOD≈BAD).

### D35 — Mode 4 AF LED human hold after packet

**Decision:** D34 instantaneous “idle → BAD” is **not** field-usable: packets
are ≤1 s and gaps may be minutes. Lock **peak-hold**:

| Item | Lock |
| --- | --- |
| Duration | `BOARD_MODE4_AF_HOLD_MS` = **10000** (10 s) |
| Owner | **CPU** AF meter / tick — not PIO; expiry calls `set_af_good(false)` |
| No more packets | After 10 s → BAD (short OFF) and **stay** BAD until new activity |
| Silence during hold | Must **not** clear GOOD early |
| Instant BAD activity | Clear to BAD immediately |
| Cold start | BAD |

**Handoff:** `RGNX_FW_MODE4_AF_LED_HOLD` (human clarification written into
implementer handoff).  
**Amends:** D34 metric/idle rule only; PIO/OFF duty/polarity unchanged.

### D34 — Mode 4 MODE_LED AF overlay via PIO OFF duty only

**Decision:** Product shows RX AF adequacy on the external MODE_LED (GPIO0)
in **`APRS_TNC` only**. Customer has no WS2812; PTT LED stays TX-primary.

Locks:

1. Waveform remains **PIO** (no CPU blink loop).  
2. Modes **1–3** unchanged (fixed counts/widths, no AF refresh).  
3. Mode **4**: still **4** OFF events; **quiet duration fixed**; **inter-blink
   period fixed**; only **OFF duty** changes (GAP = period − OFF).  
4. Polaridad: **long OFF = good AF**; **short OFF = bad AF** (not brightness).  
5. RX/AFSK updates a level variable; `mode_led_pio` pushes delays into PIO.
   Mode-4 program needs **separate OFF vs GAP** delays (today one `Y` for both).  
6. Starting ms: quiet **800**, tick **300**, OFF good **220**, OFF bad **80**;
   API `mode_led_pio_set_af_good(bool)`.

**Handoff:** `RGNX_FW_MODE4_AF_LED` (implementation-ready).  
**Normative:** `docs/device_modes_architecture.md` § Mode 4 AF / PIO refresh.

### D33 — Full revert of D32 WEAK to D31 A+ baseline

**Decision:** Tester D32 matrix **CLOSED FAIL / REGRESS**: quiet, ref (~73%),
and loud (95%) all show `dcd=0` and zero decode. Kills the live D31 baseline
that already had ref marginal + loud strong PASS.

Lock **`RGNX_FW_APRS_TNC_RX_REVERT_D31`**: implementer must **fully revert**
all D32 W1–W5 changes and restore D31 A+ RX behavior. No partial retune of
the hard DCD gate while keeping the rest. No new weak-AF features in this
handoff. After revert, tester must re-confirm 73% + loud ≥ D31 before any
future quiet work.

**Supersedes / abandons:** D32 as shipped.  
**Handoff:** `RGNX_FW_APRS_TNC_RX_REVERT_D31`.  
**Evidence:** `return_architect_D32_regress_2026-11-21.md`.

### D32 — A+ weak-AF yield (after D31 H1 quiet FAIL)

**Decision:** D31 A+ **works** live (LOUD strong, REF marginal) but H1
**not CLOSED**: QUIET ~34% → `ok=0` with heavy `flag`/`bad_fcs`. Offline
0.25× already PASS → gap is live SNR/coupling, not “need constant volume.”

Lock `RGNX_FW_APRS_TNC_RX_WEAK`: improve quiet **without** breaking loud/ref:

1. Narrow BP **prefilter** before IQ  
2. Soft decision on **raw** `m−G·s` (normalize for conf/STAT only)  
3. **Confidence/eye gate** (cut bad_fcs rain; do not lower slice blindly)  
4. **N=9** slicers  
5. SHOULD: dual PLL inertia; optional weak parallel arm  

Quiet floor stays ~34%. No D29 AGC spiral. No quiet-floor renegotiation.

**Handoff:** `RGNX_FW_APRS_TNC_RX_WEAK`.  
**Evidence:** `note_architect_D31_H1_quiet_gap_2026-11-21.md`.  
**Status:** Implemented; live **REGRESS** (all levels `dcd=0`) → **D33 revert**.

### D31 — RX rethink: Direwolf-class A+ multi-slicer (after abandon)

**Decision:** Human/tester **CLOSED FAIL** and abandoned D24–D30. D30 proved
front-end metrics (`agc`, `cdt`, `disc_tr`) can be green with **zero** frames —
delay-correlator + absolute/adaptive slice is the wrong primary architecture.

Lock **`RGNX_FW_APRS_TNC_RX_APLUS`**:

1. **Primary demod** = clean-room **A+-class**: dual-tone mark/space amplitudes
   + **N parallel slicers** with geometric `space_gain` ladder (0.5…4.0, N=5→9)
   + per-slicer DPLL/HDLC + duplicate suppress.  
2. **No operator volume / decision pot.** Relative `m − G·s` is the product
   answer (WB2OSZ Better APRS Demodulator Part 1 ideas; **no GPL code**).  
3. RX sample rate **24000 Hz** primary (RP2350 float OK; RP2040 Q15).  
4. **Offline golden (O0/O1) mandatory** on fixture WAV before live CLOSE.  
5. Demote pico_tnc delay-correlator path; stop AGC-only patch orders.

**Rejected as primary:** further D30 AGC/B1; GPL Direwolf/AetherSDR vendor;
CLOSE on `disc_tr` alone.

**Handoff:** `RGNX_FW_APRS_TNC_RX_APLUS`.  
**Evidence:** `return_architect_rx_abandon_2026-11-21.md`,
`analysis_D30_front_ok_no_frame_2026-11-21.md`.  
**Status:** Implemented; offline PASS; live LOUD PASS / QUIET FAIL → **D32**.

### D30 — Field-robust RX front-end (after D29 FAIL + ops note)

**Decision:** D29 FAIL: A1 feed OK but AGC dead-zone (`err>>7` → `agc_q8=256`
forever) and CDT on **pre-AGC** 8-bit energy blocked BPF (`dem=0`). Human/
tester ops note: product AF level is **never** constant; “raise volume” /
sweet-spot CLOSE is rejected.

Lock `RGNX_FW_APRS_TNC_RX_FIELD`:

1. Keep A1 (`ADC_BIT 8` + `adc>>4`).  
2. AGC that **moves** (multiplicative / min-step; fast attack / slow decay;
   no integer dead-zone).  
3. CDT/enable on **post-AGC** energy with hysteresis.  
4. **B1 adaptive slice** on `|dem|` envelope (primary); pico_tnc `±4096`
   secondary only. Optional B2 dual-tone normalize (Direwolf idea, clean-room).  
5. Acceptance **quiet + loud** both required.

Research basis (ideas only, no GPL): Direwolf AFSK peak/valley AGC + relative
mark/space; ESP32-IDF APRS in-band AGC + tone-band gate; WB2OSZ Better APRS
Demodulator Part 1 (pre/de-emphasis imbalance).

**Supersedes:** D29 acceptance G4 “raise player”; fixed-threshold-only stance.  
**Handoff:** `RGNX_FW_APRS_TNC_RX_FIELD`.  
**Evidence:** `analysis_D29_agc_cdt_2026-11-21.md`,
`note_architect_rx_level_ops_2026-11-21.md`.  
**Status:** Implemented; live FAIL (edges, 0 frames). Spiral **abandoned** →
**D31**.

### D29 — RX level scale: ADC_BIT 8 + digital AGC (after D28 FAIL)

**Decision:** D28 port live FAIL: `disc_tr=0`, `ok=0`, ADC pp≈133, port used
`ADC_BIT 12` vs pico_tnc default **8**. Lock **A1** (`ADC_BIT 8` +
`adc>>4` feed) plus **digital AGC** into target 8-bit AC headroom so `dem`
can cross ±4096 across radio volumes. Add STAT `dem_min`/`dem_max`. Do not
retune PLL as primary; do not revive D27.

**Handoff:** `RGNX_FW_APRS_TNC_RX_LEVEL`.  
**Evidence:** `analysis_D28_stat_vs_code_2026-11-21.md`.  
**Status:** Implemented; tester **FAIL** → superseded by **D30** (keep A1).

### D28 — Port libre Pico TNC RX (Option R1; end clean-room RX thrash)

**Decision:** After D24–D27 still `ok=0` (D27 regresses abort), adopt tester
**Option R1**: base on-device Bell-202 **RX** on
[amedes/pico_tnc](https://github.com/amedes/pico_tnc) (**BSD-3**), sample rate
**13200 Hz** (11 samples/bit), BPF + delay correlator + LPF + digital PLL as
in that project. Keep RIGNEXO KISS CDC / PTT / modes / STAT / pins. No GPL
vendoring. Handoff: `RGNX_FW_APRS_TNC_RX_PORT`.

**Overrides:** D21 clean-room **for RX modem files only**; D27 mid-bit path is
**superseded** as the primary RX strategy.

**Why:** Proven on RP2040 Pico; license-compatible; stops rediscovering bit sync.

### D27 — RX mid-bit sync (after L2 abort/align_slip)

**Decision:** D26 PASS; L2 under WAV: `ok=0`, `abort`+`align_slip` dominate,
`kiss_drop=0`, L1 still PASS. Order `RGNX_FW_APRS_TNC_RX_SYNC`: replace
aggressive C-lite edge-emit with **hysteresis + mid-bit sample + slow SPB
pull-to-nominal**; goal `ok≥1` and lower abort/align rates on same fixture.
Do not rewrite discriminator or chase DCD first.

**Evidence:** `findings_rx_debug_D25_2026-11-21.md` §10–§11.

### D26 — Fix RX STAT CDC transport (after L1 PASS)

**Decision:** Tester D25 bench: L1 tones PASS; STAT truncates ~128 B so L2
invisible; 0 KISS remains. Order **STAT writer fix only**
(`RGNX_FW_APRS_TNC_RX_DEBUG_TX`): `tud_cdc_write_available` wait +
`tud_task`/flush per line (or equivalent); no demod redesign until complete
L2 is readable.

**Evidence:** `tester/findings_rx_debug_D25_2026-11-21.md`.

### D25 — APRS TNC RX layered debug over CDC (tone-first)

**Decision:** After RX_CLOCK still 0 KISS on live inject, add
`RGNX_FW_APRS_TNC_RX_DEBUG`: host-triggered ASCII dump on CDC (`RGNX STAT` /
KISS cmd `0x0C`) with layers L0 energy/DCD/ADC, **L1 discriminator
transitions (priority)**, L2 HDLC counts, L3 KISS send/drop, Lclk SPB/timer.
Clear via `RGNX CLR` / cmd `0x0D`. Instrumentation only — no demod redesign
in the same handoff.

**Why:** Tester cannot localize “no audio” vs “tones but no HDLC” from silent
CDC alone.

### D24 — APRS TNC RX clock: hybrid true-9600 + RX freq assist

**Decision:** Tester FAIL (0 KISS on host/external AFSK inject; HW ruled out;
offline PASS at exact 9600, FAIL at `1e6/104` Hz). Choose option **D**:

1. Fix shared TX/RX sample timer to **average exactly 9600 Hz** (Bresenham
   104/105 µs or equivalent) — forbid fixed `1000000/9600 → 104`.
2. Add **C-lite** RX frequency tracking (fractional SPB ~7.7–8.3) so
   cross-source 1200 baud still locks.

**Rejected alone:** B (live at 9615 Hz); C without fixing truncation.

**Normative:** `docs/aprs_tnc_mode_architecture.md` § RX baud-clock fix.
Handoff: `RGNX_FW_APRS_TNC_RX_CLOCK`.

### D23 — APRS TNC RX contract (post-TX PASS)

**Decision:** After TX audio/PTT PASS, lock `RGNX_FW_APRS_TNC_RX` in
`aprs_tnc_mode_architecture.md` § RX implementer contract: delay-discriminator
demod @ 9600 on GPIO28, NRZI→HDLC streaming RX reusing `hdlc_crc16`, KISS
emit without FCS, mute RX during own TX, DCD + CSMA into TX SM, authorized
file list and tester acceptance.

**Why:** TX proved tones; RX/CSMA must not be improvised against the working
TX path.

### D22 — APRS TNC implementer locks (post-shell)

**Decision:** After mode-shell PASS, bind TX/RX implementer rules in
`aprs_tnc_mode_architecture.md`: sample rate **9600**, clean-room IP, FCS
append/strip, TX state machine (PTT→TXDELAY→frame→TXTAIL), module split
`kiss_cdc` / `hdlc_ax25` / `afsk_bell202`, max frame 512, no GPIO0 audio,
escalate pin/license/scope changes. TX and RX remain separate handoffs.

**Why:** Shell only proved USB/LED/KISS plumbing; modem DSP must not be
improvised.

## RGNX_FW_CAT_IF_HAMLIB_V1 — 2026-11-20

### D20 — Hamlib-compatible `IF;` frame (length + VFO byte)

**Decision:** After WSJT-X Test CAT FAIL (`kenwood_set_freq: unsupported VFO
None` while `current_vfo=None`), treat the Kenwood `IF;` reply as a
**fixed-length Hamlib contract**, not a loose stub.

- Wire length **38** including `;` (Hamlib strips `;` and expects **37**).
- `priv->info[30]` MUST be `0`/`1`/`2` (A/B/MEM) from soft `fr_vfo`.
- `info[28]` TX from live PTT; `info[29]` MD from soft mode; `info[32]` split
  from `fr_vfo != ft_vfo`.
- Six ASCII spaces after frequency; RIT field six **digits** (no `+`/`-`
  that shifts later bytes).
- Cold-start normative string:
  `IF00014074000      000000000020000000;`

**Why:** Soft-state FA/MD set-then-get PASS on Linux scripts does not exercise
Hamlib’s `kenwood_get_vfo_if` → `set_freq(VFO_NONE)` path used by WSJT-X.
A short `IF` leaves `current_vfo` unset.

**Evidence:** tester handoff 2026-11-20; Hamlib `kenwood.c` /
`kenwood_get_vfo_if`; default `if_len=37`.

## RGNX_FW_CAT_STATE_RAM_V1 — 2026-11-20

### D19 — CAT soft state in RAM (set = get)

**Decision:** After partial tester PASS on CAT PTT, upgrade Kenwood stubs to a
RAM soft-radio state. **Set = get** for at least:

- `FA` / `FB` (frequency)
- `MD` (mode)
- `FW` (filter **width**)
- `SL` / `SH` (SSB cut / width-related)
- `PC` (power)
- `FR` / `FT` (VFO select)
- `IF;` built from current state + live PTT bit

Defaults on `cat_kenwood_init` MUST be the **coherent cold-start digi profile**
(14.074 MHz USB, FW 2700, SL00/SH08, PC100, VFO A, SQ open, gains full,
SM zero, ID019). No flash. PTT only TX/RX. Further opcodes MAY be added when
tester logs them.

**Why:** Apps set frequency, width, and other knobs then read them back; they
also **poll before set**. Random/`?;` on first poll breaks CAT tests.

## RGNX_FW_CAT_PTT_V1 — 2026-11-20

### D18 — Third mode `COMPOSITE_CAT` with Kenwood ASCII CAT

**Decision:** Add mode 3: same USB composite as mode 1 (UAC2 + CDC), PTT from
Kenwood-style CAT (`TX;` / `TX0;` / `TX1;` assert; `RX;` release). RTS/DTR MUST
NOT key PTT in this mode. Emulate **TS-2000-class** ASCII (`;`-terminated).
Stub FA/FB/MD/IF/ID/AI; unknown → `?;`. MODE_LED = **3** off pulses. Cycle:
COMPOSITE → AUDIO_VOX → COMPOSITE_CAT → COMPOSITE.

**Alternatives considered:** Icom CI-V (common but binary/heavy); Yaesu
(fragmented / often dual-COM RTS). Deferred as non-goals for v1.

**Why:** Apps that PTT via CAT; Kenwood profile is the simplest widely offered
“rig type” for a fake interface.

## Process — ZERO CODE LAW reinforcement (2026-11-19)

### D17 — Architect never edits firmware source

**Decision:** Strengthen `docs/architect_role_hard_stop.md` with an absolute
**ZERO CODE LAW**: while role = architect, no writes to `src/**`, root
`CMakeLists.txt`, `scripts/**`, or other compilable inputs — including when the
human asks to “generate/fix code” without an explicit *act as implementer*.
Correct response: normative docs + **pending** implementer handoff only.

**Why:** An architect session incorrectly patched `board_config.h` /
`mode_store.c` for MODE_SWITCH polarity; human required a stronger rule so it
cannot recur.

## RGNX_FW_DEVICE_MODES_V1 — 2026-11-19

### D8 — Boot-time only mode change

**Decision:** Device mode is selected only at power-up / USB attach. If
`MODE_SWITCH` is held (active **high**, D16) during the debounce window,
advance one step in the cycle and persist. Runtime presses do not change mode.
Operator must disconnect to change personality.

**Why:** Avoids hot USB re-enumeration; matches human request; reliable across
OSes.

### D9 — Modes and PTT sources (extended by D18)

**Decision:**

- `COMPOSITE`: UAC2 + CDC; PTT from RTS/DTR as today.
- `AUDIO_VOX`: UAC2 only (no CDC); PTT from VOX on **speaker** (TX) audio.
- `COMPOSITE_CAT` (D18): UAC2 + CDC; PTT from Kenwood CAT; RTS ignored.

Default / corrupt / unknown mode byte → effective `COMPOSITE` (+ heal rules).
MODE_LED: PIO mostly-on with 1 / 2 / **3** off pulses.

### D10 — Distinct USB PID per mode

**Decision:** AUDIO_VOX SHOULD use a different USB product ID than COMPOSITE
so hosts do not keep stale COM bindings.

### D11 — Mode switch / LED polarities

**Decision (superseded by D16):** Originally active-low. Board is active-high.

### D16 — MODE_SWITCH active high (tester polarity FAIL → H1)

**Decision:** Match mainboard: **+3.3V — SW — GPIO15 — R pull-down — GND**.
`BOARD_MODE_SWITCH_ACTIVE_LEVEL = 1`; firmware uses `gpio_pull_down` (not
pull-up). Idle = low = not held → USB reconnect must **not** advance mode.
Pressed at boot = high for settle+hold window → one advance.

**Why:** Tester showed every USB replug toggled mode because idle low looked
like “held” under the old active-low contract.

### D12 — Anti-glitch flash writes (amendment)

**Decision:** At most **one** mode-related flash write per boot. Button is
sampled once with settle + continuous 50–100 ms active window; long hold still
advances once. Compare-before-write; ignore switch after boot decision.

### D13 — Mode LED PIO off-pulse encoding (amendment)

**Decision:** LED mostly ON. `COMPOSITE` = one short OFF pulse per ~1–1.5 s
cycle; `AUDIO_VOX` = two short OFF pulses per cycle. Pattern runs forever via
**PIO SM**, not CPU main-loop blinking.

### D14 — Unknown mode value (amendment)

**Decision:** Any invalid magic/checksum/unknown mode byte → effective
`COMPOSITE` for this boot; heal with a single write (or one write that also
applies button advance). Never crash; never rewrite loops every boot when
already valid.

### D15 — MODE_LED owns GPIO0; no UART stdio on 0/1 (tester return)

**Decision (Option A):** Keep MODE_LED on GPIO **0**. After
`mode_led_pio_start()`, GPIO0 is exclusive to the MODE_LED PIO SM.
`stdio_init_all()` and UART0 on default pins 0/1 are **forbidden**. Temporary
UART mux inside TinyUSB `board_init()` before LED start is tolerated.
On-module WS2812 (16) is not MODE_LED.

**Why:** Product pin map already assigned LED to 0; tester proved UART reclaim
hides the PIO pattern. Moving the LED would break the agreed hardware map.

## RGNX_FW_PINMAP_V1 — 2026-11-19

### D5 — Primary board and shared pin map

**Decision:** Product primary target is Waveshare **RP2350-Zero**. Firmware
also builds for Waveshare **RP2040-Zero** with the **identical** GPIO map for
backward compatibility. One `board_config.h` serves both; no per-chip pin
tables.

**Canonical pins:**

| Signal | GPIO |
| --- | --- |
| Audio in (ADC2) | 28 |
| PWM audio out | 2 |
| PTT relay | 14 |
| PTT LED | 27 |
| Mode switch (reserved) | 15 |
| Mode LED (reserved) | 0 |

### D6 — Mode pins deferred (superseded)

**Original:** defines only, no runtime.
**Superseded by D8–D11 (`RGNX_FW_DEVICE_MODES_V1`):** pins are active for
boot-time mode UI; full behavior in `docs/device_modes_architecture.md`.

### D7 — Doc drift closed

**Decision:** Supersede the old documented map (26 / 14 / 15 / 29). Active
runtime pins already matched 28 / 2 / 14 / 27 in `board_config.h`; product docs
now match that map.

## RGNX_FW_ASBUILT_V1 — 2026-11-19

### D1 — Agentic pattern source

**Decision:** Mirror the `b02_hil/firmware` agentic layout (`AGENTS.md`,
`docs/methodology.md`, `docs/architect_role_hard_stop.md`, three
`agent-workspaces/*` roles and handoffs), adapted to this tree’s paths
(`src/`, `scripts/`, Pico CMake/Ninja) instead of ESP-IDF `components/` /
`idf.py`.

**Why:** Human requested the same mental model already used on HIL firmware;
keeps role hard stops portable across repos.

### D2 — As-built vs aspirational docs

**Decision (original):** Prefer documenting drift over hiding it.
**Superseded by D5–D7 (`RGNX_FW_PINMAP_V1`):** docs and active pins are aligned;
reserved mode pins are explicit.

### D3 — No implementer work from ASBUILT handoff

**Decision:** `RGNX_FW_ASBUILT_V1` closed with documentation only. Pin-map
follow-up opened later as `RGNX_FW_PINMAP_V1`.

### D4 — Build artefact layout

**Decision:** Document the existing `build/<chip>/` layout (`rp2350` default,
`rp2040` via `CHIP`) and tester ownership of running `./scripts/build.sh` /
`flash.sh` as part of the as-built process contract.

**Why:** Matches current scripts after the chip-folder reorganization; agents
must not invent a second build convention.
