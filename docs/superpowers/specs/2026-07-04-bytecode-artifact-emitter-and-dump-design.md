# Bytecode Artifact Emitter and Rust VM Dump Design

## Goal

Implement the next backend phase after Rust VM Phase 0 by creating a minimal end-to-end `.cdbc` artifact loop:

```text
.cd source
  -> compiler_design --emit-bytecode output.cdbc input.cd
  -> compiler-design-vm dump output.cdbc
  -> canonical .cdbc text
```

This phase combines the previous roadmap Phase 1 and Phase 2 at the format layer. It adds a C++ bytecode artifact emitter and a Rust `.cdbc` parser/formatter, but it does not execute bytecode in Rust.

## Scope

This phase includes:

- C++ CLI support for:

  ```sh
  compiler_design --emit-bytecode output.cdbc input.cd
  ```

- A C++ `.cdbc` text emitter for the current full `BytecodeOp` set.
- A Rust `.cdbc` parser that supports the same artifact grammar.
- A Rust `dump` command that parses an artifact and emits canonical `.cdbc` text.
- Integration tests that compile `.cd` fixtures into `.cdbc`, dump them through Rust VM, and compare canonical output.

This phase does not include:

- Rust bytecode execution;
- replacing `--run-bytecode`;
- changing the C++ bytecode VM behavior;
- binary bytecode encoding;
- GC, task scheduling, or JIT implementation;
- semantic bytecode verification beyond what is required to parse and format the artifact.

The C++ bytecode VM remains frozen/reference. The Rust VM remains the target for future backend execution work.

## CLI Design

### C++ compiler

Add:

```sh
compiler_design --emit-bytecode output.cdbc input.cd
```

Rules:

- `--emit-bytecode` must be followed by exactly one output path.
- The input source path remains the last non-flag argument.
- If the output path or input path is missing, print usage and exit with code `64`.
- Successful artifact emission writes the `.cdbc` file and produces no stdout.
- Front-end diagnostics keep existing stderr shape and exit behavior.
- If compilation fails, the final output path must not contain partial artifact content. The implementation can satisfy this by rendering to memory first and writing the file only after successful compilation/emission.
- Existing modes remain available:
  - default AST print;
  - `--tokens`;
  - `--ir`;
  - `--bytecode` debug print;
  - `--run`;
  - `--run-bytecode`.

`--emit-bytecode` is an artifact output mode, not a debug-print mode. It should not change `--bytecode` output.

### Rust VM

Add:

```sh
compiler-design-vm dump program.cdbc
```

Rules:

- `dump` reads `program.cdbc`, parses it, and writes canonical `.cdbc` text to stdout.
- `dump` does not execute the program.
- `--help` remains supported.
- Unknown commands still print usage and exit `64`.
- Read errors and parse errors exit `1`.
- Parse errors use this shape:

  ```text
  error: parse error at line X: <message>
  ```

## `.cdbc` Format

The format is line-oriented and canonical. The emitter and Rust formatter must produce exactly this style.

### Overall structure

Section order is fixed:

1. Header.
2. `constants:` section.
3. `names:` section.
4. `main registers=N:` section.
5. Zero or more function sections in function-index order.

Example:

```text
cdbc 0.1

constants:
  c0 = number 1
  c1 = string "hello"
  c2 = bool true
  c3 = nil

names:
  n0 = "x"

main registers=4:
  r0 = constant c0
  store_var n0, r0
  r1 = load_var n0
  r2 = constant c1
  print r2

function f0 name="add" arity=2 registers=4:
  param 0 = "a"
  param 1 = "b"
  r2 = add r0, r1
  return r2
```

Rules:

- Header is exactly `cdbc 0.1`.
- Blank lines separate sections.
- Empty sections are allowed and still printed.
- Indented entries use two spaces.
- Indexes are zero-based decimal integers.

### References

Reference prefixes:

- `cN`: constant index.
- `nN`: name index.
- `rN`: register index.
- `fN`: function index.

### Values

Constants use explicit tags:

```text
c0 = nil
c1 = number 1.25
c2 = bool true
c3 = string "escaped string"
```

Strings use double quotes and backslash escapes for:

- `\\`
- `\"`
- `\n`
- `\r`
- `\t`

The Rust parser should reject malformed string escapes.

### Functions and parameters

Function headers use:

```text
function fK name="function_name" arity=N registers=M:
```

Each function lists parameters explicitly before instructions:

```text
  param 0 = "a"
  param 1 = "b"
```

The parameter index indicates the register position. Parameter `0` is stored in `r0`, parameter `1` in `r1`, and so on. This matches the current bytecode VM calling convention.

The parser should require the number of `param` lines to match the function `arity`.

### Instructions

Instructions come in these forms.

Dest instruction:

```text
  r3 = add r1, r2
```

No-dest instruction:

```text
  print r3
  store_var n0, r1
  jump 12
  jump_if_false r0, 18
```

List operands:

```text
  r3 = array [r0, r1]
  r4 = call r2 [r0, r1]
```

The list syntax is used for arrays and call arguments because it is easier to parse and less ambiguous than `callee(args)` debug-print syntax.

## Opcode Coverage

The C++ emitter and Rust parser/formatter must support all current `BytecodeOp` values:

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
- `negate`
- `not`
- `add`
- `subtract`
- `multiply`
- `divide`
- `equal`
- `not_equal`
- `greater`
- `greater_equal`
- `less`
- `less_equal`
- `jump`
- `jump_if_false`
- `jump_if_true`

If a future language feature adds a bytecode opcode, that feature must update:

1. `docs/bytecode-text-format.md`;
2. C++ artifact emitter;
3. Rust parser/formatter;
4. artifact tests.

## C++ Components

Add:

```text
include/BytecodeTextEmitter.hpp
src/BytecodeTextEmitter.cpp
```

Responsibilities:

- Convert a `BytecodeProgram` to canonical `.cdbc` text.
- Write to `std::ostream`.
- Escape strings consistently with the format spec.
- Emit sections in canonical order.
- Emit functions in `BytecodeProgram::functions()` order.
- Emit function parameter lines before function instructions.
- Keep artifact formatting independent from `BytecodeProgram::print()` debug output.

Non-responsibilities:

- Source compilation.
- File I/O.
- CLI parsing.
- Runtime execution.

Update:

```text
src/main.cpp
```

Responsibilities:

- Parse `--emit-bytecode output.cdbc`.
- Compile source to `BytecodeProgram` using the existing pipeline.
- Render artifact to an in-memory string.
- Write the final output path only after successful compilation/rendering.

Update:

```text
CMakeLists.txt
```

Responsibilities:

- Add `src/BytecodeTextEmitter.cpp` to the `compiler_design` target.

## Rust Components

Add or update:

```text
vm-rs/src/main.rs
vm-rs/src/bytecode.rs
vm-rs/src/format.rs
```

### `main.rs`

Responsibilities:

- Dispatch `--help` and `dump <file.cdbc>`.
- Read input files.
- Print canonical dump output.
- Print command/read/parse errors and choose exit codes.

### `bytecode.rs`

Responsibilities:

- Define Rust data structures for `.cdbc` artifacts:
  - `Program`;
  - `Constant`;
  - `Function`;
  - `Instruction`;
  - small reference wrappers or plain indexes for constants, names, registers, and functions.

These structures represent the artifact format, not the eventual runtime heap or executor.

### `format.rs`

Responsibilities:

- Parse `.cdbc` text into `bytecode.rs` structures.
- Format structures back into canonical `.cdbc` text.
- Provide unit-testable functions such as:

  ```rust
  pub fn parse_program(source: &str) -> Result<Program, ParseError>;
  pub fn format_program(program: &Program) -> String;
  ```

- Include parse errors with line numbers.

The parser can be a small hand-written line parser. No parser generator is needed for this phase.

## Testing Strategy

### C++/Rust integration artifact tests

Add:

```text
tests/run_bytecode_artifact_tests.py
```

The runner should:

1. Discover fixture directories under `tests/bytecode_artifacts/`.
2. For each fixture, run:

   ```sh
   compiler_design --emit-bytecode <tmp>/actual.cdbc <fixture>/input.cd
   ```

3. Compare `actual.cdbc` to `<fixture>/expected.cdbc`.
4. Run:

   ```sh
   cargo run --manifest-path vm-rs/Cargo.toml -- dump <tmp>/actual.cdbc
   ```

5. Compare Rust dump stdout to `<fixture>/expected.cdbc`.
6. Fail if either command writes unexpected stderr or exits non-zero.

Recommended fixtures:

```text
tests/bytecode_artifacts/arithmetic/input.cd
tests/bytecode_artifacts/arithmetic/expected.cdbc

tests/bytecode_artifacts/control_flow/input.cd
tests/bytecode_artifacts/control_flow/expected.cdbc

tests/bytecode_artifacts/functions_arrays/input.cd
tests/bytecode_artifacts/functions_arrays/expected.cdbc
```

Fixture coverage:

- `arithmetic`: constants, arithmetic, comparisons, and `print`.
- `control_flow`: `if`, `while`, logical branches, jumps.
- `functions_arrays`: functions, function expressions or closures, calls, arrays, indexing, index assignment, and `len`.

Add this test to CTest only after it is stable enough to run in normal builds.

### Rust unit tests

Add unit tests for:

- parsing a minimal artifact;
- string escape parsing/formatting;
- rejecting a bad header;
- rejecting an unknown opcode;
- parse-format round-trip for a small artifact;
- parsing all current opcode shapes.

### Existing tests

Existing tests must still pass:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

## Documentation Updates

Update:

- `docs/bytecode-text-format.md` with the finalized line-oriented grammar from this design.
- `README.md` with the new artifact command and Rust dump command, clearly noting this phase still does not execute in Rust.
- `AGENTS.md` with the new verification command and extension workflow for bytecode artifact format changes.
- `docs/roadmap.md` to mark backend Phase 1 and Phase 2 format work as implemented after code lands.

## Acceptance Criteria

This phase is complete when:

1. `compiler_design --emit-bytecode output.cdbc input.cd` writes canonical `.cdbc` text for supported programs.
2. The artifact emitter supports all current `BytecodeOp` values.
3. `compiler-design-vm dump output.cdbc` parses and reprints canonical `.cdbc` text.
4. Rust parser/formatter supports all current opcode shapes.
5. Artifact integration tests pass for arithmetic, control flow, and functions/arrays fixtures.
6. Existing C++ golden tests still pass.
7. Rust unit tests pass.
8. `--run-bytecode` behavior is unchanged.
9. No Rust bytecode executor is introduced in this phase.

## Risks and Mitigations

Risk: artifact formatting diverges from parser expectations.

Mitigation: use canonical dump round-trip tests where C++ output and Rust dump must match the same `expected.cdbc`.

Risk: adding `--emit-bytecode` complicates CLI parsing.

Mitigation: keep one explicit form, `--emit-bytecode output.cdbc input.cd`, and reject missing output/input with exit `64`.

Risk: parser grows too complex.

Mitigation: keep the format line-oriented with fixed section order and explicit operand shapes.

Risk: this phase accidentally becomes an executor implementation.

Mitigation: keep `vm-rs` modules limited to CLI, artifact structures, parser, and formatter. Do not add `vm.rs` execution code in this phase.
