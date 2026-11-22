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
3. MODE_LED (GPIO0 only — not WS2812/PTT LED): steady ON with **1** / **2** /
   **3** short dark flashes for modes 1 / 2 / 3 — not a 50% blink. Pattern
   still visible after full boot (no UART reclaim of GPIO0; D15).
4. Unknown stored mode → boots as `COMPOSITE` and heals per architecture
   (single-write policy).
5. Mode LED via PIO does not disturb USB enumeration/audio.
6. `COMPOSITE`: CDC `…-if03` present; RTS keys PTT; VOX does not.
7. `AUDIO_VOX`: audio present, **no** CDC/COM; TX audio keys PTT.
8. `COMPOSITE_CAT` (`docs/cat_ptt_mode_architecture.md`): CDC present; RTS does
   **not** key PTT; `TX;`/`TX1;` keys PTT; `RX;` releases; unknown → `?;`.
9. CAT soft state (`RGNX_FW_CAT_STATE_RAM_V1`): **before any set**, first gets
   return the coherent cold-start profile (14.074 USB, FW2700, etc. — see
   architecture). After set of `FA`/`MD`/`FW`/`SL`/`SH`/`PC`/…, matching get
   and `IF;` return RAM values (reset to defaults on power cycle).
10. CAT Hamlib IF (`RGNX_FW_CAT_IF_HAMLIB_V1`): `IF;` is **38** chars on the
    wire; after stripping `;`, payload length **37** with
    `info[30]=='0'` (VFO A) at cold-start. WSJT-X / Hamlib **Test CAT** must
    not fail with `unsupported VFO None`. Scripted FA/IF alone is insufficient.
11. APRS TNC mode (`RGNX_FW_APRS_TNC_V1` / `docs/aprs_tnc_mode_architecture.md`):
    mode 4 LED = **4** OFF pulses; USB **CDC only** (no UAC2); RTS does not
    key PTT; KISS TX keys PTT + AFSK on GPIO2; RX path returns KISS on CDC
    when a valid frame is demodulated (phased acceptance as implementer
    phases land).
12. APRS TNC RX (`RGNX_FW_APRS_TNC_RX`): known AFSK into GPIO28 → matching KISS
    DATA on CDC (no FCS); no RX echo of own TX; CSMA delays TX when DCD busy;
    see architecture § RX implementer contract acceptance list.
13. APRS TNC RX clock (`RGNX_FW_APRS_TNC_RX_CLOCK` / D24): after timer+PLL fix,
    host/external 1200-baud Bell-202 inject (e.g. bench WAV) MUST yield KISS
    DATA on CDC; TX smoke still PASS. Do not treat FT8 analog path as root cause.
14. APRS TNC RX debug (`RGNX_FW_APRS_TNC_RX_DEBUG` / D25): `RGNX STAT` while
    idle vs WAV → L0/L1 show quiet vs **rising `disc_tr`**; `CLR` resets
    counters. Use before further demod algorithm changes.
15. APRS TNC STAT TX (`RGNX_FW_APRS_TNC_RX_DEBUG_TX` / D26): under WAV load,
    each STAT includes L0–L3 + Lclk + END (≥3 polls, no ~128 B truncation).
16. APRS TNC RX sync (`RGNX_FW_APRS_TNC_RX_SYNC` / D27): same WAV fixture →
    `ok≥1`… (**FAIL** on bench — superseded by D28).
17. APRS TNC RX port (`RGNX_FW_APRS_TNC_RX_PORT` / D28): pico_tnc-based RX @
    13200 Hz → fixture WAV yields `ok≥1` + KISS DATA (manifest); TX smoke PASS;
    BSD-3 attribution present. (**FAIL** live — frozen dem; see D29.)
18. APRS TNC RX level (`RGNX_FW_APRS_TNC_RX_LEVEL` / D29): `ADC_BIT 8` + AGC;
    under WAV `disc_tr`↑ and `dem` reaches ±4096 band; then `ok≥1` + KISS;
    TX smoke PASS. (**FAIL** live — AGC stuck / CDT pre-AGC; see D30.)
19. APRS TNC RX field (`RGNX_FW_APRS_TNC_RX_FIELD` / D30): working AGC (moves
    on quiet+loud), CDT post-AGC, B1 adaptive `|dem|` slice; **both** quiet and
    loud inject of the same fixture yield `ok≥1` and/or KISS; TX smoke PASS.
    Do **not** CLOSE on one volume. (**FAIL** / abandoned with D24–D29 spiral;
    see D31.)
20. APRS TNC RX A+ (`RGNX_FW_APRS_TNC_RX_APLUS` / D31): clean-room Direwolf-class
    dual-tone + multi-slicer. **O0/O1 offline** fixture decode required before
    live. Live quiet+loud → `ok≥1` and/or KISS; TX smoke PASS. Do **not** CLOSE
    on `disc_tr`/AGC alone. (**PARTIAL:** LOUD PASS, QUIET FAIL — see D32.)
21. APRS TNC RX weak (`RGNX_FW_APRS_TNC_RX_WEAK` / D32): after A+ lands, live
    matrix **~34% / ~73% / loud** all decode (`ok≥1` and/or KISS); offline O0/O1
    still PASS; TX smoke PASS. Prefer section A on quiet if window allows.
    (**CLOSED FAIL / REGRESS** — live `dcd=0` all levels; see D33.)
22. APRS TNC RX revert (`RGNX_FW_APRS_TNC_RX_REVERT_D31` / D33): restore D31 A+
    path; live ~73% + loud decode ≥ D31; offline O0/O1 PASS. No D32 gate stack.
23. Mode 4 AF LED (`RGNX_FW_MODE4_AF_LED` / D34): always 4 OFF; quiet+tick period
    fixed; long OFF when AF good, short when bad; modes 1–3 unchanged; PIO only.
24. Mode 4 AF LED hold (`RGNX_FW_MODE4_AF_LED_HOLD` / D35): after good-level
    packet, long OFF persists ~10 s through silence; bad activity → short OFF;
    silence alone must not clear GOOD before hold ends.
25. Mode 4 AF LED duty (`RGNX_FW_MODE4_AF_LED_DUTY` / D36): with TICK=300 and
    4 pulses fixed, GOOD OFF ≈260 ms vs BAD OFF ≈50 ms must be clearly
    different at a glance (max contrast); do not use mid values that looked alike.
26. Mode 4 AF LED metric (`RGNX_FW_MODE4_AF_LED_METRIC` / D37): FCS-OK → GOOD +
    10 s hold; mid-pp must not clear hold; noise≠GOOD; pp assist ~120/80/40;
    loud/73% with KISS shows long OFF; ≤50% no KISS shows short OFF.
27. Mode 4 AF LED debug (`RGNX_FW_MODE4_AF_LED_DEBUG` / D38): `RGNX STAT` shows
    `af_pp` / `af_good` / `af_hold` / `af_reason` / `led_off_ms` (etc.); dump
    complete under load. Lab matrix 51/63/75/95 (2026-11-21): dump **PASS**;
    product LED still GOOD via `pp_good` assist without FCS-OK — see
    `return_architect_D38_af_led_matrix_2026-11-21.md` (fixed by D39).
28. Mode 4 packet LED (`RGNX_FW_MODE4_PKT_LED` / D39): idle/cold → **150/150**
    like modes 1–3; FCS-only; no `pp_good`; CLR → baseline. (D39 short-OFF
    polarity superseded by D40.)
29. Mode 4 packet OK long OFF (`RGNX_FW_MODE4_PKT_LED_LONG` / D40.0): FCS-OK →
    long OFF; baseline **150/150**; FCS-only. (Widths: see D40.1.)
30. Mode 4 LED tweak (`RGNX_FW_MODE4_PKT_LED_TWEAK` / D40.1): FCS → 240/60,
    hold ~3 s (superseded widths by D40.2).
31. Mode 4 LED tweak2 (`RGNX_FW_MODE4_PKT_LED_TWEAK2` / D40.2): FCS → OFF
    **250** / GAP **50**, hold ~3 s; four edges countable.
32. Both `CHIP=rp2350` and `CHIP=rp2040` build.
