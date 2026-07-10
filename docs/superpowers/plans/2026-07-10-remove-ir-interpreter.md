# Remove IR Interpreter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the C++ IR interpreter and `compiler_design --run`, and make `.cdbc` plus the Rust VM the only runtime execution verification path.

**Architecture:** First move test ownership: `run_golden_tests.py` becomes compiler-output/frontend-only, while `run_rust_vm_tests.py` owns all `run.out` and runtime-error execution checks. Then remove the C++ interpreter from the CLI/build/source tree, adjust Rust VM runtime diagnostics to preserve existing `.run.err` goldens, and update docs. The C++ compiler continues to emit IR, bytecode debug output, and stable `.cdbc` artifacts.

**Tech Stack:** C++17, CMake, Python unittest/golden runners, Rust `compiler-design-vm`, `.cdbc` bytecode artifacts.

---

## File Structure

- Modify `tests/run_golden_tests.py`: remove `--run` success checks and runtime-error ownership.
- Modify `tests/run_golden_tests_selftest.py`: update golden runner selftests for the new ownership boundary.
- Modify `tests/run_rust_vm_tests.py`: discover all `run.out` success fixtures and runtime-error fixtures; compare stdout/stderr/exit through Rust VM execution.
- Modify `tests/cli_multi_source_tests.py`: replace compiler `--run` execution checks with `--emit-bytecode` plus Rust VM execution; add a test that `--run` is rejected.
- Modify `CMakeLists.txt`: remove `src/IRInterpreter.cpp`; pass `vm-rs` into CLI multi-source tests.
- Modify `src/main.cpp`: remove `IRInterpreter` include/flag/execution path and reject `--run` with usage exit 64.
- Delete `include/IRInterpreter.hpp` and `src/IRInterpreter.cpp`.
- Modify `vm-rs/src/main.rs` and `vm-rs/src/vm.rs`: print runtime execution errors in the existing language diagnostic shape.
- Modify `README.md` and `docs/roadmap.md`: document Rust VM execution as the sole runtime path.

---

### Task 1: Update golden runner selftests for compiler-only ownership

**Files:**
- Modify: `tests/run_golden_tests_selftest.py`
- Test: `tests/run_golden_tests_selftest.py`

- [ ] **Step 1: Confirm a clean workspace**

Run:

```sh
git status --short
```

Expected: no output.

- [ ] **Step 2: Edit success-output selftests to remove `run.out` from compiler-owned outputs**

In `tests/run_golden_tests_selftest.py`, update `test_update_rewrites_only_existing_success_outputs_by_default` so it still checks `ast.out`, `ir.out`, and `bytecode.out` are not created, and no longer checks `run.out` as an output owned by this runner. The relevant final assertions should become:

```python
            self.assertFalse((case_dir / "ir.out").exists())
            self.assertFalse((case_dir / "bytecode.out").exists())
```

Update `test_update_missing_creates_missing_success_outputs_explicitly` so it expects 3 compiler-output checks instead of 4 and iterates only compiler-owned golden names:

```python
            self.assertEqual(len(results), 3)
            self.assertTrue(all(result.passed for result in results), results)
            for golden_name in ("ast.out", "ir.out", "bytecode.out"):
                self.assertEqual(
                    (case_dir / golden_name).read_text(encoding="utf-8"),
                    "new output\n",
                )
            self.assertFalse((case_dir / "run.out").exists())
```

- [ ] **Step 3: Replace runtime-error golden runner selftests**

Delete the existing methods `test_runtime_error_case_with_unexpected_stdout_fails` and `test_runtime_error_case_without_run_err_fails_for_run_mode` from `tests/run_golden_tests_selftest.py`.

Add this replacement method in their place:

```python
    def test_runtime_error_cases_are_not_checked_by_compiler_golden_runner(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            runtime_dir = golden_dir / "runtime_errors"
            runtime_dir.mkdir(parents=True)
            (runtime_dir / "division_by_zero.cd").write_text("1 / 0;\n", encoding="utf-8")
            (runtime_dir / "division_by_zero.run.err").write_text("Runtime error: division by zero\n", encoding="utf-8")
            (runtime_dir / "division_by_zero.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(root)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("no golden test checks", results[0].message)
```

This asserts runtime-error fixtures no longer create compiler-runner checks.

- [ ] **Step 4: Replace args.txt success selftest so it checks compiler output mode, not `--run`**

Rename `test_success_case_args_txt_replaces_default_input_path` to `test_success_case_args_txt_replaces_default_input_path_for_ir` and replace its body with:

```python
    def test_success_case_args_txt_replaces_default_input_path_for_ir(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            case_dir = root / "multi_file"
            case_dir.mkdir()
            (case_dir / "lib.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "main.cd").write_text("print 2;\n", encoding="utf-8")
            (case_dir / "args.txt").write_text("lib.cd main.cd\n", encoding="utf-8")
            (case_dir / "ir.out").write_text("ir\n", encoding="utf-8")

            commands: list[list[str]] = []

            def fake_run(command, text, capture_output, check):
                commands.append([str(part) for part in command])
                return subprocess.CompletedProcess(command, 0, "ir\n", "")

            with unittest.mock.patch.object(run_golden_tests.subprocess, "run", side_effect=fake_run):
                results = run_golden_tests.check_success_case(Path("compiler"), case_dir, False)

            self.assertTrue(all(result.passed for result in results), results)
            self.assertIn(["compiler", "--ir", str(case_dir / "lib.cd"), str(case_dir / "main.cd")], commands)
```

- [ ] **Step 5: Add a selftest that `run.out`-only success fixtures are valid but ignored**

Add this method near the other success-case tests:

```python
    def test_run_out_only_success_case_is_owned_by_rust_vm_runner(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "run_only_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "run.out").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(root, stdout="unexpected compiler output\n")

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("no golden test checks", results[0].message)
```

- [ ] **Step 6: Run the selftest and verify it fails before implementation**

Run:

```sh
python3 tests/run_golden_tests_selftest.py
```

Expected: FAIL, because `tests/run_golden_tests.py` still runs `--run` and still owns runtime-error fixtures.

---

### Task 2: Make the golden runner compiler-output/frontend-only

**Files:**
- Modify: `tests/run_golden_tests.py`
- Test: `tests/run_golden_tests_selftest.py`

- [ ] **Step 1: Remove `--run` from success checks**

In `tests/run_golden_tests.py`, replace `SUCCESS_CHECKS` with:

```python
SUCCESS_CHECKS = (
    ("default(ast)", (), "ast.out"),
    ("--ir", ("--ir",), "ir.out"),
    ("--bytecode", ("--bytecode",), "bytecode.out"),
)
```

- [ ] **Step 2: Make run.out count as a valid fixture marker but not a compiler-owned expected file**

In `check_success_case`, replace the initial expected-name setup with:

```python
    compiler_golden_names = tuple(golden_name for _, _, golden_name in SUCCESS_CHECKS)
    fixture_marker_names = (*compiler_golden_names, "run.out")
```

Replace the no-expected-files check with:

```python
    if not any((case_dir / golden_name).exists() for golden_name in fixture_marker_names):
        update_hint = " Pass --update-missing with --update to create them." if update else ""
        return [
            CheckResult(
                case_dir.name,
                False,
                (
                    f"FAIL {case_dir.name} has no expected files; "
                    f"expected at least one of: {', '.join(fixture_marker_names)}"
                    f"{update_hint}"
                ),
            )
        ]
```

Then add this early return immediately after it:

```python
    if not any((case_dir / golden_name).exists() for golden_name in compiler_golden_names):
        return []
```

This lets `run.out`-only fixtures be valid but ignored by this runner.

- [ ] **Step 3: Remove runtime-error discovery and checks from `run_all`**

In `run_all`, delete this loop:

```python
    for source in discover_runtime_error_cases(golden_dir):
        if case_matches(f"runtime_errors/{source.stem}", case_filters):
            results.extend(check_runtime_error_case(compiler, source, update))
```

Leave helper functions in place only if they are still referenced nowhere; a later cleanup step may remove dead helpers. Prefer deleting `discover_runtime_error_cases`, `unexpected_runtime_stdout_result`, `check_runtime_error_execution`, and `check_runtime_error_case` in this task if the resulting diff is straightforward.

- [ ] **Step 4: Run the golden runner selftests**

Run:

```sh
python3 tests/run_golden_tests_selftest.py
```

Expected: PASS.

- [ ] **Step 5: Run the compiler golden runner**

Run:

```sh
cmake --build build --target compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS with a lower check count than before, because `run.out` and runtime-error checks moved to Rust VM tests.

- [ ] **Step 6: Commit the golden runner migration**

Run:

```sh
git add tests/run_golden_tests.py tests/run_golden_tests_selftest.py
git commit -m "test: move golden execution checks to VM runner"
```

Expected: commit succeeds.

---

### Task 3: Expand Rust VM runner to own run.out and runtime-error fixtures

**Files:**
- Modify: `tests/run_rust_vm_tests.py`
- Test: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Add runtime-error discovery and expected-file helpers**

In `tests/run_rust_vm_tests.py`, add this function after `discover_golden_cases`:

```python
def discover_runtime_error_cases(root: Path) -> list[Path]:
    if not root.is_dir():
        return []
    return sorted(root.glob("*.cd"))
```

Add this helper after `expected_output`:

```python
def expected_runtime_error(source: Path) -> tuple[str, str]:
    stem = source.with_suffix("")
    return read_text(stem.with_suffix(".run.err")), read_text(stem.with_suffix(".exit")).strip()
```

- [ ] **Step 2: Split bytecode emission into a helper**

Add this helper before `check_case`:

```python
def emit_bytecode(compiler: Path, sources: list[Path], artifact: Path, name: str) -> tuple[list[CheckResult], bool]:
    compile_command = [str(compiler), "--emit-bytecode", str(artifact), *(str(source) for source in sources)]
    compiled = run_command(compile_command)
    results: list[CheckResult] = []
    compile_name = f"{name} emit"
    if compiled.returncode != 0:
        results.append(CheckResult(
            compile_name,
            False,
            f"FAIL {compile_name} exited with {compiled.returncode}\n\nSTDOUT:\n{compiled.stdout}\nSTDERR:\n{compiled.stderr}",
        ))
        return results, False
    if compiled.stdout:
        results.append(CheckResult(compile_name, False, f"FAIL {compile_name} produced unexpected stdout\n\n{compiled.stdout}"))
    if compiled.stderr:
        results.append(CheckResult(compile_name, False, f"FAIL {compile_name} produced unexpected stderr\n\n{compiled.stderr}"))
    if not artifact.is_file():
        results.append(CheckResult(compile_name, False, f"FAIL {compile_name} did not create {artifact}"))
        return results, False
    if not any(result.name == compile_name and not result.passed for result in results):
        results.append(CheckResult(compile_name, True))
    return results, True
```

Then update `check_case` to call `emit_bytecode` instead of duplicating compile logic. The start of `check_case` should become:

```python
def check_case(compiler: Path, vm_manifest: Path, case_dir: Path) -> list[CheckResult]:
    sources = compiler_inputs(case_dir)
    expected = expected_output(case_dir)
    results: list[CheckResult] = []

    with tempfile.TemporaryDirectory() as temp_dir:
        artifact = Path(temp_dir) / "program.cdbc"
        emit_results, can_run = emit_bytecode(compiler, sources, artifact, case_dir.name)
        results.extend(emit_results)
        if not can_run:
            return results

        run_command_line = ["cargo", "run", "--quiet", "--manifest-path", str(vm_manifest), "--", "run", str(artifact)]
```

Keep the existing success run comparison after that block.

- [ ] **Step 3: Add runtime-error execution checks**

Add this function after `check_case`:

```python
def check_runtime_error_case(compiler: Path, vm_manifest: Path, source: Path) -> list[CheckResult]:
    expected_err, expected_exit = expected_runtime_error(source)
    results: list[CheckResult] = []

    with tempfile.TemporaryDirectory() as temp_dir:
        artifact = Path(temp_dir) / "program.cdbc"
        case_name = f"runtime_errors/{source.stem}"
        emit_results, can_run = emit_bytecode(compiler, [source], artifact, case_name)
        results.extend(emit_results)
        if not can_run:
            return results

        run_command_line = ["cargo", "run", "--quiet", "--manifest-path", str(vm_manifest), "--", "run", str(artifact)]
        executed = run_command(run_command_line)
        run_name = f"runtime_errors/{source.stem} rust-run"
        if executed.stdout:
            results.append(CheckResult(run_name, False, f"FAIL {run_name} produced unexpected stdout\n\n{executed.stdout}"))
        if executed.stderr != expected_err:
            results.append(CheckResult(
                run_name,
                False,
                f"FAIL {run_name} stderr mismatch\n\n" + unified_diff(expected_err, executed.stderr, "expected stderr", "actual stderr"),
            ))
        if str(executed.returncode) != expected_exit:
            results.append(CheckResult(
                run_name,
                False,
                f"FAIL {run_name} exit code mismatch\nexpected: {expected_exit}\nactual: {executed.returncode}",
            ))
        if not any(result.name == run_name and not result.passed for result in results):
            results.append(CheckResult(run_name, True))

    return results
```

- [ ] **Step 4: Discover all run.out goldens and runtime-error fixtures**

In `main`, replace the allowlist block with this simpler discovery:

```python
    case_dirs = discover_artifact_cases(tests_root / "bytecode_artifacts")
    if args.goldens:
        case_dirs.extend(discover_golden_cases(tests_root / "golden"))

    if args.cases:
        wanted = set(args.cases)
        case_dirs = [path for path in case_dirs if path.name in wanted]
```

After the success-case loop, add runtime-error checks:

```python
    if args.goldens:
        runtime_sources = discover_runtime_error_cases(tests_root / "golden" / "runtime_errors")
        if args.cases:
            wanted = set(args.cases)
            runtime_sources = [source for source in runtime_sources if source.stem in wanted or f"runtime_errors/{source.stem}" in wanted]
        for source in runtime_sources:
            results.extend(check_runtime_error_case(compiler, vm_manifest, source))
```

Make sure this runs before failed-result reporting.

- [ ] **Step 5: Run Rust VM tests and observe runtime-error diagnostic mismatch before VM fix**

Run:

```sh
cmake --build build --target compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case division_by_zero
```

Expected: FAIL with stderr mismatch showing actual `error: runtime error: division by zero` and expected `Runtime error: division by zero`.

Do not commit this task yet; the next task fixes the VM diagnostic and then commits the runner plus VM changes together.

---

### Task 4: Preserve runtime-error diagnostic shape in Rust VM CLI

**Files:**
- Modify: `vm-rs/src/main.rs`
- Modify: `vm-rs/src/vm.rs`
- Test: `tests/run_rust_vm_tests.py`, Rust unit tests

- [ ] **Step 1: Update `RuntimeError` display**

In `vm-rs/src/vm.rs`, change the `fmt::Display for RuntimeError` implementation from lower-case runtime error to upper-case language diagnostic:

```rust
impl fmt::Display for RuntimeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Runtime error: {}", self.message)
    }
}
```

- [ ] **Step 2: Remove the extra `error:` prefix for VM runtime execution errors**

In `vm-rs/src/main.rs`, replace the body of `fn run(path: &str) -> Result<(), String>` with:

```rust
fn run(path: &str) -> Result<(), String> {
    let source = fs::read_to_string(path)
        .map_err(|error| format!("error: failed to read `{}`: {}", path, error))?;
    let program = format::parse_program(&source).map_err(|error| format!("error: {}", error))?;
    let output = vm::VM::new(&program)
        .run()
        .map_err(|error| error.to_string())?;
    print!("{}", output);
    Ok(())
}
```

This keeps file and parse errors in CLI `error:` style, but runtime execution errors use `Runtime error: ...`.

- [ ] **Step 3: Run focused runtime-error VM check**

Run:

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case division_by_zero
```

Expected: PASS for the selected runtime-error case.

- [ ] **Step 4: Run Rust unit tests**

Run:

```sh
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: PASS.

- [ ] **Step 5: Run all Rust VM integration tests**

Run:

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: PASS. The check count should be higher than before because it now includes all `run.out` success fixtures and runtime-error fixtures.

- [ ] **Step 6: Commit the Rust VM runner and diagnostic migration**

Run:

```sh
git add tests/run_rust_vm_tests.py vm-rs/src/main.rs vm-rs/src/vm.rs
git commit -m "test: verify execution through Rust VM"
```

Expected: commit succeeds.

---

### Task 5: Migrate CLI multi-source tests away from --run

**Files:**
- Modify: `tests/cli_multi_source_tests.py`
- Modify: `CMakeLists.txt`
- Test: CTest `cli_multi_source`

- [ ] **Step 1: Update CMake to pass `vm-rs` to CLI tests**

In `CMakeLists.txt`, replace the `cli_multi_source` test command with:

```cmake
add_test(
    NAME cli_multi_source
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cli_multi_source_tests.py $<TARGET_FILE:compiler_design> ${CMAKE_SOURCE_DIR}/vm-rs
)
```

- [ ] **Step 2: Update CLI test setup and add VM helper methods**

In `tests/cli_multi_source_tests.py`, update `setUp` to require compiler and VM path:

```python
    def setUp(self) -> None:
        if len(sys.argv) != 3:
            self.fail("usage: cli_multi_source_tests.py <compiler> <vm-rs>")
        self.compiler = Path(sys.argv[1]).resolve()
        if not self.compiler.is_file():
            self.fail(f"compiler not found: {self.compiler}")
        self.vm = Path(sys.argv[2]).resolve()
        self.vm_manifest = self.vm / "Cargo.toml" if self.vm.is_dir() else self.vm
        if not self.vm_manifest.is_file():
            self.fail(f"vm manifest not found: {self.vm_manifest}")
```

Add these helper methods below `run_compiler`:

```python
    def emit_and_run_vm(self, root: Path, *sources: Path) -> subprocess.CompletedProcess[str]:
        artifact = root / "program.cdbc"
        emitted = self.run_compiler("--emit-bytecode", str(artifact), *(str(source) for source in sources))
        self.assertEqual(emitted.returncode, 0, emitted.stderr)
        self.assertEqual(emitted.stdout, "")
        self.assertEqual(emitted.stderr, "")
        self.assertTrue(artifact.is_file())
        return subprocess.run(
            ["cargo", "run", "--quiet", "--manifest-path", str(self.vm_manifest), "--", "run", str(artifact)],
            text=True,
            capture_output=True,
            check=False,
        )
```

- [ ] **Step 3: Replace successful `--run` calls with VM execution**

In these tests, replace `completed = self.run_compiler("--run", ...)` with `completed = self.emit_and_run_vm(root, ...)` and keep the existing stdout/stderr/returncode assertions:

- `test_run_accepts_multiple_input_files_in_order`
- `test_direct_input_files_parse_as_one_combined_source`
- `test_direct_input_files_lex_as_one_combined_source`
- `test_direct_cli_files_with_export_still_share_entry_scope`
- `test_import_path_resolves_relative_to_importing_file`
- `test_canonical_duplicate_import_spellings_are_deduplicated`
- `test_import_text_inside_string_and_comment_is_ignored_by_loader`

For example, the first test should call:

```python
            completed = self.emit_and_run_vm(root, lib, main)
```

- [ ] **Step 4: Replace missing-file `--run` test with `--emit-bytecode`**

In `test_missing_second_input_file_reports_path`, create an artifact path and call `--emit-bytecode`:

```python
            artifact = root / "program.cdbc"
            completed = self.run_compiler("--emit-bytecode", str(artifact), str(existing), str(missing))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, f"failed to open input file: {missing}\n")
            self.assertFalse(artifact.exists())
```

- [ ] **Step 5: Replace stdin execution test with AST stdin test**

Rename `test_stdin_still_works_when_no_input_files_are_given` to `test_stdin_still_prints_ast_when_no_input_files_are_given` and replace its body with:

```python
    def test_stdin_still_prints_ast_when_no_input_files_are_given(self) -> None:
        completed = self.run_compiler(input_text="print 7;\n")

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stderr, "")
        self.assertIn("print", completed.stdout)
        self.assertIn("7", completed.stdout)
```

- [ ] **Step 6: Add a CLI test that `--run` is rejected**

Add this test near other CLI mode tests:

```python
    def test_run_mode_is_removed(self) -> None:
        completed = self.run_compiler("--run", input_text="print 1;\n")

        self.assertEqual(completed.returncode, 64)
        self.assertEqual(completed.stdout, "")
        self.assertIn("Usage:", completed.stderr)
        self.assertNotIn("[--run]", completed.stderr)
```

- [ ] **Step 7: Run CLI multi-source tests and verify they fail before CLI removal**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
```

Expected: FAIL, because `compiler_design --run` is still accepted and usage still advertises it.

Do not commit this task yet; the next task removes the CLI mode and then commits tests plus implementation together.

---

### Task 6: Remove C++ --run mode and IRInterpreter source files

**Files:**
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Delete: `include/IRInterpreter.hpp`
- Delete: `src/IRInterpreter.cpp`
- Test: `tests/cli_multi_source_tests.py`, build

- [ ] **Step 1: Remove IRInterpreter include and run flag from main**

In `src/main.cpp`, delete:

```cpp
#include "IRInterpreter.hpp"
```

Change usage text to remove `[--run]`:

```cpp
    std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [file ...]\n"
```

Delete this variable:

```cpp
    bool runIr = false;
```

- [ ] **Step 2: Explicitly reject `--run` during argument parsing**

Replace the existing `--run` parser branch:

```cpp
        } else if (arg == "--run") {
            runIr = true;
```

with:

```cpp
        } else if (arg == "--run") {
            printUsage(argv[0]);
            return 64;
```

- [ ] **Step 3: Remove run mode from mode validation and IR compilation conditions**

In `src/main.cpp`, update the `emitBytecodePath` validation condition from:

```cpp
        if (inputPaths.empty() || showTokens || showIr || showBytecode || runIr) {
```

to:

```cpp
        if (inputPaths.empty() || showTokens || showIr || showBytecode) {
```

Update default AST mode from:

```cpp
        if (!emitBytecodePath && !showIr && !showBytecode && !runIr) {
```

to:

```cpp
        if (!emitBytecodePath && !showIr && !showBytecode) {
```

Update IR compilation condition from:

```cpp
        if (emitBytecodePath || showIr || showBytecode || runIr) {
```

to:

```cpp
        if (emitBytecodePath || showIr || showBytecode) {
```

- [ ] **Step 4: Remove the IRInterpreter execution block**

Delete this block from `src/main.cpp`:

```cpp
            if (runIr) {
                separateSection();
                IRInterpreter interpreter(std::cout);
                interpreter.execute(ir);
            }
```

- [ ] **Step 5: Remove IRInterpreter from CMake and delete files**

In `CMakeLists.txt`, remove this source from `compiler_design`:

```cmake
    src/IRInterpreter.cpp
```

Delete the source and header:

```sh
git rm include/IRInterpreter.hpp src/IRInterpreter.cpp
```

- [ ] **Step 6: Run focused build and CLI tests**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
```

Expected: PASS.

- [ ] **Step 7: Check no active source references remain**

Run:

```sh
grep -R -n -- "IRInterpreter\|--run\|IR interpreter" CMakeLists.txt include src tests/run_golden_tests.py tests/run_rust_vm_tests.py tests/cli_multi_source_tests.py tests/run_golden_tests_selftest.py README.md docs/roadmap.md || true
```

Expected: no matches in active source/tests except README/roadmap docs that will be updated in the next task. If test names still contain `test_run_...` but no `--run` CLI string, rename them for clarity.

- [ ] **Step 8: Commit CLI and source removal**

Run:

```sh
git add CMakeLists.txt src/main.cpp tests/cli_multi_source_tests.py
git add -u include/IRInterpreter.hpp src/IRInterpreter.cpp
git commit -m "refactor: remove C++ IR interpreter"
```

Expected: commit succeeds.

---

### Task 7: Update documentation and agent memory

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify if present: `AGENTS.md`
- Test: grep checks

- [ ] **Step 1: Update README architecture overview**

In `README.md`, replace references to the C++ IR interpreter with Rust VM execution. The opening overview should say the project includes a lexer/parser/type checker, IR and bytecode emission, `.cdbc` artifact emission, and the standalone Rust VM execution backend.

Replace the component bullet:

```markdown
- IR interpreter: executes that virtual-register IR directly.
```

with:

```markdown
- Rust VM: executes emitted `.cdbc` bytecode artifacts via `compiler-design-vm`.
```

- [ ] **Step 2: Update README CLI examples**

In `README.md`, remove examples using:

```sh
./build/compiler_design --run examples/hello.cd
./build/compiler_design --run lib.cd main.cd
```

Ensure the execution example uses:

```sh
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

Update the paragraph that currently says `--run` executes the C++ IR interpreter so it instead says:

```markdown
`--bytecode` remains a debug-print mode for inspecting compiler output. Program execution is handled by the Rust VM via `.cdbc` artifacts:
```

- [ ] **Step 3: Update roadmap active text**

In `docs/roadmap.md`, update the implemented baseline bullet from C++ `--run` execution to:

```markdown
- Bytecode artifact emission and Rust VM execution via `.cdbc`.
```

Update guiding principles to remove `--run` alignment and say:

```markdown
- Keep bytecode lowering and Rust VM execution aligned for every supported user-visible feature covered by execution tests.
```

Replace future touch-point text that mentions `IR interpreter, bytecode lowering, and Rust VM behavior` with:

```markdown
- bytecode lowering and Rust VM behavior
```

Replace builtin guidance about separate C++ IR-interpreter and Rust VM implementations with:

```markdown
Each builtin should define behavior for bytecode lowering and Rust VM execution, with focused VM coverage for runtime behavior.
```

- [ ] **Step 4: Update AGENTS.md if it exists as a repository file**

Run:

```sh
if [ -f AGENTS.md ]; then grep -n "IR interpreter\|--run\|IRInterpreter" AGENTS.md || true; fi
```

If `AGENTS.md` exists, update active architecture and workflow text so it no longer says `--run` uses the C++ IR interpreter. Keep the full verification command list unchanged except that its meaning changes: `run_golden_tests.py` is compiler/frontend-only and `run_rust_vm_tests.py` owns execution.

- [ ] **Step 5: Check active docs/tests/source no longer advertise removed run mode**

Run:

```sh
grep -R -n -- "--run\|C++ IR interpreter\|IRInterpreter\|IR interpreter" README.md docs/roadmap.md CMakeLists.txt include src tests/run_golden_tests.py tests/run_rust_vm_tests.py tests/cli_multi_source_tests.py tests/run_golden_tests_selftest.py AGENTS.md 2>/dev/null || true
```

Expected: no active references to `--run`, `IRInterpreter`, or C++ IR interpreter remain, except historical `docs/superpowers/` files which this command intentionally does not search.

- [ ] **Step 6: Commit documentation updates**

Run:

```sh
git add README.md docs/roadmap.md AGENTS.md 2>/dev/null || git add README.md docs/roadmap.md
git commit -m "docs: document Rust VM execution path"
```

Expected: commit succeeds.

---

### Task 8: Full verification and cleanup

**Files:**
- Verify: whole repository
- Clean: `tests/__pycache__/`

- [ ] **Step 1: Run the full verification suite**

Run:

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

Expected: every command exits 0. CTest includes the migrated CLI multi-source tests, compiler golden tests, Rust VM execution tests, frontend session tests, and flow facts tests.

- [ ] **Step 2: Confirm no generated or accidental files remain**

Run:

```sh
git status --short
```

Expected: no output. If source/doc changes remain, inspect and commit them. If golden runtime files changed, treat them as regressions unless they were explicitly required by the plan.

- [ ] **Step 3: Report final verification evidence**

In the final response, list each verification command and the observed pass counts, including the new golden runner and Rust VM runner counts. Do not claim completion without this fresh evidence.

---

## Self-Review Notes

- Spec coverage: Tasks 1-2 migrate the golden runner; Tasks 3-4 make Rust VM execution own success and runtime-error checks; Tasks 5-6 remove `--run` and `IRInterpreter`; Task 7 updates documentation; Task 8 verifies the full suite.
- Red-flag scan: every task names exact files, commands, and expected outcomes; no deferred work remains.
- Type/signature consistency: runner helper names are consistent (`emit_bytecode`, `check_runtime_error_case`, `discover_runtime_error_cases`, `emit_and_run_vm`). CLI behavior is explicit: `--run` prints usage and exits 64.
