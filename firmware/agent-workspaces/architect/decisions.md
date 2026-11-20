# Architect decisions — `rignexo/firmware`

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
