# Runtime Source Diagnostics and Call Stacks Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Carry source provenance through the C++ compiler and `.cdbc` artifact so the standalone Rust VM reports runtime source locations, snippets, and ordered call stacks while remaining compatible with metadata-free artifacts.

**Architecture:** Add a C++ source table and optional `SourceSpan` to AST, IR, and bytecode metadata. Emit the source table and sparse instruction-location records in additive `.cdbc` debug sections; parse them into Rust `DebugSource`/`DebugLocation` tables. Runtime frames attach the failing instruction and caller call-site locations to a structured `RuntimeError`, and the CLI formats the embedded source snippet and stack.

**Tech Stack:** C++17 compiler/IR/bytecode pipeline, canonical `.cdbc` text emitter, Rust 2021 parser/VM, Python golden and artifact runners, CMake/CTest, Cargo tests.

---

## File map and boundaries

- Create `include/SourceMap.hpp` and `src/SourceMap.cpp` for `SourceFile` and
  `SourceSpan` value types and source-line/caret helpers shared by AST, IR, and
  bytecode layers.
- Modify `include/Token.hpp`, `src/Lexer.cpp`, and `src/FrontendSession.cpp` to
  preserve source identities while retaining existing combined-source parser
  coordinates.
- Modify `include/Ast.hpp`, `src/Ast.cpp`, and `src/Parser.cpp` so every
  concrete expression/statement has an optional syntactic-start span and
  `Program` owns the source-file table.
- Modify `include/IR.hpp`, `src/IR.cpp`, `include/IRCompiler.hpp`, and
  `src/IRCompiler.cpp` to attach spans to emitted instructions and copy the
  program source table.
- Modify `include/Bytecode.hpp`, `src/Bytecode.cpp`,
  `src/BytecodeCompiler.cpp`, `include/BytecodeTextEmitter.hpp`, and
  `src/BytecodeTextEmitter.cpp` to preserve and emit sparse debug metadata.
- Modify `vm-rs/src/bytecode.rs` and `vm-rs/src/format.rs` to parse, validate,
  and canonically format optional `debug_sources` and `debug_locations` sections.
- Modify `vm-rs/src/vm.rs` and `vm-rs/src/main.rs` to carry structured runtime
  errors, attach frames, and render source snippets and call stacks.
- Add focused C++/Rust/Python fixtures under `tests/`, update
  `docs/bytecode-text-format.md`, `README.md`, and `docs/roadmap.md`, and
  refresh only the affected runtime/artifact goldens.

## Task 1: Establish source-map value types and failing unit coverage

**Files:**

- Create: `include/SourceMap.hpp`
- Create: `src/SourceMap.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/source_map_tests.cpp`

- [ ] **Step 1: Write failing source-map tests**

Add a C++ executable with assertions for one-based locations and safe line
extraction:

```cpp
#include "SourceMap.hpp"

#include <cassert>

int main()
{
    SourceFile file{"demo.cd", "let x = 1;\nprint x;\n"};
    assert(sourceLine(file, SourceLocation{2, 1}) == "print x;");
    assert(sourceLine(file, SourceLocation{9, 1}).empty());
    assert(formatSourceSpan(SourceSpan{0, 2, 7}, {file}) == "demo.cd:2:7");
}
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```sh
cmake -S . -B build
cmake --build build --target source_map_tests
```

Expected: CMake reports the new target/source symbols are not yet defined.

- [ ] **Step 3: Implement the minimal shared types**

Define the exact public values in `include/SourceMap.hpp`:

```cpp
struct SourceFile {
    std::string path;
    std::string text;
};

struct SourceSpan {
    std::size_t source = 0;
    int line = 0;
    int column = 0;
};

std::string sourceLine(const SourceFile& file, SourceLocation location);
std::string formatSourceSpan(const SourceSpan& span, const std::vector<SourceFile>& files);
```

`sourceLine` returns an empty string for non-positive/out-of-range lines and
never indexes past the source. `formatSourceSpan` returns `<unknown>:line:column`
when `span.source` is out of range. Add `src/SourceMap.cpp` to the compiler and
`source_map_tests` targets in `CMakeLists.txt`.

- [ ] **Step 4: Run the focused test and commit**

Run `cmake --build build --target source_map_tests && build/source_map_tests`.
Expected: exit 0 with no output. Commit:

```sh
git add CMakeLists.txt include/SourceMap.hpp src/SourceMap.cpp tests/source_map_tests.cpp
git commit -m "feat: add shared source map values"
```

## Task 2: Preserve source identities and attach spans to the AST

**Files:**

- Modify: `include/Token.hpp`, `src/Lexer.cpp`
- Modify: `include/Ast.hpp`, `src/Ast.cpp`
- Modify: `src/Parser.cpp`
- Modify: `include/FrontendSession.hpp`, `src/FrontendSession.cpp`
- Modify: `tests/frontend_session_tests.cpp`

- [ ] **Step 1: Add a failing parser/source-table assertion**

Extend `tests/frontend_session_tests.cpp` with a two-file load that asserts
the assembled program retains both path/text entries and that a parsed `PrintStmt`
has the first file's span:

```cpp
Program program = frontend.loadFiles({first.string(), second.string()});
assert(program.sources.size() == 2);
const auto* print = dynamic_cast<const PrintStmt*>(program.statements.front().get());
assert(print && print->span && print->span->source == 0);
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run `cmake --build build --target frontend_session_tests && build/frontend_session_tests`.
Expected: compilation fails because `Program::sources`, `Stmt::span`, and
`Token::source` do not exist.

- [ ] **Step 3: Add token provenance without changing diagnostic coordinates**

Add `std::optional<std::size_t> source` to `Token` and initialize it to
`std::nullopt` in lexer-created tokens. Add `std::optional<SourceSpan> span` to
the `Expr` and `Stmt` bases. Parser constructors set each node's span from its
opening token; expression nodes with no dedicated token use the earliest child
span. Preserve the existing token `line`/`column` values used by parse errors.

Add `std::vector<SourceFile> sources` to `Program`. In `FrontendSession`, assign
stable source IDs in `ParsedUnit`, annotate tokens after lexing, and populate
the program table with display path and original text. For combined direct
inputs, map the existing combined line ranges to each direct input's local
source ID while leaving the combined line in `Token::line`; `assembleProgram`
converts node spans to local one-based lines before returning. Stdin gets one
entry with path `<stdin>`.

- [ ] **Step 4: Run front-end tests and existing golden parsing**

Run:

```sh
cmake --build build --target frontend_session_tests compiler_design
build/frontend_session_tests
python3 tests/run_golden_tests.py ./build/compiler_design --case parse_errors
```

Expected: all commands pass and parse-error text is byte-for-byte unchanged.

- [ ] **Step 5: Commit the AST provenance slice**

```sh
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp src/Parser.cpp include/FrontendSession.hpp src/FrontendSession.cpp tests/frontend_session_tests.cpp
git commit -m "feat: attach source spans to parsed nodes"
```

## Task 3: Propagate spans through IR

**Files:**

- Modify: `include/IR.hpp`, `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`, `src/IRCompiler.cpp`
- Create: `tests/ir_source_location_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write a failing IR metadata test**

Compile a parsed `1 / 0` program and assert the divide instruction has the
binary expression's source span and that the IR program owns the source table:

```cpp
IRProgram ir = compiler.compile(program, resolved);
assert(ir.sources().size() == 1);
const auto& divide = ir.instructions().at(2);
assert(divide.op == IROp::Divide);
assert(divide.span && divide.span->line == 1);
```

- [ ] **Step 2: Run the test and verify it fails**

Run `cmake --build build --target ir_source_location_tests`.
Expected: compilation fails because `IRInstruction::span`, `IRProgram::sources`,
and the span-aware compiler API do not exist.

- [ ] **Step 3: Add IR metadata and scoped compiler state**

Add `std::optional<SourceSpan> span` to `IRInstruction`, a source vector plus
`setSources`/`sources` accessors to `IRProgram`, and a private
`std::optional<SourceSpan> currentSpan_` in `IRCompiler`. Make every `emit*`
helper copy `currentSpan_` into the instruction. Wrap every statement and
expression lowering entry point in an RAII span guard that saves/restores the
previous span; use the function declaration span for implicit `return nil`.
Copy `program.sources` into `ir_` before lowering. Keep jump patching and IR
printing independent of metadata so existing `ir.out` files do not change.

- [ ] **Step 4: Run the focused and existing IR tests**

Run `cmake --build build --target ir_source_location_tests compiler_design && build/ir_source_location_tests` followed by
`python3 tests/run_golden_tests.py ./build/compiler_design --case array`.
Expected: the new test passes and existing AST/IR outputs remain unchanged.

- [ ] **Step 5: Commit IR propagation**

```sh
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp tests/ir_source_location_tests.cpp CMakeLists.txt
git commit -m "feat: preserve source spans in register IR"
```

## Task 4: Preserve metadata in bytecode objects and emit debug sections

**Files:**

- Modify: `include/Bytecode.hpp`, `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `include/BytecodeTextEmitter.hpp`, `src/BytecodeTextEmitter.cpp`
- Create: `tests/golden/runtime_diagnostics/input.cd`
- Create: `tests/bytecode_artifacts/runtime_diagnostics/expected.cdbc`

- [ ] **Step 1: Add a failing artifact assertion**

Create `tests/golden/runtime_diagnostics/input.cd` containing:

```cd
fun fail() { return 1 / 0; }
fail();
```

Add an expected artifact that includes the exact sections:

```text
debug_sources:
  s0 path=".../input.cd" text="fun fail() { return 1 / 0; }\\nfail();\\n"

debug_locations:
  main 1 = s0:2:1
  function f0 1 = s0:1:22
```

Run the artifact test for this case before implementation; it must fail because
no debug sections are emitted.

- [ ] **Step 2: Add bytecode metadata containers and lowering**

Add `std::optional<SourceSpan> span` to `BytecodeInstruction`, a source-file
vector and accessors to `BytecodeProgram`, and copy `IRProgram::sources()` in
`BytecodeCompiler::compile`. `lowerInstruction` copies `instruction.span`; no
opcode or ordinary bytecode text changes.

- [ ] **Step 3: Emit canonical sparse sections**

After all function instructions in `writeBytecodeText`, emit `debug_sources:`
and `debug_locations:` only when their vectors are non-empty. Use the existing
`escapedString` format for path/text. Emit main mappings as `main <index> =
s<source>:<line>:<column>` and function mappings as `function f<index> <index>
= ...`, sorted by section then instruction index. Do not add locations to
`--bytecode`/`--ir` human-readable printers.

- [ ] **Step 4: Run the artifact test and commit**

Run `cmake --build build --target compiler_design` and
`python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs`.
Expected: the new case passes after recording the repository-relative path
normalization, and existing cases fail only until their expected artifacts are
refreshed with the additive sections. Review all diffs, then commit:

```sh
git add include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp include/BytecodeTextEmitter.hpp src/BytecodeTextEmitter.cpp tests/golden/runtime_diagnostics tests/bytecode_artifacts/runtime_diagnostics
git commit -m "feat: emit bytecode source debug metadata"
```

## Task 5: Parse and format optional Rust debug metadata

**Files:**

- Modify: `vm-rs/src/bytecode.rs`
- Modify: `vm-rs/src/format.rs`
- Add tests in: `vm-rs/src/format.rs` test module

- [ ] **Step 1: Add failing Rust parser tests**

Add tests for one source/mapping and malformed references:

```rust
#[test]
fn parses_debug_sources_and_locations() {
    let program = parse_program(DEBUG_ARTIFACT).expect("valid debug artifact");
    assert_eq!(program.debug_sources[0].path, "demo.cd");
    assert_eq!(program.main.locations[0].as_ref().unwrap().line, 1);
}

#[test]
fn rejects_location_for_unknown_instruction() {
    let error = parse_program(DEBUG_ARTIFACT.replace("main 0", "main 9")).unwrap_err();
    assert!(error.message.contains("instruction"));
}
```

- [ ] **Step 2: Run the Rust tests and verify they fail**

Run `cargo test --manifest-path vm-rs/Cargo.toml format::tests::parses_debug_sources_and_locations`.
Expected: compilation fails because `Program::debug_sources` and body location
tables are not defined.

- [ ] **Step 3: Define Rust metadata and parser boundaries**

Add these exact values in `vm-rs/src/bytecode.rs`:

```rust
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct DebugSource { pub path: String, pub text: String }

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct DebugLocation { pub source: usize, pub line: usize, pub column: usize }

pub struct FunctionBody {
    pub registers: usize,
    pub instructions: Vec<Instruction>,
    pub locations: Vec<Option<DebugLocation>>,
}

pub struct Program {
    pub constants: Vec<Constant>,
    pub names: Vec<String>,
    pub main: FunctionBody,
    pub functions: Vec<Function>,
    pub debug_sources: Vec<DebugSource>,
}
```

Initialize empty location vectors for all existing unit-test programs. Extend
`format::Parser` so instruction parsing stops at `debug_sources:` and
`debug_locations:`. Parse quoted path/text entries, then sparse mappings into
each body's `locations` vector. Validate source indexes, positive coordinates,
section/function indexes, and instruction bounds after all functions are
known. Reject duplicate mappings and missing section markers with `ParseError`.

- [ ] **Step 4: Implement canonical formatting and legacy compatibility**

`format_program` prints the two optional sections after function bodies using
the existing quote/escape helpers. It prints only present mappings, in source
index and section/instruction order. For an empty metadata table it emits the
same output as before. Update every Rust test `FunctionBody`/`Program` literal
to include `locations: Vec::new()` and `debug_sources: Vec::new()`.

- [ ] **Step 5: Run parser/formatter tests and commit**

Run `cargo test --manifest-path vm-rs/Cargo.toml`.
Expected: all Rust tests pass, including round-trip and legacy-artifact tests.
Commit:

```sh
git add vm-rs/src/bytecode.rs vm-rs/src/format.rs
git commit -m "feat: parse cdbc debug metadata"
```

## Task 6: Attach source-aware runtime errors and call stacks

**Files:**

- Modify: `vm-rs/src/vm.rs`
- Modify: `vm-rs/src/main.rs`
- Add tests in: `vm-rs/src/vm.rs` test module

- [ ] **Step 1: Add failing VM tests for location and stack order**

Construct a `Program` with a failing main instruction and a function that
fails through a `Call`. Assert the structured fields and exact display text:

```rust
#[test]
fn runtime_error_reports_inner_location_then_outer_call_site() {
    let program = program_with_debug_failure();
    let error = VM::new(&program).run().unwrap_err();
    assert_eq!(error.location.unwrap().line, 1);
    assert_eq!(error.stack[0].function, "fail");
    assert_eq!(error.stack[1].function, "main");
    assert!(error.to_string().contains("Call stack:\n"));
}
```

Also cover a native helper failure, a closure call, empty/invalid source text,
and a metadata-free legacy program whose `to_string()` is exactly
`Runtime error: <message>`.

- [ ] **Step 2: Run the focused tests and verify they fail**

Run `cargo test --manifest-path vm-rs/Cargo.toml vm::tests::runtime_error_reports_inner_location_then_outer_call_site`.
Expected: compilation fails because `RuntimeError` has no location/stack and
`FunctionBody` has no location lookup.

- [ ] **Step 3: Implement structured error values and frame metadata**

Replace `RuntimeError { message: String }` with:

```rust
pub struct StackFrame {
    pub function: String,
    pub location: Option<DebugLocation>,
}

pub struct RuntimeError {
    pub message: String,
    pub location: Option<DebugLocation>,
    pub stack: Vec<StackFrame>,
}
```

Add `function: String` and `function_index: Option<usize>` to `Frame`. Main
uses function `main` and `None`; function calls use the bytecode function's
name/index. At each instruction boundary, catch an escaping error and attach
the current instruction's location only when it has none. In `call_function`,
catch the callee error, append a frame for the caller's `Call` instruction
location, and rethrow; `run` appends the outer `main` frame. Do not overwrite
an inner location. Keep direct `execute_native_call` errors with empty stack
and no location.

- [ ] **Step 4: Render embedded snippets and stacks**

Implement `RuntimeError::fmt` with the exact branches:

```rust
if let Some(location) = self.location.as_ref() {
    write!(f, "Runtime error at {}:{}:{}: {}\n", path, location.line, location.column, self.message)?;
    write!(f, "  {}\n", source_line);
    write!(f, "  {}^\n", " ".repeat(location.column.saturating_sub(1)))?;
}
```

Then print `Call stack:` and one `  at name (path:line:column)` per frame only
when at least one frame has a valid location. Invalid source/line/column
lookups use the legacy one-line message and omit the snippet without panicking.
`main.rs` continues to print the error to stderr and return code 1.

- [ ] **Step 5: Run all Rust tests and commit**

Run `cargo test --manifest-path vm-rs/Cargo.toml`.
Expected: all unit tests pass with old native behavior unchanged and new exact
stack assertions green. Commit:

```sh
git add vm-rs/src/vm.rs vm-rs/src/main.rs
git commit -m "feat: report runtime source locations and call stacks"
```

## Task 7: Add end-to-end fixtures and documentation

**Files:**

- Create/update: `tests/golden/runtime_errors/*.cd`, matching `.run.err` and
  `.exit` files
- Create/update: `tests/bytecode_artifacts/*/expected.cdbc`
- Modify: `tests/run_rust_vm_tests.py` only if a fixture needs explicit source
  path normalization
- Modify: `docs/bytecode-text-format.md`, `README.md`, `docs/roadmap.md`

- [ ] **Step 1: Add failing runtime fixtures**

Add these source programs and expected failure categories:

```cd
// runtime_errors/source_location.cd
print 1 / 0;
```

```cd
// runtime_errors/nested_call_stack.cd
fun inner() { return [1][2]; }
fun outer() { return inner(); }
outer();
```

Create an imported pair where the failure occurs in the imported module and
the caller is in the entry file. Before updating expected output, run
`python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
--case source_location`; it must fail with the old one-line expectation.

- [ ] **Step 2: Refresh expected diagnostics deliberately**

Use the exact path emitted by the compiler, source line, caret, and innermost-
to-outermost stack order. Ensure every runtime-error fixture has empty stdout
and exit file `1`; do not alter parse/type/import fixtures.

- [ ] **Step 3: Add artifact parser/round-trip coverage**

Add expected artifacts with `debug_sources` and sparse mappings for main,
function, direct multi-file, and imported-module cases. Run:

```sh
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: C++ emission and Rust `dump` output match byte-for-byte.

- [ ] **Step 4: Update user-facing documentation**

Document the optional sections and validation in `docs/bytecode-text-format.md`,
the runtime output shape and standalone artifact behavior in `README.md`, and
change the first roadmap item from active to complete only after verification.

- [ ] **Step 5: Commit fixtures and docs**

```sh
git add tests/golden/runtime_errors tests/bytecode_artifacts docs/bytecode-text-format.md README.md docs/roadmap.md tests/run_rust_vm_tests.py
git commit -m "test: cover runtime source diagnostics end to end"
```

## Task 8: Full verification and cleanup

**Files:**

- Modify only expected goldens if verification exposes intentional output
  drift; remove generated `tests/__pycache__/`.

- [ ] **Step 1: Build and run the complete required suite**

Run each command from `AGENTS.md`:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
rm -rf tests/__pycache__
cargo test --manifest-path vm-rs/Cargo.toml
```

- [ ] **Step 2: Verify compatibility and repository hygiene**

Manually run a metadata-free hand-written artifact through `cargo run --quiet
--manifest-path vm-rs/Cargo.toml -- run <artifact>` and confirm its error is the
legacy one-line form. Run `git diff --check` and `git status --short`; only
source, tests, docs, and planned commits may remain.

- [ ] **Step 3: Record final evidence**

Capture the exact pass counts from CTest, the golden runner, artifact runner,
Rust VM runner, and Cargo. Do not claim completion until every command exits
zero and the runtime-error goldens contain the reviewed location/stack output.

## Plan self-review

- Source provenance is covered by Tasks 1–2, including stdin, direct files,
  imports, and combined-coordinate remapping.
- IR and bytecode propagation is covered by Tasks 3–4; ordinary `--ir` and
  `--bytecode` printers remain stable while emitted artifacts gain debug data.
- Rust parsing, validation, legacy compatibility, formatting, VM error wrapping,
  native-call attribution, snippets, closures, and call-stack order are covered
  by Tasks 5–6.
- End-to-end goldens, artifact round trips, docs, roadmap update, full commands,
  and cleanup are covered by Tasks 7–8.
- No placeholders or undefined helper names are used; `SourceSpan`,
  `DebugSource`, `DebugLocation`, `StackFrame`, `Program::sources`, and
  `FunctionBody::locations` are introduced before later tasks consume them.
