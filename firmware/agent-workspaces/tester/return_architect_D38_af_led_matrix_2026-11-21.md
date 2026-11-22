# RETURN — D38 AF LED volume matrix (debug STAT closed)

Date: 2026-11-21  
From: tester  
ID: `RGNX_FW_MODE4_AF_LED_DEBUG`  
To: architect

## Verdict

| Item | Result |
| --- | --- |
| D38 STAT dump (`Laf` / `Lafcfg` / `Lled`) | **PASS** — present every poll |
| Volume ↔ LED coherence (product intent) | **FAIL** — confirmed with numbers |
| Matrix levels | 51% / 63% / 75% / **95%** complete |

## Matrix (WAV loop, KISS CLR `0x0D` / STAT `0x0C`, ~110s each)

| Level | ok / kiss | af_pp (sampled) | LED |
| --- | --- | --- | --- |
| 51% | 0 / 0 | ~71–118 | 9/11 GOOD, 2× `pp_bad` |
| 63% | 0 / 0 | ~85–116 | 10/11 GOOD, 1× `hold_exp` |
| 75% | 0 / 0 | ~80–125 | 11/11 GOOD `pp_good` |
| 95% | 0 / 0 | ~73–129 | 9/11 GOOD, 1× `hold_exp`, 1× `pp_bad` |

Never saw `af_reason=fcs` this session. `led_off_ms` tracks hold: 260 when
GOOD, 50 when BAD. `Lafcfg` matches board: pp 40/80/120, hold 10000.

## Root cause (evidence-backed)

1. **Assist `pp≥120` arms 10 s GOOD without FCS-OK** — LED shows “packet
   quality GOOD” while `ok=0`.
2. **Under hold, mid/weak `af_pp` keeps GOOD** (by design D37) — volume down
   does not immediately show BAD.
3. **CLR does not clear AF hold** — `afsk_rx_debug_clear` leaves `s_af_hold_*`.
4. Sampled `af_pp` often &lt;120 with `reason=pp_good` → brief ≥120 spikes
   between 12 s STAT polls re-arm hold.

## Session note (decode)

All four levels this session: `ok=0` despite `dcd=1` and rising `bad_fcs`.
Prior D33 loud/73%/63% had FCS-OK. Sticky ADC range stayed ~1970…2105 across
levels. Possible inject/loop-phase variance — **does not invalidate** the LED
vs metric finding (LED GOOD without packets is proven). Re-smoke loud decode
optional if human wants RX health check; AF-LED design return stands.

## Ask architect

Open a follow-up ID (suggested product fixes — choose one policy):

- **A (preferred):** GOOD only from FCS-OK (+ hold); drop or severely raise
  `pp_good` assist so amplitude alone never arms GOOD.
- **B:** Keep assist but much shorter hold and/or require sustained pp, not
  single 200 ms window.
- CLR / mode entry: clear AF hold + force BAD so debug and UX reset cleanly.

D38 debug itself can be **CLOSED PASS** (instrumentation delivered). Product
LED behavior remains open pending design choice.
