# Implementer handoff — `rignexo/firmware`

## Current — `RGNX_FW_CAT_IF_HAMLIB_V1` (**implemented** 2026-11-20)

```text
ID: RGNX_FW_CAT_IF_HAMLIB_V1
Date: 2026-11-20
Role: implementer only (no build/flash)
Normative: docs/cat_ptt_mode_architecture.md § IF frame (Hamlib / WSJT-X)
Architect: D20
Status: implemented 2026-11-20 — awaiting tester (WSJT-X Test CAT)
```

### Changes made

- `src/cat_kenwood.c` — rewrote `reply_if` to fixed **38-byte** wire frame
  (37-char payload + `;`). Removed `+` RIT sign pattern.
- Field map: freq from A/B via `fr_vfo`; TX=`ptt_is_active`; MD=`md_code`;
  VFO=`fr_vfo`; split=`fr_vfo != ft_vfo`.
- Cold-start idle target:
  `IF00014074000      000000000020000000;`
- `FR` set now accepts `0`/`1`/`2` (A/B/MEM) so IF VFO byte can track MEM.
- Soft-state set/get for FA/FB/MD/FW/… unchanged; PTT/RTS unchanged.

### Suggested tester

```text
IF;   → IF00014074000      000000000020000000;  (len 38, PTT idle)
FR1; IF;  → info[30] == '1'
FR0; IF;  → info[30] == '0'
FA00007074000; FA; IF;  → freq digits match
# WSJT-X: Rig=TS-2000, PTT=CAT, Test CAT → no "unsupported VFO None"
```

### Acceptance (implementer)

- [x] `reply_if` emits 38-byte Hamlib frame; cold-start string matches
- [x] VFO/split follow FR/FT; TX follows PTT; MD follows soft
- [x] Notes; **no** hardware PASS claim

---

## Prior — `RGNX_FW_CAT_STATE_RAM_V1` (**implemented** 2026-11-20)

Linux suite PASS; app CAT FAIL → fixed by IF-Hamlib above.

## Prior — `RGNX_FW_CAT_PTT_V1` / MODE_SWITCH / LED / PINMAP
