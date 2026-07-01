# Compile-Time Scope Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move lexical scope lookup from IR runtime execution into the compiler/type-checker pass, using compiler-generated unique variable names in IR.

**Architecture:** Extend `TypeChecker` to produce `ResolvedNames` for declarations, reads, and assignments. Change `IRCompiler` to consume `ResolvedNames`, remove `BeginScope`/`EndScope` from IR, and simplify `IRInterpreter` back to a flat defensive variable map.

**Tech Stack:** C++17, CMake, AST side-table resolution, register IR, Python golden tests, CTest.

---

## File Structure

- Modify: `include/TypeChecker.hpp` — add `ResolvedNames`, `Binding`, resolved-name counter, and change `check()` return type.
- Modify: `src/TypeChecker.cpp` — resolve names, reject undefined reads/assignments, record unique names.
- Modify: `include/IRCompiler.hpp` — include `TypeChecker.hpp`, require `ResolvedNames` in `compile()`.
- Modify: `src/IRCompiler.cpp` — use resolved names and stop emitting scope boundaries.
- Modify: `src/main.cpp` — pass resolved names from `TypeChecker` to `IRCompiler`.
- Modify: `include/IR.hpp` and `src/IR.cpp` — remove `BeginScope`, `EndScope`, and emitters.
- Modify: `include/IRInterpreter.hpp` and `src/IRInterpreter.cpp` — replace runtime scope stack with flat map.
- Move fixtures from `tests/golden/runtime_errors` to `tests/golden/type_errors`: `undefined_variable`, `assign_undefined`, `block_local_escape`.
- Modify: affected `tests/golden/**/*.ir.out` after intentional IR name/scope output changes.
- Modify: `README.md`, `AGENTS.md`, `docs/roadmap.md` — document compile-time scope resolution.

## Task 0: Prepare Workspace and Baseline

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
compile-time-scope-resolution
```

If the user explicitly authorizes working on the current branch instead, record that authorization in the final report.

- [ ] **Step 2: Run baseline verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected current baseline:

```text
100% tests passed, 0 tests failed out of 2
golden tests: 54 passed, 0 failed
Ran 9 tests
OK
```

If baseline fails before scope-resolution edits, stop and report the failure.

## Task 1: Add Red Compile-Time Scope Error Fixtures

**Files:**
- Delete: `tests/golden/runtime_errors/undefined_variable.cd`, `.run.err`, `.exit`
- Delete: `tests/golden/runtime_errors/assign_undefined.cd`, `.run.err`, `.exit`
- Delete: `tests/golden/runtime_errors/block_local_escape.cd`, `.run.err`, `.exit`
- Create: `tests/golden/type_errors/undefined_variable.cd`, `.err`, `.exit`
- Create: `tests/golden/type_errors/assign_undefined.cd`, `.err`, `.exit`
- Create: `tests/golden/type_errors/block_local_escape.cd`, `.err`, `.exit`

- [ ] **Step 1: Move undefined read to type errors**

Run:

```bash
cat > tests/golden/type_errors/undefined_variable.cd <<'CASE'
print missing;
CASE
cat > tests/golden/type_errors/undefined_variable.err <<'CASE'
Type error: undefined variable `missing`
CASE
cat > tests/golden/type_errors/undefined_variable.exit <<'CASE'
1
CASE
rm tests/golden/runtime_errors/undefined_variable.cd \
   tests/golden/runtime_errors/undefined_variable.run.err \
   tests/golden/runtime_errors/undefined_variable.exit
```

Expected: undefined reads are represented as type errors.

- [ ] **Step 2: Move undefined assignment to type errors**

Run:

```bash
cat > tests/golden/type_errors/assign_undefined.cd <<'CASE'
missing = 1;
CASE
cat > tests/golden/type_errors/assign_undefined.err <<'CASE'
Type error: undefined variable `missing`
CASE
cat > tests/golden/type_errors/assign_undefined.exit <<'CASE'
1
CASE
rm tests/golden/runtime_errors/assign_undefined.cd \
   tests/golden/runtime_errors/assign_undefined.run.err \
   tests/golden/runtime_errors/assign_undefined.exit
```

Expected: undefined assignments are represented as type errors.

- [ ] **Step 3: Move block-local escape to type errors**

Run:

```bash
cat > tests/golden/type_errors/block_local_escape.cd <<'CASE'
{
  let x = 1;
}
print x;
CASE
cat > tests/golden/type_errors/block_local_escape.err <<'CASE'
Type error: undefined variable `x`
CASE
cat > tests/golden/type_errors/block_local_escape.exit <<'CASE'
1
CASE
rm tests/golden/runtime_errors/block_local_escape.cd \
   tests/golden/runtime_errors/block_local_escape.run.err \
   tests/golden/runtime_errors/block_local_escape.exit
```

Expected: block-local escape is represented as a type error.

- [ ] **Step 4: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: the new type-error fixtures fail because current `TypeChecker` still treats unresolved variables as `unknown`; failures include unexpected stdout and exit-code mismatches for `type_errors/undefined_variable`, `type_errors/assign_undefined`, and `type_errors/block_local_escape`.

- [ ] **Step 5: Commit red fixture migration**

Run:

```bash
git add tests/golden/runtime_errors tests/golden/type_errors
git commit -m "test: move scope errors to type goldens"
```

Expected: commit succeeds with fixture moves/additions only.

## Task 2: Add ResolvedNames to TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Replace TypeChecker header with resolver-aware declarations**

Run:

```bash
cat > include/TypeChecker.hpp <<'TYPECHECKER_HPP'
#pragma once

#include "Ast.hpp"
#include "Token.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class StaticType {
    Unknown,
    Nil,
    Number,
    Bool,
    String,
};

class TypeError final : public std::runtime_error {
public:
    explicit TypeError(const std::string& message);
};

class ResolvedNames {
public:
    const std::string& letName(const LetStmt& statement) const;
    const std::string& variableName(const VariableExpr& expression) const;
    const std::string& assignmentName(const AssignExpr& expression) const;

private:
    friend class TypeChecker;

    void clear();
    void recordLet(const LetStmt& statement, std::string name);
    void recordVariable(const VariableExpr& expression, std::string name);
    void recordAssignment(const AssignExpr& expression, std::string name);

    std::unordered_map<const LetStmt*, std::string> letNames_;
    std::unordered_map<const VariableExpr*, std::string> variableNames_;
    std::unordered_map<const AssignExpr*, std::string> assignmentNames_;
};

class TypeChecker {
public:
    const ResolvedNames& check(const Program& program);

private:
    struct Binding {
        StaticType type;
        std::string resolvedName;
    };

    using Scope = std::unordered_map<std::string, Binding>;

    void beginScope();
    void endScope();
    Scope& currentScope();
    const Scope& currentScope() const;
    Binding* findVariable(const std::string& name);
    const Binding* findVariable(const std::string& name) const;
    Binding declareVariable(const LetStmt& statement, StaticType type);
    std::string makeResolvedName(const std::string& sourceName);

    void checkStatement(const Stmt& statement);
    StaticType checkExpression(const Expr& expression);
    StaticType checkLetInitializer(const LetStmt& statement);
    StaticType resolveAnnotation(const Token& typeName) const;
    void checkAssignable(const std::string& context, StaticType expected, StaticType actual) const;
    StaticType checkUnary(const UnaryExpr& expression);
    StaticType checkBinary(const BinaryExpr& expression);

    std::vector<Scope> scopes_;
    ResolvedNames resolvedNames_;
    std::size_t nextResolvedName_ = 0;
};

std::string staticTypeName(StaticType type);
TYPECHECKER_HPP
```

Expected: `TypeChecker::check()` now returns `const ResolvedNames&`, and scopes store resolved bindings.

- [ ] **Step 2: Add ResolvedNames implementations**

In `src/TypeChecker.cpp`, add `#include <stdexcept>` after the existing includes if it is not already included:

```cpp
#include <stdexcept>
```

Add these methods after `staticTypeName`:

```cpp
const std::string& ResolvedNames::letName(const LetStmt& statement) const
{
    const auto found = letNames_.find(&statement);
    if (found == letNames_.end()) {
        throw std::logic_error("missing resolved let name");
    }
    return found->second;
}

const std::string& ResolvedNames::variableName(const VariableExpr& expression) const
{
    const auto found = variableNames_.find(&expression);
    if (found == variableNames_.end()) {
        throw std::logic_error("missing resolved variable name");
    }
    return found->second;
}

const std::string& ResolvedNames::assignmentName(const AssignExpr& expression) const
{
    const auto found = assignmentNames_.find(&expression);
    if (found == assignmentNames_.end()) {
        throw std::logic_error("missing resolved assignment name");
    }
    return found->second;
}

void ResolvedNames::clear()
{
    letNames_.clear();
    variableNames_.clear();
    assignmentNames_.clear();
}

void ResolvedNames::recordLet(const LetStmt& statement, std::string name)
{
    letNames_.emplace(&statement, std::move(name));
}

void ResolvedNames::recordVariable(const VariableExpr& expression, std::string name)
{
    variableNames_.emplace(&expression, std::move(name));
}

void ResolvedNames::recordAssignment(const AssignExpr& expression, std::string name)
{
    assignmentNames_.emplace(&expression, std::move(name));
}
```

Expected: resolved-name lookups fail loudly if IR compilation uses an unchecked AST.

- [ ] **Step 3: Change `check()` to reset and return resolved names**

Replace `void TypeChecker::check(const Program& program)` implementation with:

```cpp
const ResolvedNames& TypeChecker::check(const Program& program)
{
    scopes_.clear();
    resolvedNames_.clear();
    nextResolvedName_ = 0;
    beginScope();
    for (const auto& statement : program.statements) {
        checkStatement(*statement);
    }
    endScope();
    return resolvedNames_;
}
```

Expected: a successful check returns the resolved side table.

- [ ] **Step 4: Update variable lookup return types**

Replace both `findVariable` implementations with:

```cpp
TypeChecker::Binding* TypeChecker::findVariable(const std::string& name)
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

const TypeChecker::Binding* TypeChecker::findVariable(const std::string& name) const
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}
```

Expected: lookup returns type plus resolved name.

- [ ] **Step 5: Replace declaration helper and add unique-name helper**

Replace `declareVariable` with:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(const LetStmt& statement, StaticType type)
{
    auto& scope = currentScope();
    if (scope.find(statement.name.lexeme) != scope.end()) {
        throw TypeError("variable `" + statement.name.lexeme + "` already declared in this scope");
    }

    Binding binding{type, makeResolvedName(statement.name.lexeme)};
    resolvedNames_.recordLet(statement, binding.resolvedName);
    scope.emplace(statement.name.lexeme, binding);
    return binding;
}

std::string TypeChecker::makeResolvedName(const std::string& sourceName)
{
    return sourceName + "#" + std::to_string(nextResolvedName_++);
}
```

Expected: every `let` gets a stable unique internal name.

- [ ] **Step 6: Update let statement checking to call the new declaration helper**

In `checkStatement`, replace:

```cpp
        const StaticType declared = checkLetInitializer(*let);
        declareVariable(let->name, declared);
```

with:

```cpp
        const StaticType declared = checkLetInitializer(*let);
        declareVariable(*let, declared);
```

Expected: let declarations are recorded in `ResolvedNames`.

- [ ] **Step 7: Update variable expression checking to reject undefined reads**

Replace the `VariableExpr` branch in `checkExpression` with:

```cpp
    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        const Binding* binding = findVariable(variable->name.lexeme);
        if (!binding) {
            throw TypeError("undefined variable `" + variable->name.lexeme + "`");
        }
        resolvedNames_.recordVariable(*variable, binding->resolvedName);
        return binding->type;
    }
```

Expected: undefined reads are type errors.

- [ ] **Step 8: Update assignment expression checking to reject undefined assignments and record resolved target**

Replace the `AssignExpr` branch in `checkExpression` with:

```cpp
    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const StaticType value = checkExpression(*assign->value);
        Binding* target = findVariable(assign->name.lexeme);
        if (!target) {
            throw TypeError("undefined variable `" + assign->name.lexeme + "`");
        }
        if (isKnown(target->type) && isKnown(value) && target->type != value) {
            throw TypeError("cannot assign " + staticTypeName(value) + " to `" + assign->name.lexeme
                + "` of type " + staticTypeName(target->type));
        }
        resolvedNames_.recordAssignment(*assign, target->resolvedName);
        return isKnown(target->type) ? target->type : value;
    }
```

Expected: undefined assignments are type errors and assignment targets get resolved names.

- [ ] **Step 9: Build and run golden tests to verify migrated scope errors pass**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: migrated type-error fixtures pass, but build may fail until `main.cpp` and `IRCompiler` are updated for the changed `check()` signature. If build fails only because `TypeChecker::check` now returns a value that is ignored, continue to Task 3.

## Task 3: Make IRCompiler Use Resolved Names

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Update `include/IRCompiler.hpp`**

Run:

```bash
cat > include/IRCompiler.hpp <<'IRCOMPILER_HPP'
#pragma once

#include "Ast.hpp"
#include "IR.hpp"
#include "TypeChecker.hpp"

#include <stdexcept>
#include <string>

class IRCompileError final : public std::runtime_error {
public:
    explicit IRCompileError(const std::string& message);
};

class IRCompiler {
public:
    IRProgram compile(const Program& program, const ResolvedNames& resolvedNames);

private:
    void compileStatement(const Stmt& statement);
    IRRegister compileExpression(const Expr& expression);
    IRRegister emitUnary(TokenType op, IRRegister value);
    IRRegister emitBinary(TokenType op, IRRegister left, IRRegister right);

    IRProgram ir_;
    const ResolvedNames* resolvedNames_ = nullptr;
};
IRCOMPILER_HPP
```

Expected: `IRCompiler` requires resolved names.

- [ ] **Step 2: Update `IRCompiler::compile`**

In `src/IRCompiler.cpp`, replace:

```cpp
IRProgram IRCompiler::compile(const Program& program)
{
    ir_ = IRProgram();
    for (const auto& statement : program.statements) {
        compileStatement(*statement);
    }
    return std::move(ir_);
}
```

with:

```cpp
IRProgram IRCompiler::compile(const Program& program, const ResolvedNames& resolvedNames)
{
    ir_ = IRProgram();
    resolvedNames_ = &resolvedNames;
    for (const auto& statement : program.statements) {
        compileStatement(*statement);
    }
    resolvedNames_ = nullptr;
    return std::move(ir_);
}
```

Expected: compiler stores resolved names while compiling.

- [ ] **Step 3: Remove block scope emission from IRCompiler**

In `compileStatement`, replace the block branch:

```cpp
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        ir_.emitBeginScope();
        for (const auto& child : block->statements) {
            compileStatement(*child);
        }
        ir_.emitEndScope();
        return;
    }
```

with:

```cpp
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        for (const auto& child : block->statements) {
            compileStatement(*child);
        }
        return;
    }
```

Expected: compiler no longer emits runtime scope operations.

- [ ] **Step 4: Use resolved let names**

In the `LetStmt` branch, replace:

```cpp
        ir_.emitStoreVar(let->name.lexeme, value);
```

with:

```cpp
        ir_.emitStoreVar(resolvedNames_->letName(*let), value);
```

Expected: declarations use unique internal names.

- [ ] **Step 5: Use resolved variable names**

In `compileExpression`, replace:

```cpp
        return ir_.emitLoadVar(variable->name.lexeme);
```

with:

```cpp
        return ir_.emitLoadVar(resolvedNames_->variableName(*variable));
```

Expected: reads use resolved nearest binding names.

- [ ] **Step 6: Use resolved assignment names**

In `compileExpression`, replace:

```cpp
        ir_.emitAssignVar(assign->name.lexeme, value);
```

with:

```cpp
        ir_.emitAssignVar(resolvedNames_->assignmentName(*assign), value);
```

Expected: assignments update resolved nearest binding names.

- [ ] **Step 7: Pass resolved names from `main.cpp`**

In `src/main.cpp`, replace:

```cpp
        TypeChecker typeChecker;
        typeChecker.check(program);
```

with:

```cpp
        TypeChecker typeChecker;
        const ResolvedNames& resolvedNames = typeChecker.check(program);
```

Then replace:

```cpp
            IRProgram ir = compiler.compile(program);
```

with:

```cpp
            IRProgram ir = compiler.compile(program, resolvedNames);
```

Expected: the CLI passes resolver output into IR compilation.

- [ ] **Step 8: Build and run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: build succeeds. Golden failures are expected for IR output because names now include `#N` and scope boundary instructions may still print until IR ops are removed in Task 4.

## Task 4: Remove Runtime Scope Operations from IR and Interpreter

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`

- [ ] **Step 1: Remove scope opcodes and emitters from `include/IR.hpp`**

Edit `include/IR.hpp` to remove these enum entries:

```cpp
    BeginScope,
    EndScope,
```

Also remove these declarations:

```cpp
    void emitBeginScope();
    void emitEndScope();
```

Expected: public IR no longer exposes scope boundary instructions.

- [ ] **Step 2: Remove scope cases and emitters from `src/IR.cpp`**

In `isBinary`, remove these false cases:

```cpp
    case IROp::BeginScope:
    case IROp::EndScope:
```

Delete the implementations:

```cpp
void IRProgram::emitBeginScope()
{
    emit(IRInstruction{IROp::BeginScope, std::nullopt, std::nullopt, std::nullopt, 0});
}

void IRProgram::emitEndScope()
{
    emit(IRInstruction{IROp::EndScope, std::nullopt, std::nullopt, std::nullopt, 0});
}
```

In `irOpName`, delete:

```cpp
    case IROp::BeginScope:
        return "begin_scope";
    case IROp::EndScope:
        return "end_scope";
```

Expected: IR.cpp no longer references scope opcodes.

- [ ] **Step 3: Replace `IRInterpreter.hpp` with flat-map declarations**

Run:

```bash
cat > include/IRInterpreter.hpp <<'IRINTERPRETER_HPP'
#pragma once

#include "IR.hpp"

#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class IRRuntimeError final : public std::runtime_error {
public:
    explicit IRRuntimeError(const std::string& message);
};

class IRInterpreter {
public:
    explicit IRInterpreter(std::ostream& output);

    void execute(const IRProgram& program);
    const std::unordered_map<std::string, Value>& globals() const;

private:
    Value readConstant(const IRProgram& program, std::size_t index) const;
    std::string readName(const IRProgram& program, std::size_t index) const;
    IRRegister readDest(const IRInstruction& instruction) const;
    IRRegister readLeft(const IRInstruction& instruction) const;
    IRRegister readRight(const IRInstruction& instruction) const;
    const Value& readRegister(IRRegister reg) const;
    void writeRegister(IRRegister reg, Value value);

    Value executeUnaryNumber(const std::string& opName, IRRegister value, Value (*operation)(double));
    Value executeBinaryNumber(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeBinaryComparison(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeAdd(IRRegister left, IRRegister right);

    std::ostream& output_;
    std::vector<Value> registers_;
    std::unordered_map<std::string, Value> globals_;
};
IRINTERPRETER_HPP
```

Expected: interpreter header has no scope stack helpers.

- [ ] **Step 4: Simplify interpreter constructor and execution reset**

In `src/IRInterpreter.cpp`, replace the constructor:

```cpp
IRInterpreter::IRInterpreter(std::ostream& output)
    : output_(output)
    , scopes_(1)
{
}
```

with:

```cpp
IRInterpreter::IRInterpreter(std::ostream& output)
    : output_(output)
{
}
```

Replace execution reset:

```cpp
    registers_.assign(program.registerCount(), Value::nil());
    scopes_.clear();
    scopes_.emplace_back();
```

with:

```cpp
    registers_.assign(program.registerCount(), Value::nil());
    globals_.clear();
```

Expected: interpreter starts each run with an empty flat map.

- [ ] **Step 5: Replace variable operation switch cases with flat-map logic**

In `IRInterpreter::execute`, replace `LoadVar`, `StoreVar`, `AssignVar`, `BeginScope`, and `EndScope` cases with:

```cpp
        case IROp::LoadVar: {
            const std::string name = readName(program, instruction.operand);
            const auto found = globals_.find(name);
            if (found == globals_.end()) {
                throw IRRuntimeError("undefined variable `" + name + "`");
            }
            writeRegister(readDest(instruction), found->second);
            break;
        }
        case IROp::StoreVar: {
            const std::string name = readName(program, instruction.operand);
            globals_.insert_or_assign(name, readRegister(readLeft(instruction)));
            break;
        }
        case IROp::AssignVar: {
            const std::string name = readName(program, instruction.operand);
            auto found = globals_.find(name);
            if (found == globals_.end()) {
                throw IRRuntimeError("undefined variable `" + name + "`");
            }
            found->second = readRegister(readLeft(instruction));
            break;
        }
```

Expected: runtime uses flat internal names.

- [ ] **Step 6: Replace globals() implementation**

Replace:

```cpp
const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    if (scopes_.empty()) {
        throw IRRuntimeError("scope stack is empty");
    }
    return scopes_.front();
}
```

with:

```cpp
const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    return globals_;
}
```

Expected: public API returns flat map.

- [ ] **Step 7: Delete scope helper implementations from `src/IRInterpreter.cpp`**

Delete these functions completely:

```cpp
std::unordered_map<std::string, Value>& IRInterpreter::currentScope()
const std::unordered_map<std::string, Value>& IRInterpreter::currentScope() const
std::unordered_map<std::string, Value>* IRInterpreter::findScopeContaining(const std::string& name)
const std::unordered_map<std::string, Value>* IRInterpreter::findScopeContaining(const std::string& name) const
const Value& IRInterpreter::loadVariable(const std::string& name) const
void IRInterpreter::declareVariable(const std::string& name, Value value)
void IRInterpreter::assignVariable(const std::string& name, Value value)
```

Expected: no `scopes_`, `currentScope`, `findScopeContaining`, `loadVariable`, `declareVariable`, or `assignVariable` references remain.

- [ ] **Step 8: Verify scope symbols are gone**

Run:

```bash
grep -R "BeginScope\|EndScope\|emitBeginScope\|emitEndScope\|scopes_\|currentScope\|findScopeContaining" -n include src || true
```

Expected: only `TypeChecker` compile-time `scopes_` / `currentScope` references remain. There should be no `BeginScope`, `EndScope`, `emitBeginScope`, `emitEndScope`, or `IRInterpreter` scope-stack references.

- [ ] **Step 9: Build and run golden tests**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: build succeeds. Golden failures are expected for IR output changes and moved error fixtures until refresh.

- [ ] **Step 10: Commit implementation source changes**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp include/IRCompiler.hpp src/IRCompiler.cpp src/main.cpp include/IR.hpp src/IR.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp
git commit -m "feat: resolve lexical scope at compile time"
```

Expected: commit succeeds with source changes only.

## Task 5: Refresh Golden Outputs

**Files:**
- Modify: affected files under `tests/golden/**`

- [ ] **Step 1: Refresh goldens**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Expected: update mode succeeds with zero failures.

- [ ] **Step 2: Inspect lexical-scope IR outputs**

Run:

```bash
sed -n '1,120p' tests/golden/block/ir.out
sed -n '1,120p' tests/golden/block_assignment_outer/ir.out
sed -n '1,120p' tests/golden/block_assignment_inner_shadow/ir.out
grep -R "begin_scope\|end_scope" -n tests/golden || true
```

Expected:

- IR names include suffixes such as `x#0` and `x#1`.
- No golden file contains `begin_scope` or `end_scope`.

- [ ] **Step 3: Inspect migrated type errors**

Run:

```bash
cat tests/golden/type_errors/undefined_variable.err
cat tests/golden/type_errors/assign_undefined.err
cat tests/golden/type_errors/block_local_escape.err
find tests/golden/runtime_errors -maxdepth 1 -type f | sort
```

Expected:

```text
Type error: undefined variable `missing`
Type error: undefined variable `missing`
Type error: undefined variable `x`
```

Runtime-error fixtures should no longer include `undefined_variable`, `assign_undefined`, or `block_local_escape`.

- [ ] **Step 4: Run golden verification**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass with zero failures. Record the pass count in the final report.

- [ ] **Step 5: Commit golden updates**

Run:

```bash
git add tests/golden
git commit -m "test: refresh compile-time scope goldens"
```

Expected: commit succeeds with fixture and golden-output changes only.

## Task 6: Update Documentation

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README scope/type wording**

In `README.md`, replace this sentence:

```text
Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated variables are still accepted and are not fully inferred yet. Blocks introduce lexical scope: variables declared inside a block are not visible outside it, and inner blocks may shadow outer variables. Re-declaring a variable in the same scope is a type error.
```

with:

```text
Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated variables are still accepted and are not fully inferred yet. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.
```

Expected: README describes compile-time scope resolution.

- [ ] **Step 2: Update AGENTS semantics**

In `AGENTS.md`, replace:

```text
- Blocks introduce lexical scope: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, and same-scope duplicate declarations are type errors.
- Assignment has the form `name = expression`, is right-associative, updates an existing variable, and evaluates to the assigned value.
- Assigning to an undefined variable is a runtime error.
```

with:

```text
- Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, and same-scope duplicate declarations are type errors.
- Assignment has the form `name = expression`, is right-associative, updates the nearest resolved binding, and evaluates to the assigned value.
- Reading or assigning an undefined variable is a type error before IR compilation.
```

Expected: agent memory no longer says undefined assignment is a runtime error.

- [ ] **Step 3: Update roadmap implementation note**

In `docs/roadmap.md`, after:

```text
Status: implemented. Blocks now introduce real variable scopes.
```

add:

```text
Implementation note: lexical names are resolved during type checking; generated IR uses unique internal variable names rather than runtime scope operations.
```

Expected: roadmap records the improved scope architecture.

- [ ] **Step 4: Review documentation diff**

Run:

```bash
git diff -- README.md AGENTS.md docs/roadmap.md
```

Expected: diff only documents compile-time scope resolution and undefined-variable type errors.

- [ ] **Step 5: Commit docs**

Run:

```bash
git add README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document compile-time scope resolution"
```

Expected: commit succeeds with documentation changes only.

## Task 7: Final Verification

**Files:**
- Verify all touched files.

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

```text
100% tests passed, 0 tests failed out of 2
golden tests: 0 failed
Ran 9 tests
OK
```

Record the exact golden pass count in the final report.

- [ ] **Step 2: Verify runtime scope op removal**

Run:

```bash
grep -R "BeginScope\|EndScope\|emitBeginScope\|emitEndScope\|begin_scope\|end_scope" -n include src tests/golden || true
```

Expected: no output.

- [ ] **Step 3: Verify repository status**

Run:

```bash
git status --short
```

Expected: no output.

- [ ] **Step 4: Summarize recent commits**

Run:

```bash
git log --oneline -8
```

Expected: recent commits include spec, plan, fixture migration, implementation, golden refresh, and docs.

Final report should mention:

- `BeginScope` / `EndScope` were removed from IR.
- IR now uses compiler-generated names like `x#0`.
- Runtime interpreter uses a flat map again.
- Undefined reads/assignments and block-local escapes are type errors.
- Exact verification commands and results.
