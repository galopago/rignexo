# Note to architect — Mode 4 AF LED: code behavior vs human/lab

Date: 2026-11-21  
From: tester (+ human)  
Evidence: fixture v2 matrix 95/75/63/51% + `afsk_bell202.c` / `board_config.h`

## Human

Could not find coherence between blink shape and packet quality while changing
volume. Lab STAT agrees.

### Human eyes ≡ STAT (serious)

Human reports **majority long OFF** at all volumes. Lab STAT same:
95/75% = 11/11×260; 63% = 9/11; 51% = 8/11 (ok=0). Not a perception
error — the LED **lives** in long-OFF whether ok=296 or ok=0. Product
signal is broken.

### Human UX polarity — **SUPERSEDED evening 2026-11-21**

Earlier same-day ask “good = short OFF” is **withdrawn** (D39 80 ms invisible
to human; persistence of vision).

**Current authoritative rule** (also in
`note_architect_D39_short_off_invisible_2026-11-21.md`):

| Condition | OFF / GAP | When |
| --- | --- | --- |
| Idle / bad / no FCS | **150 / 150** (modes 1–3) | Cold start; hold expired; no recent FCS-OK |
| Packet good | **Long OFF** (GAP = 300−OFF; ↓→↓ fixed 300) | **Only** FCS-OK (`afsk_on_rx_frame`) |

Metric: FCS-only. Forbidden: promote via `af_pp`. Long OFF must be clearly
>150 (e.g. ~220–260) so the dark flash survives retinal persistence.

## What the code actually does

Binary overlay on mode-4 LED: **GOOD** = OFF 260 ms, **BAD** = OFF 50 ms
(TICK 300). Only `mode_led_pio_set_af_good(bool)`.

### Path A — FCS (intended “packet quality”)

`afsk_on_rx_frame` → `afsk_af_led_arm_good(FCS)` → GOOD + **10 s hold**.

### Path B — amplitude assist (every 200 ms)

Window pp = max−min of ADC samples since last update:

| Condition | Action |
| --- | --- |
| clip | BAD |
| ACTIVITY≤pp≤BAD (40…80) | BAD |
| pp≥GOOD (120) | **GOOD + same 10 s hold** as FCS |
| 81…119 (mid) | **no change** (sticky previous) |
| pp&lt;ACTIVITY | no promote to GOOD |

While hold armed: silence/mid/weak **keep GOOD**; only clip or expiry → BAD.

### Why humans see nonsense

1. Path B uses the **same hold/GOOD** as Path A → LED cannot mean “packet”.
2. On this bench pp sits ~70–130 at all volumes; decode goes 296→0 while LED
   often stays GOOD via `pp_good`.
3. 10 s hold + mid-band sticky → volume changes don’t map to blink changes.
4. CLR does not clear AF hold (`afsk_rx_debug_clear`).

## Recommendations (product)

**R0 (polarity, current):** idle/bad → **150/150**; FCS-OK → **long OFF**.  
**R1:** FCS-only arm; delete `pp_good` promote.  
**R2:** Amplitude never grants packet blink.  
**R3:** Hold glanceable; expiry → 150/150.  
**R4:** CLR / mode entry → 150/150.

## Ask

See D39 note for latest human rule (long OFF on FCS). Do not implement
“good = short OFF.”

---

## Human constraint VIOLATED (frequency / pulse cadence) — 2026-11-21

**Human said (authoritative):** Mode 4 must **NOT** change pulse frequency;
**only** alter OFF duration. Cadence should stay like the other modes’ pulse
train.

**What shipped (D34/D36):**

| | Modes 1–3 | Mode 4 “GOOD” | Mode 4 “BAD” |
| --- | ---: | ---: | ---: |
| OFF | **150** | **260** | **50** |
| GAP (ON between OFFs) | **150** | **40** | **250** |
| OFF→OFF period | 300 | 300 (TICK) | 300 (TICK) |

Engineer claim: “TICK fixed → frequency fixed; only OFF duty.”  
**Human reality:** GAP = TICK−OFF so the **ON between pulses** swings 40↔250 ms.
With OFF=260/GAP=40 the four dips **merge** into near-continuous dark — not
“same pulse train as modes 1–3, only OFF tweaked.” Modes 1–3 stay **150/150**;
mode 4 never does.

**Ask architect:** restore human rule — **fixed pulse cadence** matching
modes 1–3 feel; vary **only OFF** within a band that does **not** collapse
the visible pulse train. Polarity per human table above still applies.

### Clarification (human): falling-edge spacing

Human: space between **falling edges must always be the same**.

Code fact: Mode 4 already keeps ↓→↓ = OFF+GAP = **TICK 300 ms** constant.
GAP=40 vs 250 is the complement inside that fixed period, not a different
↓ spacing. Human pain: extreme duty (260/40) makes the four ↓ edges
unreadable / feel like a different rhythm. Fix duty band so the fixed
300 ms cadence stays **visually** like modes 1–3 (150/150 family), not
just mathematically fixed.
