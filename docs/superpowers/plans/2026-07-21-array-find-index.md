# Array `findIndex` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `findIndex(array, predicate)` and `array.findIndex(predicate)` with snapshot, short-circuit, and `-1` no-match semantics.

**Architecture:** Register `findIndex` as a shadowable native function and lower it through the existing `native_call` path. Reuse `checkArrayPredicate` for static callback validation, add member-call recognition, and execute snapshot iteration in the Rust VM without new opcodes.

**Tech Stack:** C++17 compiler/type checker/IR/bytecode emitter, Rust VM, Python golden/artifact runners, Cargo, and CTest.

---

### Task 1: Define the red fixtures

**Files:**
- Create: `tests/golden/array_find_index/input.cd`
- Create: `tests/golden/array_find_index/run.out`
- Create: `tests/golden/array_find_index_shadowing/input.cd`
- Create: `tests/golden/array_find_index_shadowing/run.out`
- Create: `tests/golden/runtime_errors/array_find_index_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/array_find_index_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/array_find_index_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/array_find_index_dynamic_non_bool.cd`
- Create: `tests/golden/runtime_errors/array_find_index_dynamic_non_bool.run.err`
- Create: `tests/golden/runtime_errors/array_find_index_dynamic_non_bool.exit`
- Create: `tests/golden/type_errors/array_find_index_non_array.cd`
- Create: `tests/golden/type_errors/array_find_index_non_array.err`
- Create: `tests/golden/type_errors/array_find_index_non_array.exit`
- Create: `tests/golden/type_errors/array_find_index_callback_return.cd`
- Create: `tests/golden/type_errors/array_find_index_callback_return.err`
- Create: `tests/golden/type_errors/array_find_index_callback_return.exit`

- [x] **Step 1: Add success and error source fixtures.**

Use a typed match, no-match, empty-array, snapshot mutation, and shadowing
case. The success source must exercise both function and member forms:

```cd
let values: [number] = [4, 7, 9];
print findIndex(values, fun (value: number): bool { return value > 6; });
print values.findIndex(fun (value: number): bool { return value == 2; });
let snapshot: [number] = [1, 2];
print snapshot.findIndex(fun (value: number): bool {
  push(snapshot, 9);
  return value == 2;
});
print snapshot;
print [].findIndex(fun (value: number): bool { return true; });
```

Expected output is `1`, `-1`, `1`, `[1, 2, 9, 9]`, `-1` on separate lines.
Use this shadowing source:

```cd
let findIndex = fun (array, predicate) { return "shadowed findIndex"; };
print findIndex([1], fun (value) { return true; });
print [1, 2].findIndex(fun (value) { return value == 2; });
```

Use these dynamic-error sources:

```cd
// array_find_index_dynamic_non_array.cd
fun id(value) { return value; }
findIndex(id(1), fun (value) { return true; });
```

```cd
// array_find_index_dynamic_non_bool.cd
fun id(value) { return value; }
findIndex([1], id(fun (value) { return nil; }));
```

Use these type-error sources:

```cd
// array_find_index_non_array.cd
findIndex(1, fun (value) { return true; });
```

```cd
// array_find_index_callback_return.cd
findIndex([1], fun (value: number): number { return 1; });
```

The dynamic cases must produce no stdout and the runtime messages
`findIndex expects array as first argument` and
`findIndex expects callback to return bool`. The type cases must produce no
stdout and reject the known non-array and non-boolean callback result.

- [x] **Step 2: Run the new fixtures before implementation.**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case array_find_index
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case array_find_index --goldens
```

Expected: the new sources fail because `findIndex` is not registered or is not
recognized as builtin yet.

### Task 2: Register and statically type `findIndex`

**Files:**
- Modify: `include/NativeStdlib.hpp`
- Modify: `src/NativeStdlib.cpp`
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [x] **Step 1: Add the native kind and registry entry.**

Add `NativeFunctionKind::FindIndex` beside `Find` and register
`{"findIndex", 2, NativeFunctionKind::FindIndex}` in the native signature table,
updating its `std::array` size.

- [x] **Step 2: Add static call and member handling.**

Add `checkArrayFindIndex` beside `checkArrayFind`; call
`checkArrayPredicate(callToken, arrayTypeInfo, predicateExpression,
"findIndex")` and return `simpleType(StaticType::Number)`. Dispatch the new
native kind in `checkNativeStdlibCall`, add `findIndex` to
`isBuiltinMemberName`, and add a one-argument member branch that checks the
receiver and calls `checkArrayFindIndex`.

- [x] **Step 3: Build and verify static red cases turn green.**

Run:

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case array_find_index
```

Expected: known non-array and callback-return fixtures produce their expected
type errors, while the success AST/IR/bytecode outputs are not yet generated.

### Task 3: Lower and execute the native

**Files:**
- Modify: `src/IRCompiler.cpp`
- Modify: `vm-rs/src/vm.rs`

- [x] **Step 1: Add member-call native lowering.**

Include `expression.name.lexeme == "findIndex"` in the existing member-call
native-name condition so the receiver is passed as the first native argument.

- [x] **Step 2: Implement snapshot short-circuit execution.**

Dispatch `"findIndex"` in `execute_native_call` to a helper that validates two
arguments, array receiver, one-argument function callback, and boolean callback
results. Clone `array.elements` before iteration; for each `(index, element)`
call the predicate with the cloned element, return `Value::number(index as f64)`
on `Value::Bool(true)`, continue on `Value::Bool(false)`, and return
`Value::number(-1.0)` after the snapshot is exhausted.

- [x] **Step 3: Add focused Rust unit coverage.**

Extend the native callback test with a matching index, a no-match `-1`, and an
empty-array `-1`. Keep the existing predicate clone in every later assertion
because `Value` is moved into `execute_native_call`.

- [x] **Step 4: Verify runtime behavior.**

Run:

```sh
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case array_find_index --goldens
```

Expected: the Rust unit suite and the new success/runtime-error execution
cases pass.

### Task 4: Generate artifacts and update documentation

**Files:**
- Create: `tests/golden/array_find_index/ast.out`
- Create: `tests/golden/array_find_index/ir.out`
- Create: `tests/golden/array_find_index/bytecode.out`
- Create: `tests/bytecode_artifacts/array_find_index/expected.cdbc`
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/bytecode-text-format.md`
- Modify: `docs/roadmap.md`
- Modify: this plan

- [x] **Step 1: Generate and review compiler outputs.**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case array_find_index --update
```

Remove any empty generated `module-interface.out` files, then review the
success AST, IR, bytecode, and run output. Emit the artifact with
`compiler_design --emit-bytecode` and save the resulting `.cdbc` as the
`expected.cdbc` fixture beside its `input.cd` and `run.out`.

- [x] **Step 2: Document the public behavior.**

Document the shadowing rule, zero-based index result, `-1` no-match behavior,
snapshot short-circuit semantics, static callback checks, and `native_call`
support in `README.md`, `AGENTS.md`, and `docs/bytecode-text-format.md`. Mark
`findIndex` implemented in Phase 17 of `docs/roadmap.md` and add it to the
completed helper list.

- [x] **Step 3: Mark this plan complete.**

Change every checkbox in this file to `[x]` after its corresponding command
has passed and its outputs have been reviewed.

### Task 5: Run the complete verification suite and commit

**Files:**
- Modify: none beyond the files above

- [x] **Step 1: Run the repository verification commands.**

Run `cmake -S . -B build`, `cmake --build build`,
`ctest --test-dir build --output-on-failure`,
`python3 tests/run_golden_tests.py ./build/compiler_design`,
`python3 tests/run_golden_tests_selftest.py`,
`python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs`,
`python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens`,
`cargo test --manifest-path vm-rs/Cargo.toml`, and
`git diff --check`. Remove `tests/__pycache__` after Python tests.

- [x] **Step 2: Commit the focused slice.**

Run `git status --short`, stage only the `findIndex` implementation, docs,
fixtures, and plan, then commit with
`git commit -m "feat: add array find index helper"`.
