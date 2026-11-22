# Note to architect — Mode 4 AF LED: OFF-time encoding OK, must be **eye-obvious**

Date: 2026-11-21  
From: tester + human  
Clarifies / supersedes misread of “intra-blink” constraint in chat.

## Human position (corrected)

1. Encoding AF quality as **how long the LED stays OFF** in each blink **is
   fine** — that is the intended metaphor.  
2. What fails today: the difference is **not noticeable** to a human eye.  
3. Do **not** abandon OFF-duty for a totally different scheme unless forced;
   **make the OFF contrast large enough to see cold**, without a briefing.  
4. Keep mode identity: still **4** OFF events + quiet ON (structure unchanged).

## What is wrong numerically (today)

| | BAD | GOOD |
| --- | ---: | ---: |
| `OFF_*_MS` | **80** | **220** |
| `TICK_MS` | 300 (fixed) | 300 |
| GAP = TICK−OFF | 220 | 80 |

Both still look like “four quick dips on green.” 80 vs 220 ms dark is too
close for casual viewing; product already admits this is not a 50% blink.

## Ask — same encoding, **obvious** OFF contrast

Keep: fixed inter-blink period (or document if TICK must grow), 4 pulses,
GAP = TICK − OFF, D35 hold OK.

Change: **much larger** GOOD vs BAD OFF widths so a naïve observer answers
“long dark flash” vs “tiny flicker” in a couple of seconds.

Guidance (starting points for architect to lock — not sacred):

| | Suggested direction |
| --- | --- |
| BAD OFF | Very short flicker, e.g. **30–50 ms** (still visible as a dip) |
| GOOD OFF | Clearly long dark, e.g. **≥250–350 ms** — if needed **raise TICK**
  (e.g. 400–500 ms) so GOOD OFF can be long **and** GAP still readable |
| Rule of thumb | GOOD OFF should feel ~**3–5×** BAD OFF or more, not ~2.7× (220/80) |
| Hold | Keep 10 s GOOD after packet; transition GOOD→BAD must also be obvious
  once hold ends (long dips → short flickers) |

## Acceptance (human, no lecture)

1. “Are the dark flashes long or short?” → matches GOOD/BAD without explaining
   milliseconds.  
2. Still counts **four** dips as mode 4.  
3. Reject another ±20 ms tweak around 80/220.

## Non-goals (this note)

- Replacing OFF-duty with stutter/double-dip **unless** OFF contrast still
  fails after a serious widen.  
- Dropping AF-from-LED entirely as first response.

## Ask

Amend normative `OFF_GOOD` / `OFF_BAD` (and TICK if required) for
**field-obvious** long vs short dark flashes; implementer + human smoke.
