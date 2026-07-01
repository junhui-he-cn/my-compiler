# Lexical Scope Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make block statements introduce lexical variable scopes with inner shadowing, nearest-binding assignment, block-local lifetime, and same-scope duplicate declaration errors.

**Architecture:** Add `BeginScope` and `EndScope` operations to the existing register IR, emit them around `BlockStmt` lowering, and replace the interpreter's flat variable map with a stack of scope maps. Keep parser and AST shapes unchanged because block syntax is already present.

**Tech Stack:** C++17, CMake, recursive-descent parser, register IR, IR interpreter, Python golden CLI tests, CTest.

---

## File Structure

- Modify: `include/IR.hpp` — add scope opcodes and emitter declarations.
- Modify: `src/IR.cpp` — add scope emitters, opcode names, and no-operand IR printing.
- Modify: `src/IRCompiler.cpp` — emit scope boundaries around block statements.
- Modify: `include/IRInterpreter.hpp` — replace flat globals storage with scope stack helpers.
- Modify: `src/IRInterpreter.cpp` — execute scope operations and lexical variable lookup/declaration/assignment.
- Modify: `tests/golden/block/input.cd`, `tests/golden/block/run.out`, later refresh `tests/golden/block/ast.out` and `tests/golden/block/ir.out`.
- Create: `tests/golden/block_assignment_outer/input.cd` and `tests/golden/block_assignment_outer/run.out`.
- Create: `tests/golden/block_assignment_inner_shadow/input.cd` and `tests/golden/block_assignment_inner_shadow/run.out`.
- Create: `tests/golden/runtime_errors/duplicate_declaration.cd`, `.run.err`, `.exit`.
- Create: `tests/golden/runtime_errors/block_local_escape.cd`, `.run.err`, `.exit`.
- Modify: existing `*.ir.out` golden files after intentional IR output changes.
- Modify: `README.md` — document block lexical scope and duplicate declaration behavior.
- Modify: `AGENTS.md` — update current semantics and roadmap hints.
- Modify: `docs/roadmap.md` — mark Phase 1 as implemented.

## Task 0: Prepare Workspace and Baseline

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
lexical-scope
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

Expected current baseline after the roadmap/spec commits:

```text
100% tests passed, 0 tests failed out of 2
golden tests: 41 passed, 0 failed
Ran 7 tests
OK
```

If baseline fails before lexical-scope edits, stop and report the failure.

## Task 1: Add Red Golden Coverage for Lexical Scope

**Files:**
- Modify: `tests/golden/block/input.cd`
- Modify: `tests/golden/block/run.out`
- Create: `tests/golden/block_assignment_outer/input.cd`
- Create: `tests/golden/block_assignment_outer/run.out`
- Create: `tests/golden/block_assignment_inner_shadow/input.cd`
- Create: `tests/golden/block_assignment_inner_shadow/run.out`
- Create: `tests/golden/runtime_errors/duplicate_declaration.cd`
- Create: `tests/golden/runtime_errors/duplicate_declaration.run.err`
- Create: `tests/golden/runtime_errors/duplicate_declaration.exit`
- Create: `tests/golden/runtime_errors/block_local_escape.cd`
- Create: `tests/golden/runtime_errors/block_local_escape.run.err`
- Create: `tests/golden/runtime_errors/block_local_escape.exit`

- [ ] **Step 1: Rewrite the existing block fixture to assert shadowing**

Run:

```bash
cat > tests/golden/block/input.cd <<'CASE'
let x = "global";
{
  let x = "inner";
  print x;
}
print x;
CASE

cat > tests/golden/block/run.out <<'CASE'
inner
global
CASE
```

Expected: the fixture now describes lexical shadowing instead of leaked block locals.

- [ ] **Step 2: Add a success fixture for assigning an outer binding from a block**

Run:

```bash
mkdir -p tests/golden/block_assignment_outer
cat > tests/golden/block_assignment_outer/input.cd <<'CASE'
let x = 1;
{
  x = 2;
}
print x;
CASE

cat > tests/golden/block_assignment_outer/run.out <<'CASE'
2
CASE
```

Expected: the fixture exists and only checks run output before goldens are refreshed.

- [ ] **Step 3: Add a success fixture for assigning an inner shadow**

Run:

```bash
mkdir -p tests/golden/block_assignment_inner_shadow
cat > tests/golden/block_assignment_inner_shadow/input.cd <<'CASE'
let x = 1;
{
  let x = 10;
  x = 20;
  print x;
}
print x;
CASE

cat > tests/golden/block_assignment_inner_shadow/run.out <<'CASE'
20
1
CASE
```

Expected: the fixture exists and will fail before lexical scopes are implemented.

- [ ] **Step 4: Add a runtime-error fixture for duplicate declaration in one scope**

Run:

```bash
cat > tests/golden/runtime_errors/duplicate_declaration.cd <<'CASE'
let x = 1;
let x = 2;
CASE

cat > tests/golden/runtime_errors/duplicate_declaration.run.err <<'CASE'
IR runtime error: variable `x` already declared in this scope
CASE

cat > tests/golden/runtime_errors/duplicate_declaration.exit <<'CASE'
1
CASE
```

Expected: the fixture records the runtime error for same-scope redeclaration.

- [ ] **Step 5: Add a runtime-error fixture for block-local escape**

Run:

```bash
cat > tests/golden/runtime_errors/block_local_escape.cd <<'CASE'
{
  let x = 1;
}
print x;
CASE

cat > tests/golden/runtime_errors/block_local_escape.run.err <<'CASE'
IR runtime error: undefined variable `x`
CASE

cat > tests/golden/runtime_errors/block_local_escape.exit <<'CASE'
1
CASE
```

Expected: the fixture records that block-local names are not visible after block exit.

- [ ] **Step 6: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: golden tests fail. The output should include failures for at least:

```text
block --run
block_assignment_inner_shadow --run
runtime_errors/duplicate_declaration --run
runtime_errors/block_local_escape --run
```

`block_assignment_outer --run` may already pass because the old flat environment also updates the single binding.

- [ ] **Step 7: Commit red fixtures**

Run:

```bash
git add tests/golden/block/input.cd tests/golden/block/run.out \
        tests/golden/block_assignment_outer/input.cd tests/golden/block_assignment_outer/run.out \
        tests/golden/block_assignment_inner_shadow/input.cd tests/golden/block_assignment_inner_shadow/run.out \
        tests/golden/runtime_errors/duplicate_declaration.cd \
        tests/golden/runtime_errors/duplicate_declaration.run.err \
        tests/golden/runtime_errors/duplicate_declaration.exit \
        tests/golden/runtime_errors/block_local_escape.cd \
        tests/golden/runtime_errors/block_local_escape.run.err \
        tests/golden/runtime_errors/block_local_escape.exit
git commit -m "test: add lexical scope golden coverage"
```

Expected: commit succeeds with only fixture changes.

## Task 2: Add IR Scope Operations and Block Lowering

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Add scope opcodes and emitter declarations in `include/IR.hpp`**

Edit `include/IR.hpp` so `IROp` contains `BeginScope` and `EndScope` after `AssignVar`:

```cpp
enum class IROp {
    Constant,
    LoadVar,
    StoreVar,
    AssignVar,
    BeginScope,
    EndScope,
    Print,
    Negate,
    Not,
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Jump,
    JumpIfFalse,
};
```

Also add emitter declarations after `emitAssignVar`:

```cpp
    void emitBeginScope();
    void emitEndScope();
```

Expected: the header exposes scope operations to the compiler.

- [ ] **Step 2: Add no-operand scope emitters in `src/IR.cpp`**

In `isBinary`, add `BeginScope` and `EndScope` to the false cases:

```cpp
    case IROp::BeginScope:
    case IROp::EndScope:
```

Add these methods after `IRProgram::emitAssignVar`:

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

Add opcode names in `irOpName` after `AssignVar`:

```cpp
    case IROp::BeginScope:
        return "begin_scope";
    case IROp::EndScope:
        return "end_scope";
```

Expected: IR printing emits lines like `0001  begin_scope` and `0005  end_scope` with no operands.

- [ ] **Step 3: Emit scope boundaries around block statements**

In `src/IRCompiler.cpp`, replace the existing `BlockStmt` branch:

```cpp
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        for (const auto& child : block->statements) {
            compileStatement(*child);
        }
        return;
    }
```

with:

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

Expected: every block statement lowers to a scoped IR region.

- [ ] **Step 4: Build and observe runtime still RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: build succeeds, but tests still fail because the interpreter does not yet execute `BeginScope` or `EndScope`. Failures may include `unknown` behavior or unchanged flat-scope runtime behavior until Task 3 is complete.

## Task 3: Implement Runtime Scope Stack

**Files:**
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`

- [ ] **Step 1: Replace interpreter variable storage declarations**

In `include/IRInterpreter.hpp`, add helper declarations after `writeRegister`:

```cpp
    std::unordered_map<std::string, Value>& currentScope();
    const std::unordered_map<std::string, Value>& currentScope() const;
    std::unordered_map<std::string, Value>* findScopeContaining(const std::string& name);
    const std::unordered_map<std::string, Value>* findScopeContaining(const std::string& name) const;
    const Value& loadVariable(const std::string& name) const;
    void declareVariable(const std::string& name, Value value);
    void assignVariable(const std::string& name, Value value);
```

Replace the old `globals_` member:

```cpp
    std::unordered_map<std::string, Value> globals_;
```

with:

```cpp
    std::vector<std::unordered_map<std::string, Value>> scopes_;
```

Expected: the header declares scope-stack helpers and storage.

- [ ] **Step 2: Initialize the global scope in the constructor**

In `src/IRInterpreter.cpp`, replace the constructor:

```cpp
IRInterpreter::IRInterpreter(std::ostream& output)
    : output_(output)
{
}
```

with:

```cpp
IRInterpreter::IRInterpreter(std::ostream& output)
    : output_(output)
    , scopes_(1)
{
}
```

Expected: `globals()` can return an empty global scope even before `execute` runs.

- [ ] **Step 3: Reset scopes at execution start**

In `IRInterpreter::execute`, replace:

```cpp
    registers_.assign(program.registerCount(), Value::nil());
    globals_.clear();
```

with:

```cpp
    registers_.assign(program.registerCount(), Value::nil());
    scopes_.clear();
    scopes_.emplace_back();
```

Expected: each program execution starts with a fresh global scope.

- [ ] **Step 4: Execute variable operations through lexical helpers**

In the `IROp::LoadVar`, `IROp::StoreVar`, and `IROp::AssignVar` cases, replace the existing flat-map code with:

```cpp
        case IROp::LoadVar: {
            const std::string name = readName(program, instruction.operand);
            writeRegister(readDest(instruction), loadVariable(name));
            break;
        }
        case IROp::StoreVar: {
            const std::string name = readName(program, instruction.operand);
            declareVariable(name, readRegister(readLeft(instruction)));
            break;
        }
        case IROp::AssignVar: {
            const std::string name = readName(program, instruction.operand);
            assignVariable(name, readRegister(readLeft(instruction)));
            break;
        }
```

Expected: all variable reads/writes use lexical scope lookup.

- [ ] **Step 5: Execute scope boundary operations**

Add these cases after `AssignVar` and before `Print`:

```cpp
        case IROp::BeginScope:
            scopes_.emplace_back();
            break;
        case IROp::EndScope:
            if (scopes_.size() <= 1) {
                throw IRRuntimeError("cannot end global scope");
            }
            scopes_.pop_back();
            break;
```

Expected: block entry pushes a scope and block exit pops it.

- [ ] **Step 6: Return the global scope from `globals()`**

Replace the existing `globals()` implementation:

```cpp
const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    return globals_;
}
```

with:

```cpp
const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    if (scopes_.empty()) {
        throw IRRuntimeError("scope stack is empty");
    }
    return scopes_.front();
}
```

Expected: existing public API returns global bindings.

- [ ] **Step 7: Add scope helper implementations**

Add these methods after `writeRegister` and before `executeUnaryNumber`:

```cpp
std::unordered_map<std::string, Value>& IRInterpreter::currentScope()
{
    if (scopes_.empty()) {
        throw IRRuntimeError("scope stack is empty");
    }
    return scopes_.back();
}

const std::unordered_map<std::string, Value>& IRInterpreter::currentScope() const
{
    if (scopes_.empty()) {
        throw IRRuntimeError("scope stack is empty");
    }
    return scopes_.back();
}

std::unordered_map<std::string, Value>* IRInterpreter::findScopeContaining(const std::string& name)
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        if (scope->find(name) != scope->end()) {
            return &*scope;
        }
    }
    return nullptr;
}

const std::unordered_map<std::string, Value>* IRInterpreter::findScopeContaining(const std::string& name) const
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        if (scope->find(name) != scope->end()) {
            return &*scope;
        }
    }
    return nullptr;
}

const Value& IRInterpreter::loadVariable(const std::string& name) const
{
    const auto* scope = findScopeContaining(name);
    if (!scope) {
        throw IRRuntimeError("undefined variable `" + name + "`");
    }
    return scope->at(name);
}

void IRInterpreter::declareVariable(const std::string& name, Value value)
{
    auto& scope = currentScope();
    if (scope.find(name) != scope.end()) {
        throw IRRuntimeError("variable `" + name + "` already declared in this scope");
    }
    scope.emplace(name, std::move(value));
}

void IRInterpreter::assignVariable(const std::string& name, Value value)
{
    auto* scope = findScopeContaining(name);
    if (!scope) {
        throw IRRuntimeError("undefined variable `" + name + "`");
    }
    auto found = scope->find(name);
    found->second = std::move(value);
}
```

Expected: helper functions implement nearest-binding lookup and same-scope duplicate declaration errors.

- [ ] **Step 8: Build and run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: runtime behavior for lexical scope is now correct, but tests still fail because IR goldens need intentional refresh after adding `begin_scope` and `end_scope`.

- [ ] **Step 9: Commit implementation before refreshing goldens**

Run:

```bash
git add include/IR.hpp src/IR.cpp src/IRCompiler.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp
git commit -m "feat: execute block lexical scopes"
```

Expected: commit succeeds with source changes only.

## Task 4: Refresh Golden Outputs

**Files:**
- Modify: affected files under `tests/golden/**`

- [ ] **Step 1: Refresh golden outputs**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Expected: command succeeds and reports all checks passing in update mode.

- [ ] **Step 2: Inspect the scope-related goldens**

Run:

```bash
sed -n '1,120p' tests/golden/block/ir.out
sed -n '1,120p' tests/golden/block/run.out
sed -n '1,120p' tests/golden/block_assignment_inner_shadow/run.out
sed -n '1,80p' tests/golden/runtime_errors/duplicate_declaration.run.err
sed -n '1,80p' tests/golden/runtime_errors/block_local_escape.run.err
```

Expected output includes:

```text
begin_scope
end_scope
inner
global
20
1
IR runtime error: variable `x` already declared in this scope
IR runtime error: undefined variable `x`
```

- [ ] **Step 3: Run full golden tests after refresh**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected:

```text
golden tests: 49 passed, 0 failed
```

If the count differs because fixture coverage changed, verify there are zero failures and record the observed count in the final report.

- [ ] **Step 4: Commit refreshed goldens**

Run:

```bash
git add tests/golden
git commit -m "test: refresh lexical scope goldens"
```

Expected: commit succeeds with golden fixture updates only.

## Task 5: Update Documentation

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README language semantics**

In `README.md`, replace:

```text
Type annotations on `let` declarations are currently syntax-only: they are parsed and shown in the AST, but they are not type-checked yet. Blocks group statements for control flow but do not introduce lexical scope yet.
```

with:

```text
Type annotations on `let` declarations are currently syntax-only: they are parsed and shown in the AST, but they are not type-checked yet. Blocks introduce lexical scope: variables declared inside a block are not visible outside it, and inner blocks may shadow outer variables. Re-declaring a variable in the same scope is a runtime error.
```

Expected: README describes implemented block scope semantics.

- [ ] **Step 2: Update AGENTS current semantics**

In `AGENTS.md`, replace:

```text
- Blocks group statements for control flow but do not introduce lexical scope yet.
```

with:

```text
- Blocks introduce lexical scope: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, and same-scope duplicate declarations are runtime errors.
```

Expected: agent memory no longer lists lexical scope as a limitation.

- [ ] **Step 3: Update roadmap Phase 1 status**

In `docs/roadmap.md`, replace this heading:

```text
## Phase 1: Lexical Scope
```

with:

```text
## Phase 1: Lexical Scope — Implemented
```

Then replace:

```text
Goal: make blocks introduce real variable scopes.
```

with:

```text
Status: implemented. Blocks now introduce real variable scopes.
```

Expected: roadmap shows Phase 1 completion without claiming later phases are complete.

- [ ] **Step 4: Run documentation diff review**

Run:

```bash
git diff -- README.md AGENTS.md docs/roadmap.md
```

Expected: diff only documents lexical scope semantics and roadmap status.

- [ ] **Step 5: Commit documentation updates**

Run:

```bash
git add README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document lexical scope semantics"
```

Expected: commit succeeds with documentation changes only.

## Task 6: Final Verification

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
Ran 7 tests
OK
```

Record the exact golden pass count in the final report.

- [ ] **Step 2: Check repository status**

Run:

```bash
git status --short
```

Expected: no output.

- [ ] **Step 3: Summarize commits and behavior**

Run:

```bash
git log --oneline -5
```

Expected: recent commits include lexical scope test, implementation, golden refresh, and docs commits.

Final report should mention:

- block-local variables no longer escape
- inner blocks can shadow outer variables
- assignment updates the nearest existing binding
- duplicate declarations in the same scope now fail at runtime
- exact verification commands and results
