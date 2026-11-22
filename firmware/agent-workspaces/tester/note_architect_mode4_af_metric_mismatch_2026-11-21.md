# Note to architect — Mode 4 AF LED metric **does not match** live decode

Date: 2026-11-21  
From: tester (code review + prior bench STAT)  
Human: D36 OFF contrast visible, but “quality/volume” detector feels wrong

## Verdict

Detection is **not adequate** for this hardware path. LED GOOD/BAD is driven
mainly by **ADC peak-to-peak thresholds that this bench never reaches**, and
the update loop **cancels FCS-OK GOOD** within one refresh.

D36 (260/50 OFF) can look different, but the boolean fed into it stays BAD.

## What the code does

`afsk_af_led_update()` every `BOARD_MODE4_AF_UPDATE_MS` (200 ms):

```text
pp = sticky adc_max − adc_min   (12-bit, window since last update)
active = pp >= 40
if active && (clip || pp <= 180) → force BAD (clears hold)
if pp >= 250 || (already GOOD && pp > 180) → arm GOOD + 10 s hold
```

Also: `afsk_on_rx_frame` (FCS-OK) calls `afsk_af_led_arm_good()`.

Constants (`board_config.h`):

| Symbol | Value |
| --- | ---: |
| `AF_PP_GOOD` | **250** |
| `AF_PP_BAD` | **180** |
| `AF_PP_ACTIVITY` | 40 |

## What the bench actually saw (D31/D33 loud = decode PASS)

Typical STAT sticky ADC while **kiss_ok≥1 / section A OK**:

- `adc_min` ≈ 1960–1973, `adc_max` ≈ 2103–2113  
- **pp ≈ 130–150**

So on the same inject that **decodes well**:

| Check | Result |
| --- | --- |
| `pp >= 250` (instant GOOD) | **Never** |
| `pp <= 180` (instant BAD) | **Always** while audio present |
| FCS-OK arms GOOD | Yes, briefly |
| Next `afsk_af_led_update` | Sees active + pp≤180 → **`force_bad()`** |

Net: LED tracks a **pp ladder written for ~250 counts**, not “frames decode
OK.” Operator sees BAD (short OFF) even when KISS is flowing.

## Product mismatch

Human intent (and lab tests):  
**good decode / usable volume → long OFF; bad/unusable → short OFF.**

Code intent (docs):  
**raw ADC pp bands 250 / 180**, optional FCS-OK hold — but hold is broken by
the BAD branch on this inject level.

## Ask of architect

Concrete threshold / metric proposal:
**`note_architect_mode4_af_metric_thresholds_2026-11-21.md`**

(Primary FCS-OK + hold fix; suggested pp assist `GOOD=120` / `BAD=80` /
`ACTIVITY=40`; do not force BAD on mid-pp while hold armed.)

Keep D36 OFF widths; problem is the **boolean**, not 260/50.

## Non-goals

- Another OFF ms tweak.  
- Gating the demod on the LED path.
