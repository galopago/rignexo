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
ID: RGNX_FW_MODE4_PKT_LED_LONG D40
Date: 2026-11-21
Commands: CLR idle STAT; ~90s dense STAT + KISS (WAV playing)
Result: PASS (STAT) — human eye confirm pending
Evidence:
  - Idle: off=150 gap=150 reason=clr
  - Live: ok=90 kiss_ok=49; off_hist 220×20 / 150×11
  - fcs → 220/80; hold_exp → 150/150; no 80/260/50
Technical conclusion: D40 commanded widths match rule; ask human if long
  dark flash is obvious vs baseline.
```

```text
ID: RGNX_FW_MODE4_AF_LED_PKT D39 @95% recheck
Date: 2026-11-21
Commands: CLR/STAT ~45s @95% v2 (immediate recheck)
Result: PASS decode + FCS→OFF 80
Evidence:
  - Early polls ok=0 bits↑ bad_fcs↑; by t≈44s KISS B05/C01
  - Final: ok=5 kiss_ok=2 af_reason=fcs led_off=80 gap=220
  - Prior 120s window ok=0 was inject/timing that slice — not “95% deaf”
Technical conclusion: D39 short-OFF on FCS confirmed; baseline 150 until FCS.
```

```text
ID: RGNX_FW_MODE4_AF_LED_PKT D39 @95%
Date: 2026-11-21
Commands: idle STAT; then CLR/STAT ~120s @95% v2
Result: PARTIAL (superseded by recheck)
Evidence:
  - Idle 150/150 PASS; that window ok=0/kiss=0 (no FCS→80 yet)
Technical conclusion: First slice no FCS; recheck PASS.
```

```text
ID: FIXTURE_V2_REBASE matrix CLOSED
Date: 2026-11-21
Result: decode floor ~63%; 51% FAIL; LED still GOOD via pp_good without FCS @51%
```

```text
ID: FIXTURE_V2_REBASE @51%
Date: 2026-11-21
Level: 51%; afsk_rx_bench_2min.wav v2
Commands: KISS CLR/STAT ~120s
Result: FAIL decode — ok=0; LED mostly GOOD (incoherent)
Evidence:
  - Final: ok=0 kiss_ok=0 bad_fcs=1910 dcd=1
  - reasons: 8× pp_good OFF=260, 3× pp_bad OFF=50; never fcs
Technical conclusion: Below floor. Assist pp_good drives GOOD LED with 0 packets.
```

```text
ID: FIXTURE_V2_REBASE @63%
Date: 2026-11-21
Level: 63%; afsk_rx_bench_2min.wav v2
Commands: KISS CLR/STAT ~120s
Result: PASS marginal
Evidence:
  - Final: ok=4 kiss_ok=4 bad_fcs=1940 dcd=1
  - KISS: A02×2, D03, H-REPEAT
  - LED: early fcs/hold_exp; late polls pp_good OFF=260 without new ok
Technical conclusion: Floor still around low-60s %; LED can stay GOOD via
  pp_good after last FCS.
```

```text
ID: FIXTURE_V2_REBASE @75%
Date: 2026-11-21
Level: 75%; afsk_rx_bench_2min.wav v2
Commands: KISS CLR/STAT ~120s
Result: PASS
Evidence:
  - Final: ok=82 kiss_ok=58 bad_fcs=1805 dcd=1
  - KISS frames=60; af_reason=fcs; led_off_ms=260
Technical conclusion: Solid at 75% (weaker than 95% as expected).
```

```text
ID: FIXTURE_V2_REBASE @95%
Date: 2026-11-21
Level: 95% loud; afsk_rx_bench_2min.wav v2
Commands: KISS CLR/STAT ~120s
Result: PASS strong — decode restored
Evidence:
  - Final: ok=296 kiss_ok=103 bad_fcs=1233 dcd=1
  - KISS DATA=104 frames; A/B/C/D/E/F/H/END across cycles
  - af_reason=fcs all polls; led_off_ms=260
Technical conclusion: v1 silence pad was the ok=0 cause; v2+loud OK.
```

```text
ID: RGNX_FW_MODE4_AF_LED_DEBUG
Date: 2026-11-21
Result: CLOSED PASS (instrumentation); product LED FAIL → return architect
Evidence: matrix 51/63/75/95 — see D38-* rows + return_architect_D38_af_led_matrix_2026-11-21.md
Technical conclusion: Laf/Lled works. Assist pp≥120+hold → GOOD without FCS-OK.
  NOTE: that matrix used v1 sparse fixture — invalidate for decode conclusions.
```

```text
ID: RGNX_FW_MODE4_AF_LED_DEBUG D38-95
Date: 2026-11-21
CHIP / board: rp2350; mode 4 KISS TNC; ttyACM0
Level: ~95% loud WAV loop (human); D38 flash
Commands: KISS CLR 0x0D; 11× STAT 0x0C; ~110s
Result: D38 STAT PASS; decode FAIL this window (ok=0); LED mostly GOOD
Evidence:
  - af_pp ~73–129 (polls ≥120 at #2=129, #11=124); ok=kiss=0; bad_fcs→1132
  - LED: 9/11 GOOD off=260 pp_good; #1 hold_exp BAD; #9 pp_bad BAD (pp=73)
  - Never af_reason=fcs; dcd=1 adc~1970…2105 (same band as quieter levels)
  - Prior D33 loud was ok≈65 — this session all levels ok=0 (inject note)
Technical conclusion: Loud still GOOD-without-packets via pp_good assist.
  Matrix complete → return architect.
Human observations: Playing @95% (“sondando”) during capture.
```

```text
ID: RGNX_FW_MODE4_AF_LED_DEBUG D38-63
Date: 2026-11-21
CHIP / board: rp2350; mode 4 KISS TNC; ttyACM0
Level: ~63% PC/VLC WAV loop (human); D38 flash
Commands: KISS CLR 0x0D; 11× STAT 0x0C; ~110s
Result: D38 STAT PASS; decode FAIL this window (ok=0); LED mostly GOOD
Evidence:
  - af_pp ~85–116 (no sampled ≥120); ok=kiss=0; bad_fcs→1139; dcd=1
  - LED: 10/11 GOOD off=260 pp_good; 1/11 BAD off=50 reason=hold_exp (#5)
  - Prior D33 exploratorio-63 was marginal PASS (ok=1) — this slice no FCS-OK
  - Same assist/hold pattern: inter-poll pp spikes re-arm GOOD without packets
Technical conclusion: 63% still shows GOOD LED without decode; one visible
  hold expiry → BAD then re-arm. Matrix: loud pending.
Human observations: Playing @63% during capture.
```

```text
ID: RGNX_FW_MODE4_AF_LED_DEBUG D38-51
Date: 2026-11-21
CHIP / board: rp2350; mode 4 KISS TNC; ttyACM0
Level: ~51% PC/VLC WAV loop (human); D38 flash
Commands: KISS CLR 0x0D; 11× STAT 0x0C; ~110s
Result: D38 STAT PASS; decode FAIL (ok=0); LED GOOD↔BAD flicker (mostly GOOD)
Evidence:
  - af_pp ~71–118 (no sampled poll ≥120); ok=kiss=0; bad_fcs→1166; dcd=1
  - LED: 9/11 GOOD off=260 reason=pp_good; 2/11 BAD off=50 reason=pp_bad (#2,#8)
  - STAT#1 hold_ms=9473 after CLR (hold leftover from prior level)
  - Sampled pp often mid/weak while reason=pp_good → brief ≥120 spikes between
    STAT polls re-arm 10s hold; under hold mid-pp keeps GOOD
Technical conclusion: At 51% decode dead but LED mostly GOOD with brief BAD
  when hold expires into pp≤80 — still incoherent with packet quality.
Human observations: Playing @51% during capture.
```

```text
ID: RGNX_FW_MODE4_AF_LED_DEBUG D38-75
Date: 2026-11-21
CHIP / board: rp2350; mode 4 KISS TNC; ttyACM0
Level: ~75% PC/VLC WAV loop (human); D38 flash
Commands: KISS CLR 0x0D; 11× STAT 0x0C; ~110s
Result: D38 STAT dump PASS; decode FAIL this window (ok=0); AF LED stuck GOOD
Evidence:
  - Laf/Lafcfg/Lled present every STAT (D38 OK)
  - Lafcfg: pp_act=40 pp_bad=80 pp_good=120 hold_cfg_ms=10000
  - af_pp ~80–125 (only STAT#5 ≥120); reason=pp_good all 11 polls
  - af_good=af_hold=1; led_off_ms=260 led_good=1 entire window
  - L2: bits↑ bad_fcs→1215 ok=0; L3 kiss_ok=0; 0 KISS DATA
  - dcd=1 adc~1969…2104 (front end live)
  - STAT#1 already hold_ms=4060 after CLR → CLR does not clear AF hold
Technical conclusion: At 75% LED shows GOOD (OFF 260) via pp_good hold while
  decode fails — matches human volume↔blinks incoherence. Assist path
  (pp≥120) arms 10s GOOD without FCS-OK; mid-pp keeps GOOD under hold.
Human observations: Playing @75% during capture.
```

```text
ID: RGNX_FW_APRS_TNC_RX_REVERT_D31 exploratorio-63
Date: 2026-11-21
Level: 63% loop; D33
Result: PASS (marginal) — ok=1 kiss A01
Evidence: dcd=1; kiss_ok=1 ok1=1; much bad_fcs
Technical conclusion: Live decode floor ≈ mid-50s–low-60s % on this PC/VLC path.
```

```text
ID: RGNX_FW_APRS_TNC_RX_REVERT_D31 exploratorio-50
Date: 2026-11-21
Level: 50% PC/VLC loop; D33 baseline
Result: FAIL — ok=0 kiss=0
Evidence:
  - dcd=1; flag/entered↑ (~3.6k); bad_fcs→1332; 0 KISS
Technical conclusion: Floor for FCS-OK on this bench sits between 50% and
  63% (34%/50% hunt-only; 63%+ decode).
```

```text
ID: LAB_CLOSE_OK_FIELD_HANDOFF
Date: 2026-11-21
Result: CLOSED OK — human to field trials
Baseline: D33 (=D31 A+). Loud+73% PASS; volume floor ~63% on bench.
Technical conclusion: Lab RX/KISS path accepted for field; quiet-wide
  product still optional follow-up.
```

```text
ID: RGNX_FW_APRS_TNC_RX_REVERT_D31
Date: 2026-11-21
Result: CLOSED PASS (73% + loud re-check)
Evidence: see H1-LOUD + H1-REF73 below. D32 regress cleared.
```

```text
ID: RGNX_FW_APRS_TNC_RX_REVERT_D31 H1-REF73
Date: 2026-11-21
Level: ~73% loop; D33 flash
Commands: KISS CLR/STAT ~110s
Result: PASS
Evidence:
  - dcd=1; ok=5 kiss_ok=4; KISS A01, C02, C04, H05
  - flag/bad_fcs↑; better than first D31@73% (was ok=1 only)
Technical conclusion: Ref level restored after D32 revert.
```

```text
ID: RGNX_FW_APRS_TNC_RX_REVERT_D31 H1-LOUD
Date: 2026-11-21
Level: loud loop; D33 (=D31 A+) flash
Commands: KISS CLR/STAT ~110s; TX smoke
Result: PASS — D32 regress cleared on loud
Evidence:
  - dcd=1; ok=65 kiss_ok=25; n_sl=5; ok0/1/2 active
  - KISS B–I this window (loop phase; A not in slice)
  - TX no payload echo
Technical conclusion: Revert restores loud decode. Still need 73% re-check.
```

```text
ID: RGNX_FW_APRS_TNC_RX_WEAK H1-LOUD95
Date: 2026-11-21
Level: 95% loop; D32 flash
Commands: KISS CLR/STAT ~110s; TX smoke
Result: FAIL — dcd=0; REGRESS vs D31 loud PASS
Evidence:
  - Identical gate-off signature; 0 KISS; TX no payload echo
  - adc ~1960…2105
Technical conclusion: D32 matrix CLOSED FAIL — see
  return_architect_D32_regress_2026-11-21.md
```

```text
ID: RGNX_FW_APRS_TNC_RX_WEAK H1-REF73
Date: 2026-11-21
Level: ~73% loop; D32 flash
Commands: KISS CLR/STAT ~110s
Result: FAIL — dcd=0; REGRESS vs D31@73% (ok=1)
Evidence:
  - Same signature as quiet: dcd=0 flag=bits=ok=kiss=0 entire window
  - adc ~1970…2105; n_sl=9
Technical conclusion: D32 gating breaks previously working ref level.
```

```text
ID: RGNX_FW_APRS_TNC_RX_WEAK H1-QUIET
Date: 2026-11-21
Level: ~35% PC/VLC, WAV loop; D32 W1–W5 flash
Commands: KISS CLR 0x0D; STAT 0x0C; ~110s
Result: FAIL — total gate (dcd=0)
Evidence:
  - All polls: dcd=0 flag=0 bits=0 ok=0 kiss=0 skip=0 conf=0 eye~0
  - adc sticky still ~1970…2106; n_sl=9; timers advance
Technical conclusion: D32 DCD/raw-ene gate blocks quiet path entirely;
  regress vs D31@34% (which hunted flags). Continue matrix ref/loud.
```

```text
ID: RGNX_FW_APRS_TNC_RX_APLUS H1-LOUD
Date: 2026-11-21
Level: loud PC volume, WAV loop
Commands: KISS CLR/STAT; ~110s; 2× TX smoke
Result: PASS this level — strong
Evidence:
  - Final STAT: ok=64 kiss_ok=25 dup_drop=39; ok0=10 ok1=29 ok2=25
  - KISS: A01–A10 full + B/C/E/F/G/H/I (27 DATA frames captured)
  - flag/entered↑; bad_fcs↑; dcd=1; TX no payload echo
Technical conclusion: A+ decodes well when inject is loud.
```

```text
ID: RGNX_FW_APRS_TNC_RX_APLUS H1-QUIET
Date: 2026-11-21
Level: VLC/PC volume 34% loop (ref PASS was ~73%)
Commands: KISS CLR 0x0D; 8× STAT 0x0C; ~110s
Result: FAIL this level — ok=0 kiss=0
Evidence:
  - dcd=1; flag/entered↑ (~3k); bad_fcs→997; ok=ok0..4=0
  - adc sticky ~1973…2101; 0 KISS DATA
Technical conclusion: Below ref level, A+ hunts flags but no FCS-OK.
  Overall H1: FAIL (quiet required). No CLOSE.
```

```text
ID: RGNX_FW_APRS_TNC_RX_APLUS
Date: 2026-11-21
CHIP / board: rp2350 D31 afsk_aplus @24k; ttyACM0
Fixture: afsk_rx_bench_2min.wav external (one level, ~110s)
Commands: CLR; 8× STAT; KISS listen; 2× TX smoke
Result: PARTIAL — first live ok/KISS at ~73%; H1 quiet FAIL; loud pending
Evidence:
  - STAT2+: ok=1 kiss_ok=1 ok2=1; flag/entered↑; bad_fcs→1117
  - KISS DATA: payload RGNX-H-REPEAT (H05); section A 0 this window
  - dcd=1; adc 1972…2107; m/s amp ~0 in STAT print; n_sl=5 last_sl=2
  - TX host no own-echo
Technical conclusion: A+ broke the ok=0 drought on live audio. Yield low
  (1 OK vs many bad_fcs). Need loud level next; quiet@34% FAIL.
```

```text
ID: RGNX_FW_APRS_TNC_RX_* (D24–D30 chain)
Date: 2026-11-21
Result: CLOSED FAIL — ABANDONED (human + tester)
Evidence: never ok≥1 / KISS on afsk_rx_bench_2min.wav across clock, debug,
  sync, pico_tnc port, A1/AGC, field front-end. Last D30: front metrics only.
Technical conclusion: stop patch spiral; architect+human replan. See
  return_architect_rx_abandon_2026-11-21.md. Prior TX/shell PASS unchanged.
```

```text
ID: RGNX_FW_APRS_TNC_RX_FIELD
Date: 2026-11-21
CHIP / board: rp2350 D30; ttyACM0
Fixture: afsk_rx_bench_2min.wav external; one level (~110s)
Commands: CLR; 8× STAT; KISS listen; 2× TX smoke
Result: PARTIAL then superseded by chain ABANDON — front PASS; ok/KISS FAIL
Evidence:
  - agc_q8=8192 (max) all polls; cdt=dcd=1; disc_tr 1.7k→35k; adapt=1
  - dem_min≈-2.3e6 dem_max≈1.3e6 dem_env live; adc still 123…131 pk=1
  - bits≈110k; ok=bad_fcs=abort=kiss=0; 0 KISS frames
  - TX host no own-echo
Technical conclusion: D29 AGC/CDT fixed in spirit; decoder still yields 0
  frames (possible noise edges at max AGC). See
  analysis_D30_front_ok_no_frame_2026-11-21.md → architect.
```

```text
ID: RGNX_FW_APRS_TNC_RX_LEVEL
Date: 2026-11-21
CHIP / board: rp2350; D29 flash; ttyACM0 RIGNEXO KISS
Fixture: afsk_rx_bench_2min.wav external (human)
Commands: CLR; ~100s listen + 7× STAT; 2× TX smoke
Result: FAIL — G1/G2/G3; G4+AGC/CDT code issue
Evidence:
  - adc 8-bit sticky 123…131 mid127 (≡ D28 ~1971…2104>>4); ene=0 dcd=0
  - disc_tr=0 dem_min=dem_max=0 bits=0 agc_q8=256 (stuck min)
  - ok=kiss=0; timers advancing; TX host no own-echo
Technical conclusion: A1 scale OK; AGC update dead-zone (err>>7=0) + CDT
  on pre-AGC blocks demod. See analysis_D29_agc_cdt_2026-11-21.md.
```

```text
ID: RGNX_FW_APRS_TNC_RX_PORT
Date: 2026-11-21
CHIP / board: rp2350; mode 4 KISS TNC; cafe/RIGNEXO KISS on ttyACM0
Fixture: fixtures/afsk_rx_bench_2min.wav (external player → audio in)
Commands: RGNX CLR; listen 95s + 6× RGNX STAT; 3× KISS TX smoke
Result: FAIL — ok=0, 0 KISS (return architect)
Evidence:
  - STAT complete each poll (L0–L3+Lclk+END); dump_fail=0
  - Final STAT: ene~1717 dcd=1 adc_min=1971 adc_max=2104 adc_mid=2037
    L1 disc_tr=0 both=0; L2 bits=64633 abort=0 bad_fcs=0 ok=0;
    L3 kiss_ok=kiss_drop=0; t104/t105 Bresenham both advancing
  - Mid-run: ene/dcd vary (ene 763–2657; dcd toggles); bits↑ steadily
  - CDC: raw_n=1777 (STAT only); kiss_frames=0; section A 0/10
  - TX host: 3× DATA "TX-SMOKE-D28"; raw_n=0 during TX (no own-echo)
Human observations: WAV playing during window (human); PTT/AFSK audible
  not re-confirmed this run
Technical conclusion: D28 port runs (ADC energy, CDT, bit counter) but
  never FRAME_OK / KISS. Narrow ADC span (~133 pp) vs prior D25 wider swing
  may be level/coupling or scaled cdt — not proven root. Return architect.
```

```text
ID: RGNX_FW_APRS_TNC_RX
Date: 2026-11-21
CHIP / board: rp2350; mode1 aplay S16 → TNC GPIO28 path
Commands: host Bell-202 inject; offline demod at 9600 vs 1e6/104 Hz
Result: FAIL (return architect) — timer baud skew
Evidence:
  - Live: 0 KISS CDC bytes; audio inject audible (S16 exclusive)
  - Offline exact 9600: FRAME_OK DEMOD-TEST-1
  - Offline 9615.38 Hz (firmware 104 µs timer): 0 OK; NONE_ALIGN/ABORT
  - Root: 1000000/9600 int → 104 µs; bit clock 1201.92 vs host 1200
Human observations: FT8 interface OK; cable firm; no same-board loopback
Technical conclusion: Architect must choose timer/PLL option A–D (see handoff).
```

```text
ID: RGNX_FW_APRS_TNC_RX_EXT_WAV
Date: 2026-11-21
CHIP / board: rp2350 TNC; external player → TNC audio in
Fixture: fixtures/afsk_rx_bench_2min.wav (41 pkts, ~121s)
Commands: CDC listen 130s while human plays WAV externally
Result: FAIL — 0 CDC bytes / 0 KISS (0/40 expected YES)
Evidence:
  - Serial open cafe:4101; raw_n=0 entire run
  - Eliminates host USB sound-card path as sole cause
Human observations: WAV playing during capture window
Technical conclusion: Reinforces timer/baud-skew return to architect; not
  UAC2 playback routing.
```

```text
ID: RGNX_FW_APRS_TNC_RX_CLOCK
Date: 2026-11-21
CHIP / board: rp2350; cafe:4101 re-enum 11:21 after flash
Fixture: same afsk_rx_bench_2min.wav external play
Commands: build PASS; CDC listen 130s
Result: FAIL — still 0 CDC / 0 KISS (0/40 YES)
Evidence: raw_n=0 entire window post-D24 Bresenham+C-lite
Human observations: WAV executing during listen
Technical conclusion: Clock fix alone did not yield live decode; return /
  deepen (level? disc? flash verify? further PLL).
```

```text
ID: RGNX_FW_APRS_TNC_RX_DEBUG_TX
Date: 2026-11-21
CHIP / board: rp2350 TNC; external WAV playing
Commands: RGNX CLR; 3× RGNX STAT
Result: D26 PASS (full STAT); demod still ok=0
Evidence:
  - Each STAT: L0–L3+Lclk+END (~300 B), dump_fail=0
  - L1 both=1 disc_tr↑; L2 bits↑ abort↑ align_slip↑ bad_fcs few ok=0
  - kiss_ok=kiss_drop=0
Technical conclusion: Branch to HDLC abort/align — not tones, not STAT TX.
```

```text
ID: RGNX_FW_APRS_TNC_RX_SYNC
Date: 2026-11-21
CHIP / board: rp2350; D27 flash; external WAV
Commands: RGNX CLR; 3× RGNX STAT
Result: FAIL (ok=0); abort regress; align_slip=0
Evidence:
  - STAT complete; STAT3: bits=11169 abort=1594 ok=0 align_slip=0 disc_tr=3
  - D26 STAT3 baseline: abort=428 align_slip=70 disc_tr=6565 ok=0
Technical conclusion: Mid-bit/hyst stopped align_slip but worsened abort and
  nearly killed disc_tr; still no FRAME_OK.
```

```text
ID: RGNX_FW_COMPOSITE_USB_AUDIO_OUT
Date: 2026-11-21
CHIP / board: rp2350; cafe:4011 mode 1; headphones on audio out
Commands: aplay hw:CARD=interface 1 kHz 60s + AFSK tones; mixer max
Result: FAIL
Evidence:
  - ALSA stream Status Running @ 48 kHz Altset1 S16 mono
  - Human: absolute silence on mode-1 GPIO2 path
Technical conclusion: Host USB audio reaches device enum/stream; PWM/speaker
  path not producing audible output. Blocks mode1→TNC demod inject.
```

```text
ID: RGNX_FW_APRS_TNC_TX
Date: 2026-11-20
CHIP / board: rp2350; APRS_TNC
Commands: KISS DATA bursts via ttyACM0; TXDELAY=50; 3× attention retest
Result: PASS (CLOSED)
Evidence:
  - Frames sent; device cafe:4101
Human observations:
  - Headphone: AFSK/APRS-like bursts
  - Each burst: PTT LED on + relay click; off after
Technical conclusion: KISS→HDLC→AFSK TX + modem PTT timing accepted.
```

```text
ID: RGNX_FW_APRS_TNC_MODE_SHELL
Date: 2026-11-20
CHIP / board: rp2350; human selected APRS_TNC
Commands: host USB/KISS/RTS suite after flash
Result: PASS (CLOSED)
Evidence:
  - cafe:4101 RIGNEXO KISS TNC; CDC only; no Audio
  - …KISS_TNC…-if00; KISS cmds+data+escapes; no crash
  - ID; ignored; RTS toggle kept device up
Human observations:
  - MODE_LED: 4 dark pulses/cycle
  - RTS test: relay and PTT LED never fired
Technical conclusion: Mode-4 shell acceptance met (USB/LED/KISS/no RTS→PTT).
```

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

