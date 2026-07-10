# Nullable Logical Multi-Narrowing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow supported logical nullable guards to narrow multiple simple variables in the same branch.

**Architecture:** Change `TypeChecker::IfNarrowing` from one optional narrowing per branch to vectors. Keep the existing resolved-name narrowing stack and apply all branch narrowings around branch body checking.

**Tech Stack:** C++17 TypeChecker, Python golden tests, markdown docs.

---

## Tasks

### Task 1: Add failing golden fixture

Files:
- Create `tests/golden/nullable_logical_multi_narrowing/input.cd`
- Create `tests/golden/nullable_logical_multi_narrowing/run.out`

Steps:
1. Create fixture directory.
2. Write `input.cd` with two nullable variables guarded by `&&`, and two nullable variables guarded by `||` with use in `else`.
3. Write `run.out` with expected numbers.
4. Run `python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_logical_multi_narrowing` and confirm it fails with `number?` for the second guarded variable.

### Task 2: Implement multi-narrowing in TypeChecker

Files:
- Modify `include/TypeChecker.hpp`
- Modify `src/TypeChecker.cpp`

Steps:
1. Replace `IfNarrowing` optional fields with `std::vector<Narrowing> thenNarrowings` and `elseNarrowings`.
2. Replace `withOptionalNarrowing` with a helper that pushes all narrowings for a branch and pops the same count afterward.
3. Update direct nil comparison narrowing to append one narrowing to the proper vector.
4. Update logical `&&` to concatenate then narrowings from left and right.
5. Update logical `||` to concatenate else narrowings from left and right.
6. Rebuild and run the focused golden fixture.

### Task 3: Update docs

Files:
- Modify `README.md`
- Modify `docs/roadmap.md`

Steps:
1. Update nullable docs to say supported logical guards can narrow simple variables, including multiple variables.
2. Update roadmap Phase 15G status to mention multi-variable logical narrowing.
3. Run `python3 tests/run_golden_tests.py ./build/compiler_design --case nullable`.

### Task 4: Full verification and commit

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
git status --short
```

Commit message:

```sh
git commit -m "feat: narrow multiple nullable variables in logical guards"
```
