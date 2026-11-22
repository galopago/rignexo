# Tester feedback — `rignexo/firmware`

## 2026-11-21 — Architect D40.2 → implementer

Note accepted. Handoff `RGNX_FW_MODE4_PKT_LED_TWEAK2`: OFF **250** / GAP **50**;
hold 3000 unchanged.

## 2026-11-21 — Architect D40.1 tweak → implementer

Landed; superseded by D40.2 (+10 ms OFF).

## 2026-11-21 — Architect D40: packet OK = long OFF

D40.0 landed (220/80 + 2 s). Superseded widths by D40.1 tweak.

## 2026-11-21 — Architect D39: packet LED 150/150 + short on FCS

Code-review note accepted; D39 landed then polarity superseded by D40.

## 2026-11-21 — D40.2: +10 ms OFF → 250/50

Human: un poco más notorio. Tester OK: OFF_PKT **250**, GAP **50**, hold 3000.
Note `note_architect_D40_led_tweak2_2026-11-21.md`.

## 2026-11-21 — D40 tweak APPROVED: 240/60 + hold 3000

Human acepta. Pendiente architect/implementer constants.
Note `note_architect_D40_led_tweak_2026-11-21.md`.

## 2026-11-21 — LED: good packet → OFF largo (visión humana)

Human: persistencia retiniana → dips cortos se pierden; mejor **OFF largo**
con FCS-OK; idle/malo sigue **150/150**. Anula preferencia previa “bueno =
corto”. Note D39 actualizada → D40.

## 2026-11-21 — D39: human no ve OFF corto; STAT sí 80

Código manda OFF_PKT=80 vs baseline 150 (Δ70 ms) — imperceptible. STAT
reporta `s_last_off_ms` (comandado). Note:
`note_architect_D39_short_off_invisible_2026-11-21.md`. Pedir OFF≪150.

## 2026-11-21 — Mode 4 bad/idle = 150/150 like modes 1–3

Human: paquete malo / mala decode = **150/150** (igual otros modos). Paquete
bueno = OFF más corto (↓→↓ sigue 300). Hoy 50/250 y 260/40 estaban mal.
**Métrica:** bueno = solo FCS-OK; malo/idle = sin FCS reciente; prohibido
promover por `af_pp`. Note code review § Normative metric.

## 2026-11-21 — Mode 4: human said keep pulse frequency, only OFF

Human: no cambiar frecuencia de pulsos en modo 4; solo tiempo OFF. Código
actual: TICK=300 fijo pero GAP=TICK−OFF → 40 vs 250 ms; ritmo percibido ≠
modos 1–3 (150/150). Constraint violada. Note code review actualizada.

## 2026-11-21 — OFF times only (human ask)

Depuración: mayoría **largos 260 ms** (no cortos 50). 260>50. No
relabel GOOD/BAD in that answer.

## 2026-11-21 — AF LED: ojos = STAT, mayoría OFF largo

Human: mayoría OFF largos. STAT: 260 casi siempre (incluso @51% ok=0 y
@95% ok=296). Señal de producto rota. Polaridad deseada: largo=sin
paquete, corto=FCS.

## 2026-11-21 — AF LED polarity (human)

Sin paquete / malo = OFF **largo** (como otros modos en frío). Paquete bueno =
OFF **corto**. Hoy está al revés (GOOD=260, BAD=50). Anotado en
`note_architect_mode4_af_led_code_review_2026-11-21.md`.

## 2026-11-21 — Fixture v2 matrix CLOSED

95/75/63/51%: decode floor ~63%; 51% ok=0 but LED mostly `pp_good`/260.
Confirms AF-LED assist issue with valid stimulus.

## 2026-11-21 — Fixture v1 bad: ~40s packets + ~80s silence

Human: WAV “2 min” looked empty after start. Confirmed: v1 had 41 pkts only
until t≈41.6 s then silence pad to 121 s — bad for loop benches; likely
contributed to D38 matrix `ok=0`. Regenerated **v2** (3 cycles, 116 pkts,
~117 s active). Old kept as `*_v1_sparse_pad.*`. Generator:
`fixtures/gen_afsk_rx_bench_2min.py`.

## 2026-11-21 — D38 matrix CLOSED → architect

95% done; matrix 51/63/75/95: LED GOOD sin FCS-OK. Return:
`return_architect_D38_af_led_matrix_2026-11-21.md`. Pedir: GOOD solo FCS o
assist mucho más estricto; CLR limpia hold.

## 2026-11-21 — D38 @95%: ok=0; LED 9/11 GOOD

`af_pp`~73–129 (picos ≥120); reasons pp_good/hold_exp/pp_bad; nunca `fcs`.
See `test_runs.md` D38-95.

## 2026-11-21 — D38 @63%: ok=0; LED 10/11 GOOD (1× hold_exp)

`af_pp`~85–116; off=260 casi todo; un BAD por `hold_exp`. Sin FCS-OK este
slice (antes D33@63 era marginal PASS). See `test_runs.md` D38-63.

## 2026-11-21 — D38 @51%: ok=0; LED GOOD↔BAD (mayormente GOOD)

`af_pp`~71–118; 9/11 GOOD off=260; 2/11 BAD off=50. Decode muerto; spikes
`pp≥120` entre polls re-arman hold. See `test_runs.md` D38-51.

## 2026-11-21 — D38 @75%: STAT OK; LED GOOD vs ok=0

Captura ~110s / 11× STAT: Laf/Lled OK. `af_pp`~80–125, `reason=pp_good`,
`led_off_ms=260` todo el rato; `ok=kiss=0`. Hold por assist `pp≥120` sin
FCS-OK; CLR no limpia hold. Matriz 63/loud pendiente.
See `test_runs.md` ID `RGNX_FW_MODE4_AF_LED_DEBUG D38-75`.

## 2026-11-21 — Architect D38: AF LED debug STAT

Debug-STAT note accepted. Implementer shipped
`RGNX_FW_MODE4_AF_LED_DEBUG`: RGNXDBG `Laf`/`Lled` with af_pp/good/hold/reason
and led_off_ms. Tester volume matrix in progress (75% done).

## 2026-11-21 — Architect D37: AF LED = packet quality metric

Mismatch + threshold notes accepted. Implementer
`RGNX_FW_MODE4_AF_LED_METRIC` (FCS-OK primary; hold fix; pp 120/80/40).

## 2026-11-21 — Architect D36: AF LED OFF contrast 260/50

Human: GOOD vs BAD looked alike at 220/80. Implementer pending
`RGNX_FW_MODE4_AF_LED_DUTY`: OFF_GOOD=260, OFF_BAD=50; TICK/count fixed.

## 2026-11-21 — Architect D33: FULL REVERT D32 → D31

Regress note accepted. New ID `RGNX_FW_APRS_TNC_RX_REVERT_D31`: undo all
W1–W5; restore D31 A+ live baseline. No partial DCD retune. Tester re-checks
73% + loud after flash.

## 2026-11-21 — Architect D32: quiet-gap yield order

H1 quiet note accepted. New ID `RGNX_FW_APRS_TNC_RX_WEAK`: prefilter, raw
`m−G·s`, conf gate, N=9; keep ~34% floor; protect loud/ref. Tester waits
for implementer offline re-proof then matrix 34/73/loud.

## 2026-11-21 — Architect rethink: D31 A+ multi-slicer

Abandon accepted. New ID `RGNX_FW_APRS_TNC_RX_APLUS`: clean-room Direwolf-class
relative mark/space + parallel space_gain slicers; offline O0/O1 before live;
no GPL vendor; no AGC-only sequel. Tester waits for implementer offline proof.

## 2026-11-21 — Mode 4 AF LED: human says imperceptible

D34/D35: GOOD=220 ms OFF vs BAD=80 ms @ TICK 300 — too subtle. Hold 10 s OK
in intent but transition hard to see. Note for architect:
`note_architect_mode4_led_readability_2026-11-21.md` (widen contrast / change
rhythm / attention on expiry).

## 2026-11-21 — Need live AF LED metric + OFF timer debug

Human: volume up/down vs blinks incoherent. Ask architect for RGNXDBG of
`af_pp`/`af_good`/`af_reason`/`led_off_ms` (etc.) like packet STAT.
`note_architect_mode4_af_led_debug_stat_2026-11-21.md`.

## 2026-11-21 — Mode 4 AF metric wrong vs live decode

D36 contrast OK; detector not. Bench decode at pp≈130–150 but GOOD needs
pp≥250 and pp≤180 forces BAD — cancels FCS-OK hold. Note:
`note_architect_mode4_af_metric_mismatch_2026-11-21.md`.
Threshold proposal: FCS-OK primary; pp assist ~120/80/40 —
`note_architect_mode4_af_metric_thresholds_2026-11-21.md`.

## 2026-11-21 — Mode 4 AF LED: OFF timing OK, must be eye-obvious

Human clarification: encoding by **apagado** duration is correct; 80 vs 220
ms is invisible. Widen contrast (TICK up if needed); keep 4 pulses. Note:
`note_architect_mode4_led_simplicity_2026-11-21.md`.

## 2026-11-21 — Mode 4 AF LED: too convoluted for naïve users

Human: whole OFF/TICK/hold scheme needs prior knowledge — wrong for field.
Want obvious metaphor (solid vs blink / slow vs fast) or split mode vs AF.
`note_architect_mode4_led_simplicity_2026-11-21.md`.

## 2026-11-21 — Lab CLOSED OK → field trials

Human closes lab tests OK. Baseline D33/D31 A+. Loud+73% PASS; quiet floor
documented (~63% edge). Next: field radio tests; report back if needed.

## 2026-11-21 — D33 REVERT CLOSED PASS (73% + loud)

Loud ok=65; 73% ok=5 kiss A01/C/H. D32 regress cleared. Quiet still open.

## 2026-11-21 — D32 WEAK matrix FAIL / REGRESS

35% / 73% / 95%: all `dcd=0`, 0 KISS. Breaks D31 ref+loud. CLOSED FAIL.
`return_architect_D32_regress_2026-11-21.md` — revert/retune DCD gate.

## 2026-11-21 — D31 H1: LOUD PASS, QUIET FAIL → no CLOSE

LOUD: ok=64 kiss_ok=25, full section A + more. QUIET 34%: ok=0. Ref 73%:
marginal 1 frame. H1 dual-level **not** met. Architect: weak-AF gap.
Note: `note_architect_D31_H1_quiet_gap_2026-11-21.md` (quiet improve w/o
breaking loud; modem **does** work at ref/loud).

## 2026-11-21 — D31 A+ first live ok=1 + KISS (PARTIAL)

One inject level ~110s: `ok=1` `kiss_ok=1`, KISS `RGNX-H-REPEAT` (ok2).
`flag`/`entered`/`bad_fcs` move. Section A 0 this window. Need quiet+loud
for CLOSE. TX host no-echo OK.

## 2026-11-21 — RX live decode ABANDONED (CLOSED FAIL)

Human stops D24–D30 patch spiral; will replan with architect.
`return_architect_rx_abandon_2026-11-21.md`. No more RX tester work until
a **new** handoff ID after rethink. TX/shell prior PASS unchanged.

## 2026-11-21 — Architect answer: D30 field-robust RX

Ops note accepted. Normative § Field-robust RX front-end + decision D30 +
implementer pending `RGNX_FW_APRS_TNC_RX_FIELD`. Quiet+loud CLOSE required;
AGC must move; CDT post-AGC; B1 adaptive `|dem|` slice. No “raise volume”.

## 2026-11-21 — D30 FIELD: front OK, still 0 frames

AGC max 8192, cdt=1, disc_tr↑, dem huge, adapt=1 — but ok=kiss=0.
`analysis_D30_front_ok_no_frame_2026-11-21.md` → architect (not CLOSE).

## 2026-11-21 — Ops constraint to architect: level is not constant

Human: cannot maintain constant AF volume; fixed-threshold product answer
is unacceptable. Note: `note_architect_rx_level_ops_2026-11-21.md`.
Acceptance must cover quiet+loud inject; AGC/CDT must track level.

## 2026-11-21 — D29 RX_LEVEL FAIL (AGC stuck + CDT pre-AGC)

A1 visible (8-bit adc 123…131). G1–G3 FAIL: ene/bits/disc/dem=0,
`agc_q8=256` forever. Code: `err>>7` never steps gain; CDT on pre-AGC
blocks demod. `analysis_D29_agc_cdt_2026-11-21.md` → architect.

## 2026-11-21 — D28 RX_PORT live FAIL (ok=0, 0 KISS)

Flashed D28; external `afsk_rx_bench_2min.wav` + CLR/STAT 95s: **ok=0**,
**0 KISS**, bits↑, abort/bad_fcs=0, ene/dcd vary, ADC span ~133 pp.
Return: `return_architect_rx_port_D28_2026-11-21.md`.
Deep code×STAT: `analysis_D28_stat_vs_code_2026-11-21.md` — frozen `dem`
(`disc_tr=0`); prefer align upstream `ADC_BIT 8` + raise inject level.

## 2026-11-21 — D28 FAIL: frozen dem → A1+AGC (D29)

Port live: disc_tr=0, ok=0, ADC pp≈133, ADC_BIT 12 vs upstream 8.
Architect **D29**: ADC_BIT 8 + digital AGC + dem_min/max. Implementer
`RGNX_FW_APRS_TNC_RX_LEVEL`.

## 2026-11-21 — RX strategy: port pico_tnc (D28 / R1)

D27 FAIL (ok=0, abort regress). Tester recommended R1. Architect **D28**:
BSD-3 amedes/pico_tnc RX @ 13200 Hz. Implementer `RGNX_FW_APRS_TNC_RX_PORT`.

## 2026-11-21 — L2: abort+align_slip, ok=0 → bit sync (D27)

D26 STAT PASS. L2: bits↑ ok=0 abort↑ align_slip↑ kiss_drop=0; L1 still PASS.
Architect **D27** mid-bit sync. Implementer `RGNX_FW_APRS_TNC_RX_SYNC`.

## 2026-11-21 — APRS TNC RX still 0 KISS after clock; need debug layers

RX_CLOCK live FAIL. Tester asked tone-first observability (L0 energy → L1
disc transitions → L2 HDLC → L3 KISS). Architect **D25** CDC `RGNX STAT` /
cmd `0x0C`. Implementer `RGNX_FW_APRS_TNC_RX_DEBUG`.

## 2026-11-21 — APRS TNC RX demod FAIL (baud clock; HW OK)

Inject/external WAV → 0 KISS. Offline: exact 9600 PASS, `1e6/104` FAIL.
Root: integer timer → 9615 Hz; phase-only sync. Architect **D24** hybrid
(true 9600 + RX freq assist). Implementer `RGNX_FW_APRS_TNC_RX_CLOCK`.

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
