# Architect Handoff — `rignexo/firmware`

All agentic development methodology documentation in this firmware tree is
maintained in English.

Before starting any task in this file, run the **Role Boundary Check**:

- Read `ROLE.md` in this folder, `../../AGENTS.md`, and `../../docs/methodology.md`.
- Confirm the request is **architecture and documentation**, not firmware
  implementation, builds, or formal validation.
- If the task includes code, tests, or flash steps, stop and tell the human that
  the implementer or tester must execute those parts unless explicitly
  redirected.

## Current Status

Active / recent handoffs:

- **`RGNX_FW_CAT_IF_HAMLIB_V1`** — docs complete; implementer **pending**
  (WSJT-X / Hamlib VFO None FAIL).
- `RGNX_FW_CAT_STATE_RAM_V1` — implemented; Linux suite PASS; app CAT FAIL
  (superseded by IF-Hamlib handoff).
- `RGNX_FW_CAT_PTT_V1` (**docs + code**; base CAT PTT OK)

## RGNX_FW_CAT_IF_HAMLIB_V1

```text
Role boundary check: architect docs only; ZERO CODE LAW.
ID: RGNX_FW_CAT_IF_HAMLIB_V1
Date: 2026-11-20
Source: tester return — WSJT-X Test CAT FAIL (below)
Objective:
  Normative Hamlib-compatible IF; frame (length 38 + VFO byte) so
  current_vfo resolves; pending implementer in cat_kenwood reply_if.
Authorized: docs/cat_ptt_mode_architecture.md § IF frame, decisions D20,
  implementer handoff pending, test_strategy criterion 10.
Constraints: no src/; no change to PTT/RTS rules; keep soft-state set=get.
Status: docs complete; implementer pending 2026-11-20
```

## RETURN FROM TESTER — WSJT-X `unsupported VFO None` (2026-11-20)

```text
From: tester (human Mac WSJT-X + Hamlib 4.7.1; device COMPOSITE_CAT)
To: architect
Action: CLOSED for docs as RGNX_FW_CAT_IF_HAMLIB_V1 (D20)
```

### Confirmed host settings (not misconfig)

- Rig: Kenwood TS-2000
- Serial Port: `/dev/cu.usbmodem104` (correct; not Bluetooth)
- Baud 9600, Handshake None, PTT Method CAT
- Test CAT → Rig failure

### Hamlib log (smoking gun)

- `rig_get_freq` **succeeds**: `freqMainA=14074000`, `modeMainA=USB`, `widthMainA=2200`
- Cache: `vfo=VFOA, current_vfo=None`
- Then: `kenwood_set_freq: unsupported VFO None` → Invalid parameter
  *while setting frequency*

### Architect diagnosis (D20)

Hamlib TS-2000 `get_vfo = kenwood_get_vfo_if` needs a **37-char** `IF` payload
after stripping `;` (wire **38**). Soft-state `IF` was **37 including `;`**
→ length reject → `current_vfo` never set → WSJT-X `set_freq(None)` fails.
Also lock `info[30]` from `fr_vfo` (cold-start `0` = VFO A).

Linux scripted CAT PASS does **not** cover this path.

Normative: `docs/cat_ptt_mode_architecture.md` § IF frame.
Implementer: `../implementer/handoff.md` `RGNX_FW_CAT_IF_HAMLIB_V1`.

Evidence: `../tester/handoff.md`; human screenshot 2026-11-20.

## RGNX_FW_CAT_STATE_RAM_V1

```text
Role boundary check: architect docs only; ZERO CODE LAW.
ID: RGNX_FW_CAT_STATE_RAM_V1
Date: 2026-11-20
Source:
  Human + tester: CAT mode works partially; apps set frequency **and** other
  knobs (e.g. filter width) but device answers fixed stubs. Want set values
  held in RAM so get/IF match.
Objective:
  Normative soft-radio RAM state for FA/FB/MD/FW/SL/SH/PC/FR/FT + IF;
  pending implementer.
Authorized: docs/cat_ptt_mode_architecture.md, decisions, implementer handoff
  pending, test_strategy acceptance bullets.
Constraints: no src/; no flash VFO; no real RF.
Status: docs complete; implementer pending 2026-11-20
```
- `RGNX_FW_MODE_SWITCH_POLARITY` — H1 / D16; implementer reported implemented
  — tester re-validate as needed
- LED/GPIO0 (D15 Option A): docs closed; implementer LED fix done
- `RGNX_FW_PINMAP_V1` / `RGNX_FW_ASBUILT_V1` (closed / complete)

## RGNX_FW_CAT_PTT_V1

```text
Role boundary check:
  Architect docs only. ZERO CODE LAW — no src/ edits.
ID: RGNX_FW_CAT_PTT_V1
Date: 2026-11-20
Source:
  Human: third mode; PTT via CAT for apps that do not use RTS; same as
  sound+serial plus command decoder; survey radio CAT options; implement
  TX on/off for real; stub other commands so hosts do not crash.
Objective:
  (1) Survey CAT options; choose Kenwood ASCII TS-2000-class for v1.
  (2) Normative docs/cat_ptt_mode_architecture.md.
  (3) Extend device_modes / architecture for mode 3 + 3-pulse LED.
  (4) Pending implementer phases (enum/LED → parser/PTT → stubs).
Constraints:
  Architect MUST NOT edit src/.
  No CI-V / Yaesu as primary profile in v1.
Acceptance (architect):
  - Options surveyed; Kenwood chosen with rationale (D18).
  - Mode 3 USB/PTT/LED/stubs specified.
  - Implementer pending list concrete.
Status: docs complete; implementer pending 2026-11-20
```

## RETURN FROM TESTER — MODE_SWITCH polarity (2026-11-19)

```text
From: tester (mode toggles on USB reconnect without button)
Architect + human: **H1** — match firmware to board (active high + pull-down)
Status: docs + code aligned 2026-11-19 (D16); tester should re-validate
```

Idle was low with pull-down hardware while firmware expected active-low → every
boot looked “held”. Fix: `ACTIVE_LEVEL = 1`, `gpio_pull_down`, docs updated.

Evidence: `../tester/handoff.md`, `RGNX_FW_MODE_SWITCH_POLARITY`.

---

## RETURN FROM TESTER — `RGNX_FW_DEVICE_MODES_V1` LED/GPIO0 (2026-11-19)

```text
From: tester (MODE_LED FAIL — GPIO0 vs UART0 TX)
Architect response: CLOSED for docs 2026-11-19 — Option A chosen
Next: implementer pending, then tester re-flash / observe LED
```

### Failure (summary)

`board_init()` then `mode_led_pio_start()` then `stdio_init_all()` remuxed
GPIO0 back to UART TX → PIO pattern invisible on MODE_LED.

### Architect decision (Option A)

- Keep `MODE_LED` on GPIO **0**.
- **Forbid** `stdio_init_all()` and any UART0 TX/RX on pins **0/1** after LED
  start (and do not rely on UART console in v1).
- Do **not** move MODE_LED; do **not** require remapping UART to other pins.
- Clarify UX: mostly-ON with 1 vs 2 short OFF dips — not 50% blink.
- WS2812 GPIO16 / PTT LED GPIO27 are not MODE_LED.

Normative rewrite: `docs/device_modes_architecture.md` § Mode LED.
Decision record: `decisions.md` D15.
Implementer task: `../implementer/handoff.md` (remove/guard stdio; keep GPIO0
on PIO for life of run).

### Evidence pointers

- `../tester/handoff.md`, `../tester/test_runs.md` (FAIL LED)

---

## RGNX_FW_DEVICE_MODES_V1

```text
Role boundary check:
  Architect-owned documentation only. No src/, scripts/, cmake/ninja/build.
ID: RGNX_FW_DEVICE_MODES_V1
Date: 2026-11-19
Source:
  - Human (architect): dual modes — current composite sound+serial vs
    audio-only with VOX PTT; mode pushbutton + mode LED; permanent config;
    change only at power-up while button held; disconnect required (no
    runtime USB re-enumeration) for OS compatibility without serial.
Objective:
  (1) Normative docs/device_modes_architecture.md.
  (2) Cross-link architecture.md + TECHNICAL_DEFINITIONS (mode pins active,
      polarities).
  (3) Pending implementer handoff with phased tasks and tester acceptance.
Reason:
  Audio-only personality for hosts that fail composite CDC; keep USB
  descriptor choice fixed for the whole attach session.
Authorized files (architect):
  - docs/**
  - TECHNICAL_DEFINITIONS.md
  - agent-workspaces/architect/**
  - agent-workspaces/implementer/handoff.md (pending only)
  - docs/test_strategy.md (acceptance notes)
Module boundaries and contracts:
  - See docs/device_modes_architecture.md (COMPOSITE vs AUDIO_VOX,
    boot sequence, flash persist, dual descriptors, VOX on speaker path,
    MODE_SWITCH=15 active low, MODE_LED=0 active high).
Constraints:
  - No runtime mode change; no runtime USB re-init for mode switch.
  - Architect MUST NOT edit src/.
Non-goals:
  - Implementing flash/USB/VOX code in this session.
  - More than two modes; VOX on ADC RX; host-tuned thresholds.
Acceptance criteria (architect slice):
  - Normative mode doc exists and is linked from architecture.md.
  - Pin table lists mode switch/LED as active with polarities.
  - Implementer pending lists phases 1–3 and tester criteria.
Status: **RETURNED TO ARCHITECT** 2026-11-19 (tester LED FAIL — GPIO0 vs UART).
  Prior: docs complete; implementer implemented; compile PASS; hardware LED FAIL.
  Block implementer rework until architect chooses option A/B/C above.
Amendment 2026-11-19 (human):
  - Anti-glitch / at most one flash write per boot on mode button.
  - MODE_LED: always mostly on; mode1 = 1 off pulse; mode2 = 2 off pulses;
    cyclic forever; prefer PIO so CPU/USB never blocked by blinking.
  - Unknown stored mode: define safe behavior (COMPOSITE + heal).
  Updated docs/device_modes_architecture.md + decisions D12–D14.
```

## RGNX_FW_PINMAP_V1

```text
Role boundary check:
  Architect-owned documentation only. No src/ edits. Pending implementer
  task for board_config.h reserved defines only.
ID: RGNX_FW_PINMAP_V1
Date: 2026-11-19
Source:
  - Human (architect role): clarify primary board RP2350-Zero with pin map
    audio in 28, PTT LED 27, mode switch 15, PTT relay 14, mode LED 0,
    PWM audio out 2; mode switch/LED reserved for later (written only);
    also build RP2040-Zero with same pin map for backward compatibility.
Objective:
  (1) Make RP2350-Zero the documented primary target with the pin table above.
  (2) State shared pin map with RP2040-Zero / CHIP=rp2040 builds.
  (3) Reserve MODE_SWITCH=15 and MODE_LED=0 with no runtime behavior yet.
  (4) Align TECHNICAL_DEFINITIONS, architecture, README, BUILD, test_strategy.
  (5) Open a narrow implementer task for board_config.h comment + defines.
Reason:
  Remove prior doc drift (old 26/14/15/29 table) and lock product wiring.
Authorized files (architect):
  - docs/**
  - TECHNICAL_DEFINITIONS.md, README.md, BUILD.md
  - agent-workspaces/architect/**
  - agent-workspaces/implementer/handoff.md (pending only)
Module boundaries and contracts:
  - Active: RADIO_AUDIO_IN=28, RADIO_AUDIO_OUT=2, RADIO_PTT_OUT=14,
    TX_STATUS_INDICATOR=27.
  - Reserved: MODE_SWITCH=15, MODE_LED=0 (defines only; no init/read/drive).
  - One shared board_config.h for both chips; no per-chip pin #ifdefs.
Constraints:
  - Architect MUST NOT edit src/.
  - Mode feature behavior is out of scope.
Non-goals:
  - Implementing mode switch/LED logic.
  - Changing USB/audio/PTT runtime logic (active pins already match header).
Acceptance criteria (architect slice):
  - Docs name RP2350-Zero primary and RP2040-Zero same-map compatibility.
  - Pin tables updated everywhere listed above.
  - Implementer pending task lists exact defines to add.
Open questions:
  - Mode switch/LED polarity and UX (future handoff).
Status: docs complete; implementer pending 2026-11-19
```

## RGNX_FW_ASBUILT_V1

```text
Role boundary check:
  Architect-owned documentation only. No src/, CMakeLists.txt, scripts/,
  cmake/ninja/build/flash. Docs-only capture of existing firmware.
ID: RGNX_FW_ASBUILT_V1
Date: 2026-11-19
Source:
  - Human request: transform firmware/ into an agentic project mirroring
    misistemhil b02_hil/firmware (architect / implementer / tester + handoffs);
    architect reviews current code and writes the architecture that exists
    (stack, base example, modules) as if those were the instructions that led
    to the present tree.
Objective:
  (1) Scaffold AGENTS.md, docs/methodology.md, docs/architect_role_hard_stop.md,
      docs/test_strategy.md, and agent-workspaces for architect/implementer/tester.
  (2) Write docs/architecture.md as as-built contracts for the current code.
  (3) Record decisions and close this handoff without opening implementer work.
  (4) Point README.md at AGENTS.md.
Reason:
  Establish role boundaries and a reverse-engineered architecture baseline
  before further feature iterations.
Authorized files (architect):
  - AGENTS.md
  - docs/**
  - agent-workspaces/**
  - README.md (short agentic pointer only)
Expected changes (done by architect):
  - Create agentic tree adapted from b02_hil pattern to Pico/TinyUSB paths.
  - Document stack: Pico SDK + TinyUSB cdc_uac2, pinned TinyUSB ref, CHIP
    build dirs, module map, USB interfaces, pin contract, non-goals.
  - Explicitly record board_config.h vs TECHNICAL_DEFINITIONS.md pin drift.
Module boundaries and contracts:
  - See docs/architecture.md (normative as-built).
  - Pin runtime truth: src/board_config.h
  - Product vocabulary: TECHNICAL_DEFINITIONS.md
  - Build/host: BUILD.md
Constraints:
  - English for agentic methodology docs.
  - Scope firmware/ only; do not modify ../hardware/.
  - No source edits; no compile/flash.
Non-goals:
  - No new firmware features.
  - No full rewrite of TECHNICAL_DEFINITIONS.md.
  - No ESP-IDF/HIL content port from b02 beyond the workflow pattern.
Acceptance criteria:
  - AGENTS.md and three ROLE.md guides exist with Pico-adapted ownership.
  - docs/architecture.md describes stack, layers, modules, USB, pins, build.
  - Pin drift is documented as an open question.
  - implementer/handoff.md has no fake "implemented" firmware tasks.
  - Architect task closes on docs alone.
Open questions:
  - Reconcile board_config.h (GPIO 28/2/14/27) with documented RP2350 map
    (26/14/15/29) in a future handoff.
Status: CLOSED (docs complete) 2026-11-19
```
