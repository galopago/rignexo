# Deep analysis — D28 STAT vs pico_tnc port code

Date: 2026-11-21  
Role: tester (code+evidence; no `src/` edits)  
Run: `RGNX_FW_APRS_TNC_RX_PORT` FAIL

## 1. Observed signature (must explain all of these)

| Observation | Value |
| --- | --- |
| `bits` | ↑ ~64k in ~95 s |
| `disc_tr` | **0** every poll |
| `ok` / `bad_fcs` / `abort` | **0** |
| `kiss_*` | 0 |
| `ene` | 763…2657 (varies) |
| `dcd` | toggles 0/1 |
| `adc_min`…`max` | **1971…2104** (~133 pp), mid ~2037 |
| Timers `t104`/`t105` | advancing (13200 Bresenham OK) |

Bit-rate check: 64633 / 95 s ≈ 680 bit/s ≈ **0.57 × 1200**. Matches CDT
duty ~50–60% (demod only runs while `cdt`).

## 2. Code path (what STAT fields actually mean)

```text
adc_read() 12-bit
  → pico_tnc_rx_process_adc
       avg / val / cdt_lvl (ene) / cdt (dcd)
       if !cdt: return          ← explains bits ≈ 0.57×1200
       bell202_decode(raw adc)
         BPF >> ADC_BIT
         delay correlator (DELAYED_N=6)
         LPF → dem
       if dem < -4096: bit=1; elif dem >= 4096: bit=0
       decode2(PLL):
         on PLL wrap: decode_bit(NRZI)  → dbg_bits++  ALWAYS
         if bit != pval: dbg_disc_tr++; PLL pull
```

Critical:

1. **`bits` does not mean “good AFSK edges”.** It increments on every PLL
   sample tick while CDT is on, even if `bit` is constant.
2. **`disc_tr` is the only edge counter.** `disc_tr=0` + `bits↑` ⇒
   **`tp->bit` never flipped** for ~64k PLL decisions.
3. **`abort` is NOT HDLC abort.** In this port it only increments when
   `data_cnt >= DATA_LEN` (overflow). Six-ones path calls `output_packet`
   then returns to FLAG — no `dbg_abort++`. So `abort=0` is expected when
   the machine never leaves FLAG / never fills a giant frame.
4. **`ok` / `bad_fcs` require entering DATA after flag `0x7E`.** Stuck
   constant bit never produces `01111110` → never DATA → never FCS path.

So the STAT is not “mystery HDLC”; it is **frozen discriminator + live PLL**.

## 3. Why the discriminator stays frozen

### 3a. Quantitative — amplitude

Sticky ADC peak–peak ≈ **133 counts** on a 12-bit converter (±~66 around mid).

After BPF and `>> ADC_BIT` (12), delay product `m`, then `m >> 16` into LPF,
the soft decision `dem` must reach **±4096** (`LPF_THRESHOLD`) to change
`bit`. With only ±66 raw AC, correlator output is expected to sit in a
dead-band around 0 → **bit stays at reset value 0** → `disc_tr=0`.

CDT can still assert: `cdt_lvl` tracks `val*val` and thresholds are
1024/2048. Observed `ene` 763–2657 straddles those gates — consistent with
“some AC energy, not enough for Bell-202 slice”.

### 3b. Port divergence vs upstream (high-confidence)

Upstream `amedes/pico_tnc` (`tnc.h`):

```c
#define ADC_BIT 8       // adc bits 8 or 12
//#define ADC_BIT 12
```

Default path: ADC FIFO **shifts to 8 bits**; samples are 0…255; BPF uses
`>> 8`.

Our port (`tnc_rx_types.h`):

```c
#define ADC_BIT 12   /* comment: 12-bit ADC path */
```

Feed path (`afsk_bell202.c`): raw `adc_read()` (0…4095), **no** `>> 4` and
**no** FIFO 8-bit shift.

So we run the **12-bit / `ADC_BIT 12`** branch of the math, which upstream
documents as optional, while the **battle-tested default is 8-bit**. With a
weak inject, the 12-bit path + threshold ±4096 is a harsh combination.

Even if `ADC_BIT` were corrected to 8 with `(adc>>4)`, **133 pp on 12-bit ≈
8 counts pp on 8-bit** — still pathologically small vs a healthy AFSK
(tens of counts on 8-bit). So amplitude remains a co-equal issue.

### 3c. What this is NOT

- Not KISS FIFO (`kiss_drop=0`, zero frames).
- Not “STAT truncated” (full L0–L3+END).
- Not “RX timer dead” (`t104`/`t105`↑).
- Not “CDT stuck off” (dcd toggles; bits track CDT duty).
- Not comparable 1:1 to D25 `disc_tr` (different counter semantics).

## 4. Recommended next development order

### Order A — Architect lock (one decision)

Pick **one** sample-width contract matching pico_tnc:

| Option | Change | Intent |
| --- | --- | --- |
| **A1 (preferred)** | `ADC_BIT 8` + feed `(int)(adc_read() >> 4)` (or FIFO shift-8) | Match upstream default scale |
| **A2** | Keep `ADC_BIT 12` but **prove** dem reaches ±4096 with this hardware (needs louder inject and/or gain) | Stay on 12-bit branch deliberately |

Do not leave “12-bit comment” without A1 or measured dem headroom.

### Order B — Implementer (after A)

1. Apply A1 or A2 wiring exactly.
2. Extend STAT (small): sticky `dem_min`/`dem_max` (or `bit_flip` count
   already = `disc_tr`) so next bench can see threshold gap without guesswork.
3. Optional: rename/document that `abort` = overflow only.

### Order C — Tester re-run

Same fixture + CLR/STAT:

| Gate | Pass if |
| --- | --- |
| G1 | `disc_tr` ↑ materially while WAV plays |
| G2 | `ok ≥ 1` and/or `bad_fcs ≥ 1` (proves left FLAG) |
| G3 | KISS DATA matches section A (≥1) |
| G4 | If G1 fails: `adc` pp and `dem` range still dead → **level/coupling**, not more PLL work |

Also raise host/player level if `adc` pp stays &lt;~500 on 12-bit after A1.

### Explicit non-goals (do not burn cycle)

- Another clean-room mid-bit experiment.
- Blaming KISS/CDC.
- Treating `bits↑` alone as demod health.

## 5. One-line root claim for handoff

**D28 live FAIL is a frozen Bell-202 soft-decision (`dem` never crosses
±4096) under weak ADC swing + 12-bit/`ADC_BIT 12` feed that diverges from
upstream’s default 8-bit path; PLL/HDLC never see mark↔space edges, so
`ok` and KISS stay zero.**
