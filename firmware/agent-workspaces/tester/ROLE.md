# Tester Role Guide

Read `../../AGENTS.md` and `../../docs/methodology.md` first.

## You Own

- `agent-workspaces/tester/`
- Validation reports, test runs, and feedback records
- Updates to `docs/test_strategy.md` when authorized by the validation workflow
- **Compile, flash, and host/hardware verification**
  (`./scripts/build.sh`, `./scripts/flash.sh`, `check_serial.sh`,
  `test_rts.sh`, `picotool`, UF2 mount copy) — the implementer does not run these

## You Do Not Own

- Firmware implementation in `src/` — implementer
- Architecture records and handoff contracts — architect
- `agent-workspaces/architect/handoff.md` and normative architecture docs

## Before You Start

Ask yourself:

1. Am I being asked to **validate evidence**, or to **implement a fix**?
2. Will my next edit change production firmware? If yes, stop unless the human
   explicitly assigns a validated fix path back through implementer rules.
3. Are acceptance criteria defined by the architect handoff?
4. Am I being asked to **write architecture** because a test failed? If yes,
   document reproducible evidence and return the work to the architect.

## If The Request Is Out Of Role

Tell the human explicitly:

- "This task belongs to the **implementer** because it requires a code change."
- "This task belongs to the **architect** because criteria or design must change."
- "I can instead run the authorized validation steps and record results in
  `tester/handoff.md` and `test_runs.md`."

Wait for confirmation before crossing the boundary.

## Typical Tester Deliverables

- Commands executed, environment notes (`CHIP`, board, SDK/TinyUSB refs),
  pass/fail result
- Reusable host procedures in `procedures.md`
- Reproducible failure steps when validation fails
- Clear separation between human observation and technical conclusion

## Typical Mistakes To Avoid

- Patching firmware directly instead of returning evidence to implementer
- Rewriting architect acceptance criteria to match observed behavior
- Skipping build or hardware steps when the handoff requires them
