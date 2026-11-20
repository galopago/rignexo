# Test Strategy

Initial strategy for `rignexo/firmware`: build checks, host USB/serial scripts,
and hardware observation. Expanded per handoff.

All agentic development methodology documentation in this firmware tree is
maintained in English.

Baseline architecture: `docs/architecture.md`. Device modes:
`docs/device_modes_architecture.md` (`RGNX_FW_DEVICE_MODES_V1`). Host steps:
root `BUILD.md`.

## Levels

- Build: `./scripts/build.sh` (default `CHIP=rp2350` → `build/rp2350/`;
  `CHIP=rp2040` → `build/rp2040/`).
- Flash: `./scripts/flash.sh` / `picotool` / UF2 mount copy.
- Host serial: `./scripts/check_serial.sh`, `./scripts/test_rts.sh`.
- Host audio: select **TinyUSB headset** playback/capture in the OS mixer;
  observe PWM / ADC pins as required by the handoff.
- Hardware: human confirmation of GPIO levels (PTT, TX indicator) and passive
  audio network behavior.

## Test Record

Each run must be documented in `agent-workspaces/tester/test_runs.md` with:

- Handoff ID and change description.
- `CHIP` / `PICO_BOARD`, Pico SDK path, TinyUSB git ref.
- Commands executed.
- Connected hardware.
- Observed result.
- Relevant evidence or logs.

## Initial Criteria (as-built composite device)

- Project configures and links for the selected chip/board.
- UF2/ELF produced under `build/<chip>/`.
- After flash: USB enumerates as composite CDC + UAC2 (**TinyUSB headset**,
  VID `0xCafe`); CDC data port is `…-if03`.
- CDC echo responds without requiring `tud_cdc_connected()` gating.
- RTS (default) drives PTT + TX indicator per `src/board_config.h`.
- Speaker path can feed PWM; microphone path can sample ADC when the
  corresponding UAC2 interfaces are active.

Exact GPIO numbers for pass/fail must match **`src/board_config.h`** and
`TECHNICAL_DEFINITIONS.md` (shared RP2350-Zero / RP2040-Zero map): ADC **28**,
PWM **2**, PTT relay **14**, PTT LED **27**, mode switch **15**, mode LED **0**.

## Device modes criteria (`RGNX_FW_DEVICE_MODES_V1`)

When the modes implementation is under test:

1. Boot without button → last saved mode (or `COMPOSITE` if virgin/invalid);
   no spurious multi-writes on clean boots.
2. Hold mode button on USB attach → **exactly one** advance despite bounce/long
   hold; survives unplug/replug without button.
3. MODE_LED (GPIO0 only — not WS2812/PTT LED): steady ON with **1** short dark
   flash (`COMPOSITE`) or **2** (`AUDIO_VOX`) per ~1 s — not a 50% blink.
   Pattern still visible after full boot (no `stdio_init_all`/UART reclaim of
   GPIO0; architect D15 / Option A).
4. Unknown stored mode → boots as `COMPOSITE` and heals per architecture
   (single-write policy).
5. Mode LED via PIO does not disturb USB enumeration/audio.
6. `COMPOSITE`: CDC `…-if03` present; RTS keys PTT; VOX does not.
7. `AUDIO_VOX`: audio present, **no** CDC/COM for this device; TX audio keys
   PTT; silence/hang releases PTT.
8. Both `CHIP=rp2350` and `CHIP=rp2040` build.
