# Technical Definitions

## Purpose

This document defines the technical vocabulary, boundaries, and implementation-facing requirements for the RP2350 composite sound and serial interface project. It is intended to guide later firmware and hardware decisions without forcing a single implementation before the required tests and board checks are complete.

The project target is a USB-connected interface for HF digital communication software, especially narrow-band modes such as FT8. The device bridges three domains:

- Host computer USB audio/control.
- RP2350 firmware and peripherals.
- HF radio audio and transmit-control signals.

## System Boundary

The project covers the firmware and minimal external passive interface needed to connect an RP2350-based RP2530-Zero-style board to an HF radio.

The project does not define a complete radio adapter product at this stage. Enclosure design, production PCB layout, radio-specific cable wiring, and any active analog or isolation circuitry are outside the first implementation scope unless testing shows they are required for safe operation.

## Hardware Platform Definition

### Target MCU

- MCU family: Raspberry Pi RP2350.
- SDK: Raspberry Pi Pico SDK.
- USB stack: Pico SDK-supported USB stack, likely TinyUSB.
- Required peripherals:
  - USB device.
  - ADC.
  - PWM.
  - GPIO output.

### Target Board

**Primary target:** Waveshare **RP2350-Zero** (`waveshare_rp2350_zero`).

**Backward-compatible build:** Waveshare **RP2040-Zero** (`waveshare_rp2040_zero`) uses the **same GPIO map**. Firmware MUST keep one shared pin map in `src/board_config.h` and continue to build both chips (`CHIP=rp2350` default, `CHIP=rp2040` for compatibility).

Header-exposed GPIOs on these Zero modules (representative set):

```text
29, 28, 27, 26, 15, 14, 8, 7, 6, 5, 4, 3, 2, 1, 0
```

The firmware must keep board pin mapping centralized in `src/board_config.h`.

## Current GPIO Pin Map

**Keep this table in sync with `src/board_config.h`.** When GPIO assignments change in firmware, update this section in the same commit.

Board profile: **RP2350-Zero (primary)** / **RP2040-Zero (same map, backward compatible)**

| Logical signal | GPIO | Peripheral | Active level | Status | Firmware define |
| --- | --- | --- | --- | --- | --- |
| `RADIO_AUDIO_IN` | **28** | ADC channel 2 | — | Active | `BOARD_RADIO_AUDIO_IN_GPIO` |
| `RADIO_AUDIO_OUT` | **2** | PWM | — | Active | `BOARD_RADIO_AUDIO_OUT_GPIO` |
| `RADIO_PTT_OUT` | **14** | GPIO output (PTT relay) | Active high | Active | `BOARD_RADIO_PTT_OUT_GPIO` |
| `TX_STATUS_INDICATOR` | **27** | GPIO output (PTT LED) | Active high | Active | `BOARD_TX_INDICATOR_GPIO` |
| `MODE_SWITCH` | **15** | GPIO input (boot mode select) | Active high | Active | `BOARD_MODE_SWITCH_GPIO` |
| `MODE_LED` | **0** | PIO-driven output (mode indicator) | Active high | Active | `BOARD_MODE_LED_GPIO` |

Related constants (also in `board_config.h`):

| Parameter | Value |
| --- | --- |
| ADC input index | 2 (`BOARD_ADC_INPUT_INDEX`, GPIO28 = ADC2) |
| Audio sample rate | 48000 Hz (`BOARD_AUDIO_SAMPLE_RATE_HZ`) |
| Audio sample width | 16 bit (`BOARD_AUDIO_BITS`) |
| PTT active level | 1 = high (`BOARD_PTT_ACTIVE_LEVEL`) |
| PTT USB line | RTS if `BOARD_PTT_USE_RTS_LINE` = 1, else DTR |
| TX indicator active level | 1 = high (`BOARD_TX_INDICATOR_ACTIVE_LEVEL`) |
| PWM wrap | 255 (`BOARD_PWM_WRAP`) |
| PWM clock divider | 4.0 (`BOARD_PWM_CLKDIV`) |
| Mode switch active level | 1 = high (`BOARD_MODE_SWITCH_ACTIVE_LEVEL`) |
| Mode LED active level | 1 = high (`BOARD_MODE_LED_ACTIVE_LEVEL`) |

Pin notes:

- GPIO **28**, **2**, **14**: radio audio in/out and PTT relay — wire to the transceiver interface.
- GPIO **27**: PTT / TX status LED — external LED (GPIO → resistor → LED → GND). Follows the same state as PTT (high = transmitting).
- GPIO **15** (`MODE_SWITCH`): pushbutton to **+3.3V** with pull-down to GND (pressed = high); **held at power-up / USB attach** advances the persistent device mode (cyclic). Ignored while running. See `docs/device_modes_architecture.md`.
- GPIO **0** (`MODE_LED`): exclusive owner after boot LED start (PIO). Pattern = mostly ON with **1** short OFF (`COMPOSITE`) or **2** short OFFs (`AUDIO_VOX`) per ~1 s cycle — not a 50% blink. **MUST NOT** use Waveshare default UART0 TX on GPIO 0 / `stdio_init_all()` that remuxes pin 0. On-module WS2812 (GPIO 16) is not MODE_LED. See `docs/device_modes_architecture.md`.

Free header GPIOs (not assigned): **29, 26, 8, 7, 6, 5, 4, 3, 1**.

## Current Firmware Stage

Firmware is based on the official TinyUSB **`cdc_uac2`** example, built with TinyUSB `family_support.cmake` + Pico SDK (`BOARD=pico_sdk`, `PICO_BOARD=waveshare_rp2350_zero`).

USB layout (official example):

- USB audio interfaces **0–2** (control, speaker streaming, microphone streaming).
- USB CDC interfaces **3–4** (comm, data); host CDC port is **`…-if03`**.
- USB IDs: VID **`0xCafe`**, product **TinyUSB headset** (official example descriptors).

Working behavior (validated on hardware):

- USB CDC serial echo (no `tud_cdc_connected()` gate).
- RTS → GPIO **14** (PTT relay) and GPIO **27** (PTT LED) via `cdc_line_state.c`.
- USB speaker audio → GPIO **2** PWM (`audio_pwm.c`); USB microphone ← GPIO **28** ADC (`audio_adc.c`).

Source layout: `src/` (application), `scripts/` (build/flash/host checks), TinyUSB at `external/tinyusb` (cloned by `scripts/build.sh` under `firmware/`).

## Implementation Stages

The project should advance one independently testable stage at a time:

| Stage | Firmware surface | Hardware pins active | Validation target |
| --- | --- | --- | --- |
| 1 | USB CDC only | GPIO 14 PTT relay, GPIO 27 PTT LED | Host can open CDC, toggle RTS, and observe both GPIOs |
| 2 | CDC + PWM transmit-audio hardware bring-up | GPIO 2 PWM, GPIO 14, GPIO 27 | GPIO 2 produces a measurable PWM waveform without adding USB Audio yet |
| 3 | CDC + ADC receive-audio hardware bring-up | GPIO 28 ADC, GPIO 14, GPIO 27 | Firmware can sample ADC without disturbing CDC/PTT |
| 4 | Composite USB CDC + one USB Audio direction | GPIOs from prior stages | Host enumeration remains stable after adding one audio function |
| 5 | Full composite USB CDC + bidirectional USB Audio | GPIO 28, 2, 14, 27 | Final FT8-oriented audio/PTT behavior |
| 6 | Device modes | GPIO 15, GPIO 0 + USB / VOX / CAT / KISS | Boot-time modes: `COMPOSITE`, `AUDIO_VOX`, `COMPOSITE_CAT`, `APRS_TNC` — see `docs/device_modes_architecture.md`, `docs/cat_ptt_mode_architecture.md`, `docs/aprs_tnc_mode_architecture.md` |

Do not move to the next stage until the current stage is flashed and validated on the host.

## Logical Signals

The implementation should use logical signal names internally. Physical GPIO assignments should be provided by a board configuration layer.

| Logical signal | Direction | Required function | Physical requirement |
| --- | --- | --- | --- |
| `RADIO_AUDIO_IN` | Radio to MCU | ADC sampling of received audio | Must map to an ADC-capable exposed GPIO |
| `RADIO_AUDIO_OUT` | MCU to radio | PWM-generated transmit audio | Must map to a PWM-capable exposed GPIO |
| `RADIO_PTT_OUT` | MCU to radio | Transmit control (PTT relay) derived from host RTS | Must map to an exposed GPIO output |
| `TX_STATUS_INDICATOR` | MCU to indicator | Visual transmit-state output (PTT LED) mirroring PTT | Must map to an exposed GPIO output |
| `MODE_SWITCH` | Human to MCU | Boot-time mode advance (hold on connect) | GPIO **15**, active high |
| `MODE_LED` | MCU to indicator | PIO pattern for persistent device mode | GPIO **0**, active high; 1–4 off pulses (modes 1–4) |

The names above describe signal intent. Pin numbers are listed in **Current GPIO Pin Map**.

## Transmit Status Indicator

The device provides a simple GPIO status output for local transmit-state visualization. It is not part of the radio interface and does not carry audio or PTT to the transceiver.

Implementation-facing requirements:

- The indicator must follow the same logical transmit state as `RADIO_PTT_OUT` (copies RTS/PTT).
- When host RTS is active and PTT is asserted, the indicator GPIO must be active.
- When RTS is inactive, on host disconnect, or after firmware failsafe handling, the indicator must be inactive.
- The indicator is a plain GPIO output on a header pin for an external LED.
- The indicator is updated together with PTT in the main control path.

Pin assignment: GPIO **27** — see **Current GPIO Pin Map**.

## Audio Definitions

### Receive Audio Path

Receive audio is the signal flowing from the HF radio into the RP2350 and then to the host computer.

Implementation-facing requirements:

- The MCU input must use the RP2350 ADC.
- The first implementation may be monophonic.
- The sampling path should be designed for voice-band or narrow-band digital-mode audio, not high-fidelity audio.
- ADC scaling, biasing, DC blocking, and passive filtering must be treated as hardware-interface decisions.
- Firmware should separate ADC acquisition from USB audio packet production so timing and buffering can be tested independently.

Open parameters:

- Passive input network values.

Pin assignment: see **Current GPIO Pin Map** (`RADIO_AUDIO_IN` → GPIO 28).

### Transmit Audio Path

Transmit audio is the signal flowing from the host computer into the RP2350 and then to the HF radio.

Implementation-facing requirements:

- The MCU output must be generated by PWM.
- The first implementation may be monophonic.
- PWM configuration must be selected to support acceptable reconstruction for narrow-band digital modes.
- Any output smoothing, attenuation, DC blocking, or level adjustment should be passive in the first design.
- Firmware should separate USB audio reception from PWM sample output so underruns and timing behavior can be measured.

Open parameters:

- Passive output network values.

Pin assignment: see **Current GPIO Pin Map** (`RADIO_AUDIO_OUT` → GPIO 2).

## Serial and PTT Definitions

### Host Control Interface

The host computer must have a way to expose an RTS state or equivalent control signal to the firmware. The expected implementation is a USB CDC-style serial interface, but this document does not require CDC if another host-compatible method is later selected.

Implementation-facing requirements:

- Firmware must observe the host RTS state or a clearly defined equivalent.
- RTS active state must be translated into the logical `RADIO_PTT_OUT` signal.
- The PTT GPIO default state after reset must be receive/inactive.
- Firmware must avoid unintended transmit assertion during boot, USB enumeration, reset, and host disconnect.

Open parameters:

- USB class used for host control.
- Whether a delay or debounce is required when changing PTT state.
- Whether the target radio requires isolation, open-drain behavior, or level shifting.

Pin assignment: see **Current GPIO Pin Map** (`RADIO_PTT_OUT` → GPIO 14, active high).

## USB Device Definition

The final desired host-facing behavior is a practical combination of:

- One audio function for receive/transmit audio.
- One serial/control function for RTS/PTT control.

The current firmware implements composite CDC + UAC2 with speaker PWM (GPIO **2**) and microphone ADC capture (GPIO **28**).

## Firmware Architecture Definitions

The later firmware should keep these responsibilities separate:

- Board configuration: maps logical signals to GPIO numbers and peripheral instances.
- USB device layer: handles enumeration and class-specific host communication.
- Audio input service: samples ADC and fills receive-audio buffers.
- Audio output service: consumes transmit-audio buffers and updates PWM.
- PTT control service: maps host RTS state to `RADIO_PTT_OUT` and `TX_STATUS_INDICATOR`.
- Timing and buffering: manages sample pacing, underrun/overrun tracking, and state transitions.

These are responsibility boundaries, not required source-file names.

## Build and Tooling Definitions

The project should use the Raspberry Pi Pico SDK without requiring globally activated environment variables.

Implementation-facing requirements:

- Build documentation and scripts must not assume the user has exported SDK variables in the current shell.
- Commands should use explicit paths to required tools when practical.
- SDK and tool discovery may inspect known install locations, but should not modify shell startup files or global configuration.
- The build process should remain reproducible from a normal terminal session.

Expected path-like inputs:

```text
PICO_SDK_DIR=/absolute/path/to/pico-sdk
CMAKE=/absolute/path/to/cmake
NINJA=/absolute/path/to/ninja
ARM_GCC=/absolute/path/to/arm-none-eabi-gcc
```

These names are documentation placeholders. They do not imply exported environment variables are required.

## Safety and Electrical Constraints

The first prototype should assume the HF radio interface is electrically sensitive until proven otherwise.

Implementation-facing requirements:

- The PTT output must power up inactive.
- GPIO voltage limits must not be exceeded.
- Radio audio input levels must be attenuated or biased as required before reaching the ADC.
- PWM output must not feed DC or excessive level into the radio audio input.
- Passive protection, coupling, and filtering should be defined before connecting to a real radio.

Any active isolation or buffering must be treated as a later design decision, not part of the initial no-extra-active-components goal.

## Initial Validation Targets

A later implementation can be considered minimally useful when it demonstrates:

- Stable USB enumeration on the host computer.
- Host-visible control interface with observable RTS changes.
- `RADIO_PTT_OUT` follows the intended RTS state and remains inactive when disconnected.
- `TX_STATUS_INDICATOR` follows the same transmit state as `RADIO_PTT_OUT`.

Stage 2 and later validation targets:

- ADC samples can be acquired from the selected input pin.
- PWM output can produce a measurable audio-frequency signal path.
- Build commands work with explicit tool and SDK paths from a clean shell.

## Pin Assignment Status

Active radio interface pins (**28**, **2**, **14**) and PTT LED (**27**) are assigned. Mode switch (**15**) and mode LED (**0**) are assigned for boot-time device modes (`docs/device_modes_architecture.md`). Before connecting to a real radio, still verify schematic, levels, and PTT polarity for your transceiver.

Remaining checks before hardware hookup:

- Exact RP2350-Zero / RP2040-Zero board schematic and pin routing (shared map).
- Radio audio input levels and passive networks for GPIO **28** (ADC2).
- PWM reconstruction and levels for GPIO **2** before the radio mic/acc input.
- PTT polarity and isolation for GPIO **14** (relay) and your radio.
- LED resistor value for GPIO **27** PTT indicator wiring.
- Whether any header GPIO has boot, debug, flash, or USB constraints.

## Terminology

- ADC: Analog-to-digital converter used for radio receive audio into the MCU.
- PWM: Pulse-width modulation used to synthesize radio transmit audio from the MCU.
- RTS: Request To Send, a serial-control line used by host software to request transmit.
- PTT: Push To Talk, the radio control input that switches the transmitter on.
- TX status indicator: A local GPIO output that shows when transmit/PTT is active.
- Host: The computer running digital-mode software.
- Radio: The HF transceiver connected to the interface.
- Logical signal: A firmware-level signal name independent of physical pin assignment.
