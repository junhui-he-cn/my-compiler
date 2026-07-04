# Remove C++ Bytecode VM Design

## Goal

Remove the C++ bytecode executor and make the Rust `compiler-design-vm` the only bytecode execution backend.

After this change, the C++ `compiler_design` binary remains the compiler and IR interpreter, while bytecode execution happens through `.cdbc` artifacts and the Rust VM:

```text
compiler_design input.cd                       # AST default mode
compiler_design --tokens input.cd
compiler_design --ir input.cd
compiler_design --bytecode input.cd            # debug bytecode print
compiler_design --run input.cd                 # IR interpreter
compiler_design --emit-bytecode out.cdbc input.cd
compiler-design-vm dump out.cdbc
compiler-design-vm run out.cdbc
```

The old C++ `--run-bytecode` mode is removed rather than reimplemented as a wrapper around Rust VM.

## Motivation

The project now has a stable `.cdbc` text artifact loop and a Rust VM executor with parity coverage for current bytecode semantics. Keeping a second in-process C++ bytecode VM creates duplicate runtime semantics, duplicate test expectations, and unclear ownership for future backend work.

Removing the C++ VM makes the architecture clearer:

- C++ owns language front-end, type checking, IR, bytecode lowering, debug bytecode printing, and `.cdbc` emission.
- Rust owns `.cdbc` parsing, canonical dump, and bytecode execution.
- Future backend work such as GC, task scheduling, and JIT targets Rust only.

## Scope

This phase includes:

- Delete C++ VM implementation files:
  - `include/BytecodeVM.hpp`
  - `src/BytecodeVM.cpp`
- Remove `src/BytecodeVM.cpp` from `CMakeLists.txt`.
- Remove `--run-bytecode` from `src/main.cpp`:
  - CLI parsing;
  - usage text;
  - bytecode construction condition;
  - execution branch;
  - `#include "BytecodeVM.hpp"`.
- Keep `--bytecode` debug printing.
- Keep `--emit-bytecode` artifact emission.
- Update Python golden runner behavior so it no longer discovers or runs `run_bytecode.out`, `.run_bytecode.err`, or `.run_bytecode.exit` checks.
- Delete old `run_bytecode.out`, `.run_bytecode.err`, and `.run_bytecode.exit` fixture files to avoid dead expectations.
- Keep Rust VM integration coverage through `tests/run_rust_vm_tests.py --goldens`.
- Update documentation and roadmap so Rust VM is the only bytecode executor.

This phase does not include:

- changing bytecode opcodes;
- changing `.cdbc` format;
- changing Rust VM execution semantics;
- adding Rust VM runtime-error golden parity;
- creating a C++ wrapper that invokes the Rust VM process;
- removing bytecode lowering or debug bytecode printing from the C++ compiler.

## CLI Behavior

### Removed

This command is removed:

```sh
compiler_design --run-bytecode input.cd
```

If users pass `--run-bytecode`, the existing argument parser treats it as an unknown or invalid argument and prints usage with exit code `64`. The usage output must no longer list `--run-bytecode`.

### Replacement

Users should run bytecode through the artifact boundary:

```sh
compiler_design --emit-bytecode program.cdbc input.cd
compiler-design-vm run program.cdbc
```

For development from the repository root:

```sh
./build/compiler_design --emit-bytecode program.cdbc input.cd
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

## Test Strategy

### Golden runner

`tests/run_golden_tests.py` should stop treating `run_bytecode.out` as a success output and stop treating `.run_bytecode.err/.run_bytecode.exit` as runtime-error expectations.

Successful program fixtures continue to support:

- `ast.out`
- `ir.out`
- `bytecode.out`
- `run.out`

Runtime-error fixtures continue to support:

- `.run.err`
- `.exit`

Parse/type-error behavior is unchanged.

The golden runner selftests must be updated to remove assertions about `--run-bytecode` discovery and invocation.

### Rust VM integration runner

`tests/run_rust_vm_tests.py` remains the semantic bytecode execution test path. It currently prefers `run_bytecode.out` if present. After deleting those files, it should compare against `run.out` or artifact fixture `run.out` for selected cases.

If a fixture has bytecode-specific expected output that differs from `run.out`, that fixture must be migrated to an explicit Rust VM fixture. Current expected behavior should not need this because Rust VM parity targets existing bytecode semantics and selected fixtures already match their `run.out` or artifact `run.out`.

### Fixture deletion

Delete files matching:

```text
tests/golden/**/run_bytecode.out
tests/golden/runtime_errors/*.run_bytecode.err
tests/golden/runtime_errors/*.run_bytecode.exit
```

Do not delete `bytecode.out`; it still validates debug bytecode printing.

## Documentation Updates

Update `README.md`:

- Remove `--run-bytecode` examples.
- Explain that bytecode execution uses Rust VM via `.cdbc`.

Update `AGENTS.md`:

- Remove `include/BytecodeVM.hpp`, `src/BytecodeVM.cpp` from architecture map.
- Remove `run_bytecode.out` and `.run_bytecode.*` golden conventions.
- Replace “C++ VM reference backend” language with “Rust VM is the bytecode execution backend”.
- Keep `tests/run_rust_vm_tests.py` verification command.

Update `docs/roadmap.md`:

- State that the C++ bytecode VM has been removed.
- Keep Rust VM future backend work as the target for GC/task/JIT.

Update `docs/bytecode-text-format.md` only if it still implies the C++ VM is part of bytecode execution.

Historical plans/specs may keep references to the old C++ VM because they document past work. Do not bulk-edit historical plan files unless a current docs page would mislead users.

## Acceptance Criteria

- `include/BytecodeVM.hpp` and `src/BytecodeVM.cpp` are deleted.
- `compiler_design --help` no longer lists `--run-bytecode`.
- `compiler_design --run-bytecode input.cd` exits with usage error code `64`.
- C++ build succeeds without `BytecodeVM.cpp`.
- Existing golden tests pass without any `run_bytecode` checks.
- Rust VM integration tests pass and remain the bytecode execution parity suite.
- Documentation points users to `--emit-bytecode` plus `compiler-design-vm run` for bytecode execution.

## Verification Commands

Run from repository root after implementation:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

Also verify the removed CLI explicitly:

```sh
./build/compiler_design --run-bytecode tests/bytecode_artifacts/arithmetic/input.cd
```

Expected: exits `64` and prints usage that does not mention `--run-bytecode`.
