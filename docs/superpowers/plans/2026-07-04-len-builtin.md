# `len` Builtin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `len(value)` as a builtin for arrays and strings, with static checks, IR interpreter support, and bytecode VM parity.

**Architecture:** Treat `len` as an implicit builtin only when no lexical binding named `len` exists. TypeChecker recognizes unbound `len(...)` calls directly and returns `number`; IRCompiler lowers those calls to a new `IROp::Len`; BytecodeCompiler lowers that to `BytecodeOp::Len`; both runtime engines implement the same array/string length behavior and runtime error for unsupported values. Parser, AST, and grammar remain unchanged.

**Tech Stack:** C++17, existing AST/TypeChecker/IR/Bytecode pipeline, Python golden tests.

---

## File Structure

Modify:

- `include/TypeChecker.hpp` — add `ResolvedNames::hasVariable()` and TypeChecker helpers for builtin `len` recognition/checking.
- `src/TypeChecker.cpp` — implement `hasVariable()`, static checking for unbound builtin `len`, argument count/type diagnostics, and `number` call result.
- `include/IR.hpp` — add `IROp::Len` and `IRProgram::emitLen()`.
- `src/IR.cpp` — print and name `len`, and emit `Len` instructions.
- `include/IRCompiler.hpp` — add `isBuiltinLenCall()` / `emitLenCall()` declarations if keeping helpers as class methods.
- `src/IRCompiler.cpp` — lower unshadowed `len(argument)` calls to `IROp::Len` instead of normal function calls.
- `include/IRInterpreter.hpp` — declare `executeLen()`.
- `src/IRInterpreter.cpp` — execute `IROp::Len` for arrays and strings.
- `include/Bytecode.hpp` — add `BytecodeOp::Len`.
- `src/Bytecode.cpp` — print and name `len` bytecode instructions.
- `src/BytecodeCompiler.cpp` — lower `IROp::Len` to `BytecodeOp::Len`.
- `include/BytecodeVM.hpp` — declare `executeLen()`.
- `src/BytecodeVM.cpp` — execute `BytecodeOp::Len` for arrays and strings.
- `README.md` — document `len(array|string)` and shadowing.
- `docs/roadmap.md` — mark Phase 10A implemented after code lands.
- `AGENTS.md` — update current language semantics for builtin `len`.

Create success fixtures:

- `tests/golden/len_builtin/input.cd`
- `tests/golden/len_builtin/ast.out`
- `tests/golden/len_builtin/ir.out`
- `tests/golden/len_builtin/run.out`
- `tests/golden/len_builtin/run_bytecode.out`
- `tests/golden/len_builtin_shadowing/input.cd`
- `tests/golden/len_builtin_shadowing/run.out`
- `tests/golden/len_builtin_shadowing/run_bytecode.out`

Create type-error fixtures:

- `tests/golden/type_errors/len_wrong_arity_zero.cd`
- `tests/golden/type_errors/len_wrong_arity_zero.err`
- `tests/golden/type_errors/len_wrong_arity_zero.exit`
- `tests/golden/type_errors/len_wrong_arity_two.cd`
- `tests/golden/type_errors/len_wrong_arity_two.err`
- `tests/golden/type_errors/len_wrong_arity_two.exit`
- `tests/golden/type_errors/len_static_invalid_type.cd`
- `tests/golden/type_errors/len_static_invalid_type.err`
- `tests/golden/type_errors/len_static_invalid_type.exit`
- `tests/golden/type_errors/len_result_assignment_mismatch.cd`
- `tests/golden/type_errors/len_result_assignment_mismatch.err`
- `tests/golden/type_errors/len_result_assignment_mismatch.exit`
- `tests/golden/type_errors/len_shadowed_call_non_function.cd`
- `tests/golden/type_errors/len_shadowed_call_non_function.err`
- `tests/golden/type_errors/len_shadowed_call_non_function.exit`

Create runtime-error fixture:

- `tests/golden/runtime_errors/len_dynamic_invalid_type.cd`
- `tests/golden/runtime_errors/len_dynamic_invalid_type.run.err`
- `tests/golden/runtime_errors/len_dynamic_invalid_type.exit`
- `tests/golden/runtime_errors/len_dynamic_invalid_type.run_bytecode.err`
- `tests/golden/runtime_errors/len_dynamic_invalid_type.run_bytecode.exit`

Do not modify:

- `include/Ast.hpp`, `src/Ast.cpp`
- `include/Parser.hpp`, `src/Parser.cpp`
- `docs/language-grammar.ebnf`
- `include/Value.hpp`, `src/Value.cpp` unless existing `Value` APIs are insufficient; they should be sufficient.

Reference:

- `docs/superpowers/specs/2026-07-04-len-builtin-design.md`
- `src/TypeChecker.cpp::checkCall()`
- `src/IRCompiler.cpp::emitCall()`
- `src/IR.cpp::printInstruction()` and `irOpName()`
- `src/IRInterpreter.cpp::executeIndex()` for runtime type-check style
- `src/Bytecode.cpp::printInstruction()` and `bytecodeOpName()`
- `src/BytecodeVM.cpp::executeIndex()` for bytecode runtime type-check style
- `tests/golden/array_literal/` and `tests/golden/runtime_errors/index_*` for array fixture conventions

---

### Task 1: Add Failing Golden Coverage for `len`

**Files:**
- Create success fixtures under `tests/golden/len_builtin/` and `tests/golden/len_builtin_shadowing/`
- Create type-error fixtures under `tests/golden/type_errors/`
- Create runtime-error fixture under `tests/golden/runtime_errors/`

- [ ] **Step 1: Add main `len` success fixture source and expected run outputs**

Create `tests/golden/len_builtin/input.cd`:

```cd
print len([]);
print len([1, 2, 3]);
print len("hello");
let n = len([true, "x", nil]);
print n + 1;
```

Create `tests/golden/len_builtin/run.out`:

```text
0
3
5
4
```

Create `tests/golden/len_builtin/run_bytecode.out` with the same content:

```text
0
3
5
4
```

Create `tests/golden/len_builtin/ast.out`:

```text
Program
  Print (call len (array))
  Print (call len (array 1 2 3))
  Print (call len "hello")
  Let n = (call len (array true "x" nil))
  Print (+ n 1)
```

Create `tests/golden/len_builtin/ir.out` with the intended final IR output:

```text
IR
0000  v0 = array []
0001  v1 = len v0
0002  print v1
0003  v2 = constant #0 1
0004  v3 = constant #1 2
0005  v4 = constant #2 3
0006  v5 = array [v2, v3, v4]
0007  v6 = len v5
0008  print v6
0009  v7 = constant #3 "hello"
0010  v8 = len v7
0011  print v8
0012  v9 = constant #4 true
0013  v10 = constant #5 "x"
0014  v11 = constant #6 nil
0015  v12 = array [v9, v10, v11]
0016  v13 = len v12
0017  store_var @0 n#0, v13
0018  v14 = load_var @1 n#0
0019  v15 = constant #7 1
0020  v16 = add v14, v15
0021  print v16
```

If implementation details produce different constant/name numbering, run `python3 tests/run_golden_tests.py ./build/compiler_demo --update` after implementation and review only this new `ir.out` before committing.

- [ ] **Step 2: Add shadowing success fixture**

Create `tests/golden/len_builtin_shadowing/input.cd`:

```cd
let len = 123;
print len;
```

Create `tests/golden/len_builtin_shadowing/run.out`:

```text
123
```

Create `tests/golden/len_builtin_shadowing/run_bytecode.out` with the same content:

```text
123
```

This fixture proves a user binding named `len` remains usable as a normal variable.

- [ ] **Step 3: Add wrong-arity type-error fixtures**

Create `tests/golden/type_errors/len_wrong_arity_zero.cd`:

```cd
print len();
```

Create `tests/golden/type_errors/len_wrong_arity_zero.err`:

```text
Type error at 1:11: expected 1 arguments but got 0
```

Create `tests/golden/type_errors/len_wrong_arity_zero.exit`:

```text
1
```

Create `tests/golden/type_errors/len_wrong_arity_two.cd`:

```cd
print len([1], [2]);
```

Create `tests/golden/type_errors/len_wrong_arity_two.err`:

```text
Type error at 1:19: expected 1 arguments but got 2
```

Create `tests/golden/type_errors/len_wrong_arity_two.exit`:

```text
1
```

If final columns differ, update only these new `.err` files after verifying the actual location is the call closing parenthesis.

- [ ] **Step 4: Add static invalid type and result type-error fixtures**

Create `tests/golden/type_errors/len_static_invalid_type.cd`:

```cd
print len(123);
```

Create `tests/golden/type_errors/len_static_invalid_type.err`:

```text
Type error at 1:14: len expects array or string, got number
```

Create `tests/golden/type_errors/len_static_invalid_type.exit`:

```text
1
```

Create `tests/golden/type_errors/len_result_assignment_mismatch.cd`:

```cd
let n = len([1]);
n = "bad";
```

Create `tests/golden/type_errors/len_result_assignment_mismatch.err`:

```text
Type error at 2:1: cannot assign string to `n` of type number
```

Create `tests/golden/type_errors/len_result_assignment_mismatch.exit`:

```text
1
```

- [ ] **Step 5: Add shadowed-call type-error fixture**

Create `tests/golden/type_errors/len_shadowed_call_non_function.cd`:

```cd
let len = 123;
print len([1]);
```

Create `tests/golden/type_errors/len_shadowed_call_non_function.err`:

```text
Type error at 2:14: can only call functions
```

Create `tests/golden/type_errors/len_shadowed_call_non_function.exit`:

```text
1
```

This fixture proves shadowed `len` calls do not use builtin behavior.

- [ ] **Step 6: Add dynamic runtime invalid type fixture**

Create `tests/golden/runtime_errors/len_dynamic_invalid_type.cd`:

```cd
fun id(x) {
  return x;
}
print len(id(123));
```

Create `tests/golden/runtime_errors/len_dynamic_invalid_type.run.err`:

```text
Runtime error: len expects array or string
```

Create `tests/golden/runtime_errors/len_dynamic_invalid_type.exit`:

```text
1
```

Create `tests/golden/runtime_errors/len_dynamic_invalid_type.run_bytecode.err`:

```text
Runtime error: len expects array or string
```

Create `tests/golden/runtime_errors/len_dynamic_invalid_type.run_bytecode.exit`:

```text
1
```

- [ ] **Step 7: Run golden tests and observe expected failures**

Run:

```bash
if [ ! -x ./build/compiler_demo ]; then cmake -S . -B build && cmake --build build; fi
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected before implementation: new `len` fixtures fail. The exact failure mode may include parse/type errors for undefined `len`, missing IR output, runtime errors, or stdout mismatches. Existing fixtures should keep their previous behavior.

- [ ] **Step 8: Commit failing fixtures**

Run:

```bash
git add tests/golden/len_builtin tests/golden/len_builtin_shadowing \
        tests/golden/type_errors/len_wrong_arity_zero.* \
        tests/golden/type_errors/len_wrong_arity_two.* \
        tests/golden/type_errors/len_static_invalid_type.* \
        tests/golden/type_errors/len_result_assignment_mismatch.* \
        tests/golden/type_errors/len_shadowed_call_non_function.* \
        tests/golden/runtime_errors/len_dynamic_invalid_type.*
git commit -m "test: cover len builtin behavior"
```

---

### Task 2: Add Static `len` Recognition in TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add `ResolvedNames::hasVariable()` declaration**

In `include/TypeChecker.hpp`, add this public method next to `variableName()`:

```cpp
    bool hasVariable(const VariableExpr& expression) const;
```

- [ ] **Step 2: Add builtin helper declarations to TypeChecker**

In the private section of `TypeChecker`, add these declarations near `checkCall()`:

```cpp
    bool isBuiltinLenCall(const CallExpr& expression) const;
    CheckedExpression checkBuiltinLenCall(const CallExpr& expression);
```

- [ ] **Step 3: Implement `ResolvedNames::hasVariable()`**

In `src/TypeChecker.cpp`, after `ResolvedNames::variableName()` or before it, add:

```cpp
bool ResolvedNames::hasVariable(const VariableExpr& expression) const
{
    return variableNames_.find(&expression) != variableNames_.end();
}
```

Keep `variableName()` unchanged so normal variable loads still fail loudly if resolution is missing.

- [ ] **Step 4: Implement `isBuiltinLenCall()`**

In `src/TypeChecker.cpp`, before `checkCall()`, add:

```cpp
bool TypeChecker::isBuiltinLenCall(const CallExpr& expression) const
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    return variable && variable->name.lexeme == "len" && findVariable("len") == nullptr;
}
```

This uses the current lexical scopes to ensure user bindings shadow the builtin.

- [ ] **Step 5: Implement `checkBuiltinLenCall()`**

In `src/TypeChecker.cpp`, after `isBuiltinLenCall()`, add:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkBuiltinLenCall(const CallExpr& expression)
{
    if (expression.arguments.size() != 1) {
        throw TypeError(expression.paren, "expected 1 arguments but got " + std::to_string(expression.arguments.size()));
    }

    const CheckedExpression argument = checkExpressionInfo(*expression.arguments.front());
    if (isKnown(argument.type) && argument.type != StaticType::Array && argument.type != StaticType::String) {
        throw TypeError(expression.paren, "len expects array or string, got " + staticTypeName(argument.type));
    }

    return CheckedExpression{StaticType::Number, std::nullopt, StaticType::Unknown};
}
```

This allows `Unknown` arguments through for runtime checking.

- [ ] **Step 6: Route `checkCall()` through builtin path first**

At the start of `TypeChecker::checkCall()`, before checking the callee expression, add:

```cpp
    if (isBuiltinLenCall(expression)) {
        return checkBuiltinLenCall(expression);
    }
```

The rest of `checkCall()` remains unchanged so shadowed `len` calls behave like normal calls.

- [ ] **Step 7: Build and run golden tests for static behavior**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected at this point:

- Type-error fixtures for `len` arity/type/result mismatch pass, except possible column corrections.
- Runtime and success `len` fixtures still fail because IR/bytecode lowering is not implemented yet.
- If type-error columns differ, update only the new `.err` files to match actual diagnostics.

Do not commit yet if success/runtime failures remain from missing backend implementation; continue to Task 3.

---

### Task 3: Add `Len` to IR and IR Interpreter

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`

- [ ] **Step 1: Add `IROp::Len` and `emitLen()` declaration**

In `include/IR.hpp`, add `Len` after `Index` in `enum class IROp`:

```cpp
    Call,
    Index,
    Len,
    Print,
```

Add this public method after `emitIndex()`:

```cpp
    IRRegister emitLen(IRRegister value);
```

- [ ] **Step 2: Update IR op classification, printing, and naming**

In `src/IR.cpp`, add `IROp::Len` to the non-binary cases in `isBinary()`:

```cpp
    case IROp::Len:
```

In `printInstruction()`, add a branch after `Index`:

```cpp
    } else if (instruction.op == IROp::Len) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
```

Add `IRProgram::emitLen()` after `emitIndex()`:

```cpp
IRRegister IRProgram::emitLen(IRRegister value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Len, dest, value, std::nullopt, {}, 0});
    return dest;
}
```

In `irOpName()`, add:

```cpp
    case IROp::Len:
        return "len";
```

- [ ] **Step 3: Add IRCompiler helper declarations**

In `include/IRCompiler.hpp`, add these private declarations near `emitCall()`:

```cpp
    bool isBuiltinLenCall(const CallExpr& expression) const;
    IRRegister emitLenCall(const CallExpr& expression);
```

- [ ] **Step 4: Implement IRCompiler builtin detection and lowering**

In `src/IRCompiler.cpp`, add this helper before `IRCompiler::emitCall()`:

```cpp
bool IRCompiler::isBuiltinLenCall(const CallExpr& expression) const
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    return variable && variable->name.lexeme == "len" && !resolvedNames_->hasVariable(*variable);
}
```

Add this helper after it:

```cpp
IRRegister IRCompiler::emitLenCall(const CallExpr& expression)
{
    if (expression.arguments.size() != 1) {
        throw IRCompileError("len expects exactly one argument");
    }
    const IRRegister value = compileExpression(*expression.arguments.front());
    return ir_.emitLen(value);
}
```

At the start of `IRCompiler::emitCall()`, add:

```cpp
    if (isBuiltinLenCall(expression)) {
        return emitLenCall(expression);
    }
```

This relies on TypeChecker leaving builtin `len` callee variables unresolved and resolving shadowed `len` variables normally.

- [ ] **Step 5: Add IRInterpreter len execution declaration**

In `include/IRInterpreter.hpp`, add this private method after `executeIndex()`:

```cpp
    Value executeLen(const Frame& frame, IRRegister value);
```

- [ ] **Step 6: Add IRInterpreter runtime behavior**

In `src/IRInterpreter.cpp`, add a switch case after `IROp::Index`:

```cpp
        case IROp::Len:
            writeRegister(frame, readDest(instruction), executeLen(frame, readLeft(instruction)));
            break;
```

Add this helper after `executeIndex()`:

```cpp
Value IRInterpreter::executeLen(const Frame& frame, IRRegister value)
{
    const Value& input = readRegister(frame, value);
    if (input.type() == Value::Type::Array) {
        return Value::number(static_cast<double>(input.asArray().elements->size()));
    }
    if (input.type() == Value::Type::String) {
        return Value::number(static_cast<double>(input.asString().size()));
    }
    throw IRRuntimeError("len expects array or string");
}
```

- [ ] **Step 7: Build and run golden tests for IR path**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected at this point:

- `--run` success and runtime-error behavior for `len` passes.
- `ir.out` may need update if constant/name numbering differs from the expected fixture.
- `--run-bytecode` still fails until Task 4 adds bytecode support.
- If only `tests/golden/len_builtin/ir.out` differs, update that file using actual output and review it.

Do not commit yet if bytecode failures remain; continue to Task 4.

---

### Task 4: Add `Len` to Bytecode and Bytecode VM

**Files:**
- Modify: `include/Bytecode.hpp`
- Modify: `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `include/BytecodeVM.hpp`
- Modify: `src/BytecodeVM.cpp`

- [ ] **Step 1: Add `BytecodeOp::Len`**

In `include/Bytecode.hpp`, add `Len` after `Index`:

```cpp
    Call,
    Index,
    Len,
    Print,
```

- [ ] **Step 2: Update bytecode printing and naming**

In `src/Bytecode.cpp`, add `BytecodeOp::Len` to the non-binary cases in `isBinary()`:

```cpp
    case BytecodeOp::Len:
```

In `printInstruction()`, add a branch after `Index`:

```cpp
    } else if (instruction.op == BytecodeOp::Len) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
```

In `bytecodeOpName()`, add:

```cpp
    case BytecodeOp::Len:
        return "len";
```

- [ ] **Step 3: Lower IR Len to bytecode Len**

In `src/BytecodeCompiler.cpp`, add this case to `lowerOp()` after `IROp::Index`:

```cpp
    case IROp::Len:
        return BytecodeOp::Len;
```

- [ ] **Step 4: Add BytecodeVM len declaration**

In `include/BytecodeVM.hpp`, add this private method after `executeIndex()`:

```cpp
    Value executeLen(const VMFrame& frame, BytecodeRegister value);
```

- [ ] **Step 5: Add BytecodeVM runtime behavior**

In `src/BytecodeVM.cpp`, add a switch case after `BytecodeOp::Index`:

```cpp
        case BytecodeOp::Len:
            writeRegister(frame, readDest(instruction), executeLen(frame, readLeft(instruction)));
            break;
```

Add this helper after `executeIndex()`:

```cpp
Value BytecodeVM::executeLen(const VMFrame& frame, BytecodeRegister value)
{
    const Value& input = readRegister(frame, value);
    if (input.type() == Value::Type::Array) {
        return Value::number(static_cast<double>(input.asArray().elements->size()));
    }
    if (input.type() == Value::Type::String) {
        return Value::number(static_cast<double>(input.asString().size()));
    }
    throw BytecodeRuntimeError("len expects array or string");
}
```

- [ ] **Step 6: Build and run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected after Task 4:

- All new `len` success, type-error, and runtime-error fixtures pass except possible `.err` column or `ir.out` differences.
- Existing tests pass.

If output differs only for intentionally changed new golden files, refresh them with:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Then review the updated files:

```bash
git diff -- tests/golden/len_builtin tests/golden/type_errors/len_*.err tests/golden/runtime_errors/len_dynamic_invalid_type.*
```

- [ ] **Step 7: Commit implementation and corrected goldens**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp \
        include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp \
        include/IRInterpreter.hpp src/IRInterpreter.cpp \
        include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp \
        include/BytecodeVM.hpp src/BytecodeVM.cpp \
        tests/golden/len_builtin/ir.out \
        tests/golden/type_errors/len_wrong_arity_zero.err \
        tests/golden/type_errors/len_wrong_arity_two.err \
        tests/golden/type_errors/len_static_invalid_type.err \
        tests/golden/type_errors/len_result_assignment_mismatch.err \
        tests/golden/type_errors/len_shadowed_call_non_function.err \
        tests/golden/runtime_errors/len_dynamic_invalid_type.run.err \
        tests/golden/runtime_errors/len_dynamic_invalid_type.run_bytecode.err
git commit -m "feat: add len builtin"
```

Only include golden files that actually changed due to intentional output or column corrections.

---

### Task 5: Update Documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README with `len` builtin**

In `README.md`, after the paragraph describing arrays or before the supported expressions list, add this paragraph:

```markdown
The builtin `len(value)` returns a number for arrays and strings. `len([1, 2, 3])` returns `3`, and `len("hello")` returns `5` using the current runtime string byte length. Statically known non-array and non-string arguments are type errors; unknown arguments are checked at runtime. A user binding named `len` shadows the builtin in its lexical scope.
```

- [ ] **Step 2: Update roadmap Phase 10 status**

In `docs/roadmap.md`, under `## Phase 10: Array Mutation and Collection Builtins`, add this status line immediately after the heading:

```markdown
Status: in progress. Phase 10A is implemented: `len(value)` returns array element counts or string byte lengths with IR and bytecode parity. Index assignment and array mutation helpers remain future work.
```

In the Recommended split list, replace:

```markdown
- Phase 10A: `len` builtin as a small usability slice.
```

with:

```markdown
- Phase 10A: `len` builtin as a small usability slice. Implemented.
```

- [ ] **Step 3: Update AGENTS current semantics**

In `AGENTS.md`, after the arrays bullet, add:

```markdown
- The builtin `len(value)` returns array element counts or string byte lengths as a number. User bindings named `len` shadow the builtin; unknown argument types are checked at runtime.
```

- [ ] **Step 4: Verify docs mention builtin behavior**

Run:

```bash
grep -n "builtin `len(value)`" README.md
grep -n "Phase 10A is implemented" docs/roadmap.md
grep -n "builtin `len(value)`" AGENTS.md
```

Expected: each command prints one matching line.

- [ ] **Step 5: Commit docs**

Run:

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document len builtin"
```

---

### Task 6: Full Verification and Final Review Prep

**Files:**
- Verify all changed files.

- [ ] **Step 1: Run full verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

- CMake configure and build succeed.
- `ctest` reports all tests passed.
- Golden tests report all checks passed.
- Golden runner selftests report OK.

- [ ] **Step 2: Review final diff scope**

Run:

```bash
git diff --stat abfe6cd..HEAD
git diff --name-status abfe6cd..HEAD
git status --short --branch
```

Expected:

- Changes include TypeChecker, IR, IRCompiler, IRInterpreter, Bytecode, BytecodeCompiler, BytecodeVM, new `len` fixtures, README, roadmap, AGENTS, and the design/plan docs.
- No parser, AST, grammar, or runtime `Value` representation changes.
- Worktree is clean.

---

## Self-Review Checklist

- Spec coverage:
  - `len(array)` and `len(string)` success behavior: Task 1, Task 3, Task 4.
  - Static arity checks: Task 1, Task 2.
  - Static known invalid type checks: Task 1, Task 2.
  - `len` result type is `number`: Task 1, Task 2.
  - Unknown argument type allowed statically and rejected at runtime if invalid: Task 1, Task 3, Task 4.
  - User shadowing of `len`: Task 1, Task 2, Task 3.
  - IR op and interpreter support: Task 3.
  - Bytecode op and VM support: Task 4.
  - Docs updated: Task 5.
- No parser, AST, grammar, or runtime `Value` representation changes are planned.
- Full verification command is included.
