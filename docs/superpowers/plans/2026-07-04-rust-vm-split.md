# Rust VM Split and Compiler Design Rename Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prepare Phase 0 of the Rust VM split by renaming the project to Compiler Design, renaming the C++ executable to `compiler_design`, adding a standalone Rust VM skeleton, and documenting the planned `.cdbc` bytecode text artifact.

**Architecture:** Keep the current C++ compiler and C++ bytecode VM behavior unchanged while making naming and documentation reflect the new Compiler Design direction. Add `vm-rs/` as an independent Cargo binary project with a placeholder CLI only; `.cdbc` emitting, parsing, and execution remain future phases. Treat the C++ bytecode VM as frozen/reference in docs and make Rust VM the future backend research track.

**Tech Stack:** C++17, CMake/CTest, Python golden test runner, Rust 2021 Cargo binary crate, Markdown documentation.

---

## File Structure

- `CMakeLists.txt`: rename the CMake project, executable target, include target, and CTest golden target from `compiler_demo` to `compiler_design`.
- `tests/run_golden_tests.py`: update argparse help text from `compiler_demo` to `compiler_design`.
- `README.md`: rename the project title and command examples; mention Rust VM split as planned backend work without claiming `.cdbc` support exists.
- `AGENTS.md`: update build/test commands, architecture map, binary name, and backend notes for the frozen C++ VM and future Rust VM.
- `docs/roadmap.md`: rename title and add/update backend track language for Rust VM split and C++ VM frozen status.
- `docs/bytecode-text-format.md`: create the planned stable `.cdbc` text artifact documentation.
- `vm-rs/Cargo.toml`: create standalone Rust binary crate named `compiler-design-vm`.
- `vm-rs/src/main.rs`: create placeholder CLI with `--help` and tests; do not parse or execute `.cdbc`.
- `vm-rs/README.md`: document the Rust VM skeleton, current Phase 0 behavior, and future commands.

Historical design/plan documents under `docs/superpowers/` may mention `compiler_demo` as historical context. Do not bulk-edit old completed plans/specs. Only edit the new implementation plan if it references the old executable in executable commands.

---

### Task 1: Rename C++ project and executable to `compiler_design`

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `tests/run_golden_tests.py`
- Test: CMake configure/build and golden runner with `./build/compiler_design`

- [ ] **Step 1: Verify the new executable name is absent before the rename**

Run:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
test -x ./build/compiler_design
```

Expected: the final `test -x ./build/compiler_design` fails because the project still builds `./build/compiler_demo` before this task is implemented.

- [ ] **Step 2: Update CMake target and project names**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
path = Path('CMakeLists.txt')
text = path.read_text(encoding='utf-8')
text = text.replace('project(compiler_demo LANGUAGES CXX)', 'project(compiler_design LANGUAGES CXX)')
text = text.replace('add_executable(compiler_demo', 'add_executable(compiler_design')
text = text.replace('target_include_directories(compiler_demo PRIVATE include)', 'target_include_directories(compiler_design PRIVATE include)')
text = text.replace('$<TARGET_FILE:compiler_demo>', '$<TARGET_FILE:compiler_design>')
path.write_text(text, encoding='utf-8')
PY
```

After the script, `CMakeLists.txt` should contain these exact key lines:

```cmake
project(compiler_design LANGUAGES CXX)
add_executable(compiler_design
...
target_include_directories(compiler_design PRIVATE include)
...
${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_golden_tests.py $<TARGET_FILE:compiler_design>
```

- [ ] **Step 3: Update golden runner CLI help text**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
path = Path('tests/run_golden_tests.py')
text = path.read_text(encoding='utf-8')
text = text.replace('Path to compiler_demo executable', 'Path to compiler_design executable')
path.write_text(text, encoding='utf-8')
PY
```

The argparse line should become:

```python
parser.add_argument("compiler", type=Path, help="Path to compiler_design executable")
```

- [ ] **Step 4: Build and verify the renamed executable**

Run:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
test -x ./build/compiler_design
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

- `./build/compiler_design` exists and is executable.
- CTest passes.
- Golden tests pass when invoked with `./build/compiler_design`.
- Golden runner selftests pass.

- [ ] **Step 5: Confirm the old executable name is not used in active build/test files**

Run:

```bash
grep -R "compiler_demo" -n CMakeLists.txt tests/run_golden_tests.py README.md AGENTS.md docs/roadmap.md || true
```

Expected: this command still shows old references in `README.md`, `AGENTS.md`, or `docs/roadmap.md` until Task 3 updates docs, but it must show no references in `CMakeLists.txt` or `tests/run_golden_tests.py`.

- [ ] **Step 6: Commit the C++ executable rename**

Run:

```bash
git add CMakeLists.txt tests/run_golden_tests.py
git commit -m "build: rename compiler executable"
```

---

### Task 2: Add the Rust VM skeleton project

**Files:**
- Create: `vm-rs/Cargo.toml`
- Create: `vm-rs/src/main.rs`
- Create: `vm-rs/README.md`
- Test: `cargo test --manifest-path vm-rs/Cargo.toml`
- Test: `cargo run --manifest-path vm-rs/Cargo.toml -- --help`

- [ ] **Step 1: Verify Rust project commands fail before scaffolding**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: fails because `vm-rs/Cargo.toml` does not exist before this task.

- [ ] **Step 2: Create Rust project directories**

Run:

```bash
mkdir -p vm-rs/src
```

- [ ] **Step 3: Create `vm-rs/Cargo.toml`**

Run:

```bash
cat > vm-rs/Cargo.toml <<'TOML'
[package]
name = "compiler-design-vm"
version = "0.1.0"
edition = "2021"
description = "Standalone Rust bytecode VM skeleton for Compiler Design"
license = "MIT"
publish = false

[[bin]]
name = "compiler-design-vm"
path = "src/main.rs"
TOML
```

- [ ] **Step 4: Create `vm-rs/src/main.rs` with placeholder CLI and tests**

Run:

```bash
cat > vm-rs/src/main.rs <<'RS'
use std::env;
use std::process;

const HELP: &str = "compiler-design-vm 0.1.0\n\n\
Usage:\n\
  compiler-design-vm --help\n\
  compiler-design-vm run <program.cdbc>   (planned)\n\
  compiler-design-vm dump <program.cdbc>  (planned)\n\n\
Phase 0: CLI skeleton only. .cdbc parsing and bytecode execution are not implemented in this phase.\n";

fn help_text() -> &'static str {
    HELP
}

fn main() {
    let mut args = env::args().skip(1);
    match args.next().as_deref() {
        None | Some("-h") | Some("--help") => {
            print!("{}", help_text());
        }
        Some(command) => {
            eprintln!("error: command `{}` is planned but not implemented in Phase 0", command);
            eprintln!();
            eprint!("{}", help_text());
            process::exit(64);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::help_text;

    #[test]
    fn help_mentions_phase_zero_scope() {
        let help = help_text();
        assert!(help.contains("Phase 0: CLI skeleton only"));
        assert!(help.contains(".cdbc parsing and bytecode execution are not implemented"));
    }

    #[test]
    fn help_mentions_future_run_and_dump_commands() {
        let help = help_text();
        assert!(help.contains("compiler-design-vm run <program.cdbc>"));
        assert!(help.contains("compiler-design-vm dump <program.cdbc>"));
    }
}
RS
```

- [ ] **Step 5: Create `vm-rs/README.md`**

Run:

```bash
cat > vm-rs/README.md <<'MD'
# Compiler Design VM

`compiler-design-vm` is the planned standalone Rust bytecode VM for Compiler Design.

Phase 0 status: this crate is a project skeleton and CLI placeholder. It does not parse `.cdbc` files and does not execute bytecode yet.

## Current Commands

```sh
cargo run --manifest-path vm-rs/Cargo.toml -- --help
```

## Planned Commands

```sh
compiler-design-vm run program.cdbc
compiler-design-vm dump program.cdbc
```

`run` will execute a stable Compiler Design ByteCode artifact. `dump` will parse and print a normalized view of that artifact for debugging and golden tests. These commands are part of future phases.

## Future Module Boundaries

- `format`: `.cdbc` parser and serializer.
- `bytecode`: bytecode structures and validation.
- `value`: runtime values.
- `vm`: executor.
- `heap`: GC-aware heap ownership and root scanning.
- `scheduler`: task scheduling, instruction budgets, and yield points.
- `jit`: JIT metadata and native-code experiments.
MD
```

- [ ] **Step 6: Run Rust verification**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
cargo run --manifest-path vm-rs/Cargo.toml -- --help
```

Expected:

- `cargo test` passes.
- `cargo run ... -- --help` prints help containing `compiler-design-vm`, `run <program.cdbc>`, `dump <program.cdbc>`, and `Phase 0`.

- [ ] **Step 7: Commit Rust VM skeleton**

Run:

```bash
git add vm-rs/Cargo.toml vm-rs/src/main.rs vm-rs/README.md
git commit -m "feat: scaffold rust vm project"
```

---

### Task 3: Add planned `.cdbc` bytecode text format documentation

**Files:**
- Create: `docs/bytecode-text-format.md`
- Test: documentation content checks with `grep`

- [ ] **Step 1: Verify format documentation is absent before adding it**

Run:

```bash
test -f docs/bytecode-text-format.md
```

Expected: fails because the planned format document does not exist before this task.

- [ ] **Step 2: Create `docs/bytecode-text-format.md`**

Run:

```bash
cat > docs/bytecode-text-format.md <<'MD'
# Compiler Design ByteCode Text Format

This document describes the planned stable text artifact format for Compiler Design bytecode files.

The file extension is `.cdbc`, short for Compiler Design ByteCode.

This format is not the same as the current `--bytecode` debug print. The debug print is for humans inspecting compiler output. The `.cdbc` format is a future compiler/VM contract that must be stable, versioned, and parseable by the Rust VM.

## Phase Status

Phase 0 documents the format direction only. The C++ compiler does not emit `.cdbc` files yet, and the Rust VM does not parse or execute them yet.

## Header

Every file starts with a format identifier and version:

```text
cdbc 0.1
```

Future format changes must either remain backward-compatible with `0.1` or use a new version number.

## Sections

A `.cdbc` file is organized into explicit sections:

```text
cdbc 0.1

constants:
  c0 = number 1
  c1 = string "hello"

names:
  n0 = "x"

main registers=3:
  r0 = constant c0
  store_var n0, r0
  r1 = load_var n0
  print r1

function f0 name="add_one" arity=1 registers=4:
  r1 = constant c0
  r2 = add r0, r1
  return r2
```

The exact grammar will be finalized when the C++ emitter and Rust parser are implemented. The section names and reference prefixes are reserved by this plan.

## Value Encoding

Constants use explicit value tags:

```text
c0 = nil
c1 = number 1.25
c2 = bool true
c3 = string "escaped string"
```

Strings use double quotes and backslash escapes for at least `\\`, `\"`, `\n`, `\r`, and `\t`.

## References

References use stable prefixes:

- `cN`: constant index.
- `nN`: name index.
- `rN`: register index.
- `fN`: function index.

Indexes are zero-based decimal integers.

## Opcode Names

The planned opcode names are stable snake-case names:

```text
constant
make_function
array
move
load_var
store_var
assign_var
call
index
assign_index
len
print
return
negate
not
add
subtract
multiply
divide
equal
not_equal
greater
greater_equal
less
less_equal
jump
jump_if_false
jump_if_true
```

New opcodes must be added by updating this document, the C++ bytecode artifact emitter, and the Rust VM parser/executor together.

## Non-Goals for Phase 0

Phase 0 does not define a complete parser grammar, binary encoding, verifier, execution semantics, GC layout, task scheduler, or JIT metadata format. Those belong to later Rust VM phases.
MD
```

- [ ] **Step 3: Verify the document says planned behavior, not implemented behavior**

Run:

```bash
grep -n "does not emit .cdbc files yet" docs/bytecode-text-format.md
grep -n "does not parse or execute them yet" docs/bytecode-text-format.md
grep -n "assign_index" docs/bytecode-text-format.md
```

Expected: all three `grep` commands find matching lines.

- [ ] **Step 4: Commit bytecode format documentation**

Run:

```bash
git add docs/bytecode-text-format.md
git commit -m "docs: add bytecode text format direction"
```

---

### Task 4: Update active documentation for Compiler Design and Rust VM split

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`
- Test: active-doc grep checks

- [ ] **Step 1: Update README title and command examples**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
path = Path('README.md')
text = path.read_text(encoding='utf-8')
text = text.replace('# Compiler Demo', '# Compiler Design')
text = text.replace('./build/compiler_demo', './build/compiler_design')
text = text.replace('compiler_demo executable', 'compiler_design executable')
insert_after = '`--run` executes the existing IR interpreter. `--run-bytecode` executes the newer bytecode VM. They are expected to match for implemented language features.\n'
addition = '\nBackend note: the current C++ bytecode VM is frozen as a reference backend. Future VM work will happen in the standalone Rust `compiler-design-vm` project under `vm-rs/`, using planned `.cdbc` bytecode artifacts. The `.cdbc` emitter and Rust VM executor are not implemented in Phase 0.\n'
if addition.strip() not in text:
    text = text.replace(insert_after, insert_after + addition)
path.write_text(text, encoding='utf-8')
PY
```

Expected active README changes include:

```markdown
# Compiler Design
```

and command examples using:

```sh
./build/compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

- [ ] **Step 2: Update AGENTS project memory**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
path = Path('AGENTS.md')
text = path.read_text(encoding='utf-8')
text = text.replace('This is a small C++17 compiler front-end/interpreter demo.', 'This is a small C++17 Compiler Design front-end/interpreter project.')
text = text.replace('A CLI binary named `compiler_demo`.', 'A CLI binary named `compiler_design`.')
text = text.replace('./build/compiler_demo', './build/compiler_design')
text = text.replace('The bytecode VM already exists and provides extension points for backend research.', 'The C++ bytecode VM already exists and is frozen as a reference backend; future backend research targets the Rust VM project under `vm-rs/`.')
needle = '- A parallel bytecode backend lowers register IR to bytecode; `--run-bytecode` should match `--run` for supported programs.\n'
replacement = '- A parallel C++ bytecode backend lowers register IR to bytecode; `--run-bytecode` should match `--run` for supported programs, but this C++ VM is frozen for future backend research.\n- Future VM backend work targets the Rust `compiler-design-vm` project under `vm-rs/` and planned `.cdbc` artifacts.\n'
if needle in text:
    text = text.replace(needle, replacement)
path.write_text(text, encoding='utf-8')
PY
```

- [ ] **Step 3: Update active roadmap title and backend track**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
path = Path('docs/roadmap.md')
text = path.read_text(encoding='utf-8')
text = text.replace('# Compiler Demo Language Roadmap', '# Compiler Design Language Roadmap')
text = text.replace('Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable, but they are a deferred backend track. The current near-term direction is to improve the language itself.', 'Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable. The C++ bytecode VM is frozen as a reference backend, and future backend work will move to the standalone Rust `compiler-design-vm` project under `vm-rs/`. The current near-term language direction remains improving the language itself.')
text = text.replace('The bytecode VM already exists and provides extension points for backend research. These directions are deferred while the active roadmap focuses on language features:', 'The C++ bytecode VM already exists and remains available for current behavior, but it is frozen for backend research. Future backend work targets the Rust `compiler-design-vm` project and planned `.cdbc` bytecode artifacts:')
text = text.replace('- GC groundwork: VM heap ownership, root scanning, and value reachability.\n- Task scheduling: schedulable VM threads, instruction budgets, yield points, and blocked states.\n- JIT exploration: bytecode metadata, hot function detection, and native-code experiments.\n\nBefore starting any backend track, create a dedicated backend design spec and implementation plan rather than mixing it into this language roadmap.', '- Phase 0: rename to Compiler Design, scaffold `vm-rs/`, and document the planned `.cdbc` text format.\n- Phase 1: add a C++ `.cdbc` bytecode artifact emitter.\n- Phase 2: add Rust VM `.cdbc` parser and dump support.\n- Phase 3: add Rust VM executor parity for current bytecode semantics.\n- Phase 4: explore GC heap ownership/root scanning, task scheduling, and JIT metadata/hot paths.\n\nBefore starting a backend implementation phase, create a dedicated backend design spec and implementation plan rather than mixing it into language feature work.')
path.write_text(text, encoding='utf-8')
PY
```

- [ ] **Step 4: Verify active docs use the new name and frozen VM wording**

Run:

```bash
grep -n "# Compiler Design" README.md docs/roadmap.md
grep -n "compiler_design" README.md AGENTS.md
grep -n "compiler-design-vm" README.md AGENTS.md docs/roadmap.md
grep -n "frozen" README.md AGENTS.md docs/roadmap.md
```

Expected: every `grep` command finds matches.

- [ ] **Step 5: Verify old executable name is gone from active files**

Run:

```bash
grep -R "compiler_demo" -n CMakeLists.txt tests/run_golden_tests.py README.md AGENTS.md docs/roadmap.md || true
```

Expected: no output.

- [ ] **Step 6: Run C++ docs-adjacent verification**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected: C++ build, CTest, golden tests, and selftests pass.

- [ ] **Step 7: Commit active documentation updates**

Run:

```bash
git add README.md AGENTS.md docs/roadmap.md
git commit -m "docs: rename project and document rust vm track"
```

---

### Task 5: Final cross-project verification and cleanup

**Files:**
- Verify: C++ project, Rust VM skeleton, active docs, and repository status.

- [ ] **Step 1: Run clean C++ configure/build/test cycle**

Run:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

- Configure succeeds.
- Build succeeds and produces `./build/compiler_design`.
- CTest passes.
- Golden tests pass with `./build/compiler_design`.
- Golden runner selftests pass.

- [ ] **Step 2: Run Rust VM skeleton verification**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
cargo run --manifest-path vm-rs/Cargo.toml -- --help
```

Expected:

- Cargo tests pass.
- Help output includes `compiler-design-vm`, `run <program.cdbc>`, `dump <program.cdbc>`, and `Phase 0`.

- [ ] **Step 3: Check active project naming**

Run:

```bash
grep -R "compiler_demo" -n CMakeLists.txt tests/run_golden_tests.py README.md AGENTS.md docs/roadmap.md || true
grep -R "Compiler Demo" -n README.md AGENTS.md docs/roadmap.md || true
grep -R "compiler_design" -n CMakeLists.txt README.md AGENTS.md tests/run_golden_tests.py
```

Expected:

- The first two `grep` commands produce no output for active files.
- The final `grep` command finds the renamed executable references.

- [ ] **Step 4: Check Phase 0 did not add runtime behavior**

Run:

```bash
grep -R "emit-bytecode\|cdbc" -n src include CMakeLists.txt tests || true
```

Expected: no output for `emit-bytecode` or `cdbc` in C++ source/test implementation files. `.cdbc` should only appear in docs and `vm-rs` placeholder text during Phase 0.

- [ ] **Step 5: Review diff and status**

Run:

```bash
git diff --stat HEAD~4..HEAD
git diff --name-status HEAD~4..HEAD
git status --short --branch
```

Expected:

- Diff includes CMake/test rename, Rust VM skeleton, `.cdbc` docs, active documentation updates, and the previously committed design/plan docs.
- Working tree is clean.

- [ ] **Step 6: Prepare completion summary**

Report exact verification commands and results. State that Phase 0 prepared the Rust VM split and Compiler Design rename without changing language/runtime semantics. Then use `superpowers:finishing-a-development-branch` to present integration options.
