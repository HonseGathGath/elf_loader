# Tiered CTF Ideas: ELF Loader Vulnerabilities (Pwn/Binary Exploitation)

These ideas are intentionally focused on loader internals (stack bootstrap, auxv, ELF segment mapping, and interpreter handoff), matching the spirit of point 5 in `plans.md`: make players reason about process startup metadata and memory mapping, not just a bug in a target C program.

---

## Tier 1 — Easy (Warm-up)

## 1) AT_RANDOM Pointer Reuse Leak

### Description
The loader sets `AT_RANDOM` to memory that belongs to the loader's own stack frame instead of the synthetic target stack.

### Explanation
A player leaks `auxv`, recovers the `AT_RANDOM` pointer, and reads predictable or stale bytes from loader-owned memory. This can be chained to bypass canary-based checks in a downstream target challenge.

### Solution
Copy the 16 random bytes into the synthetic stack region and set `AT_RANDOM` to that copied location. Zero sensitive bootstrap buffers before jumping to the target.

## 2) Interpreter Base Overlap Primitive

### Description
The `--interp-base` value is trusted, letting the interpreter map over meaningful memory ranges if overlap checks are missing.

### Explanation
By picking a malicious `interp_base`, a player can force collisions with previously mapped regions (main binary segments or other mappings). This creates deterministic memory clobbering and control-flow hijack opportunities.

### Solution
Track all mapped ranges and reject any new mapping that overlaps an existing one. Enforce sane base address policies and optionally randomize interpreter base in challenge-hardening mode.

## 3) `p_filesz > p_memsz` Segment Overwrite

### Description
The loader maps based on `p_memsz` but copies based on `p_filesz` without enforcing `p_filesz <= p_memsz`.

### Explanation
A crafted ELF sets `p_filesz` larger than the mapped memory and turns segment loading into a controlled out-of-bounds write. This is a clean loader-side memory corruption primitive.

### Solution
Reject any PT_LOAD header where `p_filesz > p_memsz`. Fail closed before mapping or copying.

## 4) Entry Point Trust Abuse

### Description
The loader jumps directly to `e_entry` (or interpreter entry) without verifying that the entry is inside an executable mapped segment.

### Explanation
An attacker points entry to writable data (or a region made executable due to RWX staging mistakes), gaining direct code execution under loader control flow.

### Solution
Validate entry addresses against mapped PT_LOAD ranges and required execute permission. Reject invalid or non-executable entry targets.

---

## Tier 2 — Medium (Exploit Development)

## 5) PT_LOAD Alignment Arithmetic Wraparound

### Description
Page-alignment math for mapping size/address can overflow with extreme `p_vaddr`/`p_memsz` values.

### Explanation
If computed aligned size wraps, `mmap` may allocate too little, then later zero/copy operations corrupt adjacent mappings. Players craft headers to convert parser trust into an OOB write primitive.

### Solution
Use checked arithmetic for all range math (`start`, `end`, `aligned_start`, `aligned_size`). Reject on overflow or impossible user-space ranges.

## 6) PHDR Metadata Forgery via Writable Alias

### Description
`AT_PHDR` may point to metadata that is writable at runtime due to layout overlap or segment design.

### Explanation
After loader validation, attacker-controlled writes modify in-memory program headers before target-side checks consume them. This enables metadata time-of-check/time-of-use abuse.

### Solution
Guarantee PHDR memory is immutable while in use (read-only mapping, no writable alias). Validate PHDR location is within a safe PT_LOAD region.

## 7) Synthetic Stack Layout Corruption

### Description
The synthetic stack is hand-built with fixed assumptions (`argc/argv/envp/auxv`) and limited bounds validation.

### Explanation
By forcing long argument paths or metadata growth, a player causes overlap between argument area and auxv records, corrupting aux entries and steering target startup behavior.

### Solution
Precompute exact bootstrap stack footprint, enforce hard bounds, and build stack top-down with ABI alignment checks before handoff.

## 8) Register State Residue at Handoff

### Description
The handoff clears only a subset of registers before jumping to target entry.

### Explanation
Residual register values can leak loader pointers or useful runtime state. In chained pwn challenges, this leak reduces ASLR uncertainty and improves exploit reliability.

### Solution
Adopt a strict handoff ABI scrub policy: clear caller-saved and sensitive registers, then transfer control with a minimal deterministic register contract.

---

## Tier 3 — Hard (Loader-Internals Focus)

## 9) Main/Interpreter Cross-Clobber Chain

### Description
Two-stage mapping (main ELF + PT_INTERP ELF) is exploitable when range validation is incomplete across both images.

### Explanation
Players craft a pair where one image's PT_LOAD intentionally overlaps critical pages of the other image after relocation/base application. The final jump path yields controlled corruption before execution stabilizes.

### Solution
Perform global collision validation across all images before any mapping is committed. Consider transactional mapping: validate everything first, map second.

## 10) MAP_FIXED Abuse Against Existing Process Mappings

### Description
Using `MAP_FIXED` blindly allows replacement of already-mapped memory in the current loader process.

### Explanation
A malicious ELF chooses addresses that overwrite loader-relevant state or nearby mappings, converting map time into an overwrite primitive.

### Solution
Use preflight range checks and safer mapping strategy (`MAP_FIXED_NOREPLACE` when available). Refuse mappings that are not in a strictly allowed sandbox range.

## 11) RWX Staging Window Persistence

### Description
PT_LOAD segments are initially mapped RWX before final permissions are applied.

### Explanation
If any failure path, signal race, or challenge-specific hook leaves staging permissions active, attacker data becomes executable and mutable at the same time, simplifying shellcode-style chains.

### Solution
Prefer W^X discipline: map as RW for loading, then switch to RX/RW final states with fatal-on-error semantics. Keep failure paths atomic and fully rollback mappings.

## 12) Auxv Semantic Confusion Challenge

### Description
The loader emits a synthetic auxv that can be intentionally malformed (ordering, duplicated keys, mismatched values) for challenge mode.

### Explanation
Solver must exploit logic assumptions in target startup code that trusts auxv semantics (e.g., wrong `AT_BASE`, spoofed `AT_EXECFN`, or conflicting entries). This keeps exploitation loader-oriented and metadata-driven.

### Solution
In real mode: enforce canonical auxv generation (single key instances, validated values, deterministic ordering). In challenge mode: gate malformed auxv behind a compile-time flag.

---

## Useful Add-Ons for Challenge Authors

## Difficulty Tuning Knobs

- **Easy:** fixed addresses, verbose loader logs, symbols kept, no PIE.
- **Medium:** partial ASLR, reduced logs, limited hints, mixed static/dynamic targets.
- **Hard:** stripped binaries, noisy crash signals removed, strict timeouts, minimal feedback.

## Suggested Challenge Matrix

| Idea | Primitive | Difficulty | Solver Skill Focus |
|---|---|---|---|
| AT_RANDOM Pointer Reuse Leak | info leak | Easy | auxv parsing, canary bypass setup |
| Interpreter Base Overlap Primitive | memory overlap | Easy | ELF segments, mapping intuition |
| `p_filesz > p_memsz` Segment Overwrite | OOB write | Easy | header crafting, deterministic corruption |
| Entry Point Trust Abuse | control-flow redirection | Easy | entry validation gaps |
| PT_LOAD Alignment Arithmetic Wraparound | integer overflow -> OOB | Medium | overflow modeling, mmap effects |
| PHDR Metadata Forgery via Writable Alias | TOCTOU metadata abuse | Medium | runtime header semantics |
| Synthetic Stack Layout Corruption | stack bootstrap corruption | Medium | argc/argv/envp/auxv layout |
| Register State Residue at Handoff | info leak / exploit reliability | Medium | ABI-level pwn details |
| Main/Interpreter Cross-Clobber Chain | multi-image clobber | Hard | loader graph reasoning |
| MAP_FIXED Abuse Against Existing Process Mappings | arbitrary remap overwrite | Hard | process memory cartography |
| RWX Staging Window Persistence | W^X bypass | Hard | race/failure-path exploitation |
| Auxv Semantic Confusion Challenge | logic-driven bootstrap exploit | Hard | auxv semantics, startup internals |

## Organizer Checklist (Practical)

1. Define intended exploit path in one sentence (what primitive + final goal).
2. Keep one dominant bug per challenge; avoid accidental alternate solves.
3. Test reproducibility on clean VM/container with same kernel/libc profile.
4. Verify mitigation assumptions (NX/PIE/RELRO/canary) match intended difficulty.
5. Prepare a minimal hint ladder: metadata hint -> primitive hint -> chain hint.
