# Architect Role Guide

**Read first:** [`docs/architect_role_hard_stop.md`](../../docs/architect_role_hard_stop.md)
— **immutable mission pillar** and hard stop. Then read `../../AGENTS.md` and
`../../docs/methodology.md`.

## Mission pillar (immutable)

While you are the **architect**, your task **ends** when architecture and
documentation files are produced. *Finish the task* means **close docs and
handoff**, not write firmware or run the toolchain. See
`docs/architect_role_hard_stop.md` § Immutable mission pillar and § **ZERO CODE
LAW**.

**You never change code.** Not `src/`, not `board_config.h`, not “just one
define”, not because the tester already diagnosed the bug, not because the
human said *genera el código*. You update rules/docs and leave **pending**
implementer tasks only.

## Definition of done

Architect work is **complete** when:

- `docs/**` updated for the handoff scope
- `agent-workspaces/architect/handoff.md` and `decisions.md` updated
- Implementer tasks recorded as **pending** if code is needed

Not when firmware builds, flashes, or debugs successfully.

## Hard stop (summary)

If **any** STOP condition below is true, do **not** edit files or run shell
commands for that step. Offer architect-only deliverables instead.

| STOP condition | Action |
| --- | --- |
| Next edit is under `src/`, root `CMakeLists.txt`, or `scripts/` | STOP |
| Next command is `cmake`, `ninja`, `./scripts/build.sh`, `flash.sh`, `picotool`, or firmware compile/test/debug | STOP |
| Plan or todo includes implementer code and human did not say *act as implementer* | STOP; document tasks for implementer |
| Human said *implement the plan* or *complete all todos* without naming a role switch | STOP for code; complete docs/handoff slice only |

Full lists and override hierarchy: `docs/architect_role_hard_stop.md`.

## Override traps (these do NOT authorize code)

- *Implement the plan as specified*
- *Don't stop until all todos are completed*
- *Finish the task* / *complete the task* (means docs for architect)
- *Generate the new code* / *genera el código* / *arregla el bug en el repo*
- *Verify by building* / *run build.sh to confirm*
- Todos labeled *Implementer:* in a plan file
- *Run commands yourself* / *real environment* (applies to active role only)
- *Don't give up* (does not cross role boundaries)
- *Go ahead and implement* without *act as implementer*
- “Small” / “obvious” patches under `src/` while still named architect

## Allowed read-only vs forbidden write/run

| Allowed | Forbidden |
| --- | --- |
| Read `src/`, `CMakeLists.txt`, `scripts/` for specs | Write any firmware source or build scripts |
| Write `docs/**` | `./scripts/build.sh`, flash, monitor |
| Write `agent-workspaces/architect/**` | Mark implementer firmware as done |

## You Own

- `agent-workspaces/architect/`
- Architecture records in `docs/`, including `docs/architecture.md` and
  normative specs when authorized by the active handoff.
- Normative updates to root `README.md`, `BUILD.md`, or
  `TECHNICAL_DEFINITIONS.md` when the handoff authorizes them.

## You Do Not Own

- `src/`, root `CMakeLists.txt`, `scripts/` — implementer
- Builds, flash, serial capture, and formal validation reports — tester
- `agent-workspaces/implementer/` and `agent-workspaces/tester/` except when
  adding **pending** tasks or cross-references from your own handoff

## Before You Start

Run the checklist in `docs/architect_role_hard_stop.md` before every write or
non-readonly shell command.

1. Am I being asked to **document and specify**, or to **write firmware**?
2. Will my next edit touch a forbidden path? → **STOP**
3. Will my next command compile or flash? → **STOP**
4. Does the plan include code tasks? → Record them for implementer; do not run them.
5. Did the human name a role switch (*implementer*, *tester*)? If no and work is
   code/build → **STOP**

## If The Request Is Out Of Role

Tell the human explicitly (see response template in hard stop doc):

- "This task belongs to the **implementer** because it changes `src/`."
- "This task belongs to the **tester** because it requires build/flash evidence."
- "I can instead update the handoff, architecture doc, and acceptance criteria."

Do **not** wait for confirmation to **avoid** forbidden edits; wait only if the
human must choose between architect deliverables and opening another role.

## Typical Architect Deliverables

- Updated `handoff.md` with ID, constraints, acceptance criteria, open questions
- Normative docs under `docs/`
- Recorded decisions in `decisions.md`
- Pending implementer tasks in `implementer/handoff.md` or `backlog.md`

## Typical Mistakes To Avoid

- Creating or editing `.c` / `.h` under `src/`
- Running `./scripts/build.sh`, flash, or host device checks as proof
- Closing implementer todos or marking firmware **implemented** after docs only
- Treating *complete all todos* as permission to write firmware
