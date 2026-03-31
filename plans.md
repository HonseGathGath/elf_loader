# CTF Challenge Plans with the Custom ELF Loader

This document contains concrete challenge ideas and an implementation roadmap using this custom loader.

## Goals

1. Turn this repo into a reusable CTF challenge factory.
2. Keep challenge binaries small and debuggable.
3. Offer multiple difficulty tiers using the same loader core.

## Current baseline

- Custom PT_LOAD mapper with per-segment protections.
- Minimal synthetic stack + auxv setup.
- Optional PT_INTERP loading support.
- Two targets:
  - `target.c` (benign)
  - `vuln_target.c` (intentional stack overflow)

## Challenge Ideas

## 1) Classic ret2win starter (Easy)

### Idea
Use `vuln_target.c` with a hidden `win()` and straightforward stack overflow.

### Implementation
- Keep symbols and debug info during development.
- Offer a stripped release variant for competition.
- Provide deterministic addresses via non-PIE flags.

### Learning outcome
- Offset discovery
- Control RIP/EIP
- ret2win basics

## 2) Partial RELRO / GOT overwrite (Easy-Medium)

### Idea
New target with vulnerable write primitive and dynamic linking, allowing GOT overwrite.

### Implementation
- Add `targets/got_target.c`.
- Build as dynamic executable (remove `-static` for this specific target).
- Add menu-driven primitive: arbitrary small write.

### Learning outcome
- GOT/PLT internals
- Hijacking control flow through imported functions

## 3) Format string -> leak + pivot (Medium)

### Idea
Introduce a `printf(user_input)` bug and require leak-then-exploit flow.

### Implementation
- Add `targets/fmt_target.c`.
- Require leaking libc/stack pointer before final payload.
- Use loader verbosity to show entry transitions for debugging.

### Learning outcome
- `%p` leak strategy
- stack positioning
- staged exploitation

## 4) ROP chain under constrained gadgets (Medium-Hard)

### Idea
Provide overflow with NX active and no direct `win()` call path.

### Implementation
- Keep code segment non-writable and stack non-executable.
- Add challenge objective requiring ret2libc or syscall-oriented ROP.

### Learning outcome
- gadget harvesting
- calling conventions
- chain reliability

## 5) Loader-oriented challenge (Hard)

### Idea
Participants exploit target assumptions in synthetic auxv/stack handling.

### Implementation
- Add optional malformed auxv mode in loader behind compile-time flag.
- Create target that validates specific auxv values before revealing goal.
- Force participants to reason about loader internals, not just target C bugs.

### Learning outcome
- ELF process bootstrap details
- auxv semantics
- runtime metadata abuse scenarios

## Challenge Matrix

| Challenge | Primary bug | Difficulty | Binary type |
|---|---|---|---|
| ret2win | stack overflow | Easy | static, non-PIE |
| got_target | arbitrary write | Easy-Medium | dynamic |
| fmt_target | format string | Medium | dynamic |
| rop_target | overflow + NX | Medium-Hard | static/dynamic |
| loader_auxv | logic/metadata abuse | Hard | mixed |

## Repo Evolution Plan

## Phase 1 (Now)

- [x] Refactor loader API and metadata flow.
- [x] Add vulnerable starter target.
- [x] Add baseline docs.

## Phase 2 (Next)

- [ ] Add `got_target.c` and `fmt_target.c`.
- [ ] Per-target Makefile flags (static vs dynamic).
- [ ] Add `scripts/run_<target>.sh` helpers.

## Phase 3 (Quality)

- [ ] Add automated smoke tests that run loader + targets.
- [ ] Add deterministic CI job for builds and basic runtime checks.
- [ ] Add stripped release packaging script for CTF deployment.

## Possible Extensions

- Randomized interpreter base for anti-script-kiddie hardening in advanced challenges.
- Optional seccomp profile in targets to constrain syscall surface.
- Multi-stage loader mode (load helper ELF first, then challenge ELF).
- Built-in tracing mode to teach participants loader behavior interactively.

## Authoring Checklist (per new challenge)

1. Define bug class and intended exploit path.
2. Decide flags (PIE/NX/RELRO/canary/static vs dynamic).
3. Implement target in `targets/`.
4. Add Makefile target and run script.
5. Validate with loader under normal + verbose mode.
6. Produce organizer writeup and solver hints.
