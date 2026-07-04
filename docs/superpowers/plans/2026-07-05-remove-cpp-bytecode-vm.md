# Remove C++ Bytecode VM Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the in-process C++ bytecode VM and `compiler_design --run-bytecode`, leaving Rust `compiler-design-vm run` as the bytecode execution path.

**Architecture:** Keep C++ bytecode lowering, debug bytecode printing, and `.cdbc` emission. Delete the C++ executor files and remove `--run-bytecode` from CLI/tests/docs. Use existing Rust VM integration tests as bytecode execution parity coverage.

**Tech Stack:** C++17/CMake compiler binary, Python golden runners, Rust 2021 `vm-rs`, CTest.

---

## File Structure

- Delete `include/BytecodeVM.hpp`: old in-process C++ VM API.
- Delete `src/BytecodeVM.cpp`: old in-process C++ VM implementation.
- Modify `CMakeLists.txt`: remove `src/BytecodeVM.cpp` from `compiler_design` target.
- Modify `src/main.cpp`: remove `BytecodeVM` include, `--run-bytecode` parsing, usage text, bytecode execution branch, and `runBytecode` state.
- Modify `tests/run_golden_tests.py`: remove `run_bytecode.out` and `.run_bytecode.*` discovery/execution.
- Modify `tests/run_golden_tests_selftest.py`: delete run-bytecode-specific selftests and adjust counts/names.
- Delete `tests/golden/**/run_bytecode.out` and `tests/golden/runtime_errors/*.run_bytecode.err/.exit`.
- Modify `README.md`, `AGENTS.md`, `docs/roadmap.md`, and possibly `docs/bytecode-text-format.md`: direct bytecode execution users to Rust VM.

## TDD Notes

Start by changing the tests/fixtures so they fail against the existing implementation or old expectations. Then remove the old C++ VM and update docs until the full suite passes. Keep Rust VM integration tests unchanged except where they no longer need `run_bytecode.out` fallback behavior.

---

### Task 1: Remove `run_bytecode` expectations from golden runner tests

**Files:**
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`

- [ ] **Step 1: Remove `run_bytecode.out` success check**

In `tests/run_golden_tests.py`, change `SUCCESS_CHECKS` from:

```python
SUCCESS_CHECKS = (
    ("default(ast)", (), "ast.out"),
    ("--ir", ("--ir",), "ir.out"),
    ("--bytecode", ("--bytecode",), "bytecode.out"),
    ("--run", ("--run",), "run.out"),
    ("--run-bytecode", ("--run-bytecode",), "run_bytecode.out"),
)
```

to:

```python
SUCCESS_CHECKS = (
    ("default(ast)", (), "ast.out"),
    ("--ir", ("--ir",), "ir.out"),
    ("--bytecode", ("--bytecode",), "bytecode.out"),
    ("--run", ("--run",), "run.out"),
)
```

- [ ] **Step 2: Remove runtime bytecode error execution**

In `tests/run_golden_tests.py`, replace `check_runtime_error_case` with:

```python
def check_runtime_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    return check_runtime_error_execution(
        compiler,
        source,
        update,
        ("--run",),
        ".run.err",
        ".exit",
        "--run",
        optional_when_missing=False,
    )
```

- [ ] **Step 3: Remove obsolete selftests**

In `tests/run_golden_tests_selftest.py`, delete these test methods entirely:

```python
def test_runtime_error_case_without_run_bytecode_err_skips_run_bytecode_without_invoking_compiler(self) -> None:
    ...

def test_runtime_error_case_with_orphan_run_bytecode_exit_fails_missing_stderr(self) -> None:
    ...

def test_success_case_with_run_bytecode_expected_file_runs_run_bytecode_flag(self) -> None:
    ...

def test_runtime_error_case_with_run_bytecode_expected_files(self) -> None:
    ...
```

- [ ] **Step 4: Adjust missing runtime error selftest count**

In `tests/run_golden_tests_selftest.py`, update `test_runtime_error_case_without_run_err_fails_for_run_mode` expected result length from:

```python
self.assertEqual(len(results), 2)
```

to:

```python
self.assertEqual(len(results), 1)
```

Keep the remaining assertions checking the `--run` case.

- [ ] **Step 5: Run selftests and confirm they fail before fixture cleanup**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: selftests should pass after the code edits. Golden tests should fail because existing `run_bytecode.out` files are now ignored; some success fixtures may now have no active expected output only if they previously had only `run_bytecode.out`. The failure messages should mention missing expected files or reduced check coverage, not Python exceptions.

- [ ] **Step 6: Commit golden runner changes**

Run:

```bash
git add tests/run_golden_tests.py tests/run_golden_tests_selftest.py
git commit -m "test: remove cxx run-bytecode golden mode"
```

---

### Task 2: Delete stale `run_bytecode` fixture files

**Files:**
- Delete: `tests/golden/**/run_bytecode.out`
- Delete: `tests/golden/runtime_errors/*.run_bytecode.err`
- Delete: `tests/golden/runtime_errors/*.run_bytecode.exit`

- [ ] **Step 1: Delete stale bytecode execution fixtures**

Run:

```bash
find tests/golden -name 'run_bytecode.out' -delete
find tests/golden/runtime_errors -name '*.run_bytecode.err' -delete
find tests/golden/runtime_errors -name '*.run_bytecode.exit' -delete
```

- [ ] **Step 2: Verify no stale files remain**

Run:

```bash
find tests/golden -name 'run_bytecode.out' -o -name '*.run_bytecode.err' -o -name '*.run_bytecode.exit'
```

Expected: no output.

- [ ] **Step 3: Run golden tests after fixture cleanup**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: golden tests pass without `run_bytecode` checks. Rust VM tests pass using `run.out` or artifact `run.out` expectations.

- [ ] **Step 4: Commit fixture cleanup**

Run:

```bash
git add -u tests/golden
git commit -m "test: delete stale cxx bytecode execution goldens"
```

---

### Task 3: Remove C++ VM source and `--run-bytecode` CLI

**Files:**
- Delete: `include/BytecodeVM.hpp`
- Delete: `src/BytecodeVM.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`

- [ ] **Step 1: Remove C++ VM files**

Run:

```bash
git rm include/BytecodeVM.hpp src/BytecodeVM.cpp
```

- [ ] **Step 2: Remove `BytecodeVM.cpp` from CMake**

In `CMakeLists.txt`, remove this line from `add_executable(compiler_design ...)`:

```cmake
    src/BytecodeVM.cpp
```

- [ ] **Step 3: Remove C++ VM include**

In `src/main.cpp`, delete this include:

```cpp
#include "BytecodeVM.hpp"
```

- [ ] **Step 4: Update usage text**

In `src/main.cpp`, change `printUsage` from:

```cpp
std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [--run] [--run-bytecode] [file]\n"
          << "       " << executable << " --emit-bytecode output.cdbc file\n"
          << "If file is omitted, source is read from stdin except for --emit-bytecode, which requires a file.\n";
```

to:

```cpp
std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [--run] [file]\n"
          << "       " << executable << " --emit-bytecode output.cdbc file\n"
          << "If file is omitted, source is read from stdin except for --emit-bytecode, which requires a file.\n";
```

- [ ] **Step 5: Remove `runBytecode` state and parsing**

In `src/main.cpp`, delete:

```cpp
bool runBytecode = false;
```

Delete this parser branch:

```cpp
} else if (arg == "--run-bytecode") {
    runBytecode = true;
```

- [ ] **Step 6: Remove `runBytecode` conditions**

In `src/main.cpp`, replace:

```cpp
if (inputPath.empty() || showTokens || showIr || showBytecode || runIr || runBytecode) {
```

with:

```cpp
if (inputPath.empty() || showTokens || showIr || showBytecode || runIr) {
```

Replace:

```cpp
if (!emitBytecodePath && !showIr && !showBytecode && !runIr && !runBytecode) {
```

with:

```cpp
if (!emitBytecodePath && !showIr && !showBytecode && !runIr) {
```

Replace:

```cpp
if (emitBytecodePath || showIr || showBytecode || runIr || runBytecode) {
```

with:

```cpp
if (emitBytecodePath || showIr || showBytecode || runIr) {
```

Replace:

```cpp
if (emitBytecodePath || showBytecode || runBytecode) {
```

with:

```cpp
if (emitBytecodePath || showBytecode) {
```

- [ ] **Step 7: Remove C++ bytecode execution branch**

In `src/main.cpp`, delete:

```cpp
if (runBytecode) {
    separateSection();
    BytecodeVM vm(std::cout);
    vm.execute(*bytecode);
}
```

- [ ] **Step 8: Build and verify removed CLI behavior**

Run:

```bash
cmake -S . -B build
cmake --build build
./build/compiler_design --help
./build/compiler_design --run-bytecode tests/bytecode_artifacts/arithmetic/input.cd; test $? -eq 64
```

Expected:

- Build succeeds.
- Help output does not mention `--run-bytecode`.
- `--run-bytecode` exits `64` and prints usage without listing `--run-bytecode`.

- [ ] **Step 9: Run compiler/Rust VM test subset**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

Expected: all commands pass.

- [ ] **Step 10: Commit C++ VM removal**

Run:

```bash
git add CMakeLists.txt src/main.cpp
git commit -m "refactor: remove cxx bytecode vm"
```

---

### Task 4: Update docs for Rust-only bytecode execution

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`
- Modify: `docs/bytecode-text-format.md` if needed

- [ ] **Step 1: Update README command examples**

In `README.md`, remove this line from the Run command block if present:

```sh
./build/compiler_design --run-bytecode examples/hello.cd
```

Ensure the backend note says:

```markdown
`--run` executes the C++ IR interpreter. Bytecode execution is handled by the Rust VM via `.cdbc` artifacts:

```sh
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

`--bytecode` remains a debug-print mode for inspecting compiler output.
```

Use a four-backtick outer fence if editing around Markdown code fences manually.

- [ ] **Step 2: Update AGENTS architecture map**

In `AGENTS.md`, delete this architecture bullet:

```markdown
- `include/BytecodeVM.hpp`, `src/BytecodeVM.cpp`: bytecode VM execution, VM heap boundary, and VM thread/frame state.
```

Add this bullet near Rust VM or bytecode artifact notes:

```markdown
- `vm-rs/src/vm.rs`, `vm-rs/src/value.rs`, `vm-rs/src/runtime.rs`: Rust `.cdbc` bytecode execution, runtime values, shared cells, closures, and arrays.
```

- [ ] **Step 3: Update AGENTS golden conventions**

In `AGENTS.md`, replace:

```markdown
A successful fixture may include one or more expected output files. In non-update mode, a success fixture with no expected files is a test failure. Successful fixtures may include `bytecode.out` for `--bytecode` and `run_bytecode.out` for `--run-bytecode`.
```

with:

```markdown
A successful fixture may include one or more expected output files. In non-update mode, a success fixture with no expected files is a test failure. Successful fixtures may include `bytecode.out` for `--bytecode`. Bytecode execution parity is covered by `tests/run_rust_vm_tests.py`, not by `run_bytecode.out` golden files.
```

Delete this paragraph:

```markdown
Runtime-error fixtures may include `.run_bytecode.err` and `.run_bytecode.exit` to check bytecode VM runtime diagnostics.
```

- [ ] **Step 4: Update AGENTS semantics and workflow wording**

In `AGENTS.md`, replace:

```markdown
When IR behavior changes, update both the IR interpreter and the bytecode backend unless the change is intentionally IR-only. Bytecode lowering should preserve current IR semantics, and `--run-bytecode` should match `--run` for supported programs.
```

with:

```markdown
When IR behavior changes, update both the IR interpreter and bytecode artifact/Rust VM path unless the change is intentionally IR-only. Bytecode lowering should preserve current IR semantics, and Rust VM execution should match `--run` for supported programs covered by `tests/run_rust_vm_tests.py`.
```

Replace any current-language bullet saying the C++ VM is frozen/reference with:

```markdown
- A C++ bytecode backend lowers register IR to bytecode and `.cdbc` artifacts; Rust `compiler-design-vm` is the bytecode execution backend.
```

- [ ] **Step 5: Update roadmap backend track**

In `docs/roadmap.md`, replace the deferred backend opening sentence:

```markdown
The C++ bytecode VM already exists and remains available for current behavior, but it is frozen for backend research. Future backend work targets the Rust `compiler-design-vm` project and planned `.cdbc` bytecode artifacts:
```

with:

```markdown
The former C++ bytecode VM has been removed. Future backend work targets the Rust `compiler-design-vm` project and `.cdbc` bytecode artifacts:
```

Add a phase bullet after Phase 3:

```markdown
- Phase 3B: remove the old C++ bytecode VM and `--run-bytecode`; Rust VM is now the bytecode execution backend. Implemented.
```

- [ ] **Step 6: Grep for active stale references**

Run:

```bash
grep -R "--run-bytecode\|BytecodeVM\|run_bytecode" -n CMakeLists.txt include src tests README.md AGENTS.md docs/roadmap.md docs/bytecode-text-format.md vm-rs || true
```

Expected: no active-source references remain except possibly historical docs under `docs/superpowers/` if the grep command is widened later. This exact command should not report stale active references.

- [ ] **Step 7: Run docs-related verification**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
rm -rf tests/__pycache__
```

Expected: all pass.

- [ ] **Step 8: Commit docs update**

Run:

```bash
git add README.md AGENTS.md docs/roadmap.md docs/bytecode-text-format.md
git commit -m "docs: point bytecode execution to rust vm"
```

If `docs/bytecode-text-format.md` was unchanged, omit it from `git add` or let `git add` ignore it.

---

### Task 5: Full verification and branch completion

**Files:**
- Verify all source, tests, and docs.

- [ ] **Step 1: Run full clean verification**

Run:

```bash
rm -rf build
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

Expected:

- CMake configure/build exits `0`.
- CTest passes, including `golden`, `bytecode_artifacts`, and `rust_vm`.
- Golden tests pass without `run_bytecode` checks.
- Golden runner selftests pass.
- Bytecode artifact tests pass.
- Rust VM tests pass.
- Rust unit tests pass.

- [ ] **Step 2: Verify removed CLI explicitly**

Run:

```bash
set +e
./build/compiler_design --run-bytecode tests/bytecode_artifacts/arithmetic/input.cd > /tmp/run-bytecode.stdout 2> /tmp/run-bytecode.stderr
status=$?
set -e
cat /tmp/run-bytecode.stdout
cat /tmp/run-bytecode.stderr
test "$status" -eq 64
! grep -q -- "--run-bytecode" /tmp/run-bytecode.stderr
```

Expected:

- stdout is empty.
- stderr contains usage.
- exit status is `64`.
- usage does not mention `--run-bytecode`.

- [ ] **Step 3: Verify Rust VM replacement path**

Run:

```bash
./build/compiler_design --emit-bytecode /tmp/compiler-design-functions.cdbc tests/bytecode_artifacts/functions_arrays/input.cd
cargo run --manifest-path vm-rs/Cargo.toml -- run /tmp/compiler-design-functions.cdbc
```

Expected stdout from the Rust VM run:

```text
2
3
```

- [ ] **Step 4: Review diff and status**

Run:

```bash
git diff --stat HEAD~5..HEAD
git diff --name-status HEAD~5..HEAD
git status --short --branch
```

Expected: diff includes C++ VM deletion, CLI/test/docs cleanup, and no build artifacts. Working tree is clean.

- [ ] **Step 5: Complete branch**

Use `superpowers:verification-before-completion` to report exact verification results, then use `superpowers:finishing-a-development-branch` to present merge/push/keep/discard options.
