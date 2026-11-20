# Test runs — `rignexo/firmware`

Record each validation run below.

## Template

```text
ID: <handoff id>
Date:
CHIP / board:
Pico SDK / TinyUSB refs:
Commands:
Hardware:
Result: PASS | FAIL | BLOCKED
Evidence:
Human observations:
Technical conclusion:
```

## Runs

```text
ID: RGNX_FW_CAT_IF_HAMLIB_V1
Date: 2026-11-20
CHIP / board: rp2350; COMPOSITE_CAT; Mac + Linux
Commands: build PASS; human WSJT-X Test CAT after D20 IF fix
Result: PASS
Evidence:
  - Two WSJT-X versions: Test CAT OK (TS-2000)
  - PTT via CAT on/off OK
  - Prior unsupported VFO None cleared by 38-byte IF
Human observations: works with two WSJT-X versions; emulator + CAT PTT OK
Technical conclusion: Hamlib/WSJT-X CAT path accepted for COMPOSITE_CAT.
```

```text
ID: RGNX_FW_CAT_STATE_RAM_V1
Date: 2026-11-20
CHIP / board: rp2350; COMPOSITE_CAT; …-if03
Commands: full Kenwood host suite after human flash
Result: PASS
Evidence:
  - Set→get FA/FB/MD/FW/SL/SH/PC/FR/FT/AI/RG/AG/SQ (silent set ack)
  - IF tracks FA+MD+PTT; TX/TX0/TX1/RX OK
  - Malformed/unknown → ?;
  - RTS ignored; AG050→?; but AG100/AG0050/AG0100 OK
Human observations: n/a this run
Technical conclusion: Soft-radio RAM state behaves as D19 / architecture.
```

```text
ID: RGNX_FW_CAT_PTT_V1
Date: 2026-11-20
CHIP / board: rp2350 / waveshare_rp2350_zero
Commands:
  ./scripts/build.sh → PASS
  Host CAT over …-if03 / ttyACM0 → PASS
Hardware: human-flashed; mode COMPOSITE_CAT
Result: PASS
Evidence:
  - ID019; FA/FB stubs; MD2; AI0; IF shape; ZZ→?;
  - TX1;/TX; set IF TX bit; RX; clears; no CDC reply on TX/RX
  - RTS assert does not set IF TX bit (CAT-only PTT)
  - RTS-only visual: human confirms relay+PTT LED stayed off
Human observations: device present as RIGNEXO interface if03; RTS no relay
Technical conclusion: Kenwood CAT dialect responds; PTT via CAT only; RTS ignored.
```

```text
ID: RGNX_FW_MODE_SWITCH_POLARITY_H1_BUILD
Date: 2026-11-19
CHIP / board: rp2350 / waveshare_rp2350_zero
Commands: ./scripts/build.sh → PASS
Hardware: none yet (compile after implementer H1)
Result: PASS (compile)
Evidence:
  - BOARD_MODE_SWITCH_ACTIVE_LEVEL=1; gpio_pull_down in mode_boot_resolve
  - UF2 build/rp2350/rpi_pico_composite_sound_serial.uf2
Human observations: pending flash — replug without button must keep mode
Technical conclusion: H1 polarity sources build clean.
```

```text
ID: RGNX_FW_MODE_SWITCH_POLARITY
Date: 2026-11-19
CHIP / board: rp2350 + rignexo-mainboard
Commands: code review mode_store.c + mainboard schematic (no new build)
Hardware: human report — mode changes on USB replug without button
Result: FAIL (false mode advance)
Evidence:
  - mode_boot_resolve advances when mode_switch_held_for_window() true
  - Firmware: ACTIVE_LEVEL=0 + gpio_pull_up (active low)
  - Schematic: +3V3-SW1-MODE_SWITCH-R13-GND (idle low = active-high hardware)
  - Idle low ⇒ every USB reset looks like “button held” ⇒ toggle+flash write
Human observations: mode changes on disconnect/reconnect without press
Technical conclusion: polarity mismatch firmware/docs vs mainboard; return architect H1/H2.
```

```text
ID: RGNX_FW_DEVICE_MODES_V1_LED_D15
Date: 2026-11-19
CHIP / board: rp2350 / waveshare_rp2350_zero
Pico SDK / TinyUSB refs: PICO_SDK_DIR=/home/libertad/pico/pico-sdk; TinyUSB 375e20090
Commands:
  ./scripts/build.sh  → PASS
Hardware: none yet (compile only after D15 LED fix)
Result: PASS (compile)
Evidence:
  - Relinked main.c + mode_led_pio.c only
  - main: mode_led_pio_start present; stdio_init_all not called from main
  - UF2: build/rp2350/rpi_pico_composite_sound_serial.uf2
Human observations: pending re-flash
Technical conclusion: LED-fix sources build clean under Option A.
```

```text
ID: RGNX_FW_DEVICE_MODES_V1
Date: 2026-11-19
CHIP / board: rp2350 / waveshare_rp2350_zero
Pico SDK / TinyUSB refs: PICO_SDK_DIR=/home/libertad/pico/pico-sdk; TinyUSB 375e20090
Commands:
  ./scripts/build.sh  → PASS
  Human flash → MODE_LED observation
Hardware: human-flashed board; MODE_LED on GPIO0
Result: FAIL (LED pattern)
Evidence:
  - Compile OK; mode_led.pio.h + mode_store/mode_led_pio/vox linked
  - ELF call order: board_init → mode_led_pio_start → … → stdio_init_all
  - board_init and stdio_init_all both route UART0 TX to GPIO0
    (PICO_DEFAULT_UART_TX_PIN=0 on waveshare_*_zero)
  - MODE_LED also mapped to GPIO0 → UART reclaim kills PIO drive
Human observations: LED does not blink/pulse mode pattern after plug-in
Technical conclusion: Pin mux conflict GPIO0 (MODE_LED vs UART TX), not missing PIO code.
Next: implementer fix (disable/move UART stdio or move MODE_LED per architect).
```

```text
ID: BUILD_RP2350_2026-11-19
Date: 2026-11-19
CHIP / board: rp2350 / waveshare_rp2350_zero
Pico SDK / TinyUSB refs: PICO_SDK_DIR=/home/libertad/pico/pico-sdk; TinyUSB 375e20090
Commands:
  1) ./scripts/build.sh  → FAIL (stale CMakeCache: build/ vs build/rp2350/)
  2) rm -rf build/rp2350 && ./scripts/build.sh  → PASS
Hardware: none (compile only)
Result: PASS
Evidence:
  - build/rp2350/rpi_pico_composite_sound_serial.uf2 (47K)
  - build/rp2350/rpi_pico_composite_sound_serial.elf (573K)
  - arm-none-eabi-size: text=27644 data=0 bss=22780 total=50424
Human observations: n/a
Technical conclusion: Clean configure+link OK for default CHIP=rp2350.
```

