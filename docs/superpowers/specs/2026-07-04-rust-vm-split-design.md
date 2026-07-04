# Rust VM Split and Compiler Design Rename Design

## Goal

Create a new backend direction where the bytecode VM becomes an independent Rust executable and project, while the existing C++ compiler remains responsible for the language front end and bytecode generation.

This design also renames the project from **Compiler Demo** to **Compiler Design** as a full brand rename.

## Decisions

- The Rust VM will be an independent executable process.
- The compiler/VM boundary will be a stable bytecode artifact file.
- The first artifact format will be a stable, parseable text format.
- The Rust VM project will initially live inside this repository under `vm-rs/`.
- The existing C++ bytecode VM will be frozen: it remains available for current behavior, but new backend research and VM evolution should target the Rust VM.
- Phase 0 is design and scaffolding only. It will not implement full bytecode emitting or VM execution.

## Project Rename

The project should be renamed from **Compiler Demo** to **Compiler Design**.

Naming rules:

- Human-readable project name: `Compiler Design`.
- CMake target and main compiler executable: `compiler_design`.
- README title: `# Compiler Design`.
- Test and documentation command examples should use `./build/compiler_design`.
- Rust VM package/crate: `compiler-design-vm`.
- Rust VM binary: `compiler-design-vm`.
- Rust VM directory: `vm-rs/`.
- Bytecode artifact extension: `.cdbc`, meaning Compiler Design ByteCode.

The local repository path does not need to change. Paths such as `/home/junhe/compiler` are developer-local and should not be encoded into project behavior.

## Phase 0 Scope

Phase 0 prepares the split without changing runtime behavior.

Planned Phase 0 deliverables:

```text
vm-rs/
  Cargo.toml
  README.md
  src/
    main.rs

docs/
  bytecode-text-format.md

docs/superpowers/specs/
  2026-07-04-rust-vm-split-design.md
```

Phase 0 should also update top-level project documentation and build/test references for the Compiler Design rename.

Phase 0 must not:

- implement a full `.cdbc` emitter in the C++ compiler;
- implement a full `.cdbc` parser in Rust;
- implement a Rust bytecode executor;
- change the semantics of `--run`, `--bytecode`, or `--run-bytecode`;
- remove the existing C++ bytecode VM;
- add Rust VM execution to golden tests.

## Architecture

The long-term architecture has three independent parts:

1. **Compiler Design C++ compiler**
   - Owns lexing, parsing, type checking, IR compilation, bytecode lowering, and diagnostics.
   - Will eventually emit `.cdbc` bytecode artifact files.

2. **Bytecode artifact format**
   - A stable textual contract between the compiler and VM.
   - Versioned independently from debug print output.
   - Human-readable and easy to cover with golden tests.

3. **Rust VM**
   - Owns loading, validating, and executing bytecode artifacts.
   - Becomes the main place for future backend work: GC, task scheduling, and JIT exploration.

The existing C++ `BytecodeVM` remains in the C++ compiler for now. It is a frozen reference backend for current behavior rather than the target for new backend features.

## Rust VM Project Layout

The initial Rust project should be a standalone binary crate:

```text
vm-rs/
  Cargo.toml
  README.md
  src/main.rs
```

Initial CLI behavior should be intentionally small:

```sh
compiler-design-vm --help
```

The README should describe the future target interface without claiming it is implemented:

```sh
compiler-design-vm run program.cdbc
compiler-design-vm dump program.cdbc
```

The crate should be structured so future phases can add focused modules:

```text
vm-rs/src/
  main.rs          # CLI entry point
  format.rs        # future .cdbc parser/serializer
  value.rs         # future runtime values
  bytecode.rs      # future bytecode structures
  vm.rs            # future executor
  heap.rs          # future GC-aware heap
  scheduler.rs     # future task scheduler
  jit.rs           # future JIT metadata/experiments
```

Phase 0 only needs `main.rs`; the future modules should be described in README/spec, not created empty unless the implementation plan chooses to do so for clarity.

## Bytecode Text Format Direction

The `.cdbc` format should be stable and parseable, not a copy of the current `--bytecode` debug print.

Design requirements:

- It starts with a format/version header:

  ```text
  cdbc 0.1
  ```

- It explicitly separates:
  - constants;
  - names;
  - main register count and instructions;
  - nested functions and their register counts/instructions.

- It uses stable opcode names such as:
  - `constant`
  - `make_function`
  - `array`
  - `move`
  - `load_var`
  - `store_var`
  - `assign_var`
  - `call`
  - `index`
  - `assign_index`
  - `len`
  - `print`
  - `return`
  - arithmetic/comparison/jump opcodes.

- It uses explicit value encodings:
  - `nil`
  - `number 1.23`
  - `bool true`
  - `string "escaped"`

- It uses stable references for registers, names, constants, and functions, for example:
  - `r0`
  - `n2`
  - `c1`
  - `f0`

A small planned-format example:

```text
cdbc 0.1

constants:
  c0 = number 1
  c1 = number 2

names:

main registers=3:
  r0 = constant c0
  r1 = constant c1
  r2 = add r0, r1
  print r2
```

This example is documentation for the planned stable artifact format. It is not a claim that the compiler can emit it or that Rust VM can parse it in Phase 0.

## Migration Roadmap

### Phase 0: Project split preparation

- Rename project branding from Compiler Demo to Compiler Design.
- Rename the main CMake target and executable to `compiler_design`.
- Add the `vm-rs/` Rust project skeleton.
- Add `.cdbc` bytecode text format documentation.
- Mark the C++ bytecode VM as frozen in roadmap/project memory.
- Keep existing language and runtime behavior unchanged.

### Phase 1: Bytecode artifact emitter

- Add a C++ compiler mode that emits `.cdbc` artifacts.
- Prefer a clear CLI such as:

  ```sh
  compiler_design --emit-bytecode program.cdbc input.cd
  ```

  or an equivalent syntax selected during Phase 1 design.

- Add artifact golden tests.
- Keep current debug `--bytecode` output separate unless intentionally replaced.

### Phase 2: Rust VM parser/dumper

- Implement `.cdbc` parsing in Rust.
- Add a `dump` command for validating parsed artifacts.
- Add parser/dumper tests in `vm-rs/`.
- Do not require full execution yet.

### Phase 3: Rust VM executor parity

- Implement Rust VM execution for current bytecode semantics.
- Add golden runner support for compiling with C++ and executing with Rust VM.
- Compare Rust VM stdout/stderr/exit behavior with existing expectations.
- Keep C++ `--run-bytecode` available but frozen.

### Phase 4: Backend research

After parity is stable, use the Rust VM for:

- GC heap ownership, root scanning, and value reachability;
- task scheduling, instruction budgets, yield points, and blocked states;
- JIT metadata, hot function detection, and native-code experiments.

## Testing Strategy

Phase 0 tests should be lightweight but complete for its scope.

C++ side:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Rust side:

```sh
cargo test --manifest-path vm-rs/Cargo.toml
cargo run --manifest-path vm-rs/Cargo.toml -- --help
```

Expected Phase 0 outcomes:

- The renamed compiler executable builds as `compiler_design`.
- Existing C++ tests and golden tests pass with the new executable name.
- The Rust VM skeleton builds and its placeholder tests pass.
- Documentation clearly distinguishes planned `.cdbc` behavior from implemented behavior.

## Documentation Updates

Phase 0 should update:

- `README.md`: project name, binary name, build/test/run commands, and note that Rust VM work is planned.
- `AGENTS.md`: architecture map, command names, and C++ VM frozen status.
- `docs/roadmap.md`: add or update a backend track for Rust VM split.
- `docs/bytecode-text-format.md`: planned `.cdbc` text format.
- CMake/test documentation that references `compiler_demo`.

Do not document the `.cdbc` emitter or Rust VM executor as implemented until their phases are complete.

## Risks and Mitigations

Risk: renaming `compiler_demo` to `compiler_design` breaks tests and scripts.

Mitigation: update CMake, README, golden runner instructions, CTest configuration, and any hard-coded test command examples in one implementation slice.

Risk: the `.cdbc` format becomes coupled to current debug bytecode output.

Mitigation: keep `docs/bytecode-text-format.md` explicitly separate from `--bytecode` debug print and design the artifact as a versioned contract.

Risk: freezing C++ VM creates temporary double-backend confusion.

Mitigation: document C++ VM as frozen/reference and Rust VM as the backend research target. Continue using existing tests until Rust VM parity is implemented.

Risk: Phase 0 accidentally expands into VM implementation.

Mitigation: keep Phase 0 acceptance criteria limited to rename, skeleton, documentation, and build/test validation.

## Acceptance Criteria

Phase 0 is complete when:

1. Project branding and executable naming use Compiler Design / `compiler_design` consistently.
2. `vm-rs/` exists as an independent Cargo binary project named `compiler-design-vm`.
3. `.cdbc` text format documentation exists and is clearly marked as the planned stable artifact format.
4. Roadmap/project memory state that C++ bytecode VM is frozen and Rust VM is the future backend track.
5. C++ verification commands pass with `./build/compiler_design`.
6. Rust skeleton verification commands pass.
7. No language semantics or current runtime behavior change in Phase 0.
