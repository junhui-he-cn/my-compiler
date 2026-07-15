# Array Filter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `filter(array, predicate)` and `array.filter(predicate)` as a statically checked, callback-based array helper with C++ compiler and Rust VM parity.

**Architecture:** Reuse `NativeFunctionKind`, `native_call`, member-call lowering, and Rust VM callback invocation established by `map`. Add a separate filter-specific type-check helper so `filter` preserves the source element type and requires a boolean predicate result, then snapshot source elements before invoking callbacks.

**Tech Stack:** C++17 compiler, Python golden/artifact runners, Rust bytecode VM, stable `.cdbc` text artifacts.

---

### Task 1: Establish the filter contract and red coverage

**Files:**
- Create: `docs/superpowers/specs/2026-07-15-array-filter-design.md`
- Create: `docs/superpowers/plans/2026-07-15-array-filter.md`
- Create: `tests/golden/type_errors/array_filter_callback_return.cd`
- Create: `tests/golden/type_errors/array_filter_callback_return.err`
- Create: `tests/golden/type_errors/array_filter_callback_return.exit`

- [x] **Step 1: Write the failing static-check test**

```cd
fun isPositive(value: number): number {
  return value;
}
filter([1], isPositive);
```

Expected diagnostic after implementation:

```text
Type error at 4:23: filter expects callback to return bool, got number
  filter([1], isPositive);
                        ^
```

- [x] **Step 2: Run the focused golden test and confirm it fails because `filter` is not registered**

Run: `python3 tests/run_golden_tests.py ./build/compiler_design --case array_filter_callback_return`

Expected: FAIL with the current undefined-variable diagnostic rather than the
expected filter predicate diagnostic.

### Task 2: Register the native function

**Files:**
- Modify: `include/NativeStdlib.hpp`
- Modify: `src/NativeStdlib.cpp`

- [x] **Step 1: Add `Filter` after `Map` in `NativeFunctionKind` and register `{"filter", 2, NativeFunctionKind::Filter}` in the native table.**
- [x] **Step 2: Build the compiler and run the red fixture to expose the next missing layer.**

Run: `cmake --build build && python3 tests/run_golden_tests.py ./build/compiler_design --case array_filter_callback_return`

Expected: the native function is recognized and the focused check reaches the
type-checking branch that still lacks `Filter` handling.

### Task 3: Add filter type checking and member recognition

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [x] **Step 1: Add `checkArrayFilter(callToken, arrayTypeInfo, predicateExpression)`.**

It must reject known non-arrays, provide `functionType({elementType}, bool)` as
the lambda context, reject known non-functions, generic callbacks, and callback
signatures whose arity is not one, reject a known non-`bool` return type with
`filter expects callback to return bool, got <type>`, and return `[elementType]`
when the source element type is known or a dynamic array otherwise.

- [x] **Step 2: Add the `Filter` native switch branch and add `filter` to builtin member names.**
- [x] **Step 3: Add the member-call branch with one predicate argument and the same filter helper.**
- [x] **Step 4: Run focused static tests for direct/member arity, non-array,
non-function, callback arity/parameter mismatch, non-bool return, generic
callbacks, and unknown values.**

Run: `python3 tests/run_golden_tests.py ./build/compiler_design --case array_filter`

Expected: all filter type-error fixtures pass and success fixtures now reach IR
lowering.

### Task 4: Lower both forms through native_call

**Files:**
- Modify: `src/IRCompiler.cpp`

- [x] **Step 1: Add `filter` to the member native-call name set.**
- [x] **Step 2: Compile a direct and member filter fixture with `--ir` and
verify both emit `native_call ... filter(...)` with receiver-first arguments.**

Run: `python3 tests/run_golden_tests.py ./build/compiler_design --case array_filter --update-missing --update`

Expected: IR and bytecode artifacts contain no new opcode and show the filter
native name.

### Task 5: Execute filter callbacks in the Rust VM

**Files:**
- Modify: `vm-rs/src/vm.rs`

- [x] **Step 1: Dispatch `filter` from `execute_native_call_at`.**
- [x] **Step 2: Implement `execute_native_filter` with exact arity/type checks,
source-element snapshotting, one-argument callback invocation, boolean result
validation, and fresh result-array allocation. Reuse `call_function` with the
caller and call-site so callback failures retain their stack.**
- [x] **Step 3: Add Rust unit coverage for selection, fresh arrays, snapshot
behavior, non-bool predicate results, callback arity, and native arity/type
errors.**
- [x] **Step 4: Run focused Rust VM parity and cargo tests.**

Run: `python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case array_filter && cargo test --manifest-path vm-rs/Cargo.toml`

Expected: filter success and runtime-error fixtures pass with the specified
diagnostics.

### Task 6: Complete fixtures and documentation

**Files:**
- Create/modify: `tests/golden/array_filter/`
- Create/modify: `tests/golden/array_filter_empty/`
- Create/modify: `tests/golden/array_filter_shadowing/`
- Create/modify: `tests/golden/type_errors/array_filter_*.{cd,err,exit}`
- Create/modify: `tests/golden/runtime_errors/array_filter_*.{cd,run.err,exit}`
- Create/modify: `tests/bytecode_artifacts/array_filter/`
- Create/modify: `tests/bytecode_artifacts/array_filter_shadowing/`
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [x] **Step 1: Add direct/member, closure, snapshot, empty-array, and typed
success fixtures; generate and review AST, IR, bytecode, and run outputs.**
- [x] **Step 2: Add runtime operand, arity, non-boolean callback, and callback
failure fixtures.**
- [x] **Step 3: Document implemented filter behavior and move `filter` out of
the deferred list while keeping `reduce` deferred.**

### Task 7: Verify and commit

**Files:**
- Modify: all focused implementation, fixture, and documentation files above.

- [x] **Step 1: Run the complete repository verification commands.**

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
git diff --check
```

- [x] **Step 2: Check the worktree, stage only the filter slice, and commit.**

```sh
git status --short
git add AGENTS.md README.md docs/roadmap.md docs/superpowers/specs/2026-07-15-array-filter-design.md docs/superpowers/plans/2026-07-15-array-filter.md include/NativeStdlib.hpp src/NativeStdlib.cpp include/TypeChecker.hpp src/TypeChecker.cpp src/IRCompiler.cpp vm-rs/src/vm.rs tests/golden/array_filter tests/golden/array_filter_empty tests/golden/array_filter_shadowing tests/golden/type_errors/array_filter_callback_arity.cd tests/golden/type_errors/array_filter_callback_arity.err tests/golden/type_errors/array_filter_callback_arity.exit tests/golden/type_errors/array_filter_callback_parameter_mismatch.cd tests/golden/type_errors/array_filter_callback_parameter_mismatch.err tests/golden/type_errors/array_filter_callback_parameter_mismatch.exit tests/golden/type_errors/array_filter_callback_return.cd tests/golden/type_errors/array_filter_callback_return.err tests/golden/type_errors/array_filter_callback_return.exit tests/golden/type_errors/array_filter_generic_callback.cd tests/golden/type_errors/array_filter_generic_callback.err tests/golden/type_errors/array_filter_generic_callback.exit tests/golden/type_errors/array_filter_member_arity.cd tests/golden/type_errors/array_filter_member_arity.err tests/golden/type_errors/array_filter_member_arity.exit tests/golden/type_errors/array_filter_member_non_function.cd tests/golden/type_errors/array_filter_member_non_function.err tests/golden/type_errors/array_filter_member_non_function.exit tests/golden/type_errors/array_filter_non_array.cd tests/golden/type_errors/array_filter_non_array.err tests/golden/type_errors/array_filter_non_array.exit tests/golden/type_errors/array_filter_non_function.cd tests/golden/type_errors/array_filter_non_function.err tests/golden/type_errors/array_filter_non_function.exit tests/golden/runtime_errors/array_filter_callback_failure.cd tests/golden/runtime_errors/array_filter_callback_failure.run.err tests/golden/runtime_errors/array_filter_callback_failure.exit tests/golden/runtime_errors/array_filter_dynamic_callback_arity.cd tests/golden/runtime_errors/array_filter_dynamic_callback_arity.run.err tests/golden/runtime_errors/array_filter_dynamic_callback_arity.exit tests/golden/runtime_errors/array_filter_dynamic_non_array.cd tests/golden/runtime_errors/array_filter_dynamic_non_array.run.err tests/golden/runtime_errors/array_filter_dynamic_non_array.exit tests/golden/runtime_errors/array_filter_dynamic_non_bool.cd tests/golden/runtime_errors/array_filter_dynamic_non_bool.run.err tests/golden/runtime_errors/array_filter_dynamic_non_bool.exit tests/golden/runtime_errors/array_filter_dynamic_non_function.cd tests/golden/runtime_errors/array_filter_dynamic_non_function.run.err tests/golden/runtime_errors/array_filter_dynamic_non_function.exit tests/golden/runtime_errors/array_filter_member_dynamic_non_array.cd tests/golden/runtime_errors/array_filter_member_dynamic_non_array.run.err tests/golden/runtime_errors/array_filter_member_dynamic_non_array.exit tests/bytecode_artifacts/array_filter tests/bytecode_artifacts/array_filter_shadowing
git commit -m "feat: add array filter builtin"
```
