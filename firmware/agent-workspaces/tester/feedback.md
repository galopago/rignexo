# Tester feedback — `rignexo/firmware`

## 2026-11-19 — MODE_LED invisible (returned to architect; closed Option A)

Human: after flash, mode LED did not show expected pulse pattern.

Tester conclusion: GPIO0 mux conflict — MODE_LED PIO vs Waveshare UART0 TX /
`stdio_init_all()` reclaim. Compile was PASS; hardware LED FAIL.

Architect response: Option A — keep LED on GPIO0; forbid UART stdio on 0/1;
rewrote LED rules in `docs/device_modes_architecture.md`. Implementer pending
to remove `stdio_init_all`; then **re-test LED on hardware**.

See `handoff.md` / `test_runs.md` for this handoff.
