# RP2350 Composite Sound and Serial Interface

## Agentic workflow

Firmware work is split across three roles (architect, implementer, tester) with
handoffs under `agent-workspaces/`. Start at [AGENTS.md](AGENTS.md). As-built
architecture: [docs/architecture.md](docs/architecture.md). Methodology:
[docs/methodology.md](docs/methodology.md).

## Project Intent

This project aims to build a small mixed-function interface between a computer and an HF radio for digital communication modes such as FT8. The target device should appear to the computer as the practical equivalent of a simple sound interface plus a serial/control interface, allowing suitable radio software to exchange audio and key the transmitter.

The hardware target is a Raspberry Pi Pico-class board based on the RP2350, specifically an RP2530-Zero-style module exposing a limited set of GPIO pins. The design should avoid extra active components where possible and rely on the microcontroller peripherals for the first prototype.

## Intended Functions

### Simple Audio Interface

The audio portion is intended to be minimal and may be monophonic.

- Radio-to-computer audio input should use an RP2350 ADC input.
- Computer-to-radio audio output should use PWM from an RP2350 GPIO.
- Any external analog conditioning should be kept passive if practical.
- Signal levels, filtering, DC blocking, and radio protection still need to be defined before connecting to real equipment.

This is not intended to be a high-fidelity audio design. The primary goal is to provide audio quality sufficient for narrow-band digital modes such as FT8.

### Serial and PTT Control

The serial/control portion should provide a way for host software to control the radio transmit state.

- The RTS line must be mapped to a physical GPIO.
- That GPIO is expected to drive the radio PTT control path, subject to the required electrical interface.
- RTS maps to PTT relay GPIO **14**; PTT LED GPIO **27**; speaker PWM GPIO **2**; audio ADC GPIO **28** (`board_config.h`).
- Mode switch GPIO **15** and mode LED GPIO **0** (PIO: mostly on, 1 vs 2 off
  pulses): boot-time persistent modes (`COMPOSITE` vs `AUDIO_VOX`) — see
  [docs/device_modes_architecture.md](docs/device_modes_architecture.md).

The firmware implements composite CDC + UAC2 (see [BUILD.md](BUILD.md)).

## Target Board and Available GPIO

**Primary board:** Waveshare **RP2350-Zero**. **Also built for** Waveshare **RP2040-Zero** with the **same pin map** (backward compatible).

Representative header GPIOs:

```text
29, 28, 27, 26, 15, 14, 8, 7, 6, 5, 4, 3, 2, 1, 0
```

Assigned now: **28** audio in, **2** PWM out, **14** PTT relay, **27** PTT LED. Reserved later: **15** mode switch, **0** mode LED.

Important allocation constraints:

- The ADC input must use a GPIO that supports ADC functionality on the selected package and board routing (GPIO **28** = ADC2 on this map).
- The PWM audio output can use a GPIO with a suitable PWM slice/channel.
- The RTS/PTT signal should use a GPIO that is easy to route and safe to dedicate to control.
- Pins used by board-specific boot, USB, flash, or debug functions should be avoided unless the board documentation confirms they are available.

## Software and Build Approach

Firmware development is expected to use the Raspberry Pi Pico SDK. The SDK and toolchain may already be installed on the development computer, but the project should not require globally activating SDK environment variables.

Instead of assuming environment variables such as `PICO_SDK_PATH` are already exported in the shell, build instructions should prefer explicit paths to the needed tools and SDK directories. For example, future documentation or helper scripts should discover or document full paths for commands such as:

```text
/path/to/cmake
/path/to/ninja
/path/to/arm-none-eabi-gcc
/path/to/pico-sdk
```

The intent is to make builds reproducible from a normal shell session without modifying the user's global environment.

See [BUILD.md](BUILD.md) for build, flash, and host validation steps.

## Initial Technical Direction

The firmware will likely need to combine these areas:

- USB device support, probably through TinyUSB as integrated by the Raspberry Pi Pico SDK.
- A USB audio function or equivalent audio transport for host radio software.
- A USB serial/control function where RTS state changes can be observed by firmware.
- ADC sampling for received audio.
- PWM generation for transmitted audio.
- GPIO output for RTS/PTT control.

The first milestone should be a minimal firmware that enumerates reliably over USB and exposes enough functionality to validate host-side compatibility. Audio fidelity and radio-specific electrical details can then be refined through testing.

## Current Firmware Stage

Composite USB device (RP2350-Zero primary; RP2040-Zero same pins):

- UAC2 mono speaker (48 kHz) → PWM on GPIO **2**.
- UAC2 mono microphone (48 kHz) ← ADC on GPIO **28**.
- CDC serial: RTS → PTT relay GPIO **14**, PTT LED GPIO **27**.

## Open Decisions

The following items are intentionally left undecided at this stage:

- Passive input/output network values for audio coupling and level control.
- Whether PTT requires isolation or level shifting for the target HF radio.
- Final VOX threshold / hang-time after bench tuning.
- Exact SDK, CMake, compiler, and build tool paths for the development computer.

## Non-Goals for the First Prototype

- No extra active analog circuitry unless testing proves it is required.
- No stereo audio requirement.
- No finished enclosure or production PCB requirement.
- No assumption that SDK environment variables are globally configured.
