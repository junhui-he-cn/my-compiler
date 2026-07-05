# Native Stdlib Array Push/Pop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a generic native stdlib call path and implement `push(xs, value)` / `pop(xs)` as shadowable implicit stdlib functions for mutable arrays.

**Architecture:** Introduce a small C++ native stdlib registry (`NativeStdlib`) for name/arity metadata and stdlib-call recognition. TypeChecker and IRCompiler use that registry for unshadowed `push`/`pop`; IR, bytecode, `.cdbc`, and Rust VM gain a generic `native_call` instruction that dispatches by name at runtime. The existing `len` fast path stays unchanged in this phase.

**Tech Stack:** C++17 compiler front end/IR/bytecode, Python golden tests, `.cdbc` text artifacts, Rust VM parser/formatter/executor.

---

## File Structure

- Create `include/NativeStdlib.hpp`, `src/NativeStdlib.cpp`: shared registry for native stdlib names and arity.
- Modify `CMakeLists.txt`: compile `src/NativeStdlib.cpp`.
- Modify `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: recognize unshadowed native stdlib calls, statically check `push`/`pop`, preserve existing `len` behavior.
- Modify `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: lower unshadowed `push`/`pop` to generic `IROp::NativeCall`.
- Modify `include/IR.hpp`, `src/IR.cpp`: add `NativeCall`, emitter, printer, and op name.
- Modify `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: dispatch native `push`/`pop` and mutate arrays in place.
- Modify `include/Bytecode.hpp`, `src/Bytecode.cpp`, `src/BytecodeCompiler.cpp`, `src/BytecodeTextEmitter.cpp`: add bytecode `NativeCall` and `.cdbc` output.
- Modify `vm-rs/src/bytecode.rs`, `vm-rs/src/format.rs`, `vm-rs/src/vm.rs`: parse/format/execute Rust `NativeCall`.
- Modify `tests/run_rust_vm_tests.py`: include `native_stdlib_push_pop` in golden allowlist.
- Add fixtures under `tests/golden/`, `tests/golden/type_errors/`, `tests/golden/runtime_errors/`, and `tests/bytecode_artifacts/`.
- Update `README.md`, `docs/bytecode-text-format.md`, `docs/roadmap.md`, `AGENTS.md`.

---

### Task 1: RED success fixture for push/pop behavior

**Files:**
- Create: `tests/golden/native_stdlib_push_pop/input.cd`
- Create: `tests/golden/native_stdlib_push_pop/ast.out`
- Create: `tests/golden/native_stdlib_push_pop/run.out`

- [ ] **Step 1: Add the success input**

Create `tests/golden/native_stdlib_push_pop/input.cd`:

```cd
let xs = [1, 2];
print push(xs, 3);
print xs;
print pop(xs);
print xs;

let alias = xs;
push(alias, 4);
print xs;

let mixed = [];
push(mixed, "hi");
push(mixed, true);
print pop(mixed);
print pop(mixed);
```

- [ ] **Step 2: Add AST expectation**

Create `tests/golden/native_stdlib_push_pop/ast.out`:

```text
Program
  Let xs = (array 1 2)
  Print (call push xs 3)
  Print xs
  Print (call pop xs)
  Print xs
  Let alias = xs
  Expr (call push alias 4)
  Print xs
  Let mixed = (array)
  Expr (call push mixed "hi")
  Expr (call push mixed true)
  Print (call pop mixed)
  Print (call pop mixed)
```

- [ ] **Step 3: Add run expectation**

Create `tests/golden/native_stdlib_push_pop/run.out`:

```text
nil
[1, 2, 3]
3
[1, 2]
[1, 2, 4]
true
hi
```

- [ ] **Step 4: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: `native_stdlib_push_pop` fails because unbound `push`/`pop` are currently undefined variables.

- [ ] **Step 5: Commit RED fixture**

```bash
git add tests/golden/native_stdlib_push_pop/input.cd tests/golden/native_stdlib_push_pop/ast.out tests/golden/native_stdlib_push_pop/run.out
git commit -m "test: add native stdlib push pop fixture"
```

---

### Task 2: Native stdlib registry and static checking

**Files:**
- Create: `include/NativeStdlib.hpp`
- Create: `src/NativeStdlib.cpp`
- Modify: `CMakeLists.txt`
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/push_wrong_arity.cd`
- Create: `tests/golden/type_errors/push_wrong_arity.err`
- Create: `tests/golden/type_errors/push_wrong_arity.exit`
- Create: `tests/golden/type_errors/push_non_array_static.cd`
- Create: `tests/golden/type_errors/push_non_array_static.err`
- Create: `tests/golden/type_errors/push_non_array_static.exit`
- Create: `tests/golden/type_errors/pop_wrong_arity.cd`
- Create: `tests/golden/type_errors/pop_wrong_arity.err`
- Create: `tests/golden/type_errors/pop_wrong_arity.exit`
- Create: `tests/golden/type_errors/pop_non_array_static.cd`
- Create: `tests/golden/type_errors/pop_non_array_static.err`
- Create: `tests/golden/type_errors/pop_non_array_static.exit`
- Create: `tests/golden/type_errors/push_shadowed_call_non_function.cd`
- Create: `tests/golden/type_errors/push_shadowed_call_non_function.err`
- Create: `tests/golden/type_errors/push_shadowed_call_non_function.exit`

- [ ] **Step 1: Add type-error fixtures**

Create `tests/golden/type_errors/push_wrong_arity.cd`:

```cd
push([]);
```

Create `tests/golden/type_errors/push_wrong_arity.err`:

```text
Type error at 1:5: expected 2 arguments but got 1
```

Create `tests/golden/type_errors/push_wrong_arity.exit`:

```text
1
```

Create `tests/golden/type_errors/push_non_array_static.cd`:

```cd
push(123, 1);
```

Create `tests/golden/type_errors/push_non_array_static.err`:

```text
Type error at 1:5: push expects array as first argument, got number
```

Create `tests/golden/type_errors/push_non_array_static.exit`:

```text
1
```

Create `tests/golden/type_errors/pop_wrong_arity.cd`:

```cd
pop();
```

Create `tests/golden/type_errors/pop_wrong_arity.err`:

```text
Type error at 1:4: expected 1 arguments but got 0
```

Create `tests/golden/type_errors/pop_wrong_arity.exit`:

```text
1
```

Create `tests/golden/type_errors/pop_non_array_static.cd`:

```cd
pop("x");
```

Create `tests/golden/type_errors/pop_non_array_static.err`:

```text
Type error at 1:4: pop expects array as first argument, got string
```

Create `tests/golden/type_errors/pop_non_array_static.exit`:

```text
1
```

Create `tests/golden/type_errors/push_shadowed_call_non_function.cd`:

```cd
let push = 123;
push([], 1);
```

Create `tests/golden/type_errors/push_shadowed_call_non_function.err`:

```text
Type error at 2:5: can only call functions
```

Create `tests/golden/type_errors/push_shadowed_call_non_function.exit`:

```text
1
```

- [ ] **Step 2: Create registry header**

Create `include/NativeStdlib.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>

struct NativeFunctionSignature {
    const char* name;
    std::size_t arity;
};

bool isNativeStdlibName(const std::string& name);
std::optional<std::size_t> nativeStdlibArity(const std::string& name);
```

- [ ] **Step 3: Create registry implementation**

Create `src/NativeStdlib.cpp`:

```cpp
#include "NativeStdlib.hpp"

#include <array>

namespace {

constexpr std::array<NativeFunctionSignature, 2> kNativeFunctions{{
    {"push", 2},
    {"pop", 1},
}};

} // namespace

bool isNativeStdlibName(const std::string& name)
{
    return nativeStdlibArity(name).has_value();
}

std::optional<std::size_t> nativeStdlibArity(const std::string& name)
{
    for (const NativeFunctionSignature& function : kNativeFunctions) {
        if (name == function.name) {
            return function.arity;
        }
    }
    return std::nullopt;
}
```

- [ ] **Step 4: Add registry source to build**

In `CMakeLists.txt`, add `src/NativeStdlib.cpp` to `add_executable` after `src/Lexer.cpp`:

```cmake
    src/NativeStdlib.cpp
```

- [ ] **Step 5: Declare TypeChecker helpers**

In `include/TypeChecker.hpp`, add:

```cpp
    bool isNativeStdlibCall(const CallExpr& expression) const;
    CheckedExpression checkNativeStdlibCall(const CallExpr& expression);
```

near the existing `isBuiltinLenCall` / `checkBuiltinLenCall` helpers.

- [ ] **Step 6: Include registry in TypeChecker**

In `src/TypeChecker.cpp`, add:

```cpp
#include "NativeStdlib.hpp"
```

- [ ] **Step 7: Implement native call recognition and checking**

In `src/TypeChecker.cpp`, add after `checkBuiltinLenCall`:

```cpp
bool TypeChecker::isNativeStdlibCall(const CallExpr& expression) const
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    return variable && isNativeStdlibName(variable->name.lexeme) && findVariable(variable->name.lexeme) == nullptr;
}

TypeChecker::CheckedExpression TypeChecker::checkNativeStdlibCall(const CallExpr& expression)
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    if (!variable) {
        throw TypeError("native stdlib call missing variable callee");
    }

    const std::optional<std::size_t> arity = nativeStdlibArity(variable->name.lexeme);
    if (!arity) {
        throw TypeError(variable->name, "unknown native stdlib function `" + variable->name.lexeme + "`");
    }
    if (expression.arguments.size() != *arity) {
        throw TypeError(expression.paren,
            "expected " + std::to_string(*arity) + " arguments but got " + std::to_string(expression.arguments.size()));
    }

    std::vector<CheckedExpression> arguments;
    arguments.reserve(expression.arguments.size());
    for (const auto& argument : expression.arguments) {
        arguments.push_back(checkExpressionInfo(*argument));
    }

    if (variable->name.lexeme == "push") {
        if (arguments[0].type.kind != StaticType::Unknown && arguments[0].type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "push expects array as first argument, got " + typeInfoName(arguments[0].type));
        }
        return CheckedExpression{simpleType(StaticType::Nil)};
    }

    if (variable->name.lexeme == "pop") {
        if (arguments[0].type.kind != StaticType::Unknown && arguments[0].type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "pop expects array as first argument, got " + typeInfoName(arguments[0].type));
        }
        return CheckedExpression{unknownType()};
    }

    throw TypeError(variable->name, "unknown native stdlib function `" + variable->name.lexeme + "`");
}
```

- [ ] **Step 8: Dispatch native stdlib before normal calls**

In `TypeChecker::checkCall`, keep existing `len` first, then add:

```cpp
    if (isNativeStdlibCall(expression)) {
        return checkNativeStdlibCall(expression);
    }
```

before checking the callee as a normal expression.

- [ ] **Step 9: Build and verify static checks**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: new type-error fixtures pass except possible column adjustments. Success fixture still fails at compile/runtime until IR native call support lands.

- [ ] **Step 10: Correct diagnostic columns if needed**

If any `.err` column differs, update only the column to match actual output, then rerun:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all static type-error fixtures pass.

- [ ] **Step 11: Commit static checking**

```bash
git add CMakeLists.txt include/NativeStdlib.hpp src/NativeStdlib.cpp include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/push_wrong_arity.* tests/golden/type_errors/push_non_array_static.* tests/golden/type_errors/pop_wrong_arity.* tests/golden/type_errors/pop_non_array_static.* tests/golden/type_errors/push_shadowed_call_non_function.*
git commit -m "feat: type check native stdlib push pop"
```

---

### Task 3: Add IR native_call and C++ runtime execution

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Create/refresh: `tests/golden/native_stdlib_push_pop/ir.out`
- Create: `tests/golden/runtime_errors/push_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/push_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/push_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/pop_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/pop_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/pop_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/pop_empty_array.cd`
- Create: `tests/golden/runtime_errors/pop_empty_array.run.err`
- Create: `tests/golden/runtime_errors/pop_empty_array.exit`

- [ ] **Step 1: Add runtime-error fixtures**

Create `tests/golden/runtime_errors/push_dynamic_non_array.cd`:

```cd
fun id(x) { return x; }
push(id(123), 1);
```

Create `tests/golden/runtime_errors/push_dynamic_non_array.run.err`:

```text
Runtime error: push expects array as first argument
```

Create `tests/golden/runtime_errors/push_dynamic_non_array.exit`:

```text
1
```

Create `tests/golden/runtime_errors/pop_dynamic_non_array.cd`:

```cd
fun id(x) { return x; }
pop(id("x"));
```

Create `tests/golden/runtime_errors/pop_dynamic_non_array.run.err`:

```text
Runtime error: pop expects array as first argument
```

Create `tests/golden/runtime_errors/pop_dynamic_non_array.exit`:

```text
1
```

Create `tests/golden/runtime_errors/pop_empty_array.cd`:

```cd
pop([]);
```

Create `tests/golden/runtime_errors/pop_empty_array.run.err`:

```text
Runtime error: cannot pop from empty array
```

Create `tests/golden/runtime_errors/pop_empty_array.exit`:

```text
1
```

- [ ] **Step 2: Add IR opcode and emitter declaration**

In `include/IR.hpp`, add `NativeCall` after `Call`:

```cpp
    Call,
    NativeCall,
    Index,
```

Add to `IRProgram` after `emitCall`:

```cpp
    IRRegister emitNativeCall(std::string name, std::vector<IRRegister> arguments);
```

- [ ] **Step 3: Print and name `native_call` in IR**

In `src/IR.cpp`, add `IROp::NativeCall` to the non-binary cases in `isBinary`.

Add a `printInstruction` branch after `IROp::Call`:

```cpp
    } else if (instruction.op == IROp::NativeCall) {
        out << " @" << instruction.operand;
        if (instruction.operand < program.names().size()) {
            out << " " << program.names()[instruction.operand];
        }
        out << "(";
        for (std::size_t arg = 0; arg < instruction.arguments.size(); ++arg) {
            if (arg != 0) {
                out << ", ";
            }
            out << instruction.arguments[arg];
        }
        out << ")";
```

Add to `irOpName`:

```cpp
    case IROp::NativeCall:
        return "native_call";
```

- [ ] **Step 4: Implement IR emitter**

In `src/IR.cpp`, add after `IRProgram::emitCall`:

```cpp
IRRegister IRProgram::emitNativeCall(std::string name, std::vector<IRRegister> arguments)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::NativeCall, dest, std::nullopt, std::nullopt, std::move(arguments), addName(std::move(name))});
    return dest;
}
```

- [ ] **Step 5: Lower native calls from IRCompiler**

In `include/IRCompiler.hpp`, include `NativeStdlib.hpp` is not necessary; add private helpers near `emitCall`:

```cpp
    bool isNativeStdlibCall(const CallExpr& expression) const;
    IRRegister emitNativeStdlibCall(const CallExpr& expression);
```

In `src/IRCompiler.cpp`, add `#include "NativeStdlib.hpp"`.

Add helpers near `isBuiltinLenCall`:

```cpp
bool IRCompiler::isNativeStdlibCall(const CallExpr& expression) const
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    return variable && isNativeStdlibName(variable->name.lexeme) && !resolvedNames_->hasVariable(*variable);
}

IRRegister IRCompiler::emitNativeStdlibCall(const CallExpr& expression)
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    if (!variable) {
        throw IRCompileError("native stdlib call missing variable callee");
    }

    std::vector<IRRegister> arguments;
    for (const auto& argument : expression.arguments) {
        arguments.push_back(compileExpression(*argument));
    }
    return ir_.emitNativeCall(variable->name.lexeme, std::move(arguments));
}
```

In `IRCompiler::emitCall`, keep `len` first, then add:

```cpp
    if (isNativeStdlibCall(expression)) {
        return emitNativeStdlibCall(expression);
    }
```

- [ ] **Step 6: Add interpreter declarations**

In `include/IRInterpreter.hpp`, add after `executeLen` or near other helpers:

```cpp
    Value executeNativeCall(const IRProgram& program, const Frame& frame, std::size_t nameIndex, const std::vector<IRRegister>& arguments);
    Value executeNativePush(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativePop(const Frame& frame, const std::vector<IRRegister>& arguments);
```

- [ ] **Step 7: Dispatch native calls in interpreter**

In `src/IRInterpreter.cpp`, add a switch case after `IROp::Call`:

```cpp
        case IROp::NativeCall:
            writeRegister(frame, readDest(instruction), executeNativeCall(program, frame, instruction.operand, instruction.arguments));
            break;
```

- [ ] **Step 8: Implement native runtime helpers**

In `src/IRInterpreter.cpp`, add after `executeLen` or before it:

```cpp
Value IRInterpreter::executeNativeCall(
    const IRProgram& program,
    const Frame& frame,
    std::size_t nameIndex,
    const std::vector<IRRegister>& arguments)
{
    const std::string name = readName(program, nameIndex);
    if (name == "push") {
        return executeNativePush(frame, arguments);
    }
    if (name == "pop") {
        return executeNativePop(frame, arguments);
    }
    throw IRRuntimeError("unknown native stdlib function `" + name + "`");
}

Value IRInterpreter::executeNativePush(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 2) {
        throw IRRuntimeError("push expects 2 arguments");
    }
    const Value& arrayValue = readRegister(frame, arguments[0]);
    if (arrayValue.type() != Value::Type::Array) {
        throw IRRuntimeError("push expects array as first argument");
    }
    arrayValue.asArray().elements->push_back(readRegister(frame, arguments[1]));
    return Value::nil();
}

Value IRInterpreter::executeNativePop(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 1) {
        throw IRRuntimeError("pop expects 1 arguments");
    }
    const Value& arrayValue = readRegister(frame, arguments[0]);
    if (arrayValue.type() != Value::Type::Array) {
        throw IRRuntimeError("pop expects array as first argument");
    }
    auto& elements = *arrayValue.asArray().elements;
    if (elements.empty()) {
        throw IRRuntimeError("cannot pop from empty array");
    }
    Value result = elements.back();
    elements.pop_back();
    return result;
}
```

- [ ] **Step 9: Build, update IR golden, and verify C++ runtime**

Run:

```bash
cmake --build build
./build/compiler_design --ir tests/golden/native_stdlib_push_pop/input.cd > tests/golden/native_stdlib_push_pop/ir.out
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: success fixture C++ run and runtime-error fixtures pass. Bytecode output is not required yet unless `bytecode.out` exists.

- [ ] **Step 10: Commit IR/runtime support**

```bash
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/native_stdlib_push_pop/ir.out tests/golden/runtime_errors/push_dynamic_non_array.* tests/golden/runtime_errors/pop_dynamic_non_array.* tests/golden/runtime_errors/pop_empty_array.*
git commit -m "feat: execute native stdlib push pop in IR"
```

---

### Task 4: Add bytecode and `.cdbc` native_call support

**Files:**
- Modify: `include/Bytecode.hpp`
- Modify: `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `src/BytecodeTextEmitter.cpp`
- Create/refresh: `tests/golden/native_stdlib_push_pop/bytecode.out`
- Create: `tests/bytecode_artifacts/native_stdlib_push_pop/input.cd`
- Create: `tests/bytecode_artifacts/native_stdlib_push_pop/run.out`
- Create/refresh: `tests/bytecode_artifacts/native_stdlib_push_pop/expected.cdbc`

- [ ] **Step 1: Add bytecode opcode**

In `include/Bytecode.hpp`, add `NativeCall` after `Call`:

```cpp
    Call,
    NativeCall,
    Index,
```

- [ ] **Step 2: Print and name bytecode `native_call`**

In `src/Bytecode.cpp`, add `BytecodeOp::NativeCall` to the non-binary cases in `isBinary`.

Add after the `BytecodeOp::Call` printer block:

```cpp
    } else if (instruction.op == BytecodeOp::NativeCall) {
        printNameOperand(out, program, instruction.operand);
        out << " [";
        for (std::size_t arg = 0; arg < instruction.arguments.size(); ++arg) {
            if (arg != 0) {
                out << ", ";
            }
            out << instruction.arguments[arg];
        }
        out << "]";
```

Add to `bytecodeOpName`:

```cpp
    case BytecodeOp::NativeCall:
        return "native_call";
```

- [ ] **Step 3: Lower IR NativeCall to bytecode**

In `src/BytecodeCompiler.cpp`, add:

```cpp
    case IROp::NativeCall:
        return BytecodeOp::NativeCall;
```

near the `IROp::Call` mapping. The existing generic lowering preserves destination, operand name index, and argument registers.

- [ ] **Step 4: Emit `.cdbc` native_call**

In `src/BytecodeTextEmitter.cpp`, add after `BytecodeOp::Call`:

```cpp
    case BytecodeOp::NativeCall:
        out << reg(requireDest(instruction)) << " = native_call " << nameRef(instruction.operand) << ' ';
        writeRegisterList(out, instruction.arguments);
        break;
```

- [ ] **Step 5: Add bytecode artifact fixture**

Create `tests/bytecode_artifacts/native_stdlib_push_pop/input.cd`:

```cd
let xs = [1, 2];
push(xs, 3);
print xs;
print pop(xs);
```

Create `tests/bytecode_artifacts/native_stdlib_push_pop/run.out`:

```text
[1, 2, 3]
3
```

Generate artifact:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/native_stdlib_push_pop/expected.cdbc tests/bytecode_artifacts/native_stdlib_push_pop/input.cd
```

Expected: `expected.cdbc` contains `native_call` instructions for `push` and `pop`.

- [ ] **Step 6: Refresh bytecode golden and verify C++ bytecode artifacts fail only in Rust parser if Rust is not done**

Run:

```bash
cmake --build build
./build/compiler_design --bytecode tests/golden/native_stdlib_push_pop/input.cd > tests/golden/native_stdlib_push_pop/bytecode.out
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: golden tests pass. Bytecode artifact tests may fail on Rust dump with `unknown opcode native_call` until Task 5.

- [ ] **Step 7: Commit C++ bytecode support**

```bash
git add include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp src/BytecodeTextEmitter.cpp tests/golden/native_stdlib_push_pop/bytecode.out tests/bytecode_artifacts/native_stdlib_push_pop
git commit -m "feat: lower native stdlib calls to bytecode"
```

---

### Task 5: Add Rust VM native_call parser, formatter, and execution

**Files:**
- Modify: `vm-rs/src/bytecode.rs`
- Modify: `vm-rs/src/format.rs`
- Modify: `vm-rs/src/vm.rs`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Add Rust instruction variant**

In `vm-rs/src/bytecode.rs`, add after `Call`:

```rust
    NativeCall {
        dest: usize,
        name: usize,
        arguments: Vec<usize>,
    },
```

- [ ] **Step 2: Parse `native_call`**

In `vm-rs/src/format.rs`, add after the `call` parser branch:

```rust
            "native_call" => {
                let (name, args) = split_once(line, operands, " ")?;
                Ok(Instruction::NativeCall {
                    dest,
                    name: parse_name_ref(line, name)?,
                    arguments: parse_register_list(line, args)?,
                })
            }
```

- [ ] **Step 3: Format `native_call`**

In `format_instruction`, add after `Instruction::Call`:

```rust
        Instruction::NativeCall {
            dest,
            name,
            arguments,
        } => format!(
            "r{} = native_call n{} {}",
            dest,
            name,
            format_register_list(arguments)
        ),
```

- [ ] **Step 4: Execute native calls**

In `vm-rs/src/vm.rs`, add an execution arm after `Instruction::Call`:

```rust
                Instruction::NativeCall {
                    dest,
                    name,
                    arguments,
                } => {
                    let name = self.read_name(*name)?;
                    let mut values = Vec::with_capacity(arguments.len());
                    for argument in arguments {
                        values.push(self.read_register(frame, *argument)?);
                    }
                    let result = self.execute_native_call(&name, values)?;
                    self.write_register(frame, *dest, result)?;
                }
```

Add helper methods near `execute_len`:

```rust
    fn execute_native_call(
        &self,
        name: &str,
        arguments: Vec<Value>,
    ) -> Result<Value, RuntimeError> {
        match name {
            "push" => self.execute_native_push(arguments),
            "pop" => self.execute_native_pop(arguments),
            _ => Err(RuntimeError::new(format!("unknown native stdlib function `{}`", name))),
        }
    }

    fn execute_native_push(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("push expects 2 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("push expects array as first argument"));
        };
        array.elements.borrow_mut().push(arguments[1].clone());
        Ok(Value::Nil)
    }

    fn execute_native_pop(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("pop expects 1 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("pop expects array as first argument"));
        };
        array
            .elements
            .borrow_mut()
            .pop()
            .ok_or_else(|| RuntimeError::new("cannot pop from empty array"))
    }
```

- [ ] **Step 5: Add golden allowlist**

In `tests/run_rust_vm_tests.py`, add `"native_stdlib_push_pop",` to `golden_allowlist`.

- [ ] **Step 6: Run Rust verification**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case native_stdlib_push_pop
```

Expected: all three commands pass.

- [ ] **Step 7: Commit Rust VM support**

```bash
git add vm-rs/src/bytecode.rs vm-rs/src/format.rs vm-rs/src/vm.rs tests/run_rust_vm_tests.py
git commit -m "feat: execute native stdlib calls in Rust VM"
```

---

### Task 6: Add final shadowing/runtime/parity coverage polish

**Files:**
- Create/modify: `tests/golden/native_stdlib_push_pop/ir.out`
- Create/modify: `tests/golden/native_stdlib_push_pop/bytecode.out`
- Create/modify: runtime/type fixtures from earlier tasks if exact output changed

- [ ] **Step 1: Verify all new tests together**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case native_stdlib_push_pop
```

Expected: all pass. If any golden differs only because of stable generated IR/bytecode register numbering, refresh with explicit compiler commands for only `native_stdlib_push_pop`:

```bash
./build/compiler_design --ir tests/golden/native_stdlib_push_pop/input.cd > tests/golden/native_stdlib_push_pop/ir.out
./build/compiler_design --bytecode tests/golden/native_stdlib_push_pop/input.cd > tests/golden/native_stdlib_push_pop/bytecode.out
```

Then rerun the three verification commands above.

- [ ] **Step 2: Inspect generated native_call output**

Run:

```bash
grep -R "native_call" tests/golden/native_stdlib_push_pop tests/bytecode_artifacts/native_stdlib_push_pop
```

Expected: `ir.out`, `bytecode.out`, and `expected.cdbc` contain `native_call` for `push` and `pop`.

- [ ] **Step 3: Commit coverage polish if any files changed**

If Step 1 or Step 2 changed files, commit them:

```bash
git add tests/golden/native_stdlib_push_pop tests/bytecode_artifacts/native_stdlib_push_pop tests/golden/runtime_errors/push_dynamic_non_array.* tests/golden/runtime_errors/pop_dynamic_non_array.* tests/golden/runtime_errors/pop_empty_array.* tests/golden/type_errors/push_* tests/golden/type_errors/pop_*
git commit -m "test: verify native stdlib push pop coverage"
```

If no files changed, do not create an empty commit.

---

### Task 7: Documentation updates

**Files:**
- Modify: `README.md`
- Modify: `docs/bytecode-text-format.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README**

In `README.md`, add a paragraph near `len` or array mutation docs:

```markdown
The native stdlib functions `push(array, value)` and `pop(array)` mutate arrays in place. `push` appends a value and returns `nil`; `pop` removes and returns the last value. Arrays are reference values, so aliases observe length changes. Calling `pop([])` is a runtime error. User bindings named `push` or `pop` shadow the stdlib functions, matching `len` shadowing behavior.
```

- [ ] **Step 2: Update bytecode text docs**

In `docs/bytecode-text-format.md`, add `native_call` to opcode list near `call`.

Add instruction form near call docs:

```text
rD = native_call nName [rArg0, rArg1, ...]
```

Add one sentence:

```markdown
`native_call` invokes a registered VM native stdlib function by name-table reference; in this version `push` and `pop` are supported.
```

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, update Phase 10 status:

```markdown
Phase 10C is implemented: `push(array, value)` and `pop(array)` are shadowable native stdlib functions backed by a generic `native_call` IR/bytecode path. `push` mutates in place and returns `nil`; `pop` mutates in place and returns the removed value.
```

Also update Phase 13 to mention that a native stdlib foundation now exists and `len` still awaits migration.

- [ ] **Step 4: Update AGENTS**

In `AGENTS.md`, update current semantics:

```markdown
- The native stdlib currently includes shadowable `push(array, value)` and `pop(array)`. `push` mutates arrays in place and returns nil; `pop` mutates arrays in place and returns the removed value, with runtime error on empty arrays. New stdlib functions should prefer the generic native_call path rather than bespoke opcodes; `len` remains a legacy dedicated opcode for now.
```

- [ ] **Step 5: Run docs grep checks**

Run:

```bash
rg -n "push\(|pop\(|native_call|Len|len" README.md docs/bytecode-text-format.md docs/roadmap.md AGENTS.md
```

Expected: docs describe `push/pop` as implemented, `native_call` as the stdlib extension path, and `len` as still supported.

- [ ] **Step 6: Commit docs**

```bash
git add README.md docs/bytecode-text-format.md docs/roadmap.md AGENTS.md
git commit -m "docs: document native stdlib push pop"
```

---

### Task 8: Full verification and cleanup

**Files:**
- No source edits expected.
- Remove: `tests/__pycache__/` if created.

- [ ] **Step 1: Run full verification**

Run from repo root:

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
git log --oneline -8
```

Expected: worktree is clean; recent commits cover RED tests, native stdlib registry/type checking, IR runtime, bytecode, Rust VM, docs, and this plan/spec.

- [ ] **Step 3: Final report**

Report exact command results:

```text
Implemented native stdlib push/pop.
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

- Spec coverage: Tasks implement native stdlib registry, shadowing, static checking, IR `native_call`, C++ runtime mutation, bytecode and `.cdbc`, Rust VM parser/formatter/executor, success/type/runtime/parity tests, and docs.
- Placeholder scan: The plan includes exact file paths, fixture contents, commands, expected outputs, and concrete code snippets; no placeholder implementation steps remain.
- Type consistency: Names are consistent across tasks: `NativeStdlib`, `NativeFunctionSignature`, `isNativeStdlibName`, `nativeStdlibArity`, `isNativeStdlibCall`, `checkNativeStdlibCall`, `IROp::NativeCall`, `BytecodeOp::NativeCall`, and Rust `Instruction::NativeCall`.
