# Custom ELF Loader (CTF Playground)

This repository contains a small custom ELF loader and sample binaries that can be loaded by it.

## What is included

- `src/loader.c`, `src/loader.h`: ELF mapping logic.
- `src/main.c`: loader entrypoint, argument parsing, synthetic stack + auxv setup, and handoff to ELF/interpreter entry.
- `targets/target.c`: simple benign target.
- `targets/vuln_target.c`: intentionally vulnerable target for CTF practice.

## Features

- Refactored loader API via `load_elf_image(...)` returning metadata (`entry`, `AT_PHDR` location, `PT_INTERP` path).
- ELF validation checks (magic + ELF64 checks + safer read handling).
- Segment mapping now honors ELF `PF_R/PF_W/PF_X` permissions (not always RWX).
- Optional runtime flags:
  - `--verbose`
  - `--stack-size <bytes>`
  - `--interp-base <hex|dec>`

## Build

```bash
make clean
make
```

Artifacts:

- `bin/loader`
- `bin/target`
- `bin/vuln_target`

## Run

Simple target:

```bash
./bin/loader ./bin/target
```

Verbose mode with custom stack:

```bash
./bin/loader --verbose --stack-size 2097152 ./bin/target
```

Vulnerable target:

```bash
./bin/loader ./bin/vuln_target
python3 -c 'print("A"*200)' | ./bin/loader ./bin/vuln_target
```

## Notes on the vulnerable target

`targets/vuln_target.c` is intentionally unsafe and built with insecure flags:

- `-fno-stack-protector`
- `-fno-pie -no-pie`

The overflow (`read(..., 256)` into a 64-byte stack buffer) is deliberate for challenge design and exploit practice.

## Safety

This project is for local experimentation and CTF environments only. Do not deploy the vulnerable target (or this loader behavior) in production systems.
