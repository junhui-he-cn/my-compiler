# Native Stdlib Math Builtins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add shadowable native stdlib math builtins `floor(number)`, `ceil(number)`, and `sqrt(number)` using the existing `native_call` IR/bytecode path.

**Architecture:** Extend `NativeStdlib` from arity-only metadata to named signatures with a `NativeFunctionKind`, then use that registry from the type checker and existing IR compiler native-call recognition. Runtime execution is added to the C++ IR interpreter and Rust VM native dispatch; bytecode and `.cdbc` syntax stay unchanged because `native_call` already carries the function name.

**Tech Stack:** C++17 compiler/type checker/IR interpreter, Python golden tests, `.cdbc` bytecode artifacts, Rust VM parser/formatter/executor.

---

## File Structure

- Modify `include/NativeStdlib.hpp`: add `NativeFunctionKind` and signature lookup API.
- Modify `src/NativeStdlib.cpp`: add `floor`, `ceil`, and `sqrt` signatures.
- Modify `src/TypeChecker.cpp`: use signature kinds for `push`/`pop` and new math static checks.
- Modify `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: execute math native calls in C++ `--run`.
- Modify `vm-rs/src/vm.rs`: execute math native calls in Rust VM.
- Modify `tests/run_rust_vm_tests.py`: include `native_stdlib_math` in golden allowlist.
- Create `tests/golden/native_stdlib_math/`: success fixture with AST, IR, bytecode, and run outputs.
- Create `tests/golden/type_errors/*math*`: static diagnostics.
- Create `tests/golden/runtime_errors/*math*`: runtime diagnostics.
- Create `tests/bytecode_artifacts/native_stdlib_math/`: `.cdbc` parity fixture.
- Modify `README.md`, `docs/roadmap.md`, `docs/bytecode-text-format.md`, `AGENTS.md`: document implemented math builtins and native stdlib path.

---

### Task 1: RED success fixture for math builtins

**Files:**
- Create: `tests/golden/native_stdlib_math/input.cd`
- Create: `tests/golden/native_stdlib_math/ast.out`
- Create: `tests/golden/native_stdlib_math/run.out`

- [ ] **Step 1: Add success input**

Create `tests/golden/native_stdlib_math/input.cd`:

```cd
print floor(1.9);
print floor(-1.1);
print ceil(1.1);
print ceil(-1.9);
print sqrt(9);
print sqrt(2 * 8);
```

- [ ] **Step 2: Add AST expectation**

Create `tests/golden/native_stdlib_math/ast.out`:

```text
Program
  Print (call floor 1.9)
  Print (call floor (- 1.1))
  Print (call ceil 1.1)
  Print (call ceil (- 1.9))
  Print (call sqrt 9)
  Print (call sqrt (* 2 8))
```

- [ ] **Step 3: Add run expectation**

Create `tests/golden/native_stdlib_math/run.out`:

```text
1
-2
2
-1
3
4
```

- [ ] **Step 4: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: `native_stdlib_math` fails because unbound `floor` is currently an undefined variable.

- [ ] **Step 5: Commit RED fixture**

```bash
git add tests/golden/native_stdlib_math/input.cd tests/golden/native_stdlib_math/ast.out tests/golden/native_stdlib_math/run.out
git commit -m "test: add native stdlib math fixture"
```

---

### Task 2: Native stdlib registry and type checking

**Files:**
- Modify: `include/NativeStdlib.hpp`
- Modify: `src/NativeStdlib.cpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/floor_wrong_arity.cd`
- Create: `tests/golden/type_errors/floor_wrong_arity.err`
- Create: `tests/golden/type_errors/floor_wrong_arity.exit`
- Create: `tests/golden/type_errors/floor_non_number_static.cd`
- Create: `tests/golden/type_errors/floor_non_number_static.err`
- Create: `tests/golden/type_errors/floor_non_number_static.exit`
- Create: `tests/golden/type_errors/ceil_non_number_static.cd`
- Create: `tests/golden/type_errors/ceil_non_number_static.err`
- Create: `tests/golden/type_errors/ceil_non_number_static.exit`
- Create: `tests/golden/type_errors/sqrt_non_number_static.cd`
- Create: `tests/golden/type_errors/sqrt_non_number_static.err`
- Create: `tests/golden/type_errors/sqrt_non_number_static.exit`
- Create: `tests/golden/type_errors/sqrt_shadowed_call_non_function.cd`
- Create: `tests/golden/type_errors/sqrt_shadowed_call_non_function.err`
- Create: `tests/golden/type_errors/sqrt_shadowed_call_non_function.exit`

- [ ] **Step 1: Add type-error fixtures**

Create `tests/golden/type_errors/floor_wrong_arity.cd`:

```cd
floor();
```

Create `tests/golden/type_errors/floor_wrong_arity.err`:

```text
Type error at 1:7: expected 1 arguments but got 0
```

Create `tests/golden/type_errors/floor_wrong_arity.exit`:

```text
1
```

Create `tests/golden/type_errors/floor_non_number_static.cd`:

```cd
floor("x");
```

Create `tests/golden/type_errors/floor_non_number_static.err`:

```text
Type error at 1:11: floor expects number, got string
```

Create `tests/golden/type_errors/floor_non_number_static.exit`:

```text
1
```

Create `tests/golden/type_errors/ceil_non_number_static.cd`:

```cd
ceil(true);
```

Create `tests/golden/type_errors/ceil_non_number_static.err`:

```text
Type error at 1:11: ceil expects number, got bool
```

Create `tests/golden/type_errors/ceil_non_number_static.exit`:

```text
1
```

Create `tests/golden/type_errors/sqrt_non_number_static.cd`:

```cd
sqrt([]);
```

Create `tests/golden/type_errors/sqrt_non_number_static.err`:

```text
Type error at 1:8: sqrt expects number, got array
```

Create `tests/golden/type_errors/sqrt_non_number_static.exit`:

```text
1
```

Create `tests/golden/type_errors/sqrt_shadowed_call_non_function.cd`:

```cd
let sqrt = 123;
sqrt(9);
```

Create `tests/golden/type_errors/sqrt_shadowed_call_non_function.err`:

```text
Type error at 2:7: can only call functions
```

Create `tests/golden/type_errors/sqrt_shadowed_call_non_function.exit`:

```text
1
```

- [ ] **Step 2: Verify RED for static fixtures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: new math type-error fixtures fail because `floor`, `ceil`, and `sqrt` are undefined; the shadowing fixture may already report `can only call functions` with a column determined by existing call diagnostics.

- [ ] **Step 3: Extend registry header**

Replace the contents of `include/NativeStdlib.hpp` with:

```cpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>

enum class NativeFunctionKind {
    Push,
    Pop,
    Floor,
    Ceil,
    Sqrt,
};

struct NativeFunctionSignature {
    const char* name;
    std::size_t arity;
    NativeFunctionKind kind;
};

const NativeFunctionSignature* findNativeStdlibFunction(const std::string& name);
bool isNativeStdlibName(const std::string& name);
std::optional<std::size_t> nativeStdlibArity(const std::string& name);
```

- [ ] **Step 4: Extend registry implementation**

Replace the native function table and lookup helpers in `src/NativeStdlib.cpp` with:

```cpp
#include "NativeStdlib.hpp"

#include <array>

namespace {

constexpr std::array<NativeFunctionSignature, 5> kNativeFunctions{{
    {"push", 2, NativeFunctionKind::Push},
    {"pop", 1, NativeFunctionKind::Pop},
    {"floor", 1, NativeFunctionKind::Floor},
    {"ceil", 1, NativeFunctionKind::Ceil},
    {"sqrt", 1, NativeFunctionKind::Sqrt},
}};

} // namespace

const NativeFunctionSignature* findNativeStdlibFunction(const std::string& name)
{
    for (const NativeFunctionSignature& function : kNativeFunctions) {
        if (name == function.name) {
            return &function;
        }
    }
    return nullptr;
}

bool isNativeStdlibName(const std::string& name)
{
    return findNativeStdlibFunction(name) != nullptr;
}

std::optional<std::size_t> nativeStdlibArity(const std::string& name)
{
    const NativeFunctionSignature* function = findNativeStdlibFunction(name);
    if (!function) {
        return std::nullopt;
    }
    return function->arity;
}
```

- [ ] **Step 5: Refactor `checkNativeStdlibCall`**

In `src/TypeChecker.cpp`, replace `TypeChecker::checkNativeStdlibCall` with:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkNativeStdlibCall(const CallExpr& expression)
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    if (!variable) {
        throw TypeError("native stdlib call missing variable callee");
    }

    const NativeFunctionSignature* function = findNativeStdlibFunction(variable->name.lexeme);
    if (!function) {
        throw TypeError(variable->name, "unknown native stdlib function `" + variable->name.lexeme + "`");
    }
    if (expression.arguments.size() != function->arity) {
        throw TypeError(expression.paren,
            "expected " + std::to_string(function->arity) + " arguments but got " + std::to_string(expression.arguments.size()));
    }

    std::vector<CheckedExpression> arguments;
    arguments.reserve(expression.arguments.size());
    for (const auto& argument : expression.arguments) {
        arguments.push_back(checkExpressionInfo(*argument));
    }

    switch (function->kind) {
    case NativeFunctionKind::Push:
        if (arguments[0].type.kind != StaticType::Unknown && arguments[0].type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "push expects array as first argument, got " + typeInfoName(arguments[0].type));
        }
        return CheckedExpression{simpleType(StaticType::Nil)};
    case NativeFunctionKind::Pop:
        if (arguments[0].type.kind != StaticType::Unknown && arguments[0].type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "pop expects array as first argument, got " + typeInfoName(arguments[0].type));
        }
        return CheckedExpression{unknownType()};
    case NativeFunctionKind::Floor:
    case NativeFunctionKind::Ceil:
    case NativeFunctionKind::Sqrt:
        if (arguments[0].type.kind != StaticType::Unknown && arguments[0].type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                std::string(function->name) + " expects number, got " + typeInfoName(arguments[0].type));
        }
        return CheckedExpression{simpleType(StaticType::Number)};
    }

    throw TypeError(variable->name, "unknown native stdlib function `" + variable->name.lexeme + "`");
}
```

- [ ] **Step 6: Build and verify static checks**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: math type-error fixtures pass after adjusting columns if current parser call-paren locations differ. `native_stdlib_math --run` still fails until runtime support is added.

- [ ] **Step 7: Correct type-error columns if needed**

If actual columns differ, update only the column numbers in these files:

```bash
$EDITOR tests/golden/type_errors/floor_wrong_arity.err \
        tests/golden/type_errors/floor_non_number_static.err \
        tests/golden/type_errors/ceil_non_number_static.err \
        tests/golden/type_errors/sqrt_non_number_static.err \
        tests/golden/type_errors/sqrt_shadowed_call_non_function.err
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all new type-error fixtures pass; success fixture still fails at runtime with an unknown native stdlib function until Task 3.

- [ ] **Step 8: Commit static checking**

```bash
git add include/NativeStdlib.hpp src/NativeStdlib.cpp src/TypeChecker.cpp \
    tests/golden/type_errors/floor_wrong_arity.* \
    tests/golden/type_errors/floor_non_number_static.* \
    tests/golden/type_errors/ceil_non_number_static.* \
    tests/golden/type_errors/sqrt_non_number_static.* \
    tests/golden/type_errors/sqrt_shadowed_call_non_function.*
git commit -m "feat: type check native stdlib math builtins"
```

---

### Task 3: C++ runtime execution for math native calls

**Files:**
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Create/refresh: `tests/golden/native_stdlib_math/ir.out`
- Create: `tests/golden/runtime_errors/floor_dynamic_non_number.cd`
- Create: `tests/golden/runtime_errors/floor_dynamic_non_number.run.err`
- Create: `tests/golden/runtime_errors/floor_dynamic_non_number.exit`
- Create: `tests/golden/runtime_errors/ceil_dynamic_non_number.cd`
- Create: `tests/golden/runtime_errors/ceil_dynamic_non_number.run.err`
- Create: `tests/golden/runtime_errors/ceil_dynamic_non_number.exit`
- Create: `tests/golden/runtime_errors/sqrt_dynamic_non_number.cd`
- Create: `tests/golden/runtime_errors/sqrt_dynamic_non_number.run.err`
- Create: `tests/golden/runtime_errors/sqrt_dynamic_non_number.exit`
- Create: `tests/golden/runtime_errors/sqrt_negative.cd`
- Create: `tests/golden/runtime_errors/sqrt_negative.run.err`
- Create: `tests/golden/runtime_errors/sqrt_negative.exit`

- [ ] **Step 1: Add runtime-error fixtures**

Create `tests/golden/runtime_errors/floor_dynamic_non_number.cd`:

```cd
fun id(x) { return x; }
floor(id("x"));
```

Create `tests/golden/runtime_errors/floor_dynamic_non_number.run.err`:

```text
Runtime error: floor expects number
```

Create `tests/golden/runtime_errors/floor_dynamic_non_number.exit`:

```text
1
```

Create `tests/golden/runtime_errors/ceil_dynamic_non_number.cd`:

```cd
fun id(x) { return x; }
ceil(id(true));
```

Create `tests/golden/runtime_errors/ceil_dynamic_non_number.run.err`:

```text
Runtime error: ceil expects number
```

Create `tests/golden/runtime_errors/ceil_dynamic_non_number.exit`:

```text
1
```

Create `tests/golden/runtime_errors/sqrt_dynamic_non_number.cd`:

```cd
fun id(x) { return x; }
sqrt(id([]));
```

Create `tests/golden/runtime_errors/sqrt_dynamic_non_number.run.err`:

```text
Runtime error: sqrt expects number
```

Create `tests/golden/runtime_errors/sqrt_dynamic_non_number.exit`:

```text
1
```

Create `tests/golden/runtime_errors/sqrt_negative.cd`:

```cd
sqrt(-1);
```

Create `tests/golden/runtime_errors/sqrt_negative.run.err`:

```text
Runtime error: sqrt expects non-negative number
```

Create `tests/golden/runtime_errors/sqrt_negative.exit`:

```text
1
```

- [ ] **Step 2: Verify RED for runtime fixtures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: runtime-error fixtures fail because `floor`, `ceil`, and `sqrt` are not implemented in `IRInterpreter::executeNativeCall` yet.

- [ ] **Step 3: Add interpreter helper declarations**

In `include/IRInterpreter.hpp`, add after `executeNativePop`:

```cpp
    Value executeNativeFloor(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeCeil(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeSqrt(const Frame& frame, const std::vector<IRRegister>& arguments);
```

- [ ] **Step 4: Dispatch math native calls**

In `src/IRInterpreter.cpp`, update `IRInterpreter::executeNativeCall` so it includes:

```cpp
    if (name == "floor") {
        return executeNativeFloor(frame, arguments);
    }
    if (name == "ceil") {
        return executeNativeCeil(frame, arguments);
    }
    if (name == "sqrt") {
        return executeNativeSqrt(frame, arguments);
    }
```

Place these checks after `push` / `pop` and before the unknown-function error.

- [ ] **Step 5: Implement C++ math helpers**

In `src/IRInterpreter.cpp`, add after `executeNativePop`:

```cpp
Value IRInterpreter::executeNativeFloor(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 1) {
        throw IRRuntimeError("floor expects 1 arguments");
    }
    const Value& value = readRegister(frame, arguments[0]);
    if (value.type() != Value::Type::Number) {
        throw IRRuntimeError("floor expects number");
    }
    return Value::number(std::floor(value.asNumber()));
}

Value IRInterpreter::executeNativeCeil(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 1) {
        throw IRRuntimeError("ceil expects 1 arguments");
    }
    const Value& value = readRegister(frame, arguments[0]);
    if (value.type() != Value::Type::Number) {
        throw IRRuntimeError("ceil expects number");
    }
    return Value::number(std::ceil(value.asNumber()));
}

Value IRInterpreter::executeNativeSqrt(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 1) {
        throw IRRuntimeError("sqrt expects 1 arguments");
    }
    const Value& value = readRegister(frame, arguments[0]);
    if (value.type() != Value::Type::Number) {
        throw IRRuntimeError("sqrt expects number");
    }
    if (value.asNumber() < 0.0) {
        throw IRRuntimeError("sqrt expects non-negative number");
    }
    return Value::number(std::sqrt(value.asNumber()));
}
```

`src/IRInterpreter.cpp` already includes `<cmath>` for array index checks, so no new include should be required.

- [ ] **Step 6: Build, refresh IR golden, and verify C++ runtime**

Run:

```bash
cmake --build build
./build/compiler_design --ir tests/golden/native_stdlib_math/input.cd > tests/golden/native_stdlib_math/ir.out
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: `native_stdlib_math --run`, math runtime-error fixtures, and all earlier tests pass. Bytecode golden for this fixture is not required until Task 4.

- [ ] **Step 7: Inspect IR contains native calls**

Run:

```bash
grep -R "native_call" tests/golden/native_stdlib_math/ir.out
```

Expected: output contains `native_call` lines for `floor`, `ceil`, and `sqrt`.

- [ ] **Step 8: Commit C++ runtime support**

```bash
git add include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/native_stdlib_math/ir.out \
    tests/golden/runtime_errors/floor_dynamic_non_number.* \
    tests/golden/runtime_errors/ceil_dynamic_non_number.* \
    tests/golden/runtime_errors/sqrt_dynamic_non_number.* \
    tests/golden/runtime_errors/sqrt_negative.*
git commit -m "feat: execute native stdlib math in IR"
```

---

### Task 4: Bytecode artifact and golden coverage

**Files:**
- Create/refresh: `tests/golden/native_stdlib_math/bytecode.out`
- Create: `tests/bytecode_artifacts/native_stdlib_math/input.cd`
- Create: `tests/bytecode_artifacts/native_stdlib_math/run.out`
- Create/refresh: `tests/bytecode_artifacts/native_stdlib_math/expected.cdbc`

- [ ] **Step 1: Add bytecode artifact input**

Create `tests/bytecode_artifacts/native_stdlib_math/input.cd`:

```cd
print floor(4.8);
print ceil(4.2);
print sqrt(25);
```

Create `tests/bytecode_artifacts/native_stdlib_math/run.out`:

```text
4
5
5
```

- [ ] **Step 2: Generate `.cdbc` artifact and bytecode golden**

Run:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/native_stdlib_math/expected.cdbc tests/bytecode_artifacts/native_stdlib_math/input.cd
./build/compiler_design --bytecode tests/golden/native_stdlib_math/input.cd > tests/golden/native_stdlib_math/bytecode.out
```

Expected: both generated files contain `native_call` instructions for math functions.

- [ ] **Step 3: Verify C++ golden and artifact dump**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: golden tests pass. Bytecode artifact tests may fail on Rust VM run for `native_stdlib_math` until Task 5 if Rust dispatch is not implemented yet; parser/dump should still accept `native_call` syntax.

- [ ] **Step 4: Inspect generated native calls**

Run:

```bash
grep -R "native_call" tests/golden/native_stdlib_math tests/bytecode_artifacts/native_stdlib_math
```

Expected: `bytecode.out`, `ir.out`, and `expected.cdbc` contain `native_call` lines for `floor`, `ceil`, or `sqrt`.

- [ ] **Step 5: Commit bytecode coverage**

```bash
git add tests/golden/native_stdlib_math/bytecode.out tests/bytecode_artifacts/native_stdlib_math
git commit -m "test: cover native stdlib math bytecode"
```

---

### Task 5: Rust VM execution for math native calls

**Files:**
- Modify: `vm-rs/src/vm.rs`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Dispatch math names in Rust VM**

In `vm-rs/src/vm.rs`, update `execute_native_call` to include:

```rust
            "floor" => self.execute_native_floor(arguments),
            "ceil" => self.execute_native_ceil(arguments),
            "sqrt" => self.execute_native_sqrt(arguments),
```

Keep existing `push` and `pop` arms unchanged.

- [ ] **Step 2: Add Rust math helper methods**

In `vm-rs/src/vm.rs`, add after `execute_native_pop`:

```rust
    fn execute_native_floor(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("floor expects 1 arguments"));
        }
        let Value::Number(value) = arguments[0] else {
            return Err(RuntimeError::new("floor expects number"));
        };
        Ok(Value::number(value.floor()))
    }

    fn execute_native_ceil(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("ceil expects 1 arguments"));
        }
        let Value::Number(value) = arguments[0] else {
            return Err(RuntimeError::new("ceil expects number"));
        };
        Ok(Value::number(value.ceil()))
    }

    fn execute_native_sqrt(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("sqrt expects 1 arguments"));
        }
        let Value::Number(value) = arguments[0] else {
            return Err(RuntimeError::new("sqrt expects number"));
        };
        if value < 0.0 {
            return Err(RuntimeError::new("sqrt expects non-negative number"));
        }
        Ok(Value::number(value.sqrt()))
    }
```

If Rust complains about moving out of `arguments[0]`, change each pattern to `let Value::Number(value) = &arguments[0] else { ... };` and dereference with `*value` in math calls.

- [ ] **Step 3: Add golden allowlist entry**

In `tests/run_rust_vm_tests.py`, add this string to `golden_allowlist` near other native stdlib cases:

```python
            "native_stdlib_math",
```

- [ ] **Step 4: Run Rust verification**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case native_stdlib_math
```

Expected: all three commands pass.

- [ ] **Step 5: Commit Rust VM support**

```bash
git add vm-rs/src/vm.rs tests/run_rust_vm_tests.py
git commit -m "feat: execute native stdlib math in Rust VM"
```

---

### Task 6: Documentation updates

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `docs/bytecode-text-format.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README**

In `README.md`, update the builtin paragraph near `len` / `push` / `pop` to include:

```markdown
The numeric native stdlib functions `floor(number)`, `ceil(number)`, and `sqrt(number)` each return a number. `sqrt` rejects negative inputs at runtime. User bindings with the same names shadow these stdlib functions.
```

- [ ] **Step 2: Update roadmap**

In `docs/roadmap.md`, update Phase 13 with:

```markdown
Status: in progress. Phase 13A is implemented: `floor(number)`, `ceil(number)`, and `sqrt(number)` are shadowable native stdlib functions using the generic `native_call` path. `len` remains supported through its legacy dedicated IR/bytecode opcode and still awaits migration if a unified builtin path becomes valuable.
```

Also mark numeric helpers as implemented:

```markdown
- Numeric helpers: `floor`, `ceil`, `sqrt`. Implemented.
```

- [ ] **Step 3: Update bytecode text docs**

In `docs/bytecode-text-format.md`, update the native-call sentence to:

```markdown
`native_call` invokes a registered VM native stdlib function by name-table reference; in this version `push`, `pop`, `floor`, `ceil`, and `sqrt` are supported.
```

- [ ] **Step 4: Update AGENTS**

In `AGENTS.md`, update current semantics with:

```markdown
- The numeric native stdlib includes shadowable `floor(number)`, `ceil(number)`, and `sqrt(number)` helpers. They return numbers; `sqrt` raises a runtime error for negative inputs. New stdlib functions should prefer the generic `native_call` path rather than bespoke opcodes; `len` remains a legacy dedicated opcode for now.
```

- [ ] **Step 5: Run docs grep check**

Run:

```bash
rg -n "floor\(|ceil\(|sqrt\(|native_call|Phase 13|len" README.md docs/roadmap.md docs/bytecode-text-format.md AGENTS.md
```

Expected: docs describe math helpers as implemented, mention `native_call` as the extension path, and keep `len` documented as legacy dedicated opcode.

- [ ] **Step 6: Commit docs**

```bash
git add README.md docs/roadmap.md docs/bytecode-text-format.md AGENTS.md
git commit -m "docs: document native stdlib math builtins"
```

---

### Task 7: Full verification and cleanup

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

Expected: worktree is clean; recent commits cover spec, plan, RED tests, static checking, C++ runtime, bytecode coverage, Rust VM support, and docs.

- [ ] **Step 3: Final report**

Report exact command results:

```text
Implemented native stdlib math builtins.
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

- Spec coverage: The plan covers registry metadata, shadowing, static checks, C++ runtime dispatch, Rust VM dispatch, bytecode artifacts, Rust VM parity, docs, and full verification. It explicitly leaves `len` migration out of scope.
- Placeholder scan: The plan uses concrete file paths, fixture contents, code snippets, commands, expected outputs, and commit messages. No placeholder implementation steps remain.
- Type consistency: Names are consistent with the existing codebase and the spec: `NativeFunctionKind`, `NativeFunctionSignature`, `findNativeStdlibFunction`, `floor`, `ceil`, `sqrt`, and `native_call`.
