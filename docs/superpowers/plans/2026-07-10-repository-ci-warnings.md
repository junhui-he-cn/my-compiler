# Repository CI and Optional Warnings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add GitHub Actions CI for the existing full verification suite and a CMake option that enables compiler warning flags for repository C++ targets.

**Architecture:** Add two small Python configuration tests so the infrastructure changes are test-first: one verifies the CMake warnings option is a real BOOL cache option, and one verifies the CI workflow includes the required commands. Then implement the CMake option/helper in `CMakeLists.txt`, create `.github/workflows/ci.yml`, and document warning-enabled local builds in `README.md` without changing compiler behavior.

**Tech Stack:** CMake 3.16, C++17, Python 3.9+, GitHub Actions on Ubuntu, Rust/Cargo for `vm-rs` verification.

---

## File Structure

- Create `.github/workflows/ci.yml`: repository CI workflow for pushes and pull requests to `master`.
- Create `tests/cmake_config_tests.py`: Python test that configures a temporary build and asserts `COMPILER_DESIGN_ENABLE_WARNINGS` is a real BOOL CMake option.
- Create `tests/ci_workflow_tests.py`: Python test that checks the workflow contains the required triggers and verification commands.
- Modify `CMakeLists.txt`: add the warnings option/helper, apply it to all C++ targets, and register the new Python tests with CTest.
- Modify `README.md`: document the optional warning-enabled configure command near the existing Build section.

---

### Task 1: Add failing infrastructure tests

**Files:**
- Create: `tests/cmake_config_tests.py`
- Create: `tests/ci_workflow_tests.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Confirm workspace state**

Run:

```sh
git status --short
```

Expected: no output.

- [ ] **Step 2: Add the CMake warnings-option test**

Create `tests/cmake_config_tests.py` with this exact content:

```python
#!/usr/bin/env python3

import subprocess
import sys
import tempfile
from pathlib import Path


def fail(message: str, completed: subprocess.CompletedProcess[str] | None = None) -> None:
    print(f"FAIL {message}", file=sys.stderr)
    if completed is not None:
        print("STDOUT:", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        print("STDERR:", file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: cmake_config_tests.py <repo-root>")

    repo_root = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="compiler-design-cmake-") as tmp:
        build_dir = Path(tmp) / "build"
        completed = subprocess.run(
            [
                "cmake",
                "-S",
                str(repo_root),
                "-B",
                str(build_dir),
                "-DCOMPILER_DESIGN_ENABLE_WARNINGS=ON",
            ],
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            fail("warning-enabled CMake configure exited non-zero", completed)

        cache_path = build_dir / "CMakeCache.txt"
        cache = cache_path.read_text(encoding="utf-8")
        expected = "COMPILER_DESIGN_ENABLE_WARNINGS:BOOL=ON"
        if expected not in cache:
            fail(f"expected CMake cache to contain {expected!r}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Add the CI workflow content test**

Create `tests/ci_workflow_tests.py` with this exact content:

```python
#!/usr/bin/env python3

import sys
from pathlib import Path


REQUIRED_SNIPPETS = (
    "name: CI",
    "push:",
    "pull_request:",
    "branches: [master]",
    "uses: actions/checkout@v4",
    "uses: dtolnay/rust-toolchain@stable",
    "cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON",
    "cmake --build build",
    "ctest --test-dir build --output-on-failure",
    "python3 tests/run_golden_tests.py ./build/compiler_design",
    "python3 tests/run_golden_tests_selftest.py",
    "python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs",
    "python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens",
    "cargo test --manifest-path vm-rs/Cargo.toml",
)


def fail(message: str) -> None:
    print(f"FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: ci_workflow_tests.py <repo-root>")

    repo_root = Path(sys.argv[1]).resolve()
    workflow_path = repo_root / ".github" / "workflows" / "ci.yml"
    if not workflow_path.is_file():
        fail(f"missing workflow file: {workflow_path}")

    workflow = workflow_path.read_text(encoding="utf-8")
    missing = [snippet for snippet in REQUIRED_SNIPPETS if snippet not in workflow]
    if missing:
        fail("workflow missing required snippets: " + ", ".join(repr(item) for item in missing))


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Register the infrastructure tests with CTest**

In `CMakeLists.txt`, add these CTest registrations after the existing `golden_runner_selftest` test block:

```cmake
add_test(
    NAME cmake_config
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cmake_config_tests.py ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME ci_workflow
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/ci_workflow_tests.py ${CMAKE_SOURCE_DIR}
)
```

- [ ] **Step 5: Run the new tests to verify RED**

Run:

```sh
cmake -S . -B build
ctest --test-dir build --output-on-failure -R '^(cmake_config|ci_workflow)$'
```

Expected: `cmake_config` fails because the cache contains `COMPILER_DESIGN_ENABLE_WARNINGS:UNINITIALIZED=ON` instead of `COMPILER_DESIGN_ENABLE_WARNINGS:BOOL=ON`, and `ci_workflow` fails because `.github/workflows/ci.yml` does not exist yet.

---

### Task 2: Implement the CMake warnings option

**Files:**
- Modify: `CMakeLists.txt`
- Test: `tests/cmake_config_tests.py`

- [ ] **Step 1: Add the warning option and helper function**

In `CMakeLists.txt`, add this block after `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`:

```cmake
option(COMPILER_DESIGN_ENABLE_WARNINGS "Enable compiler warning flags for project C++ targets" OFF)

function(compiler_design_apply_warnings target_name)
    if(NOT COMPILER_DESIGN_ENABLE_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
```

- [ ] **Step 2: Apply warnings to `compiler_design`**

In `CMakeLists.txt`, add this line immediately after `target_include_directories(compiler_design PRIVATE include)`:

```cmake
compiler_design_apply_warnings(compiler_design)
```

- [ ] **Step 3: Apply warnings to `frontend_session_tests`**

In `CMakeLists.txt`, add this line immediately after `target_include_directories(frontend_session_tests PRIVATE include)`:

```cmake
compiler_design_apply_warnings(frontend_session_tests)
```

- [ ] **Step 4: Apply warnings to `flow_facts_tests`**

In `CMakeLists.txt`, add this line immediately after `target_include_directories(flow_facts_tests PRIVATE include)`:

```cmake
compiler_design_apply_warnings(flow_facts_tests)
```

- [ ] **Step 5: Apply warnings to `module_symbols_tests`**

In `CMakeLists.txt`, add this line immediately after `target_include_directories(module_symbols_tests PRIVATE include)`:

```cmake
compiler_design_apply_warnings(module_symbols_tests)
```

- [ ] **Step 6: Run the CMake config test to verify GREEN for the option**

Run:

```sh
cmake -S . -B build
ctest --test-dir build --output-on-failure -R '^cmake_config$'
```

Expected: `cmake_config` passes.

- [ ] **Step 7: Build with warnings enabled**

Run:

```sh
cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON
cmake --build build
```

Expected: build succeeds. If warnings fail the build in the active compiler, fix only the specific warning-causing code or reduce the warning set while preserving `-Wall -Wextra -Wpedantic` unless the compiler does not support one of those flags.

---

### Task 3: Add GitHub Actions CI workflow

**Files:**
- Create: `.github/workflows/ci.yml`
- Test: `tests/ci_workflow_tests.py`

- [ ] **Step 1: Add the CI workflow**

Create `.github/workflows/ci.yml` with this exact content:

```yaml
name: CI

on:
  push:
    branches: [master]
  pull_request:
    branches: [master]

jobs:
  verify:
    name: Verify
    runs-on: ubuntu-latest

    steps:
      - name: Check out repository
        uses: actions/checkout@v4

      - name: Install Rust toolchain
        uses: dtolnay/rust-toolchain@stable

      - name: Configure CMake
        run: cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON

      - name: Build
        run: cmake --build build

      - name: Run CTest
        run: ctest --test-dir build --output-on-failure

      - name: Run golden tests
        run: python3 tests/run_golden_tests.py ./build/compiler_design

      - name: Run golden runner selftest
        run: python3 tests/run_golden_tests_selftest.py

      - name: Run bytecode artifact tests
        run: python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs

      - name: Run Rust VM golden parity tests
        run: python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens

      - name: Run Rust unit tests
        run: cargo test --manifest-path vm-rs/Cargo.toml
```

- [ ] **Step 2: Run the workflow content test to verify GREEN**

Run:

```sh
ctest --test-dir build --output-on-failure -R '^ci_workflow$'
```

Expected: `ci_workflow` passes.

- [ ] **Step 3: Commit CI and warning infrastructure**

Run:

```sh
git add CMakeLists.txt .github/workflows/ci.yml tests/cmake_config_tests.py tests/ci_workflow_tests.py
git commit -m "ci: add repository verification workflow"
```

Expected: commit succeeds.

---

### Task 4: Document optional warning-enabled local builds

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add warning option documentation**

In `README.md`, replace the current Build section:

```markdown
## Build

```sh
cmake -S . -B build
cmake --build build
```
```

with:

````markdown
## Build

```sh
cmake -S . -B build
cmake --build build
```

To enable compiler warnings for local C++ builds:

```sh
cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON
cmake --build build
```
````

- [ ] **Step 2: Verify README changed only the build documentation**

Run:

```sh
git diff -- README.md
```

Expected: the diff only adds the warning-enabled CMake example under `## Build`.

- [ ] **Step 3: Commit README documentation**

Run:

```sh
git add README.md
git commit -m "docs: document warning-enabled builds"
```

Expected: commit succeeds.

---

### Task 5: Full verification and cleanup

**Files:**
- Verify: whole repository
- Clean: `tests/__pycache__/`

- [ ] **Step 1: Run warning-enabled configure and build**

Run:

```sh
cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON
cmake --build build
```

Expected: both commands exit 0.

- [ ] **Step 2: Run the full verification suite**

Run:

```sh
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

Expected: every command exits 0.

- [ ] **Step 3: Confirm final workspace state**

Run:

```sh
git status --short
```

Expected: no output. If files remain modified, inspect them and either commit intentional changes or revert accidental changes.

- [ ] **Step 4: Report final verification evidence**

In the final response, list each verification command and observed pass counts. Do not claim completion without fresh verification evidence.

---

## Self-Review Notes

- Spec coverage: Task 1 creates test-first coverage for the CMake option and workflow; Task 2 implements the optional warnings option for all C++ targets; Task 3 adds the GitHub Actions workflow with the required full verification commands; Task 4 updates README only where the existing build workflow is documented; Task 5 runs the required full verification suite.
- Red-flag scan: the plan has exact file paths, code snippets, commands, expected outcomes, and commits; it contains no deferred placeholders.
- Type/name consistency: the CMake option is consistently named `COMPILER_DESIGN_ENABLE_WARNINGS`; the helper is consistently named `compiler_design_apply_warnings`; the CTest names are consistently `cmake_config` and `ci_workflow`.
