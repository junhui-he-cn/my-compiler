# CLI Multi-Source Compilation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow `compiler_design` to accept multiple input files on the CLI, concatenate them in argument order, and compile the combined source as one program.

**Architecture:** Add a focused C++ `SourceManager` that reads stdin or a vector of file paths and returns a combined source string while keeping lightweight file metadata for future imports/file-aware diagnostics. Update `main.cpp` to collect multiple input paths and use `SourceManager`; update Python test runners to support fixture-local `args.txt` so golden and bytecode/Rust VM tests can cover multi-file inputs.

**Tech Stack:** C++17 compiler CLI, Python golden/integration runners, CMake/CTest, `.cdbc` bytecode artifact tests, Rust VM integration tests.

---

## File Structure

- Create `include/SourceManager.hpp`: `SourceFile` metadata and `SourceManager` API.
- Create `src/SourceManager.cpp`: stdin/file reading and source concatenation.
- Modify `CMakeLists.txt`: compile `src/SourceManager.cpp`; add a CLI integration CTest.
- Modify `src/main.cpp`: replace single `inputPath` with `std::vector<std::string> inputPaths`, update usage, and load source through `SourceManager`.
- Create `tests/cli_multi_source_tests.py`: direct CLI integration tests for multi-file run, stdin preservation, missing files, and `--emit-bytecode` multi-file behavior.
- Modify `tests/run_golden_tests.py`: support success fixtures with fixture-local `args.txt`.
- Modify `tests/run_golden_tests_selftest.py`: cover `args.txt` discovery/command construction.
- Create `tests/golden/multi_file_functions/`: multi-file success fixture with AST, IR, bytecode, and run outputs.
- Modify `tests/bytecode_artifact_tests.py`: support artifact fixtures with fixture-local `args.txt`.
- Modify `tests/run_rust_vm_tests.py`: support artifact and golden fixtures with fixture-local `args.txt`; allowlist the multi-file golden fixture.
- Create `tests/bytecode_artifacts/multi_file_functions/`: multi-file `.cdbc` artifact fixture.
- Modify `README.md`, `docs/roadmap.md`, `AGENTS.md`: document multi-file CLI behavior and current diagnostic limitation.

---

### Task 1: RED CLI integration tests for multi-file inputs

**Files:**
- Create: `tests/cli_multi_source_tests.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add CLI integration test file**

Create `tests/cli_multi_source_tests.py`:

```python
#!/usr/bin/env python3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class CliMultiSourceTests(unittest.TestCase):
    def setUp(self) -> None:
        if len(sys.argv) != 2:
            self.fail("usage: cli_multi_source_tests.py <compiler>")
        self.compiler = Path(sys.argv[1]).resolve()
        if not self.compiler.is_file():
            self.fail(f"compiler not found: {self.compiler}")

    def run_compiler(self, *args: str, input_text: str | None = None) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.compiler), *args],
            input=input_text,
            text=True,
            capture_output=True,
            check=False,
        )

    def test_run_accepts_multiple_input_files_in_order(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            lib = root / "lib.cd"
            main = root / "main.cd"
            lib.write_text("fun add(a, b) { return a + b; }\n", encoding="utf-8")
            main.write_text("print add(1, 2);\n", encoding="utf-8")

            completed = self.run_compiler("--run", str(lib), str(main))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stderr, "")
            self.assertEqual(completed.stdout, "3\n")

    def test_emit_bytecode_accepts_multiple_input_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            lib = root / "lib.cd"
            main = root / "main.cd"
            artifact = root / "program.cdbc"
            lib.write_text("fun add(a, b) { return a + b; }\n", encoding="utf-8")
            main.write_text("print add(2, 3);\n", encoding="utf-8")

            completed = self.run_compiler("--emit-bytecode", str(artifact), str(lib), str(main))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, "")
            self.assertTrue(artifact.is_file())
            self.assertIn("add", artifact.read_text(encoding="utf-8"))

    def test_missing_second_input_file_reports_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            existing = root / "existing.cd"
            missing = root / "missing.cd"
            existing.write_text("print 1;\n", encoding="utf-8")

            completed = self.run_compiler("--run", str(existing), str(missing))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, f"failed to open input file: {missing}\n")

    def test_stdin_still_works_when_no_input_files_are_given(self) -> None:
        completed = self.run_compiler("--run", input_text="print 7;\n")

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stderr, "")
        self.assertEqual(completed.stdout, "7\n")

    def test_emit_bytecode_still_requires_at_least_one_input_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            artifact = Path(temp_dir) / "program.cdbc"

            completed = self.run_compiler("--emit-bytecode", str(artifact), input_text="print 1;\n")

            self.assertEqual(completed.returncode, 64)
            self.assertEqual(completed.stdout, "")
            self.assertFalse(artifact.exists())


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
```

- [ ] **Step 2: Add CTest entry**

In `CMakeLists.txt`, add after the existing `golden_runner_selftest` test:

```cmake
add_test(
    NAME cli_multi_source
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cli_multi_source_tests.py $<TARGET_FILE:compiler_design>
)
```

- [ ] **Step 3: Verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -R cli_multi_source
```

Expected: `cli_multi_source` fails because the current CLI accepts only one input path and prints usage when a second input path is present. The stdin test may pass.

- [ ] **Step 4: Commit RED CLI tests**

```bash
git add CMakeLists.txt tests/cli_multi_source_tests.py
git commit -m "test: add cli multi source integration tests"
```

---

### Task 2: Add SourceManager and multi-file CLI loading

**Files:**
- Create: `include/SourceManager.hpp`
- Create: `src/SourceManager.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`

- [ ] **Step 1: Add SourceManager header**

Create `include/SourceManager.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

struct SourceFile {
    std::string path;
    std::string source;
    std::size_t startLine = 1;
};

class SourceManager {
public:
    std::string loadStdin(std::istream& input);
    std::string loadFiles(const std::vector<std::string>& paths);
    const std::vector<SourceFile>& files() const;

private:
    std::vector<SourceFile> files_;
};
```

- [ ] **Step 2: Add SourceManager implementation**

Create `src/SourceManager.cpp`:

```cpp
#include "SourceManager.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string readAll(std::istream& in)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::size_t lineCount(const std::string& source)
{
    std::size_t lines = 1;
    for (char ch : source) {
        if (ch == '\n') {
            ++lines;
        }
    }
    return lines;
}

} // namespace

std::string SourceManager::loadStdin(std::istream& input)
{
    files_.clear();
    return readAll(input);
}

std::string SourceManager::loadFiles(const std::vector<std::string>& paths)
{
    files_.clear();
    std::string combined;
    std::size_t nextStartLine = 1;

    for (std::size_t index = 0; index < paths.size(); ++index) {
        const std::string& path = paths[index];
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("failed to open input file: " + path);
        }

        std::string source = readAll(file);
        if (!combined.empty() && combined.back() != '\n') {
            combined.push_back('\n');
            ++nextStartLine;
        }

        files_.push_back(SourceFile{path, source, nextStartLine});
        combined += source;
        nextStartLine += lineCount(source) - 1;
    }

    return combined;
}

const std::vector<SourceFile>& SourceManager::files() const
{
    return files_;
}
```

- [ ] **Step 3: Add SourceManager to build**

In `CMakeLists.txt`, add `src/SourceManager.cpp` to `add_executable` after `src/Parser.cpp`:

```cmake
    src/SourceManager.cpp
```

- [ ] **Step 4: Include SourceManager in main**

In `src/main.cpp`, add:

```cpp
#include "SourceManager.hpp"
```

Remove the local `readAll` and `readFile` helper functions from the anonymous namespace. Keep `printUsage`.

- [ ] **Step 5: Update usage text**

Replace `printUsage` in `src/main.cpp` with:

```cpp
void printUsage(const char* executable)
{
    std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [--run] [file ...]\n"
              << "       " << executable << " --emit-bytecode output.cdbc file [...]\n"
              << "If no file is provided, source is read from stdin except for --emit-bytecode, which requires at least one file.\n";
}
```

- [ ] **Step 6: Store multiple input paths**

In `src/main.cpp`, replace:

```cpp
    std::string inputPath;
```

with:

```cpp
    std::vector<std::string> inputPaths;
```

Add `#include <vector>` with the other standard includes if it is not already present.

- [ ] **Step 7: Parse all non-option arguments as input files**

In the argument parsing loop in `src/main.cpp`, replace the single-input branch:

```cpp
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else {
            printUsage(argv[0]);
            return 64;
        }
```

with:

```cpp
        } else {
            inputPaths.push_back(arg);
        }
```

- [ ] **Step 8: Preserve emit-bytecode input requirement**

In `src/main.cpp`, replace:

```cpp
    if (emitBytecodePath) {
        if (inputPath.empty() || showTokens || showIr || showBytecode || runIr) {
            printUsage(argv[0]);
            return 64;
        }
    }
```

with:

```cpp
    if (emitBytecodePath) {
        if (inputPaths.empty() || showTokens || showIr || showBytecode || runIr) {
            printUsage(argv[0]);
            return 64;
        }
    }
```

- [ ] **Step 9: Load source through SourceManager**

In `src/main.cpp`, replace:

```cpp
        const std::string source = inputPath.empty() ? readAll(std::cin) : readFile(inputPath);
```

with:

```cpp
        SourceManager sourceManager;
        const std::string source = inputPaths.empty()
            ? sourceManager.loadStdin(std::cin)
            : sourceManager.loadFiles(inputPaths);
```

- [ ] **Step 10: Build and verify CLI tests pass**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -R cli_multi_source
```

Expected: `cli_multi_source` passes.

- [ ] **Step 11: Run existing smoke checks**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: all existing golden and bytecode artifact tests pass.

- [ ] **Step 12: Commit SourceManager and CLI support**

```bash
git add CMakeLists.txt include/SourceManager.hpp src/SourceManager.cpp src/main.cpp
git commit -m "feat: support cli multi source inputs"
```

---

### Task 3: Add args.txt support to golden runner and success fixtures

**Files:**
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`
- Create: `tests/golden/multi_file_functions/lib.cd`
- Create: `tests/golden/multi_file_functions/main.cd`
- Create: `tests/golden/multi_file_functions/args.txt`
- Create: `tests/golden/multi_file_functions/ast.out`
- Create: `tests/golden/multi_file_functions/run.out`

- [ ] **Step 1: Add failing selftest for args.txt support**

In `tests/run_golden_tests_selftest.py`, add this test method to `GoldenRunnerTests`:

```python
    def test_success_case_args_txt_replaces_default_input_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            case_dir = root / "multi_file"
            case_dir.mkdir()
            (case_dir / "lib.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "main.cd").write_text("print 2;\n", encoding="utf-8")
            (case_dir / "args.txt").write_text("lib.cd main.cd\n", encoding="utf-8")
            (case_dir / "run.out").write_text("1\n2\n", encoding="utf-8")

            commands: list[list[str]] = []

            def fake_run(command, text, capture_output, check):
                commands.append([str(part) for part in command])
                return subprocess.CompletedProcess(command, 0, "1\n2\n", "")

            with unittest.mock.patch.object(run_golden_tests.subprocess, "run", side_effect=fake_run):
                results = run_golden_tests.check_success_case(Path("compiler"), case_dir, False)

            self.assertTrue(all(result.passed for result in results), results)
            self.assertIn(["compiler", "--run", str(case_dir / "lib.cd"), str(case_dir / "main.cd")], commands)
```

If the file does not already import `subprocess` and `unittest.mock`, add:

```python
import subprocess
import unittest.mock
```

- [ ] **Step 2: Verify RED selftest**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: the new selftest fails because `check_success_case` still requires `input.cd` and does not read `args.txt`.

- [ ] **Step 3: Add golden runner helpers**

In `tests/run_golden_tests.py`, add after `write_text`:

```python
def compiler_inputs(case_dir: Path) -> list[Path]:
    args_path = case_dir / "args.txt"
    if args_path.is_file():
        entries = read_text(args_path).split()
        return [case_dir / entry for entry in entries]
    return [case_dir / "input.cd"]
```

- [ ] **Step 4: Use fixture inputs in compiler command**

In `tests/run_golden_tests.py`, replace `run_compiler` with:

```python
def run_compiler(compiler: Path, args: tuple[str, ...], sources: list[Path]) -> subprocess.CompletedProcess[str]:
    command = [str(compiler), *args, *(str(source) for source in sources)]
    return subprocess.run(command, text=True, capture_output=True, check=False)
```

- [ ] **Step 5: Discover args.txt success cases**

In `tests/run_golden_tests.py`, replace the final success-case discovery condition:

```python
        and (case_dir / "input.cd").is_file()
```

with:

```python
        and ((case_dir / "input.cd").is_file() or (case_dir / "args.txt").is_file())
```

- [ ] **Step 6: Pass source list from check_success_case**

In `tests/run_golden_tests.py`, replace this line in `check_success_case`:

```python
    source = case_dir / "input.cd"
```

with:

```python
    sources = compiler_inputs(case_dir)
```

Then replace:

```python
        completed = run_compiler(compiler, args, source)
```

with:

```python
        completed = run_compiler(compiler, args, sources)
```

Do not change runtime-error, parse-error, or type-error fixtures in this task; those still use one `.cd` file each.

- [ ] **Step 7: Verify selftest passes**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: all selftests pass.

- [ ] **Step 8: Add multi-file golden fixture inputs**

Create `tests/golden/multi_file_functions/lib.cd`:

```cd
fun add(a, b) { return a + b; }
let banner = "sum";
```

Create `tests/golden/multi_file_functions/main.cd`:

```cd
print banner;
print add(1, 2);
```

Create `tests/golden/multi_file_functions/args.txt`:

```text
lib.cd main.cd
```

Create `tests/golden/multi_file_functions/ast.out`:

```text
Program
  Fun add(a, b)
    Return (+ a b)
  Let banner = "sum"
  Print banner
  Print (call add 1 2)
```

Create `tests/golden/multi_file_functions/run.out`:

```text
sum
3
```

- [ ] **Step 9: Verify golden RED/GREEN state**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all tests pass if Task 2 is complete. If `ast.out` differs due to exact function AST formatting, regenerate only this fixture's AST with:

```bash
./build/compiler_design tests/golden/multi_file_functions/lib.cd tests/golden/multi_file_functions/main.cd > tests/golden/multi_file_functions/ast.out
python3 tests/run_golden_tests.py ./build/compiler_design
```

- [ ] **Step 10: Generate IR and bytecode goldens**

Run:

```bash
./build/compiler_design --ir tests/golden/multi_file_functions/lib.cd tests/golden/multi_file_functions/main.cd > tests/golden/multi_file_functions/ir.out
./build/compiler_design --bytecode tests/golden/multi_file_functions/lib.cd tests/golden/multi_file_functions/main.cd > tests/golden/multi_file_functions/bytecode.out
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass, including `multi_file_functions` AST, IR, bytecode, and run checks.

- [ ] **Step 11: Commit golden runner support**

```bash
git add tests/run_golden_tests.py tests/run_golden_tests_selftest.py tests/golden/multi_file_functions
git commit -m "test: support multi file golden fixtures"
```

---

### Task 4: Add args.txt support to bytecode and Rust VM test runners

**Files:**
- Modify: `tests/bytecode_artifact_tests.py`
- Modify: `tests/run_rust_vm_tests.py`
- Create: `tests/bytecode_artifacts/multi_file_functions/lib.cd`
- Create: `tests/bytecode_artifacts/multi_file_functions/main.cd`
- Create: `tests/bytecode_artifacts/multi_file_functions/args.txt`
- Create: `tests/bytecode_artifacts/multi_file_functions/run.out`
- Create/refresh: `tests/bytecode_artifacts/multi_file_functions/expected.cdbc`

- [ ] **Step 1: Add shared fixture input helper to bytecode artifact tests**

In `tests/bytecode_artifact_tests.py`, add after `read_text`:

```python
def compiler_inputs(case_dir: Path) -> list[Path]:
    args_path = case_dir / "args.txt"
    if args_path.is_file():
        entries = read_text(args_path).split()
        return [case_dir / entry for entry in entries]
    return [case_dir / "input.cd"]
```

- [ ] **Step 2: Discover artifact cases with args.txt**

In `tests/bytecode_artifact_tests.py`, replace the discover condition:

```python
        if path.is_dir() and (path / "input.cd").is_file() and (path / "expected.cdbc").is_file()
```

with:

```python
        if path.is_dir()
        and ((path / "input.cd").is_file() or (path / "args.txt").is_file())
        and (path / "expected.cdbc").is_file()
```

- [ ] **Step 3: Compile artifact cases with multiple inputs**

In `tests/bytecode_artifact_tests.py`, replace:

```python
    source = case_dir / "input.cd"
```

with:

```python
    sources = compiler_inputs(case_dir)
```

Then replace:

```python
        compile_command = [str(compiler), "--emit-bytecode", str(actual_path), str(source)]
```

with:

```python
        compile_command = [str(compiler), "--emit-bytecode", str(actual_path), *(str(source) for source in sources)]
```

- [ ] **Step 4: Add same helper to Rust VM tests**

In `tests/run_rust_vm_tests.py`, add after `read_text`:

```python
def compiler_inputs(case_dir: Path) -> list[Path]:
    args_path = case_dir / "args.txt"
    if args_path.is_file():
        entries = read_text(args_path).split()
        return [case_dir / entry for entry in entries]
    return [case_dir / "input.cd"]
```

- [ ] **Step 5: Discover Rust artifact and golden cases with args.txt**

In `tests/run_rust_vm_tests.py`, update `discover_artifact_cases` and `discover_golden_cases` conditions so each accepts either `input.cd` or `args.txt`:

```python
        if path.is_dir()
        and ((path / "input.cd").is_file() or (path / "args.txt").is_file())
        and (path / "run.out").is_file()
```

- [ ] **Step 6: Compile Rust VM cases with multiple inputs**

In `tests/run_rust_vm_tests.py`, replace:

```python
    source = case_dir / "input.cd"
```

with:

```python
    sources = compiler_inputs(case_dir)
```

Then replace:

```python
        compile_command = [str(compiler), "--emit-bytecode", str(artifact), str(source)]
```

with:

```python
        compile_command = [str(compiler), "--emit-bytecode", str(artifact), *(str(source) for source in sources)]
```

- [ ] **Step 7: Add golden allowlist entry**

In `tests/run_rust_vm_tests.py`, add this to `golden_allowlist` near other language success cases:

```python
            "multi_file_functions",
```

- [ ] **Step 8: Add multi-file bytecode artifact fixture**

Create `tests/bytecode_artifacts/multi_file_functions/lib.cd`:

```cd
fun add(a, b) { return a + b; }
```

Create `tests/bytecode_artifacts/multi_file_functions/main.cd`:

```cd
print add(2, 5);
```

Create `tests/bytecode_artifacts/multi_file_functions/args.txt`:

```text
lib.cd main.cd
```

Create `tests/bytecode_artifacts/multi_file_functions/run.out`:

```text
7
```

- [ ] **Step 9: Generate expected `.cdbc`**

Run:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/multi_file_functions/expected.cdbc tests/bytecode_artifacts/multi_file_functions/lib.cd tests/bytecode_artifacts/multi_file_functions/main.cd
```

Expected: `expected.cdbc` is created and contains a function named `add`.

- [ ] **Step 10: Verify bytecode and Rust VM multi-file paths**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case multi_file_functions
```

Expected: both commands pass.

- [ ] **Step 11: Commit artifact/Rust runner support**

```bash
git add tests/bytecode_artifact_tests.py tests/run_rust_vm_tests.py tests/bytecode_artifacts/multi_file_functions
git commit -m "test: cover multi file bytecode and rust vm paths"
```

---

### Task 5: Documentation updates

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README CLI section**

In `README.md`, update the CLI examples near the build/test section to include:

```markdown
Multiple input files may be provided. They are read in command-line order and compiled as one combined program:

```sh
./build/compiler_design --run lib.cd main.cd
./build/compiler_design --emit-bytecode program.cdbc lib.cd main.cd
```

If no file is provided, source is read from stdin. Diagnostics currently report line and column in the combined source rather than original file names.
```

- [ ] **Step 2: Update roadmap Phase 14**

In `docs/roadmap.md`, change Phase 14 status to mention Phase 14A:

```markdown
Status: in progress. Phase 14A is implemented: the CLI accepts multiple input files and compiles them as one combined source in command-line order. File-level `import` syntax, dependency graph loading, and file-aware diagnostics remain future work.
```

Also mark the CLI source-management bullet as implemented:

```markdown
- CLI behavior for multi-file source loading. Implemented for direct CLI inputs.
```

- [ ] **Step 3: Update AGENTS current semantics**

In `AGENTS.md`, add a bullet near the backend/CLI notes:

```markdown
- The CLI accepts multiple input files for normal modes and `--emit-bytecode`; files are read in command-line order and compiled as one combined source. If no input file is provided, source is read from stdin except `--emit-bytecode`, which requires at least one file. Diagnostics still report combined-source line/column rather than original file names.
```

- [ ] **Step 4: Run docs grep check**

Run:

```bash
rg -n "multiple input|multi-file|combined source|Phase 14|import|stdin|emit-bytecode" README.md docs/roadmap.md AGENTS.md
```

Expected: docs describe direct CLI multi-file loading as implemented and keep `import` syntax as future work.

- [ ] **Step 5: Commit docs**

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document cli multi source compilation"
```

---

### Task 6: Full verification and cleanup

**Files:**
- No source edits expected.
- Remove: `tests/__pycache__/` if created.

- [ ] **Step 1: Run full verification**

Run from repository root:

```bash
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

Expected: every command exits 0.

- [ ] **Step 2: Inspect workspace state**

Run:

```bash
git status --short
git log --oneline -10
```

Expected: worktree is clean; recent commits cover spec, plan, RED CLI tests, SourceManager implementation, golden runner support, bytecode/Rust VM coverage, and docs.

- [ ] **Step 3: Final report**

Report exact command results:

```text
Implemented CLI multi-source compilation.
Verification:
- cmake -S . -B build: PASS
- cmake --build build: PASS
- ctest --test-dir build --output-on-failure: PASS
- python3 tests/run_golden_tests.py ./build/compiler_design: PASS
- python3 tests/run_golden_tests_selftest.py: PASS
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: PASS
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: PASS
- cargo test --manifest-path vm-rs/Cargo.toml: PASS
```

---

## Self-Review

- Spec coverage: The plan covers multi-file CLI behavior, stdin preservation, `--emit-bytecode` input requirements, SourceManager metadata, combined-source diagnostics limitation, golden fixtures with `args.txt`, bytecode artifact/Rust VM parity, documentation, and full verification. It keeps import syntax and file-aware diagnostics out of scope.
- Placeholder scan: The plan uses concrete file paths, code snippets, commands, expected outputs, and commit messages. It does not rely on deferred implementation notes.
- Type consistency: The planned C++ names are consistent across tasks: `SourceFile`, `SourceManager`, `loadStdin`, `loadFiles`, and `files`. Python helper naming is consistently `compiler_inputs(case_dir)` in all runners.
