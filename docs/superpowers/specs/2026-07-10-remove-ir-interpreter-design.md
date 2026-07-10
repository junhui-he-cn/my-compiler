# Remove IR Interpreter Design

## Goal

Remove the C++ IR interpreter as an execution backend and make the Rust `compiler-design-vm` plus `.cdbc` bytecode artifacts the only supported runtime execution path. The C++ compiler remains responsible for frontend parsing/type checking, IR printing, bytecode debug printing, and stable `.cdbc` artifact emission.

## Motivation

The project currently has two runtime execution implementations: the legacy C++ `IRInterpreter` behind `compiler_design --run`, and the Rust VM behind `.cdbc` artifacts. Keeping both in sync increases feature cost and makes backend validation ambiguous. The Rust VM already executes all current successful `run.out` golden fixtures through `--emit-bytecode` plus `compiler-design-vm run`, so it is ready to become the single execution verifier.

## Non-Goals

This slice does not change language semantics, bytecode opcodes, IR lowering, or `.cdbc` artifact format. It does not add a new C++ wrapper that shells out to the Rust VM. It does not remove IR generation or `--ir`, because IR remains the compiler's lowering boundary and a useful debug artifact. It does not remove `--bytecode` or `--emit-bytecode`.

## CLI Contract

Remove `--run` from `compiler_design`:

- Usage no longer advertises `--run`.
- Passing `--run` is explicitly rejected with usage text and exit code 64 so users get a clear signal that execution moved to the Rust VM workflow.
- The supported execution workflow is:

```sh
./build/compiler_design --emit-bytecode program.cdbc input.cd
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

`--emit-bytecode` keeps its current requirement that at least one input file be provided. Stdin execution is no longer supported by the compiler CLI because execution is no longer a compiler mode; users who want to execute stdin content must write it to a file before emitting `.cdbc`.

## C++ Code Removal

Remove these files from the active build and repository:

- `include/IRInterpreter.hpp`
- `src/IRInterpreter.cpp`

Update `src/main.cpp` so it no longer includes `IRInterpreter.hpp`, no longer tracks a `runIr` flag, no longer compiles IR for execution, and no longer constructs `IRInterpreter`. The compiler should still compile IR when needed for `--ir`, `--bytecode`, or `--emit-bytecode`.

Update `CMakeLists.txt` to remove `src/IRInterpreter.cpp` from `compiler_design`.

## Golden Test Runner Migration

`tests/run_golden_tests.py` should become a compiler-output/front-end runner only:

- Success checks cover default AST, `--ir`, and `--bytecode` when the corresponding expected files exist.
- It no longer runs `--run` and no longer checks `run.out`.
- A success fixture may still contain only `run.out`; that fixture is valid because Rust VM integration tests own execution output checks.
- Runtime-error fixtures under `tests/golden/runtime_errors` are no longer checked by this runner.
- Parse, type, and import error fixtures remain checked by this runner because those are compiler/front-end diagnostics.

Selftests in `tests/run_golden_tests_selftest.py` should be updated to match the new ownership. Tests that assert `--run` invocation should instead assert that the golden runner ignores `run.out`-only execution fixtures or delegates no execution work.

## Rust VM Execution Test Migration

`tests/run_rust_vm_tests.py` becomes the owner of runtime execution verification:

- It should discover all successful golden fixture directories that contain `run.out`, not only an allowlist.
- For each success fixture, it emits `.cdbc` with `compiler_design --emit-bytecode` and executes it with `compiler-design-vm run`, comparing stdout with `run.out` and requiring empty stderr and exit code 0.
- It should discover all runtime-error fixtures under `tests/golden/runtime_errors/*.cd`.
- For each runtime-error fixture, it emits `.cdbc`, executes it with the Rust VM, and compares stdout, stderr, and exit code against `.run.err` and `.exit`.
- If bytecode emission fails for a runtime-error fixture, that is a test failure unless the fixture is intentionally moved to parse/type/import errors.

The existing `tests/bytecode_artifact_tests.py` remains focused on stable `.cdbc` text artifacts and Rust VM dump canonicalization. It does not need to own golden `run.out` or runtime-error discovery.

## Rust VM Diagnostic Compatibility

The current C++ runtime-error golden files use messages such as:

```text
Runtime error: division by zero
```

The Rust VM currently prefixes command errors as `error: runtime error: ...`. To keep language runtime diagnostics stable and avoid refreshing all runtime-error goldens only for a wrapper prefix, adjust Rust VM CLI/runtime error display so runtime execution errors print in the existing language shape:

```text
Runtime error: division by zero
```

Argument, file-read, and `.cdbc` parse errors in the Rust VM may keep the lower-case `error:` CLI style; this compatibility rule applies to runtime execution errors produced by `compiler-design-vm run`.

## CLI Multi-Source Tests

`tests/cli_multi_source_tests.py` currently uses `--run` for several behaviors. Update it according to ownership:

- Multi-file execution behavior should be tested by emitting `.cdbc` and running the Rust VM when the test cares about program output.
- Compiler-only diagnostics, missing file errors, import loading order, and source snippet tests should use default AST mode, `--tokens`, or `--emit-bytecode` as appropriate.
- Stdin execution tests should be removed or rewritten as frontend/AST stdin tests, because execution is no longer a compiler CLI mode and `.cdbc` emission requires files.

## Documentation Updates

Update project documentation to remove references to C++ IR interpreter execution and `--run`:

- `README.md`: describe `.cdbc` + Rust VM execution as the supported run path.
- `docs/roadmap.md`: update baseline and guiding principles to say Rust VM execution is the sole runtime path; remove instructions to keep `--run` aligned.
- Repository agent memory (`AGENTS.md`, if present as a file): update build/verification commands and current semantics so future agents do not modify or mention the removed C++ IR interpreter.

Historical design/plan files under `docs/superpowers/` may continue to mention the old interpreter because they document past work.

## Verification Strategy

The full verification suite should remain:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

After this migration, the golden runner count will decrease because it no longer owns execution checks, while Rust VM test count will increase because it discovers all `run.out` and runtime-error fixtures.

## Success Criteria

- `IRInterpreter` source/header files are removed and no active build target references them.
- `compiler_design --run` is no longer a supported mode or documented workflow.
- All successful `run.out` golden fixtures are verified through `.cdbc` emission plus Rust VM execution.
- All runtime-error golden fixtures are verified through `.cdbc` emission plus Rust VM execution.
- Existing runtime-error diagnostic text remains stable where possible.
- Full verification passes and the working tree is clean after removing generated caches.
