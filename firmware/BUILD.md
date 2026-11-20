# Build and Host Validation

## Build

The firmware uses the **TinyUSB `cdc_uac2` build system** (`family_support.cmake`) with the Pico SDK. On first build, TinyUSB is cloned into `firmware/external/tinyusb` at a **pinned commit** (not floating `master`). Run commands from this `firmware/` directory.

```bash
./scripts/build.sh
```

Default paths:

- Pico SDK: `/home/libertad/pico/pico-sdk`
- TinyUSB: `external/tinyusb` (auto-cloned under `firmware/`)
- TinyUSB git ref: `375e20090a0a60fd2d47ba6553d189e05482dfea`
- Chip: `rp2350` (board `waveshare_rp2350_zero`)
- Build dir: `build/rp2350`

Overrides:

```bash
PICO_SDK_DIR=/path/pico-sdk TINYUSB_ROOT=/path/tinyusb TINYUSB_GIT_REF=<sha> ./scripts/build.sh
CHIP=rp2040 ./scripts/build.sh
```

Output (default chip):

```text
build/rp2350/rpi_pico_composite_sound_serial.uf2
build/rp2350/rpi_pico_composite_sound_serial.elf
```

For RP2040 (`CHIP=rp2040`), artefacts go under `build/rp2040/` with board `waveshare_rp2040_zero`.

## Flash

```bash
./scripts/flash.sh
CHIP=rp2040 ./scripts/flash.sh
```

Uses `picotool` when available, otherwise copies the UF2 to a mounted `RP2350` / `RPI-RP2` drive.

## Firmware Behavior

Composite USB device (official TinyUSB `cdc_uac2` + project GPIO/audio).
Primary board **RP2350-Zero**; **RP2040-Zero** uses the same pin map.

- **CDC serial** (`…-if03`): echo; RTS → GPIO **14** (PTT relay), GPIO **27** (PTT LED).
- **UAC2 speaker**: host playback → PWM GPIO **2** (48 kHz timer).
- **UAC2 microphone**: GPIO **28** ADC → host capture (48 kHz; needs passive bias/network on the pin).
- **Reserved (no runtime yet):** mode switch GPIO **15**, mode LED GPIO **0**.

Pin map: `src/board_config.h` / `TECHNICAL_DEFINITIONS.md`

## Host: serial port

The CDC data interface is **`if03`** in `/dev/serial/by-id/` (e.g. `usb-TinyUSB_TinyUSB_headset_*-if03`).

```bash
./scripts/check_serial.sh
./scripts/test_rts.sh
```

## Host: audio

- Playback: select **TinyUSB headset** → PWM on GPIO **2**.
- Capture: select the same device's microphone input; audio comes from GPIO **28** ADC.
