# Tester procedures — `rignexo/firmware`

Reusable host/hardware procedures. Expand when validation handoffs authorize
updates. See also root `BUILD.md`.

## Build (default RP2350)

```bash
./scripts/build.sh
```

Artefact: `build/rp2350/rpi_pico_composite_sound_serial.uf2`

## Build RP2040

```bash
CHIP=rp2040 ./scripts/build.sh
```

## Flash

```bash
./scripts/flash.sh
# or
CHIP=rp2040 ./scripts/flash.sh
```

## Host serial checks

```bash
./scripts/check_serial.sh
./scripts/test_rts.sh
```

CDC data interface appears as `…-if03` under `/dev/serial/by-id/`.
