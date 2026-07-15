# Array Reduce Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `reduce(array, initial, callback)` and `array.reduce(initial, callback)` with explicit-accumulator type checking and C++ compiler/Rust VM parity.

**Architecture:** Reuse the `map`/`filter` native registry, generic `native_call`, member-call lowering, and Rust VM callback stack path. The type checker will derive the callback signature from the initial value and array element type, while the VM snapshots source elements and threads the accumulator through each callback.

**Tech Stack:** C++17 compiler, Python golden/artifact runners, Rust bytecode VM, stable `.cdbc` text artifacts.

---

### Task 1: Establish the reduce contract and red coverage

**Files:**
- Create: `docs/superpowers/specs/2026-07-15-array-reduce-design.md`
- Create: `docs/superpowers/plans/2026-07-15-array-reduce.md`
- Create: `tests/golden/type_errors/array_reduce_callback_return.cd`
- Create: `tests/golden/type_errors/array_reduce_callback_return.err`
- Create: `tests/golden/type_errors/array_reduce_callback_return.exit`

- [x] **Step 1: Write the failing static-check test**

```cd
fun bad(acc: number, value: number): string {
  return "bad";
}
reduce([1], 0, bad);
```

Expected diagnostic after implementation:

```text
Type error at 4:19: reduce expects callback to return number, got string
  reduce([1], 0, bad);
                    ^
```

- [x] **Step 2: Run the focused golden test and confirm it fails because `reduce` is not registered.**

Run: `python3 tests/run_golden_tests.py ./build/compiler_design --case array_reduce_callback_return`

Expected: FAIL with the current undefined-variable diagnostic rather than the
expected reduce callback diagnostic.

### Task 2: Register the native function

**Files:**
- Modify: `include/NativeStdlib.hpp`
- Modify: `src/NativeStdlib.cpp`

- [x] **Step 1: Add `Reduce` after `Filter` in `NativeFunctionKind` and register `{"reduce", 3, NativeFunctionKind::Reduce}` in the native table.**
- [x] **Step 2: Build the compiler and run the red fixture to expose the next missing layer.**

Run: `cmake --build build && python3 tests/run_golden_tests.py ./build/compiler_design --case array_reduce_callback_return`

Expected: the native function is recognized and the focused check reaches the
type-checking branch that still lacks `Reduce` handling.

### Task 3: Add reduce type checking and member recognition

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [x] **Step 1: Add `checkArrayReduce(callToken, arrayTypeInfo, initialExpression, callbackExpression)`.**

It must reject known non-arrays, check the initial expression, provide
`functionType({accumulatorType, elementType}, accumulatorType)` as the lambda
context, reject known non-functions, generic callbacks, and callback signatures
whose arity is not two, reject known incompatible accumulator/element parameter
types, reject a known incompatible return type with
`reduce expects callback to return <accumulator>, got <type>`, and return the
known initial accumulator type or unknown.

- [x] **Step 2: Add the `Reduce` native switch branch and add `reduce` to builtin member names.**
- [x] **Step 3: Add the member-call branch with two arguments (`initial`, `callback`) and the same helper.**
- [x] **Step 4: Run focused static tests for direct/member arity, non-array,
non-function, callback arity, accumulator mismatch, element mismatch, return
mismatch, generic callbacks, and typed result inference.**

Run: `python3 tests/run_golden_tests.py ./build/compiler_design --case array_reduce`

Expected: all reduce type-error fixtures pass and success fixtures reach IR
lowering.

### Task 4: Lower both forms through native_call

**Files:**
- Modify: `src/IRCompiler.cpp`

- [x] **Step 1: Add `reduce` to the member native-call name set.**
- [x] **Step 2: Compile direct and member reduce fixtures with `--ir` and
verify direct arguments are `[array, initial, callback]` and member arguments
are `[receiver, initial, callback]`.**

Run: `python3 tests/run_golden_tests.py ./build/compiler_design --case array_reduce --update-missing --update`

Expected: IR and bytecode artifacts contain no new opcode and show the reduce
native name.

### Task 5: Execute reduce callbacks in the Rust VM

**Files:**
- Modify: `vm-rs/src/vm.rs`

- [x] **Step 1: Dispatch `reduce` from `execute_native_call_at`.**
- [x] **Step 2: Implement `execute_native_reduce` with exact arity/type checks,
source-element snapshotting, two-argument callback invocation, accumulator
threading, and empty-array initial-value behavior. Reuse `call_function` with
the caller and call-site so callback failures retain their stack.**
- [x] **Step 3: Add Rust unit coverage for accumulation, empty arrays, source
snapshot behavior, callback arity, and runtime operand errors.**
- [x] **Step 4: Run focused Rust VM parity and cargo tests.**

Run: `python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens && cargo test --manifest-path vm-rs/Cargo.toml`

Expected: reduce success and runtime-error fixtures pass with the specified
diagnostics.

### Task 6: Complete fixtures and documentation

**Files:**
- Create/modify: `tests/golden/array_reduce/`
- Create/modify: `tests/golden/array_reduce_empty/`
- Create/modify: `tests/golden/array_reduce_shadowing/`
- Create/modify: `tests/golden/type_errors/array_reduce_*.{cd,err,exit}`
- Create/modify: `tests/golden/runtime_errors/array_reduce_*.{cd,run.err,exit}`
- Create/modify: `tests/bytecode_artifacts/array_reduce/`
- Create/modify: `tests/bytecode_artifacts/array_reduce_shadowing/`
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [x] **Step 1: Add direct/member, closure, empty-array, source-snapshot, and
array-accumulator success fixtures; generate and review AST, IR, bytecode, and
run outputs.**
- [x] **Step 2: Add runtime operand, arity, and callback-failure fixtures.**
- [x] **Step 3: Document implemented reduce behavior and leave any more
general accumulator inference or iterator protocols deferred.**

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

- [x] **Step 2: Check the worktree, stage only the reduce slice, and commit.**

```sh
git add AGENTS.md README.md docs/roadmap.md docs/superpowers/specs/2026-07-15-array-reduce-design.md docs/superpowers/plans/2026-07-15-array-reduce.md include/NativeStdlib.hpp src/NativeStdlib.cpp include/TypeChecker.hpp src/TypeChecker.cpp src/IRCompiler.cpp vm-rs/src/vm.rs tests/golden/array_reduce tests/golden/array_reduce_empty tests/golden/array_reduce_shadowing tests/golden/type_errors/array_reduce_* tests/golden/runtime_errors/array_reduce_* tests/bytecode_artifacts/array_reduce tests/bytecode_artifacts/array_reduce_shadowing
git commit -m "feat: add array reduce builtin"
```
