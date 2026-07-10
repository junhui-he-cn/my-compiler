# Sanitizer Guardrails Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add opt-in ASan/UBSan C++ builds locally and in a separate GitHub Actions sanitizer job.

**Architecture:** Extend existing infrastructure tests first so the new CMake option and workflow job fail before implementation. Then add a target-level sanitizer helper in `CMakeLists.txt`, apply it to all repository C++ targets, add a separate `sanitizers` CI job, and document local sanitizer usage.

**Tech Stack:** CMake 3.16, C++17, GCC/Clang sanitizer flags, Python infrastructure tests, GitHub Actions, Rust toolchain for CTest VM coverage.

---

## File Structure

- Modify `tests/cmake_config_tests.py`: assert `COMPILER_DESIGN_ENABLE_SANITIZERS` is a BOOL CMake cache option when configured.
- Modify `tests/ci_workflow_tests.py`: assert `.github/workflows/ci.yml` contains the sanitizer job and commands.
- Modify `CMakeLists.txt`: add sanitizer option/helper and apply it to `compiler_design`, `frontend_session_tests`, `flow_facts_tests`, and `module_symbols_tests`.
- Modify `.github/workflows/ci.yml`: add a separate `sanitizers` job.
- Modify `README.md`: document sanitizer-enabled local builds near the Build section.

---

### Task 1: Add failing sanitizer infrastructure tests

**Files:**
- Modify: `tests/cmake_config_tests.py`
- Modify: `tests/ci_workflow_tests.py`

- [ ] **Step 1: Confirm workspace state**

Run:

```sh
git status --short
```

Expected: no output.

- [ ] **Step 2: Extend the CMake config test**

In `tests/cmake_config_tests.py`, replace the `subprocess.run` argument list inside `main()` with:

```python
            [
                "cmake",
                "-S",
                str(repo_root),
                "-B",
                str(build_dir),
                "-DCOMPILER_DESIGN_ENABLE_WARNINGS=ON",
                "-DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON",
            ],
```

Then replace the single expected-cache assertion:

```python
        expected = "COMPILER_DESIGN_ENABLE_WARNINGS:BOOL=ON"
        if expected not in cache:
            fail(f"expected CMake cache to contain {expected!r}")
```

with:

```python
        expected_options = (
            "COMPILER_DESIGN_ENABLE_WARNINGS:BOOL=ON",
            "COMPILER_DESIGN_ENABLE_SANITIZERS:BOOL=ON",
        )
        for expected in expected_options:
            if expected not in cache:
                fail(f"expected CMake cache to contain {expected!r}")
```

- [ ] **Step 3: Extend the CI workflow content test**

In `tests/ci_workflow_tests.py`, add these strings to `REQUIRED_SNIPPETS` after the existing `cargo test --manifest-path vm-rs/Cargo.toml` entry:

```python
    "sanitizers:",
    "name: Sanitizers",
    "cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON",
    "cmake --build build-sanitize",
    "ctest --test-dir build-sanitize --output-on-failure",
```

Ensure the previous `cargo test` entry has a trailing comma.

- [ ] **Step 4: Run focused tests to verify RED**

Run:

```sh
cmake -S . -B build
ctest --test-dir build --output-on-failure -R '^(cmake_config|ci_workflow)$'
```

Expected: both tests fail. `cmake_config` should fail because `COMPILER_DESIGN_ENABLE_SANITIZERS` is not a BOOL cache option yet. `ci_workflow` should fail because the sanitizer job snippets are missing.

---

### Task 2: Implement the CMake sanitizer option

**Files:**
- Modify: `CMakeLists.txt`
- Test: `tests/cmake_config_tests.py`

- [ ] **Step 1: Add the sanitizer option and helper function**

In `CMakeLists.txt`, add this block after the existing `compiler_design_apply_warnings` function:

```cmake
option(COMPILER_DESIGN_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for project C++ targets" OFF)

function(compiler_design_apply_sanitizers target_name)
    if(NOT COMPILER_DESIGN_ENABLE_SANITIZERS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()
```

- [ ] **Step 2: Apply sanitizers to `compiler_design`**

In `CMakeLists.txt`, add this line immediately after `compiler_design_apply_warnings(compiler_design)`:

```cmake
compiler_design_apply_sanitizers(compiler_design)
```

- [ ] **Step 3: Apply sanitizers to `frontend_session_tests`**

In `CMakeLists.txt`, add this line immediately after `compiler_design_apply_warnings(frontend_session_tests)`:

```cmake
compiler_design_apply_sanitizers(frontend_session_tests)
```

- [ ] **Step 4: Apply sanitizers to `flow_facts_tests`**

In `CMakeLists.txt`, add this line immediately after `compiler_design_apply_warnings(flow_facts_tests)`:

```cmake
compiler_design_apply_sanitizers(flow_facts_tests)
```

- [ ] **Step 5: Apply sanitizers to `module_symbols_tests`**

In `CMakeLists.txt`, add this line immediately after `compiler_design_apply_warnings(module_symbols_tests)`:

```cmake
compiler_design_apply_sanitizers(module_symbols_tests)
```

- [ ] **Step 6: Run focused CMake config test to verify GREEN**

Run:

```sh
cmake -S . -B build
ctest --test-dir build --output-on-failure -R '^cmake_config$'
```

Expected: `cmake_config` passes.

- [ ] **Step 7: Build with sanitizers enabled**

Run:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
```

Expected: both commands exit 0.

---

### Task 3: Add GitHub Actions sanitizer job

**Files:**
- Modify: `.github/workflows/ci.yml`
- Test: `tests/ci_workflow_tests.py`

- [ ] **Step 1: Add the sanitizer job**

In `.github/workflows/ci.yml`, append this job under `jobs:` at the same indentation level as `verify:`:

```yaml
  sanitizers:
    name: Sanitizers
    runs-on: ubuntu-latest

    steps:
      - name: Check out repository
        uses: actions/checkout@v4

      - name: Install Rust toolchain
        uses: dtolnay/rust-toolchain@stable

      - name: Configure CMake with sanitizers
        run: cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON

      - name: Build with sanitizers
        run: cmake --build build-sanitize

      - name: Run CTest with sanitizers
        run: ctest --test-dir build-sanitize --output-on-failure
```

Do not change the existing `verify` job commands.

- [ ] **Step 2: Run workflow content test to verify GREEN**

Run:

```sh
ctest --test-dir build --output-on-failure -R '^ci_workflow$'
```

Expected: `ci_workflow` passes.

- [ ] **Step 3: Commit sanitizer infrastructure**

Run:

```sh
git add CMakeLists.txt .github/workflows/ci.yml tests/cmake_config_tests.py tests/ci_workflow_tests.py
git commit -m "ci: add sanitizer guardrails"
```

Expected: commit succeeds.

---

### Task 4: Document sanitizer-enabled local builds

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add sanitizer option documentation**

In `README.md`, find this block in the Build section:

```markdown
To enable compiler warnings for local C++ builds:

```sh
cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON
cmake --build build
```
```

Add this block immediately after it:

````markdown
To enable AddressSanitizer and UndefinedBehaviorSanitizer for local C++ builds:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```
````

- [ ] **Step 2: Verify README diff**

Run:

```sh
git diff -- README.md
```

Expected: the diff only adds the sanitizer-enabled local build example under `## Build`.

- [ ] **Step 3: Commit README documentation**

Run:

```sh
git add README.md
git commit -m "docs: document sanitizer builds"
```

Expected: commit succeeds.

---

### Task 5: Sanitizer verification and full cleanup

**Files:**
- Verify: whole repository
- Clean: `tests/__pycache__/`

- [ ] **Step 1: Run sanitizer configure, build, and CTest**

Run:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

Expected: all three commands exit 0. If sanitizer CTest fails with ASan/UBSan output, stop and investigate the sanitizer finding before changing code.

- [ ] **Step 2: Run the standard full verification suite**

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

- Spec coverage: Task 1 adds failing tests for the new CMake option and workflow job; Task 2 implements target-level sanitizer flags; Task 3 adds the separate GitHub Actions sanitizer job; Task 4 documents local usage; Task 5 verifies both sanitizer and normal builds.
- Placeholder scan: no placeholders, TODOs, or deferred implementation instructions remain.
- Type/name consistency: the CMake option is consistently `COMPILER_DESIGN_ENABLE_SANITIZERS`; the helper is consistently `compiler_design_apply_sanitizers`; the CI build directory is consistently `build-sanitize`.
