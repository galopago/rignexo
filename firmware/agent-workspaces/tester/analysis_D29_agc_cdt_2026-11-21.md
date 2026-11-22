# D29 bench × code — AGC/CDT block demod

Date: 2026-11-21  
Handoff: `RGNX_FW_APRS_TNC_RX_LEVEL`

## STAT (entire ~100 s window)

| Field | Value |
| --- | --- |
| adc (8-bit sticky) | **123…131** mid 127 (pp≈8) |
| ene / dcd / bits | **0 / 0 / 0** |
| disc_tr | **0** |
| dem_min / dem_max | **0 / 0** |
| agc_q8 | **256** (stuck at min = 1.0×) |
| ok / KISS | 0 |

Timers `t104`/`t105` advance — RX alarm alive. A1 feed present
(values are 8-bit; 123…131 ≡ D28’s ~1971…2104 `>> 4`).

## Code defects that match the STAT

### 1. AGC never climbs (integer dead-zone)

```c
err = TARGET_PEAK - s_agc_peak;   /* ~50 - ~4 = 46 */
s_agc_gain_q8 += err >> GAIN_SHIFT;  /* GAIN_SHIFT=7 → 46>>7 = 0 */
```

With peak≈4, **every sample adds 0** → `agc_q8` stays **256**. Explains STAT.

### 2. CDT gates demod on **pre-AGC** AC

```c
val = adc_u8 - avg;           /* |val|~4 */
cdt_lvl += val*val …          /* → ene≈0 */
if (!cdt) return;             /* never BPF / dem / disc_tr */
dem = bell202_decode(tp, fed);
```

Same physical inject that toggled CDT on **12-bit** (larger `val`) now
fails CDT on **8-bit** (`val` ÷16 ⇒ energy ÷256). Dem sticky never
updates → `dem_min=dem_max=0`.

Even if AGC later worked, CDT-before-gain still blocks unless CDT uses
**post-AGC** energy (or thresholds retuned for 8-bit).

## Gates

| Gate | Result |
| --- | --- |
| G1 | FAIL |
| G2 | FAIL |
| G3 | FAIL |
| G4 | **Yes** — pp tiny on 8-bit **and** AGC/CDT logic prevents recovery |
| G5 | Host TX no own-echo (human PTT not rechecked) |

## Ask of architect

**Primary:** `note_architect_rx_level_ops_2026-11-21.md` — AF level is
variable; do not design around constant volume or fixed thresholds alone.

Concrete fixes for D29 fallout:

1. CDT on **post-AGC** (or level-robust detect) so weak inject can enter demod
   after gain.
2. AGC that **actually** rises when peak≪target (no `err>>7` dead-zone).
3. Keep A1 (`ADC_BIT 8` + `>>4`). Do not revive D27.
4. Acceptance: **quiet + loud** fixture passes — not one sweet-spot volume.

Tester re-run: `agc_q8`↑ → `disc_tr`↑ → `|dem|` usable → `ok`/KISS at more
than one inject level.
