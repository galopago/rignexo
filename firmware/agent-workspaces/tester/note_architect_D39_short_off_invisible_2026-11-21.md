# Note to architect — D39 “short OFF” invisible to human

Date: 2026-11-21  
From: tester + human  
ID: `RGNX_FW_MODE4_PKT_LED` (D39)

## Human

Does **not** see short-OFF after packets, while STAT sometimes reports
`led_off_ms=80` + `af_reason=fcs`.

## Code truth (what actually runs)

| State | OFF | GAP | Hold |
| --- | ---: | ---: | ---: |
| Baseline (no FCS) | **150** | **150** | — |
| Packet (FCS-OK) | **80** | **220** | **2000 ms** |

- FCS → `afsk_af_led_arm_packet` → `mode_led_pio_set_af_good(true)` →
  `apply_mode4_packet` sets `s_last_off_ms=80` and reloads PIO Y/OSR.
- STAT `Lled` prints **`s_last_off_ms`** (last **commanded** width), not an
  optical measurement.
- Path is real: recheck @95% saw `ok≥1`, KISS frames, `led_off_ms=80`,
  `af_reason=fcs`.

## Why eyes ≠ “corto” (revised — human correction)

Human: in **percentage**, 150 vs 80 is almost **2×** (150/80 ≈ 1.88) — not a
tiny change. Tester was wrong to dismiss it as “only 70 ms.”

Duty inside TICK=300: baseline OFF **50%** of tick; packet OFF **≈27%** of
tick — that *should* be visible if the LED actually stayed in packet mode.

More likely reasons eyes stay on “baseline feel”:

1. **Hold only 2 s** → most wall-clock time is 150/150; STAT 80 is a brief
   slice.
2. **Quiet ON 800 ms** between 4-pulse groups dominates the glance.
3. **GAP jumps 150→220** at the same time as OFF 150→80 — two things change;
   rhythm may not read as “same cadence, shorter OFF.”
4. **Possible PIO reload from RX IRQ** — STAT `s_last_off_ms=80` can be true
   while the SM pattern is glitchy/restarted; needs check.

Do **not** treat “Δ70 ms ⇒ invisible” as the root cause.

## Secondary risk (implementer/architect)

`afsk_on_rx_frame` runs under RX alarm → `emit_frame` → arms LED →
`pio_sm_set_enabled` + delay reload possibly from **IRQ context**. Worth
confirming SM reload is safe / deferred to `afsk_task`; could glitch pattern
even when `s_last_off_ms=80`.

## Ask

### Human revision (2026-11-21 evening) — persistence of vision

Human now leans: **long OFF when packets OK** (not short).

Reason: retinal persistence / light “stored” on the retina — brief dark dips
(short OFF) are easy to miss; a **longer dark** is what the glance actually
registers as an event.

| State | Timing (proposed) | Why |
| --- | --- | --- |
| Idle / bad / no FCS | **150/150** (modes 1–3) | Familiar baseline |
| FCS-OK (packet good) | **Long OFF** (GAP = 300−OFF; ↓→↓ fixed) | Dark flash survives human vision |

This **supersedes** the earlier “good = short OFF” preference for glance UX.
Keep: FCS-only metric; ↓→↓ period fixed; no `pp_good` promote.

Architect: pick a long OFF clearly >150 (e.g. ~220–260) so it is obviously
darker than baseline, without collapsing the four edges into one blob if
possible — or accept stronger dark and validate with human eyes.

1. Prefer **long OFF on FCS**, baseline **150/150**.  
2. Hold long enough to be glanceable (2 s may be OK if OFF is long).  
3. FCS-only; no amplitude promote.  
4. STAT `led_off_ms` = commanded.
