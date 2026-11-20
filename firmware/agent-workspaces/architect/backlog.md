# Architect backlog — `rignexo/firmware`

## Open

- Bench-tune VOX threshold / hang time after `AUDIO_VOX` implementation evidence
  (tester feedback).
- Optional later: Icom CI-V or Yaesu profile as additional modes (not v1).

## Done

- `RGNX_FW_CAT_STATE_RAM_V1` — CAT set/get soft state in RAM (2026-11-20); 
  implementer pending.
- `RGNX_FW_CAT_PTT_V1` — mode 3 Kenwood CAT PTT + initial stubs (2026-11-20);
  tester partial PASS.
- `RGNX_FW_DEVICE_MODES_V1` — boot-time persistent COMPOSITE vs AUDIO_VOX;
  normative `docs/device_modes_architecture.md` (2026-11-19). Implementer
  pending.
- `RGNX_FW_PINMAP_V1` — RP2350-Zero primary pin map + RP2040-Zero same-map;
  mode pin defines (2026-11-19).
- `RGNX_FW_ASBUILT_V1` — agentic scaffold + as-built architecture (2026-11-19).
