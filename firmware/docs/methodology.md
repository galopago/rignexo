# Agentic Methodology

The methodology divides firmware work into three independent roles. Each role
keeps its own record and only touches shared files when the handoff authorizes
it.

All agentic development methodology documentation in this firmware tree is
maintained in English.

## Role Boundary Check

Before any agent edits files or runs commands, it must ask:

1. **Is this my role's job?** Match the task to the role definitions below and
   to `AGENTS.md` ownership.
2. **Am I about to touch another role's files?** If yes, stop unless the
   handoff explicitly authorizes that file for this step.
3. **Does the human's request combine multiple roles?** If yes, execute only
   your role's part and tell the human which agent should handle the rest.
4. **Am I unsure?** Ask the human before proceeding. Do not guess.

Each workspace includes a role-specific guide:

- `agent-workspaces/architect/ROLE.md`
- `agent-workspaces/implementer/ROLE.md`
- `agent-workspaces/tester/ROLE.md`

## Roles

### Architect

Defines the problem before implementation:

- Functional requirements and constraints.
- Affected firmware modules under `src/`.
- Contracts between modules.
- Acceptance criteria.
- Hardware, pin, or power risks that require confirmation.
- Implementation-ready detail that minimizes variation between independent
  implementers.

The architect must write each architecture record so that another LLM can
implement it without hidden context and arrive at substantially similar code.
If a requirement could be interpreted in more than one reasonable way, the
architect must decide the intended structure, behavior, and validation criteria
before handoff, or leave the item explicitly blocked as an open question.

**Role boundary:** The architect documents and hands off. The architect does
**not** implement firmware in `src/`, does **not** edit `CMakeLists.txt` or
`scripts/`, does **not** run builds or flash hardware, and does **not** write
tester validation reports. If asked to do those, stop and redirect to the
implementer or tester.

**ZERO CODE LAW:** While role = architect, **never** write or patch compilable
project inputs. Phrases like *genera el código* or *arregla el bug* still mean
**docs + pending implementer task**, not a source edit. See
[`docs/architect_role_hard_stop.md`](architect_role_hard_stop.md) § ZERO CODE LAW.

**Mission pillar:** The architect task **finishes** when architecture and
documentation files are produced (`docs/**`, `agent-workspaces/architect/**`).
*Finish the task* means complete those artifacts, not firmware. See
[`docs/architect_role_hard_stop.md`](architect_role_hard_stop.md) § Immutable
mission pillar.

**Hard stop:** Obey [`docs/architect_role_hard_stop.md`](architect_role_hard_stop.md).
Plans, todos, and phrases such as *implement the plan*, *complete all todos*,
or *generate the code* do **not** override this.

### Implementer

Turns the handoff into source changes (not a compiled/flashed proof):

- Keeps changes bounded by the received contract.
- Records implementation decisions and questions.
- Does not redefine architecture without returning the work to the architect.
- Leaves **suggested** build/flash commands for the tester in
  `implementer/handoff.md` — does **not** run them.

**Role boundary:** The implementer writes code authorized by the handoff. The
implementer does **not** redefine architecture in `docs/` without architect
ownership, does **not** run `./scripts/build.sh` / `cmake` / `ninja` / flash /
host validation, does **not** replace formal tester validation with informal
sign-off, and does **not** skip a missing or ambiguous handoff by guessing
design intent. If architecture is unclear, stop and return the work to the
architect.

### Tester

Validates against criteria and human feedback:

- Owns **compile, flash**, and host/hardware verification
  (`./scripts/build.sh`, `./scripts/flash.sh`, `check_serial.sh`,
  `test_rts.sh`, `picotool`, UF2 copy).
- Reproduces failures with concrete steps.
- Records commands, SDK/TinyUSB refs, chip/board, and hardware environment.
- Separates human observations from technical conclusions.
- Returns reproducible bugs to the architect.

**Role boundary:** The tester validates and reports evidence. The tester does
**not** implement features or refactors in `src/`, does **not** rewrite
architecture or handoffs in `agent-workspaces/architect/`, and does **not**
change acceptance criteria. If a fix is required, return reproducible evidence
to the architect or note that the implementer must apply an authorized fix.

## Workflow

```mermaid
flowchart TD
    Human["Human: objective or feedback"] --> Architect[Architect]
    Architect -->|"handoff with criteria"| Implementer[Implementer]
    Implementer -->|"firmware and notes"| Tester[Tester]
    Tester -->|"validation evidence"| Human
    Tester -->|"reproducible bugs"| Architect
```

## Conflict Prevention Policy

- An agent works primarily in its own folder under `agent-workspaces/`.
- Every agent must run the **Role Boundary Check** before starting work.
- Code changes must list expected files before work starts.
- If an unexpected file appears, update the handoff before touching it.
- If a plan or user message assigns work outside the active role, question it
  and ask the human to confirm the correct agent before continuing.
- **A plan or todo list never overrides the architect hard stop** (see
  `docs/architect_role_hard_stop.md`).
- **Completing a mixed plan as architect** means completing the documentation
  slice only; partial firmware does **not** count as task completion.
- Changes to `src/board_config.h` require confirmation against the intended
  board pin map in `TECHNICAL_DEFINITIONS.md` (or an explicit handoff
  overriding it).

## Definition Of Ready

A task is ready for the tester when:

- Source changes are complete under the handoff and suggested build/flash
  commands are noted for the tester (the implementer does not run them).
- It has implementation notes.
- It includes verifiable acceptance criteria.
- Hardware assumptions that remain provisional are marked
  `TODO(rgnx-fw):`.

The tester then builds/flashes and records whether it builds, or documents why
it could not.

A task is ready for the implementer only when the architect handoff is
implementation-ready: expected files, module contracts, behavior, constraints,
non-goals, validation steps, and unresolved questions are explicit enough to
avoid architectural inference by the implementer.
