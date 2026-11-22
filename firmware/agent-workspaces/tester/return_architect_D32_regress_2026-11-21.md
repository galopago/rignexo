# Return to architect — D32 WEAK **REGRESS** (matrix FAIL)

Date: 2026-11-21  
From: tester  
Handoff: `RGNX_FW_APRS_TNC_RX_WEAK` (W1–W5)

## Verdict

**FAIL / REGRESS.** Do **not** CLOSE. D32 live decode is **worse** than D31
A+ on the same bench, fixture, and host path.

## Matrix (PC/VLC vol, WAV loop, KISS CLR `0x0D` / STAT `0x0C`)

| Level | Vol | D32 result | Compare D31 |
| --- | --- | --- | --- |
| Quiet | ~35% | **FAIL** `dcd=0`, all counters 0 | D31@34%: `dcd=1`, flag/bad_fcs↑, ok=0 |
| Ref | ~73% | **FAIL** same signature | D31@73%: **ok=1**, KISS H-REPEAT |
| Loud | **95%** | **FAIL** same signature | D31 loud: ok=64, kiss_ok=25, A01–A10 |

Every D32 poll across all three levels:

- `dcd=0`, `flag=0`, `bits=0`, `ok=0`, `kiss=0`, `skip=0`, `conf=0`
- ADC sticky still ~1960…2105 (signal present at converter)
- `n_sl=9`, RX timers advance
- TX host: no own payload echo (loud pass)

## Root claim (tester)

D32 item **“DCD on raw ene gates idle HDLC rain”** (and/or related W1–W5
front-end) **never opens** on this hardware inject. Demod never runs → total
silence vs D31 which decoded at 73% and loudly at high volume.

This is not a “quiet still hard” outcome; it is a **regression that kills
loud/ref** that already worked.

## Ask

1. **Revert or disable** the hard DCD gate that blocks when `ene` looks like
   this bench (or retune so D31-class inject opens DCD).  
2. Re-prove offline O0/O1 **and** require live **73% + loud** no-regress
   before another quiet push.  
3. Prefer keep D31 A+ as baseline until live matrix matches or beats D31.  
4. Do not stack more weak-AF features on top of a gate that never asserts.

Tester: D32 matrix **CLOSED FAIL**. Waiting new ID / revert order.
