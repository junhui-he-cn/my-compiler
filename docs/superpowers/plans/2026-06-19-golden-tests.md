# Golden CLI Tests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single Bash smoke test with an extensible CLI golden test system for AST, IR, runtime output, and runtime error behavior.

**Architecture:** CTest will run one Python golden test runner. The runner discovers fixture directories under `tests/golden`, executes `compiler_demo` in default, `--ir`, and `--run` modes, and compares exact output against golden files with unified diffs. Runtime error fixtures compare `--run` stderr and exit code.

**Tech Stack:** C++17 project built with CMake/CTest, Python 3 standard library for the golden runner, existing `compiler_demo` CLI.

---

## File Structure

- Create `tests/run_golden_tests.py`: Python runner that discovers golden fixtures, supports `--update`, compares output exactly, and prints diffs.
- Create `tests/golden/<case>/input.cd`: successful language fixtures.
- Create `tests/golden/<case>/{ast.out,ir.out,run.out}`: expected outputs for successful fixtures.
- Create `tests/golden/runtime_errors/*.cd`: runtime error fixtures.
- Create `tests/golden/runtime_errors/*.{run.err,exit}`: expected stderr and exit code for runtime error fixtures.
- Modify `CMakeLists.txt`: replace `smoke_register_ir` test with `golden` test.
- Modify `README.md`: document test commands, golden fixture layout, and update mode.
- Delete `tests/smoke_register_ir.sh`: remove old smoke script after coverage moves to golden cases.

## Task 1: Add Golden Runner and One Initial Fixture

**Files:**
- Create: `tests/run_golden_tests.py`
- Create: `tests/golden/literals/input.cd`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the first golden input fixture**

Run:

```bash
mkdir -p tests/golden/literals
cat > tests/golden/literals/input.cd <<'CASE'
print nil;
print true;
print false;
print 123;
print "hello";
CASE
```

Expected: `tests/golden/literals/input.cd` exists with the five literal print statements.

- [ ] **Step 2: Register the golden test before the runner exists**

Replace `CMakeLists.txt` with this exact content:

```cmake
cmake_minimum_required(VERSION 3.16)

project(compiler_demo LANGUAGES CXX)

# Keep the demo portable by requiring only standard C++17 facilities.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so editors and language servers can index the project.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(compiler_demo
    src/Ast.cpp
    src/IR.cpp
    src/IRCompiler.cpp
    src/IRInterpreter.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Value.cpp
    src/main.cpp
)

target_include_directories(compiler_demo PRIVATE include)

enable_testing()
add_test(
    NAME golden
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/run_golden_tests.py $<TARGET_FILE:compiler_demo>
)
```

- [ ] **Step 3: Run CTest to verify the new test entry fails**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: CTest fails because `tests/run_golden_tests.py` does not exist yet.

- [ ] **Step 4: Create the golden test runner**

Create `tests/run_golden_tests.py` with this exact content:

```python
#!/usr/bin/env python3

import argparse
import difflib
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CheckResult:
    name: str
    passed: bool
    message: str = ""


SUCCESS_MODES = (
    ("ast", (), "ast.out"),
    ("ir", ("--ir",), "ir.out"),
    ("run", ("--run",), "run.out"),
)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def unified_diff(expected: str, actual: str, fromfile: str, tofile: str) -> str:
    return "".join(
        difflib.unified_diff(
            expected.splitlines(keepends=True),
            actual.splitlines(keepends=True),
            fromfile=fromfile,
            tofile=tofile,
        )
    )


def run_compiler(compiler: Path, args: tuple[str, ...], source: Path) -> subprocess.CompletedProcess[str]:
    command = [str(compiler), *args, str(source)]
    return subprocess.run(command, text=True, capture_output=True, check=False)


def discover_success_cases(golden_dir: Path) -> list[Path]:
    return sorted(
        case_dir
        for case_dir in golden_dir.iterdir()
        if case_dir.is_dir()
        and case_dir.name != "runtime_errors"
        and (case_dir / "input.cd").is_file()
    )


def discover_runtime_error_cases(golden_dir: Path) -> list[Path]:
    runtime_dir = golden_dir / "runtime_errors"
    if not runtime_dir.is_dir():
        return []
    return sorted(runtime_dir.glob("*.cd"))


def check_success_case(compiler: Path, case_dir: Path, update: bool) -> list[CheckResult]:
    source = case_dir / "input.cd"
    results: list[CheckResult] = []

    for mode_name, args, golden_name in SUCCESS_MODES:
        golden_path = case_dir / golden_name
        if not update and not golden_path.exists():
            continue

        completed = run_compiler(compiler, args, source)
        check_name = f"{case_dir.name} --{mode_name}"

        if completed.returncode != 0:
            results.append(
                CheckResult(
                    check_name,
                    False,
                    f"FAIL {check_name} exited with {completed.returncode}\n\nSTDOUT:\n{completed.stdout}\nSTDERR:\n{completed.stderr}",
                )
            )
            continue

        if update:
            write_text(golden_path, completed.stdout)
            results.append(CheckResult(check_name, True))
            continue

        expected = read_text(golden_path)
        actual = completed.stdout
        if actual != expected:
            diff = unified_diff(expected, actual, "expected", "actual")
            results.append(CheckResult(check_name, False, f"FAIL {check_name} stdout mismatch\n\n{diff}"))
        else:
            results.append(CheckResult(check_name, True))

    return results


def check_runtime_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    stem = source.with_suffix("")
    err_path = stem.with_suffix(".run.err")
    exit_path = stem.with_suffix(".exit")
    case_name = f"runtime_errors/{source.stem} --run"

    completed = run_compiler(compiler, ("--run",), source)

    if update:
        write_text(err_path, completed.stderr)
        write_text(exit_path, f"{completed.returncode}\n")
        return [CheckResult(case_name, True)]

    results: list[CheckResult] = []

    if not err_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected stderr file: {err_path}"))
    else:
        expected_err = read_text(err_path)
        actual_err = completed.stderr
        if actual_err != expected_err:
            diff = unified_diff(expected_err, actual_err, "expected stderr", "actual stderr")
            results.append(CheckResult(case_name, False, f"FAIL {case_name} stderr mismatch\n\n{diff}"))

    if not exit_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected exit file: {exit_path}"))
    else:
        expected_exit_text = read_text(exit_path).strip()
        actual_exit_text = str(completed.returncode)
        if actual_exit_text != expected_exit_text:
            results.append(
                CheckResult(
                    case_name,
                    False,
                    f"FAIL {case_name} exit code mismatch\nexpected: {expected_exit_text}\nactual: {actual_exit_text}",
                )
            )

    if not results:
        results.append(CheckResult(case_name, True))

    return results


def run_all(compiler: Path, golden_dir: Path, update: bool) -> list[CheckResult]:
    results: list[CheckResult] = []

    for case_dir in discover_success_cases(golden_dir):
        results.extend(check_success_case(compiler, case_dir, update))

    for source in discover_runtime_error_cases(golden_dir):
        results.extend(check_runtime_error_case(compiler, source, update))

    return results


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run compiler CLI golden tests.")
    parser.add_argument("compiler", type=Path, help="Path to compiler_demo executable")
    parser.add_argument("--update", action="store_true", help="Rewrite golden files from current compiler output")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        print(f"compiler not found: {compiler}", file=sys.stderr)
        return 64

    tests_dir = Path(__file__).resolve().parent
    golden_dir = tests_dir / "golden"
    if not golden_dir.is_dir():
        print(f"golden directory not found: {golden_dir}", file=sys.stderr)
        return 64

    results = run_all(compiler, golden_dir, args.update)
    failed = [result for result in results if not result.passed]

    for failure in failed:
        print(failure.message, file=sys.stderr)

    passed_count = len(results) - len(failed)
    print(f"golden tests: {passed_count} passed, {len(failed)} failed")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 5: Make the runner executable**

Run:

```bash
chmod +x tests/run_golden_tests.py
```

Expected: command exits with status 0.

- [ ] **Step 6: Generate golden files for the literals fixture**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Expected output:

```text
golden tests: 3 passed, 0 failed
```

Expected files created:

```text
tests/golden/literals/ast.out
tests/golden/literals/ir.out
tests/golden/literals/run.out
```

- [ ] **Step 7: Run CTest and verify the first golden fixture passes**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `golden` passes and CTest reports `100% tests passed`.

- [ ] **Step 8: Commit the runner and first fixture**

Run:

```bash
git add CMakeLists.txt tests/run_golden_tests.py tests/golden/literals
git commit -m "test: add golden test runner"
```

Expected: commit succeeds.

## Task 2: Add Successful Language Golden Cases

**Files:**
- Create: `tests/golden/arithmetic/input.cd`
- Create: `tests/golden/comparison/input.cd`
- Create: `tests/golden/strings/input.cd`
- Create: `tests/golden/truthiness/input.cd`
- Create: `tests/golden/variables/input.cd`
- Create: `tests/golden/expression_statement/input.cd`
- Create: generated `ast.out`, `ir.out`, and `run.out` in each new case directory

- [ ] **Step 1: Create successful case inputs**

Run:

```bash
mkdir -p tests/golden/arithmetic tests/golden/comparison tests/golden/strings tests/golden/truthiness tests/golden/variables tests/golden/expression_statement

cat > tests/golden/arithmetic/input.cd <<'CASE'
print 1 + 2 * 3;
print (1 + 2) * 3;
print -5 + 10;
print 8 / 2;
CASE

cat > tests/golden/comparison/input.cd <<'CASE'
print 1 < 2;
print 2 <= 2;
print 3 > 4;
print 4 >= 4;
print 1 == 1;
print 1 != 2;
print "a" == "a";
print nil == nil;
CASE

cat > tests/golden/strings/input.cd <<'CASE'
print "a" + "b";
print "hello";
print "hello" == "hello";
print "hello" != "world";
CASE

cat > tests/golden/truthiness/input.cd <<'CASE'
print !nil;
print !false;
print !true;
print !0;
print !"";
CASE

cat > tests/golden/variables/input.cd <<'CASE'
let answer = 40 + 2;
let ok = answer == 42;
print answer;
print ok;
CASE

cat > tests/golden/expression_statement/input.cd <<'CASE'
let x = 10;
x + 1;
print x;
CASE
```

Expected: six new `input.cd` files exist.

- [ ] **Step 2: Generate golden outputs for successful cases**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Expected output:

```text
golden tests: 21 passed, 0 failed
```

Reason: seven successful cases run three checks each: AST, IR, and runtime output.

- [ ] **Step 3: Inspect representative generated outputs**

Run:

```bash
printf '%s\n' '--- arithmetic run.out ---'
cat tests/golden/arithmetic/run.out
printf '%s\n' '--- variables ir.out ---'
sed -n '1,80p' tests/golden/variables/ir.out
printf '%s\n' '--- truthiness ast.out ---'
sed -n '1,80p' tests/golden/truthiness/ast.out
```

Expected `tests/golden/arithmetic/run.out` content:

```text
7
9
5
4
```

Expected `tests/golden/variables/ir.out` starts with register-style IR and contains `store_var` and `load_var` instructions for `answer` and `ok`.

Expected `tests/golden/truthiness/ast.out` prints the parsed program tree and contains unary `!` expressions.

- [ ] **Step 4: Run all golden tests**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: `golden` passes and CTest reports `100% tests passed`.

- [ ] **Step 5: Commit successful golden cases**

Run:

```bash
git add tests/golden/arithmetic tests/golden/comparison tests/golden/strings tests/golden/truthiness tests/golden/variables tests/golden/expression_statement tests/golden/literals
git commit -m "test: add language golden cases"
```

Expected: commit succeeds.

## Task 3: Add Runtime Error Golden Cases

**Files:**
- Create: `tests/golden/runtime_errors/undefined_variable.cd`
- Create: `tests/golden/runtime_errors/undefined_variable.run.err`
- Create: `tests/golden/runtime_errors/undefined_variable.exit`
- Create: `tests/golden/runtime_errors/division_by_zero.cd`
- Create: `tests/golden/runtime_errors/division_by_zero.run.err`
- Create: `tests/golden/runtime_errors/division_by_zero.exit`
- Create: `tests/golden/runtime_errors/invalid_add.cd`
- Create: `tests/golden/runtime_errors/invalid_add.run.err`
- Create: `tests/golden/runtime_errors/invalid_add.exit`

- [ ] **Step 1: Create runtime error source fixtures**

Run:

```bash
mkdir -p tests/golden/runtime_errors

cat > tests/golden/runtime_errors/undefined_variable.cd <<'CASE'
print missing;
CASE

cat > tests/golden/runtime_errors/division_by_zero.cd <<'CASE'
print 1 / 0;
CASE

cat > tests/golden/runtime_errors/invalid_add.cd <<'CASE'
print "a" + 1;
CASE
```

Expected: three runtime error `.cd` files exist.

- [ ] **Step 2: Write expected runtime error files manually**

Run:

```bash
cat > tests/golden/runtime_errors/undefined_variable.run.err <<'EOF_ERR'
IR runtime error: undefined variable `missing`
EOF_ERR
cat > tests/golden/runtime_errors/undefined_variable.exit <<'EOF_EXIT'
1
EOF_EXIT

cat > tests/golden/runtime_errors/division_by_zero.run.err <<'EOF_ERR'
IR runtime error: division by zero
EOF_ERR
cat > tests/golden/runtime_errors/division_by_zero.exit <<'EOF_EXIT'
1
EOF_EXIT

cat > tests/golden/runtime_errors/invalid_add.run.err <<'EOF_ERR'
IR runtime error: add expects two numbers or two strings
EOF_ERR
cat > tests/golden/runtime_errors/invalid_add.exit <<'EOF_EXIT'
1
EOF_EXIT
```

Expected: each runtime error fixture has matching `.run.err` and `.exit` files.

- [ ] **Step 3: Run all golden tests with runtime errors included**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: `golden` passes and CTest reports `100% tests passed`.

- [ ] **Step 4: Verify the runner detects a bad runtime error golden file**

Run:

```bash
cp tests/golden/runtime_errors/division_by_zero.run.err /tmp/division_by_zero.run.err.bak
printf 'wrong error\n' > tests/golden/runtime_errors/division_by_zero.run.err
set +e
python3 tests/run_golden_tests.py ./build/compiler_demo
status=$?
set -e
mv /tmp/division_by_zero.run.err.bak tests/golden/runtime_errors/division_by_zero.run.err
test "${status}" -ne 0
```

Expected: the Python runner exits nonzero and prints a diff showing `wrong error` versus `IR runtime error: division by zero`. The final `mv` restores the correct golden file.

- [ ] **Step 5: Re-run all golden tests after restoring the file**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: `golden` passes and CTest reports `100% tests passed`.

- [ ] **Step 6: Commit runtime error golden cases**

Run:

```bash
git add tests/golden/runtime_errors
git commit -m "test: add runtime error golden cases"
```

Expected: commit succeeds.

## Task 4: Remove Old Smoke Test and Document Golden Workflow

**Files:**
- Delete: `tests/smoke_register_ir.sh`
- Modify: `README.md`

- [ ] **Step 1: Delete the old smoke test script**

Run:

```bash
git rm tests/smoke_register_ir.sh
```

Expected: `tests/smoke_register_ir.sh` is staged for deletion.

- [ ] **Step 2: Update README with golden test documentation**

Edit `README.md` so the full file is:

````markdown
# Compiler Demo

A small C++17 compiler front-end demo. It currently implements:

- Lexer: turns source text into tokens.
- Parser: builds a simple AST from tokens.
- IR compiler: lowers the AST to a small three-address intermediate representation with virtual registers.
- IR interpreter: executes that virtual-register IR directly.
- AST printer: prints the parsed program in prefix form.

## Language

Supported statements:

```text
let name = expression;
print expression;
expression;
```

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Variables: `name`
- Grouping: `(expression)`
- Unary operators: `!`, `-`
- Binary operators: `*`, `/`, `+`, `-`, `<`, `<=`, `>`, `>=`, `==`, `!=`

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, or `run.out` files to cover new syntax.

To refresh golden files after an intentional output change:

```sh
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

## Run

```sh
./build/compiler_demo examples/hello.cd
./build/compiler_demo --tokens examples/hello.cd
./build/compiler_demo --ir examples/hello.cd
./build/compiler_demo --run examples/hello.cd
```

If no file is provided, source is read from stdin.
````

- [ ] **Step 3: Run full verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: configure succeeds, build succeeds, `golden` passes, and CTest reports `100% tests passed`.

- [ ] **Step 4: Check no old smoke test is registered**

Run:

```bash
ctest --test-dir build -N
```

Expected output includes `Test #1: golden` and does not include `smoke_register_ir`.

- [ ] **Step 5: Commit documentation and smoke test removal**

Run:

```bash
git add README.md CMakeLists.txt
git commit -m "docs: document golden tests"
```

Expected: commit succeeds and includes the README update plus deletion of `tests/smoke_register_ir.sh`.

## Task 5: Final Verification and Push Readiness

**Files:**
- Verify only; no planned file edits.

- [ ] **Step 1: Run final build and test suite**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected output includes:

```text
[100%] Built target compiler_demo
100% tests passed, 0 tests failed out of 1
```

- [ ] **Step 2: Run the golden runner directly**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected output:

```text
golden tests: 24 passed, 0 failed
```

Reason: seven successful fixtures produce 21 checks, and three runtime error fixtures produce 3 checks.

- [ ] **Step 3: Check repository status**

Run:

```bash
git status --short
```

Expected: no output.

- [ ] **Step 4: Review recent commits**

Run:

```bash
git log --oneline -5
```

Expected: recent commits include:

```text
test: add golden test runner
test: add language golden cases
test: add runtime error golden cases
docs: document golden tests
```

## Self-Review Notes

- Spec coverage: Task 1 implements runner discovery, exact comparison, diff output, update mode, and CTest integration. Task 2 adds successful AST/IR/run fixtures. Task 3 adds runtime error fixtures and validates error diff behavior. Task 4 removes the old smoke script and documents the workflow. Task 5 verifies the full system.
- Scope: The plan does not add C++ unit tests, token golden tests, parser recovery tests, fuzzing, or external test dependencies.
- Type and path consistency: The runner path is consistently `tests/run_golden_tests.py`; the fixture root is consistently `tests/golden`; successful cases use `input.cd`, `ast.out`, `ir.out`, and `run.out`; runtime error cases use `.cd`, `.run.err`, and `.exit`.
