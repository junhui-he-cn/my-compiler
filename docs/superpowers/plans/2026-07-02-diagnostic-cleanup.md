# Diagnostic Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Introduce a lightweight shared diagnostic error foundation and standardize compiler error output without adding snippets/carets.

**Architecture:** Add `DiagnosticError` plus kind/location formatting, migrate existing parse/type/compile/runtime error classes to derive from it, convert lexer source errors to lex diagnostics, then refresh parse/type/runtime error goldens. Front-end diagnostics carry `line:column` when a token/location exists; IR compile/runtime diagnostics remain locationless.

**Tech Stack:** C++17, CMake, standard exceptions, recursive-descent parser, type checker, register IR, Python golden tests, CTest.

---

## File Structure

- Create: `include/Diagnostic.hpp` — diagnostic kinds, source location, `DiagnosticError`, formatting declarations.
- Create: `src/Diagnostic.cpp` — diagnostic formatting and `DiagnosticError` implementation.
- Modify: `CMakeLists.txt` — compile `src/Diagnostic.cpp`.
- Modify: `include/Parser.hpp`, `src/Parser.cpp` — make `ParseError` derive from `DiagnosticError` and use compact `line:column` format.
- Modify: `src/Lexer.cpp` — throw `Lex` diagnostics for source lexing errors.
- Modify: `include/TypeChecker.hpp`, `src/TypeChecker.cpp` — make `TypeError` derive from `DiagnosticError`; add token-located type errors.
- Modify: `include/IRCompiler.hpp`, `src/IRCompiler.cpp` — make `IRCompileError` derive from `DiagnosticError` with kind `Compile`.
- Modify: `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp` — make `IRRuntimeError` derive from `DiagnosticError` with kind `Runtime`.
- Modify: `tests/golden/parse_errors/*.err` — update parse diagnostic format.
- Modify: `tests/golden/type_errors/*.err` — update type diagnostic format and locations.
- Modify: `tests/golden/runtime_errors/*.run.err` — update runtime diagnostic format.
- Modify: `README.md`, `docs/roadmap.md`, `AGENTS.md` — document diagnostic conventions.

## Task 0: Prepare Workspace and Baseline

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
diagnostic-cleanup
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
golden tests: 72 passed, 0 failed
Ran 9 tests
OK
```

If baseline fails before diagnostic edits, stop and report the exact failing command and output.

## Task 1: Add RED Parse Diagnostic Fixture Change

**Files:**
- Modify: `tests/golden/parse_errors/logical_and_missing_rhs.err`

- [ ] **Step 1: Update one parse fixture to the new diagnostic format**

Run:

```bash
cat > tests/golden/parse_errors/logical_and_missing_rhs.err <<'CASE'
Parse error at 1:15: expected expression
CASE
```

- [ ] **Step 2: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL for `parse_errors/logical_and_missing_rhs default(ast)` because the compiler still prints:

```text
Parse error at line 1, column 15: expected expression
```

The diff should show the old `at line 1, column 15` format versus the new `at 1:15` format.

- [ ] **Step 3: Commit red parse fixture**

Run:

```bash
git add tests/golden/parse_errors/logical_and_missing_rhs.err
git commit -m "test: expect compact parse diagnostic"
```

Expected: commit succeeds with one fixture change.

## Task 2: Add Diagnostic Infrastructure and Migrate Parse/Lex Errors

**Files:**
- Create: `include/Diagnostic.hpp`
- Create: `src/Diagnostic.cpp`
- Modify: `CMakeLists.txt`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Modify: `src/Lexer.cpp`
- Modify: `tests/golden/parse_errors/invalid_assignment_target.err`
- Modify: `tests/golden/parse_errors/logical_or_missing_rhs.err`
- Modify: `tests/golden/parse_errors/while_missing_block.err`

- [ ] **Step 1: Create diagnostic header**

Run:

```bash
cat > include/Diagnostic.hpp <<'DIAGNOSTIC_HPP'
#pragma once

#include <optional>
#include <stdexcept>
#include <string>

enum class DiagnosticKind {
    Lex,
    Parse,
    Type,
    Compile,
    Runtime,
};

struct SourceLocation {
    int line = 0;
    int column = 0;
};

class DiagnosticError : public std::runtime_error {
public:
    DiagnosticError(DiagnosticKind kind, std::string message);
    DiagnosticError(DiagnosticKind kind, SourceLocation location, std::string message);

    DiagnosticKind kind() const;
    const std::optional<SourceLocation>& location() const;
    const std::string& message() const;

private:
    DiagnosticKind kind_;
    std::optional<SourceLocation> location_;
    std::string message_;
};

std::string diagnosticKindName(DiagnosticKind kind);
std::string formatDiagnostic(
    DiagnosticKind kind,
    const std::optional<SourceLocation>& location,
    const std::string& message);
DIAGNOSTIC_HPP
```

- [ ] **Step 2: Create diagnostic implementation**

Run:

```bash
cat > src/Diagnostic.cpp <<'DIAGNOSTIC_CPP'
#include "Diagnostic.hpp"

#include <utility>

std::string diagnosticKindName(DiagnosticKind kind)
{
    switch (kind) {
    case DiagnosticKind::Lex:
        return "Lex";
    case DiagnosticKind::Parse:
        return "Parse";
    case DiagnosticKind::Type:
        return "Type";
    case DiagnosticKind::Compile:
        return "Compile";
    case DiagnosticKind::Runtime:
        return "Runtime";
    }

    return "Unknown";
}

std::string formatDiagnostic(
    DiagnosticKind kind,
    const std::optional<SourceLocation>& location,
    const std::string& message)
{
    std::string formatted = diagnosticKindName(kind) + " error";
    if (location) {
        formatted += " at " + std::to_string(location->line) + ":" + std::to_string(location->column);
    }
    formatted += ": " + message;
    return formatted;
}

DiagnosticError::DiagnosticError(DiagnosticKind kind, std::string message)
    : std::runtime_error(formatDiagnostic(kind, std::nullopt, message))
    , kind_(kind)
    , message_(std::move(message))
{
}

DiagnosticError::DiagnosticError(DiagnosticKind kind, SourceLocation location, std::string message)
    : std::runtime_error(formatDiagnostic(kind, location, message))
    , kind_(kind)
    , location_(location)
    , message_(std::move(message))
{
}

DiagnosticKind DiagnosticError::kind() const
{
    return kind_;
}

const std::optional<SourceLocation>& DiagnosticError::location() const
{
    return location_;
}

const std::string& DiagnosticError::message() const
{
    return message_;
}
DIAGNOSTIC_CPP
```

- [ ] **Step 3: Add `Diagnostic.cpp` to the build**

Edit `CMakeLists.txt` so `add_executable(compiler_demo` includes `src/Diagnostic.cpp` before `src/IR.cpp`:

```cmake
add_executable(compiler_demo
    src/Ast.cpp
    src/Diagnostic.cpp
    src/IR.cpp
```

- [ ] **Step 4: Migrate `ParseError` to `DiagnosticError`**

Edit `include/Parser.hpp`:

```cpp
#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "Token.hpp"
```

Then change the class declaration to:

```cpp
class ParseError final : public DiagnosticError {
public:
    explicit ParseError(const Token& token, const std::string& message);
};
```

Edit `src/Parser.cpp` constructor implementation to:

```cpp
ParseError::ParseError(const Token& token, const std::string& message)
    : DiagnosticError(DiagnosticKind::Parse, SourceLocation{token.line, token.column}, message)
{
}
```

- [ ] **Step 5: Convert lexer source errors to lex diagnostics**

Edit `src/Lexer.cpp` and add:

```cpp
#include "Diagnostic.hpp"
```

Then replace the unexpected `&` throw with:

```cpp
            throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
                "unexpected character `&`");
```

Replace the unexpected `|` throw with:

```cpp
            throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
                "unexpected character `|`");
```

Replace the default unexpected-character throw with:

```cpp
            throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
                "unexpected character `" + std::string(1, c) + "`");
```

Replace the unterminated-string throw with:

```cpp
        throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
            "unterminated string");
```

- [ ] **Step 6: Update remaining parse error fixtures**

Run:

```bash
cat > tests/golden/parse_errors/invalid_assignment_target.err <<'CASE'
Parse error at 1:9: invalid assignment target
CASE
cat > tests/golden/parse_errors/logical_or_missing_rhs.err <<'CASE'
Parse error at 1:16: expected expression
CASE
cat > tests/golden/parse_errors/while_missing_block.err <<'CASE'
Parse error at 1:12: expected `{` after while condition, found Print `print`
CASE
```

- [ ] **Step 7: Run golden tests and verify parse GREEN, type/runtime still old**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: parse-error fixtures pass. Type and runtime fixtures still pass because their expected files have not changed yet. Overall golden tests should pass at this point.

- [ ] **Step 8: Commit diagnostic infrastructure and parse/lex migration**

Run:

```bash
git add CMakeLists.txt include/Diagnostic.hpp src/Diagnostic.cpp include/Parser.hpp src/Parser.cpp src/Lexer.cpp tests/golden/parse_errors
git commit -m "feat: add diagnostic error infrastructure"
```

Expected: commit succeeds with diagnostic files, parser/lexer changes, and parse-error fixture updates.

## Task 3: Migrate Type Errors and Type Golden Fixtures

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Modify: `tests/golden/type_errors/*.err`

- [ ] **Step 1: Update `TypeError` declaration**

Edit `include/TypeChecker.hpp`.

Add include:

```cpp
#include "Diagnostic.hpp"
```

Remove the no-longer-needed `#include <stdexcept>`.

Change `TypeError` to:

```cpp
class TypeError final : public DiagnosticError {
public:
    explicit TypeError(std::string message);
    TypeError(const Token& token, std::string message);
};
```

Change `checkAssignable` declaration to include a location token:

```cpp
    void checkAssignable(const Token& token, const std::string& context, StaticType expected, StaticType actual) const;
```

- [ ] **Step 2: Update `TypeError` constructors**

Edit `src/TypeChecker.cpp`.

Replace:

```cpp
TypeError::TypeError(const std::string& message)
    : std::runtime_error("Type error: " + message)
{
}
```

with:

```cpp
TypeError::TypeError(std::string message)
    : DiagnosticError(DiagnosticKind::Type, std::move(message))
{
}

TypeError::TypeError(const Token& token, std::string message)
    : DiagnosticError(DiagnosticKind::Type, SourceLocation{token.line, token.column}, std::move(message))
{
}
```

Keep `#include <utility>` because constructors move strings.

- [ ] **Step 3: Add token locations to declaration/type checks**

Edit `src/TypeChecker.cpp`.

In `declareVariable()`, replace duplicate declaration throw with:

```cpp
        throw TypeError(statement.name, "variable `" + statement.name.lexeme + "` already declared in this scope");
```

In `checkLetInitializer()`, replace `checkAssignable(...)` call with:

```cpp
    checkAssignable(
        statement.name,
        "cannot initialize `" + statement.name.lexeme + "` of type " + staticTypeName(declared)
            + " with " + staticTypeName(initializer),
        declared,
        initializer);
```

In `resolveAnnotation()`, replace unknown type throw with:

```cpp
    throw TypeError(typeName, "unknown type `" + typeName.lexeme + "`");
```

Replace `checkAssignable()` definition with:

```cpp
void TypeChecker::checkAssignable(const Token& token, const std::string& context, StaticType expected, StaticType actual) const
{
    if (!compatible(expected, actual)) {
        throw TypeError(token, context);
    }
}
```

- [ ] **Step 4: Add token locations to variable and assignment errors**

Edit `src/TypeChecker.cpp`.

In the `VariableExpr` branch, replace undefined-variable throw with:

```cpp
            throw TypeError(variable->name, "undefined variable `" + variable->name.lexeme + "`");
```

In the `AssignExpr` branch, replace undefined-variable throw with:

```cpp
            throw TypeError(assign->name, "undefined variable `" + assign->name.lexeme + "`");
```

Replace assignment mismatch throw with:

```cpp
            throw TypeError(assign->name, "cannot assign " + staticTypeName(value) + " to `" + assign->name.lexeme
                + "` of type " + staticTypeName(target->type));
```

- [ ] **Step 5: Add token locations to unary/binary errors**

Edit `src/TypeChecker.cpp`.

In `checkUnary()`, replace unary minus mismatch throw with:

```cpp
            throw TypeError(expression.op, "unary `-` expects number, got " + staticTypeName(right));
```

Replace unsupported unary throw with:

```cpp
        throw TypeError(expression.op, "unsupported unary operator `" + expression.op.lexeme + "`");
```

In `checkBinary()`, replace the `+` mismatch throw with:

```cpp
        throw TypeError(expression.op, "binary `+` expects two numbers or two strings, got "
            + staticTypeName(left) + " and " + staticTypeName(right));
```

Replace both `binaryTypesMessage(...)` throws with:

```cpp
            throw TypeError(expression.op, binaryTypesMessage(expression, left, right));
```

Replace unsupported binary throw with:

```cpp
        throw TypeError(expression.op, "unsupported binary operator `" + expression.op.lexeme + "`");
```

- [ ] **Step 6: Update type-error fixtures**

Run:

```bash
cat > tests/golden/type_errors/assign_undefined.err <<'CASE'
Type error at 1:1: undefined variable `missing`
CASE
cat > tests/golden/type_errors/binary_number_operator_mismatch.err <<'CASE'
Type error at 1:19: binary `*` expects numbers, got number and string
CASE
cat > tests/golden/type_errors/block_local_escape.err <<'CASE'
Type error at 4:7: undefined variable `x`
CASE
cat > tests/golden/type_errors/duplicate_declaration.err <<'CASE'
Type error at 2:5: variable `x` already declared in this scope
CASE
cat > tests/golden/type_errors/typed_assignment_mismatch.err <<'CASE'
Type error at 2:1: cannot assign string to `x` of type number
CASE
cat > tests/golden/type_errors/typed_let_number_mismatch.err <<'CASE'
Type error at 1:5: cannot initialize `x` of type number with string
CASE
cat > tests/golden/type_errors/unary_minus_non_number.err <<'CASE'
Type error at 1:17: unary `-` expects number, got string
CASE
cat > tests/golden/type_errors/undefined_variable.err <<'CASE'
Type error at 1:7: undefined variable `missing`
CASE
cat > tests/golden/type_errors/unknown_type_annotation.err <<'CASE'
Type error at 1:8: unknown type `int`
CASE
cat > tests/golden/type_errors/while_body_scope_escape.err <<'CASE'
Type error at 4:7: undefined variable `x`
CASE
```

- [ ] **Step 7: Run golden tests and verify type GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all parse and type error fixtures pass. Runtime fixtures still pass because runtime expected files have not changed yet. Overall golden tests should pass at this point.

- [ ] **Step 8: Commit type diagnostic migration**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors
git commit -m "feat: add located type diagnostics"
```

Expected: commit succeeds with type-checker and type golden changes.

## Task 4: Migrate Compile and Runtime Error Classes

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Modify: `tests/golden/runtime_errors/division_by_zero.run.err`
- Modify: `tests/golden/runtime_errors/invalid_add.run.err`

- [ ] **Step 1: Migrate `IRCompileError` declaration**

Edit `include/IRCompiler.hpp`.

Add include:

```cpp
#include "Diagnostic.hpp"
```

Remove `#include <stdexcept>` if it is only used for `IRCompileError`.

Change class declaration to:

```cpp
class IRCompileError final : public DiagnosticError {
public:
    explicit IRCompileError(std::string message);
};
```

- [ ] **Step 2: Migrate `IRCompileError` implementation**

Edit `src/IRCompiler.cpp` and replace the constructor with:

```cpp
IRCompileError::IRCompileError(std::string message)
    : DiagnosticError(DiagnosticKind::Compile, std::move(message))
{
}
```

`src/IRCompiler.cpp` already includes `<utility>`, so no new include is needed.

- [ ] **Step 3: Migrate `IRRuntimeError` declaration**

Edit `include/IRInterpreter.hpp`.

Add include:

```cpp
#include "Diagnostic.hpp"
```

Remove `#include <stdexcept>` if it is only used for `IRRuntimeError`.

Change class declaration to:

```cpp
class IRRuntimeError final : public DiagnosticError {
public:
    explicit IRRuntimeError(std::string message);
};
```

- [ ] **Step 4: Migrate `IRRuntimeError` implementation**

Edit `src/IRInterpreter.cpp` and replace the constructor with:

```cpp
IRRuntimeError::IRRuntimeError(std::string message)
    : DiagnosticError(DiagnosticKind::Runtime, std::move(message))
{
}
```

`src/IRInterpreter.cpp` already includes `<utility>`, so no new include is needed.

- [ ] **Step 5: Update runtime error fixtures**

Run:

```bash
cat > tests/golden/runtime_errors/division_by_zero.run.err <<'CASE'
Runtime error: division by zero
CASE
cat > tests/golden/runtime_errors/invalid_add.run.err <<'CASE'
Runtime error: add expects two numbers or two strings
CASE
```

- [ ] **Step 6: Run golden tests and verify runtime GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass with parse, type, and runtime diagnostic format changes.

- [ ] **Step 7: Commit compile/runtime diagnostic migration**

Run:

```bash
git add include/IRCompiler.hpp src/IRCompiler.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/runtime_errors
git commit -m "feat: standardize compile and runtime diagnostics"
```

Expected: commit succeeds with compile/runtime error class and runtime golden changes.

## Task 5: Update Documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README error note**

Edit `README.md` and add this section after the `Run` section or before `Build` if that reads better in context:

```markdown
## Diagnostics

Compiler errors are reported as `Lex`, `Parse`, `Type`, `Compile`, or `Runtime` errors. Front-end diagnostics include a `line:column` location when available, for example:

```text
Parse error at 1:15: expected expression
Type error at 1:7: undefined variable `missing`
```

Runtime diagnostics currently do not include source locations.
```

If inserting before `Build`, keep the existing `Build`, `Test`, and `Run` sections otherwise unchanged.

- [ ] **Step 2: Update roadmap status**

Edit `docs/roadmap.md` Phase 5 heading and opening from:

```markdown
## Phase 5: Diagnostic Cleanup

Goal: make errors more compiler-like and easier to test.
```

to:

```markdown
## Phase 5: Diagnostic Cleanup — Implemented

Status: implemented. Errors now use a shared diagnostic format with `Lex`, `Parse`, `Type`, `Compile`, and `Runtime` categories. Front-end diagnostics include `line:column` locations when available; snippets, carets, and multi-error recovery remain future improvements.

Goal: make errors more compiler-like and easier to test.
```

- [ ] **Step 3: Update AGENTS diagnostic rules**

Edit `AGENTS.md` in the Golden Test Conventions or Documentation Update Rules area and add:

```markdown
## Diagnostic Output Convention

Language diagnostics use this stable shape:

```text
<Kind> error at <line>:<column>: <message>
<Kind> error: <message>
```

Use locations for lexer, parser, and type errors when a source token/location is available. Compile and runtime diagnostics are currently locationless. After intentional diagnostic format changes, refresh and review parse/type/runtime error goldens. Lexer errors do not yet have a dedicated golden fixture category.
```

- [ ] **Step 4: Review documentation diff**

Run:

```bash
git diff -- README.md docs/roadmap.md AGENTS.md
```

Expected: docs describe the implemented diagnostic format only, explicitly saying snippets/carets are not implemented.

- [ ] **Step 5: Commit documentation**

Run:

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document diagnostic format"
```

Expected: commit succeeds with documentation changes only.

## Task 6: Full Verification and Cleanup

**Files:**
- Verify all changed files.
- Remove: `tests/__pycache__/` if created.

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
ctest reports 100% tests passed
golden tests report all cases passed
selftests report Ran 9 tests and OK
```

- [ ] **Step 2: Check final workspace state**

Run:

```bash
git status --short
```

Expected: clean working tree.

- [ ] **Step 3: Prepare completion summary**

Report these exact verification commands and their results:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Include a concise feature summary:

```text
Implemented shared diagnostic formatting for Lex/Parse/Type/Compile/Runtime errors. Added located parse/type/lex diagnostics, standardized compile/runtime prefixes, refreshed error goldens, and documented diagnostic conventions.
```
