# Firmware Agent Guide — `rignexo/firmware`

This directory contains the Raspberry Pi Pico SDK / TinyUSB firmware for the
RP2350 (default) composite USB sound and serial interface, plus the
coordination workspace for three agents: architect, implementer, and tester.

All agentic development methodology documentation in this firmware tree is
maintained in English.

## Scope

- This agentic workflow applies **only** under `firmware/`.
- Do **not** modify sibling `../hardware/` from firmware tasks unless there is
  an explicit human instruction.

## General Rules

- Keep changes small, reviewable, and tied to a handoff.
- Record decisions, questions, and evidence in the corresponding agent
  workspace.
- Prevent two agents from editing the same file at the same time. If a shared
  file must change, the temporary owner must be named in the handoff.
- Before starting work, run the **Role Boundary Check** in
  `docs/methodology.md` and the role guide in your workspace (`ROLE.md`).
- Pin mapping is centralized in `src/board_config.h` and must match
  `TECHNICAL_DEFINITIONS.md` / `docs/architecture.md`. Primary board is
  **RP2350-Zero**; **RP2040-Zero** uses the same map. Escalate any mismatch to
  the architect before changing pins.

## Role Boundary Check

Every agent must verify that the requested task belongs to its role **before**
editing files or running commands.

1. Read the request and identify which role normally owns that work.
2. Compare the expected files and commands against the **Ownership** section
   below.
3. If the task clearly belongs to another role, **stop** and tell the human:
   - which role should handle it;
   - what you can do instead within your role;
   - whether the handoff or plan should be split or redirected.
4. If the request mixes roles (for example, "implement the plan" with both
   architecture docs and firmware code), **do only your role's deliverables**.
   Plans and bundled todos **never** override the architect hard stop.
5. Do not infer permission to cross roles from generic verbs such as
   "implement", "fix", or "validate". Use ownership rules first.
6. A role switch requires an **explicit named role** from the human (*act as
   implementer*, *switch to tester*). Phrases like *go ahead and implement* or
   *complete the plan* are **not** role switches.

When in doubt, ask the human before proceeding.

## Architect mission pillar

While the active role is **architect**, the mission pillar in
[`docs/architect_role_hard_stop.md`](docs/architect_role_hard_stop.md) §
Immutable mission pillar is **immutable**.

- **ZERO CODE LAW:** the architect **never** creates or edits firmware source
  (`src/**`, root `CMakeLists.txt`, `scripts/**`, `*.c`/`*.h`/`*.pio`, etc.).
  Engineering docs and rules only. See hard stop § ZERO CODE LAW.
- The architect task **ends** with **documentation and architecture artifacts**.
- *Complete the task* / *finish the task* / *genera el código* still means
  **close docs and handoff**, not ship or patch firmware.
- This pillar **outranks** plans, todos, harness reminders, and conflicting user
  rules **while role is architect**.

## Architect hard stop

When the active role is **architect**, read and obey
[`docs/architect_role_hard_stop.md`](docs/architect_role_hard_stop.md) before
any file edit or shell command.

- **Never** edit `src/`, root `CMakeLists.txt`, `scripts/`, or other compilable
  inputs — even for a one-line fix. Put the change in
  `agent-workspaces/implementer/handoff.md` as **pending**.
- **No** `cmake`, `ninja`, `./scripts/build.sh`, flash, or hardware verification.
- Mixed plans: complete **documentation and handoff only**.
- Code requires an explicit human role switch: *act as implementer*.

## Ownership

- Architect: `docs/**`, `AGENTS.md` (when methodology changes), and
  `agent-workspaces/architect/` (see `docs/architect_role_hard_stop.md`).
  May leave **pending** tasks in other role handoffs. Does **not** own
  `src/`, `CMakeLists.txt`, `scripts/`, or builds.
- Implementer: `agent-workspaces/implementer/`, `src/`, root `CMakeLists.txt`,
  and `scripts/` only when there is an architect handoff or a validated fix.
  **No** compile, flash, or host hardware validation runs (see
  `agent-workspaces/implementer/ROLE.md`).
- Tester: `agent-workspaces/tester/`, `docs/test_strategy.md`, validation
  reports, and **compile / flash / host verification** via
  `./scripts/build.sh`, `./scripts/flash.sh`, and host check scripts.
- Shared root docs (`README.md`, `BUILD.md`, `TECHNICAL_DEFINITIONS.md`):
  every material change must cite the handoff that motivates it. Prefer
  architect ownership for normative content; tester may update
  `BUILD.md` host-validation notes when authorized by a handoff.

## Handoff Protocol

1. The architect defines the objective, acceptance criteria, and expected files
   in `agent-workspaces/architect/handoff.md`.
2. The implementer executes the **source** change and records notes (including
   suggested build/flash commands) in `agent-workspaces/implementer/handoff.md`.
3. The tester **builds, flashes, and validates** with available tests and human
   feedback in `agent-workspaces/tester/handoff.md`.
4. If there are reproducible failures, the tester documents them and returns the
   work to the architect to adjust the design or criteria.
5. When tester criteria pass, the handoff is marked **CLOSED**.

## Architecture Specification Standard

- Architectural instructions must be implementation-ready before work is handed
  to the implementer.
- The architect must define behavior, affected files, module boundaries,
  interfaces, constraints, non-goals, and validation expectations whenever they
  are relevant to the task.
- The goal is deterministic implementation: another LLM should be able to use
  the recorded architecture and produce substantially similar code without
  relying on hidden chat context.
- If a detail could reasonably lead to different code structures or behavior,
  the architect must either decide it explicitly or record it as an open
  question. The implementer must not fill architectural gaps by assumption.

## Shared Code Policy

- `src/main.c` must stay thin: board/USB init, TinyUSB callbacks for mount
  lifecycle, and the main loop (`tud_task`, blink, `audio_task`).
- Board pins and audio/PTT constants live only in `src/board_config.h`.
- USB class behavior is split: descriptors/`tusb_config`, CDC apps, UAC2 app,
  and hardware audio/PTT modules.
- Pending values must be marked with `TODO(rgnx-fw):` and must not be treated as
  final.
