# Tester feedback — `rignexo/firmware`

## 2026-11-20 — WSJT-X Test CAT FAIL (`unsupported VFO None`)

Human Mac WSJT-X + Hamlib 4.7.1: Rig TS-2000, port `cu.usbmodem104`, PTT CAT.
Get freq OK; set freq → `kenwood_set_freq: unsupported VFO None` with
`current_vfo=None`. Linux scripted suite had PASS on soft state.

Architect: `RGNX_FW_CAT_IF_HAMLIB_V1` (D20) — fix `IF;` length (38 wire) +
VFO byte for `kenwood_get_vfo_if`. Implementer pending; re-test WSJT-X Test
CAT after flash.

## 2026-11-20 — CAT mode partial PASS → soft state needed

Human/tester: PTT via CAT and RTS-ignore work. Apps set frequency; device
still answered fixed `FA`/`IF` stubs. Architect: `RGNX_FW_CAT_STATE_RAM_V1`
(D19) — RAM set/get for FA/FB/MD/IF. Implementer pending.

## 2026-11-19 — MODE_LED invisible (returned to architect; closed Option A)

Human: after flash, mode LED did not show expected pulse pattern.

Tester conclusion: GPIO0 mux conflict — MODE_LED PIO vs Waveshare UART0 TX /
`stdio_init_all()` reclaim. Compile was PASS; hardware LED FAIL.

Architect response: Option A — keep LED on GPIO0; forbid UART stdio on 0/1;
rewrote LED rules in `docs/device_modes_architecture.md`. Implementer pending
to remove `stdio_init_all`; then **re-test LED on hardware**.

See `handoff.md` / `test_runs.md` for this handoff.
