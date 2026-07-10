# Flow Facts Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract nullable flow-fact/narrowing logic from `TypeChecker` into a focused `FlowFacts` subsystem without changing language behavior or golden outputs.

**Architecture:** Add `FlowFacts` as a small helper that owns condition-shape analysis and the active narrowing stack. Keep binding lookup, scope ownership, diagnostics, and expression checking in `TypeChecker`, connected through a resolver callback. Add focused C++ tests for `FlowFacts`, then integrate it into the existing type checker and verify existing nullable goldens still pass.

**Tech Stack:** C++17, CMake, existing AST and `TypeUtils` classes, Python golden test runner, Rust VM parity tests.

---

## File Structure

- Create `include/FlowFacts.hpp`: public `FlowNarrowing`, `BranchFlowFacts`, and `FlowFacts` declarations.
- Create `src/FlowFacts.cpp`: expression-shape narrowing algorithm and active narrowing stack management.
- Create `tests/flow_facts_tests.cpp`: focused C++ assertions for branch fact extraction and stack restoration.
- Modify `CMakeLists.txt`: compile `src/FlowFacts.cpp` into `compiler_design`; add `flow_facts_tests` executable and CTest entry.
- Modify `include/TypeChecker.hpp`: include `FlowFacts.hpp`, remove private narrowing structs/helpers, add `FlowFacts flowFacts_` and one resolver helper declaration.
- Modify `src/TypeChecker.cpp`: clear `flowFacts_`, delegate `if` branch fact handling, delegate variable narrowed-type lookup, remove old narrowing helper implementations.

---

### Task 1: Add focused FlowFacts tests before implementation

**Files:**
- Create: `tests/flow_facts_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Confirm the workspace is clean**

Run:

```sh
git status --short
```

Expected: no output.

- [ ] **Step 2: Add the C++ FlowFacts test file**

Create `tests/flow_facts_tests.cpp` with this exact content:

```cpp
#include "Ast.hpp"
#include "FlowFacts.hpp"
#include "Token.hpp"
#include "TypeUtils.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <utility>

namespace {

Token token(TokenType type, std::string lexeme)
{
    return Token{type, std::move(lexeme), 1, 1};
}

ExprPtr variable(std::string name)
{
    return std::make_unique<VariableExpr>(token(TokenType::Identifier, std::move(name)));
}

ExprPtr nilLiteral()
{
    return std::make_unique<LiteralExpr>("nil");
}

ExprPtr nilCheck(std::string name, TokenType op)
{
    return std::make_unique<BinaryExpr>(
        variable(std::move(name)),
        token(op, op == TokenType::BangEqual ? "!=" : "=="),
        nilLiteral());
}

ExprPtr grouped(ExprPtr expression)
{
    return std::make_unique<GroupingExpr>(std::move(expression));
}

ExprPtr logical(ExprPtr left, TokenType op, ExprPtr right)
{
    return std::make_unique<LogicalExpr>(
        std::move(left),
        token(op, op == TokenType::AmpersandAmpersand ? "&&" : "||"),
        std::move(right));
}

FlowFacts::VariableNarrowingResolver resolver()
{
    return [](const VariableExpr& expression) -> std::optional<FlowNarrowing> {
        if (expression.name.lexeme == "numberValue") {
            return FlowNarrowing{"numberValue#0", simpleType(StaticType::Number)};
        }
        if (expression.name.lexeme == "stringValue") {
            return FlowNarrowing{"stringValue#1", simpleType(StaticType::String)};
        }
        return std::nullopt;
    };
}

void test_not_nil_narrows_then_branch()
{
    FlowFacts facts;
    const ExprPtr condition = nilCheck("numberValue", TokenType::BangEqual);

    const BranchFlowFacts branchFacts = facts.factsForIfCondition(*condition, resolver());

    assert(branchFacts.thenNarrowings.size() == 1);
    assert(branchFacts.thenNarrowings[0].resolvedName == "numberValue#0");
    assert(branchFacts.thenNarrowings[0].type.kind == StaticType::Number);
    assert(branchFacts.elseNarrowings.empty());
}

void test_equal_nil_narrows_else_branch_through_grouping()
{
    FlowFacts facts;
    const ExprPtr condition = grouped(nilCheck("stringValue", TokenType::EqualEqual));

    const BranchFlowFacts branchFacts = facts.factsForIfCondition(*condition, resolver());

    assert(branchFacts.thenNarrowings.empty());
    assert(branchFacts.elseNarrowings.size() == 1);
    assert(branchFacts.elseNarrowings[0].resolvedName == "stringValue#1");
    assert(branchFacts.elseNarrowings[0].type.kind == StaticType::String);
}

void test_logical_and_combines_then_facts()
{
    FlowFacts facts;
    const ExprPtr condition = logical(
        nilCheck("numberValue", TokenType::BangEqual),
        TokenType::AmpersandAmpersand,
        nilCheck("stringValue", TokenType::BangEqual));

    const BranchFlowFacts branchFacts = facts.factsForIfCondition(*condition, resolver());

    assert(branchFacts.thenNarrowings.size() == 2);
    assert(branchFacts.thenNarrowings[0].resolvedName == "numberValue#0");
    assert(branchFacts.thenNarrowings[1].resolvedName == "stringValue#1");
    assert(branchFacts.elseNarrowings.empty());
}

void test_logical_or_combines_else_facts()
{
    FlowFacts facts;
    const ExprPtr condition = logical(
        nilCheck("numberValue", TokenType::EqualEqual),
        TokenType::PipePipe,
        nilCheck("stringValue", TokenType::EqualEqual));

    const BranchFlowFacts branchFacts = facts.factsForIfCondition(*condition, resolver());

    assert(branchFacts.thenNarrowings.empty());
    assert(branchFacts.elseNarrowings.size() == 2);
    assert(branchFacts.elseNarrowings[0].resolvedName == "numberValue#0");
    assert(branchFacts.elseNarrowings[1].resolvedName == "stringValue#1");
}

void test_non_narrowable_variable_produces_no_facts()
{
    FlowFacts facts;
    const ExprPtr condition = nilCheck("dynamicValue", TokenType::BangEqual);

    const BranchFlowFacts branchFacts = facts.factsForIfCondition(*condition, resolver());

    assert(branchFacts.thenNarrowings.empty());
    assert(branchFacts.elseNarrowings.empty());
}

void test_with_narrowings_restores_stack_after_success_and_throw()
{
    FlowFacts facts;
    const std::vector<FlowNarrowing> outer{{"value#0", simpleType(StaticType::Number)}};
    const std::vector<FlowNarrowing> inner{{"value#0", simpleType(StaticType::String)}};

    facts.withNarrowings(outer, [&]() {
        const std::optional<TypeInfo> narrowedOuter = facts.narrowedTypeFor("value#0");
        assert(narrowedOuter.has_value());
        assert(narrowedOuter->kind == StaticType::Number);

        facts.withNarrowings(inner, [&]() {
            const std::optional<TypeInfo> narrowedInner = facts.narrowedTypeFor("value#0");
            assert(narrowedInner.has_value());
            assert(narrowedInner->kind == StaticType::String);
        });

        const std::optional<TypeInfo> restoredOuter = facts.narrowedTypeFor("value#0");
        assert(restoredOuter.has_value());
        assert(restoredOuter->kind == StaticType::Number);
    });

    assert(!facts.narrowedTypeFor("value#0").has_value());

    bool threw = false;
    try {
        facts.withNarrowings(outer, []() {
            throw 7;
        });
    } catch (int value) {
        threw = value == 7;
    }

    assert(threw);
    assert(!facts.narrowedTypeFor("value#0").has_value());
}

} // namespace

int main()
{
    test_not_nil_narrows_then_branch();
    test_equal_nil_narrows_else_branch_through_grouping();
    test_logical_and_combines_then_facts();
    test_logical_or_combines_else_facts();
    test_non_narrowable_variable_produces_no_facts();
    test_with_narrowings_restores_stack_after_success_and_throw();
}
```

- [ ] **Step 3: Register the failing test target in CMake**

Modify `CMakeLists.txt` so the file contains this block after the `frontend_session_tests` test registration:

```cmake
add_executable(flow_facts_tests
    tests/flow_facts_tests.cpp
    src/Ast.cpp
    src/FlowFacts.cpp
    src/TypeUtils.cpp
)
target_include_directories(flow_facts_tests PRIVATE include)
add_test(NAME flow_facts COMMAND flow_facts_tests)
```

Do not add `src/FlowFacts.cpp` to the main `compiler_design` target yet in this step.

- [ ] **Step 4: Run the focused build to verify the test fails for the expected reason**

Run:

```sh
cmake -S . -B build
cmake --build build --target flow_facts_tests
```

Expected: build fails because `include/FlowFacts.hpp` or `src/FlowFacts.cpp` does not exist yet.

---

### Task 2: Implement the standalone FlowFacts component

**Files:**
- Create: `include/FlowFacts.hpp`
- Create: `src/FlowFacts.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/flow_facts_tests.cpp`

- [ ] **Step 1: Add the FlowFacts public header**

Create `include/FlowFacts.hpp` with this exact content:

```cpp
#pragma once

#include "Ast.hpp"
#include "TypeUtils.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

struct FlowNarrowing {
    std::string resolvedName;
    TypeInfo type;
};

struct BranchFlowFacts {
    std::vector<FlowNarrowing> thenNarrowings;
    std::vector<FlowNarrowing> elseNarrowings;
};

class FlowFacts {
public:
    using VariableNarrowingResolver = std::function<std::optional<FlowNarrowing>(const VariableExpr&)>;

    void clear();
    BranchFlowFacts factsForIfCondition(
        const Expr& condition,
        const VariableNarrowingResolver& resolveVariableNarrowing) const;
    std::optional<TypeInfo> narrowedTypeFor(const std::string& resolvedName) const;
    void withNarrowings(
        const std::vector<FlowNarrowing>& narrowings,
        const std::function<void()>& body);

private:
    std::vector<FlowNarrowing> activeNarrowings_;
};
```

- [ ] **Step 2: Add the FlowFacts implementation**

Create `src/FlowFacts.cpp` with this exact content:

```cpp
#include "FlowFacts.hpp"

#include "Token.hpp"

#include <cstddef>
#include <utility>

namespace {

const Expr& ungrouped(const Expr& expression)
{
    const Expr* current = &expression;
    while (const auto* grouping = dynamic_cast<const GroupingExpr*>(current)) {
        current = grouping->expression.get();
    }
    return *current;
}

const VariableExpr* nilCheckedVariable(const Expr& left, const Expr& right)
{
    const auto* leftVariable = dynamic_cast<const VariableExpr*>(&left);
    const auto* rightLiteral = dynamic_cast<const LiteralExpr*>(&right);
    if (leftVariable && rightLiteral && rightLiteral->value == "nil") {
        return leftVariable;
    }

    const auto* rightVariable = dynamic_cast<const VariableExpr*>(&right);
    const auto* leftLiteral = dynamic_cast<const LiteralExpr*>(&left);
    if (rightVariable && leftLiteral && leftLiteral->value == "nil") {
        return rightVariable;
    }

    return nullptr;
}

class NarrowingStackGuard {
public:
    NarrowingStackGuard(std::vector<FlowNarrowing>& activeNarrowings, std::size_t savedSize)
        : activeNarrowings_(activeNarrowings)
        , savedSize_(savedSize)
    {
    }

    ~NarrowingStackGuard()
    {
        activeNarrowings_.resize(savedSize_);
    }

    NarrowingStackGuard(const NarrowingStackGuard&) = delete;
    NarrowingStackGuard& operator=(const NarrowingStackGuard&) = delete;

private:
    std::vector<FlowNarrowing>& activeNarrowings_;
    std::size_t savedSize_;
};

} // namespace

void FlowFacts::clear()
{
    activeNarrowings_.clear();
}

BranchFlowFacts FlowFacts::factsForIfCondition(
    const Expr& condition,
    const VariableNarrowingResolver& resolveVariableNarrowing) const
{
    const Expr& narrowedCondition = ungrouped(condition);

    if (const auto* logical = dynamic_cast<const LogicalExpr*>(&narrowedCondition)) {
        const BranchFlowFacts left = factsForIfCondition(*logical->left, resolveVariableNarrowing);
        const BranchFlowFacts right = factsForIfCondition(*logical->right, resolveVariableNarrowing);

        BranchFlowFacts result;
        if (logical->op.type == TokenType::AmpersandAmpersand) {
            result.thenNarrowings = left.thenNarrowings;
            result.thenNarrowings.insert(
                result.thenNarrowings.end(),
                right.thenNarrowings.begin(),
                right.thenNarrowings.end());
        } else if (logical->op.type == TokenType::PipePipe) {
            result.elseNarrowings = left.elseNarrowings;
            result.elseNarrowings.insert(
                result.elseNarrowings.end(),
                right.elseNarrowings.begin(),
                right.elseNarrowings.end());
        }
        return result;
    }

    const auto* binary = dynamic_cast<const BinaryExpr*>(&narrowedCondition);
    if (!binary || (binary->op.type != TokenType::BangEqual && binary->op.type != TokenType::EqualEqual)) {
        return BranchFlowFacts{};
    }

    const VariableExpr* variable = nilCheckedVariable(*binary->left, *binary->right);
    if (!variable) {
        return BranchFlowFacts{};
    }

    std::optional<FlowNarrowing> narrowing = resolveVariableNarrowing(*variable);
    if (!narrowing) {
        return BranchFlowFacts{};
    }

    BranchFlowFacts result;
    if (binary->op.type == TokenType::BangEqual) {
        result.thenNarrowings.push_back(std::move(*narrowing));
    } else {
        result.elseNarrowings.push_back(std::move(*narrowing));
    }
    return result;
}

std::optional<TypeInfo> FlowFacts::narrowedTypeFor(const std::string& resolvedName) const
{
    for (auto it = activeNarrowings_.rbegin(); it != activeNarrowings_.rend(); ++it) {
        if (it->resolvedName == resolvedName) {
            return it->type;
        }
    }
    return std::nullopt;
}

void FlowFacts::withNarrowings(
    const std::vector<FlowNarrowing>& narrowings,
    const std::function<void()>& body)
{
    if (narrowings.empty()) {
        body();
        return;
    }

    const std::size_t savedSize = activeNarrowings_.size();
    activeNarrowings_.insert(activeNarrowings_.end(), narrowings.begin(), narrowings.end());
    NarrowingStackGuard guard(activeNarrowings_, savedSize);
    body();
}
```

- [ ] **Step 3: Add `src/FlowFacts.cpp` to the main compiler target**

Modify the `add_executable(compiler_design ...)` source list in `CMakeLists.txt` by inserting `src/FlowFacts.cpp` after `src/FrontendSession.cpp`:

```cmake
    src/FrontendSession.cpp
    src/FlowFacts.cpp
    src/IR.cpp
```

- [ ] **Step 4: Run the focused FlowFacts test**

Run:

```sh
cmake -S . -B build
cmake --build build --target flow_facts_tests
ctest --test-dir build --output-on-failure -R '^flow_facts$'
```

Expected: build succeeds and CTest reports the `flow_facts` test passed.

- [ ] **Step 5: Commit the standalone component**

Run:

```sh
git add CMakeLists.txt include/FlowFacts.hpp src/FlowFacts.cpp tests/flow_facts_tests.cpp
git commit -m "refactor: add flow facts helper"
```

Expected: commit succeeds.

---

### Task 3: Integrate FlowFacts into TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: `tests/golden/nullable_if_narrowing/input.cd`, `tests/golden/nullable_if_else_narrowing/input.cd`, `tests/golden/nullable_logical_narrowing/input.cd`, `tests/golden/nullable_logical_multi_narrowing/input.cd`

- [ ] **Step 1: Update the TypeChecker header includes and private declarations**

In `include/TypeChecker.hpp`, add this include near the top:

```cpp
#include "FlowFacts.hpp"
```

Remove these private structs from `TypeChecker`:

```cpp
    struct Narrowing {
        std::string resolvedName;
        TypeInfo type;
    };

    struct IfNarrowing {
        std::vector<Narrowing> thenNarrowings;
        std::vector<Narrowing> elseNarrowings;
    };
```

Replace these private helper declarations:

```cpp
    std::optional<Narrowing> nonNilNarrowingForVariable(const VariableExpr& variable);
    IfNarrowing ifNarrowing(const Expr& condition);
    void withNarrowings(const std::vector<Narrowing>& narrowings, const std::function<void()>& body);
```

with this declaration:

```cpp
    std::optional<FlowNarrowing> nonNilNarrowingForVariable(const VariableExpr& variable) const;
```

Replace this private member:

```cpp
    std::vector<Narrowing> narrowings_;
```

with this member:

```cpp
    FlowFacts flowFacts_;
```

- [ ] **Step 2: Clear flow facts at the start of each type-check run**

In `src/TypeChecker.cpp`, inside `TypeChecker::check`, after `returnContexts_.clear();`, add:

```cpp
    flowFacts_.clear();
```

The reset block should end like this:

```cpp
    functionDepth_ = 0;
    loopDepth_ = 0;
    returnContexts_.clear();
    flowFacts_.clear();
```

- [ ] **Step 3: Delegate if-statement narrowing to FlowFacts**

In `src/TypeChecker.cpp`, replace the `ifStmt` branch inside `TypeChecker::checkStatement` with this exact block:

```cpp
    if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&statement)) {
        checkExpression(*ifStmt->condition);
        const BranchFlowFacts branchFacts = flowFacts_.factsForIfCondition(
            *ifStmt->condition,
            [this](const VariableExpr& variable) {
                return nonNilNarrowingForVariable(variable);
            });
        flowFacts_.withNarrowings(branchFacts.thenNarrowings, [&]() {
            checkStatement(*ifStmt->thenBranch);
        });
        if (ifStmt->elseBranch) {
            flowFacts_.withNarrowings(branchFacts.elseNarrowings, [&]() {
                checkStatement(*ifStmt->elseBranch);
            });
        }
        return;
    }
```

- [ ] **Step 4: Delegate active narrowed-type lookup**

Replace `TypeChecker::variableType` with this exact implementation:

```cpp
TypeInfo TypeChecker::variableType(const Binding& binding) const
{
    if (std::optional<TypeInfo> narrowed = flowFacts_.narrowedTypeFor(binding.resolvedName)) {
        return *narrowed;
    }
    return binding.type;
}
```

- [ ] **Step 5: Keep only the TypeChecker-owned resolver helper**

Replace the existing `nonNilNarrowingForVariable`, `ifNarrowing`, and `withNarrowings` implementations in `src/TypeChecker.cpp` with this exact implementation:

```cpp
std::optional<FlowNarrowing> TypeChecker::nonNilNarrowingForVariable(const VariableExpr& variable) const
{
    const Binding* binding = findVariable(variable.name.lexeme);
    if (!binding || !isNullable(binding->type)) {
        return std::nullopt;
    }
    return FlowNarrowing{binding->resolvedName, *binding->type.nullableOf};
}
```

The functions `TypeChecker::ifNarrowing` and `TypeChecker::withNarrowings` should no longer exist after this edit.

- [ ] **Step 6: Run focused build and nullable behavior checks**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design flow_facts_tests
ctest --test-dir build --output-on-failure -R '^flow_facts$'
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_if_narrowing
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_if_else_narrowing
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_logical_narrowing
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_logical_multi_narrowing
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_narrowing_outside_branch
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_narrowing_field_unsupported
```

Expected: all commands pass. No golden files change.

- [ ] **Step 7: Check for leftover TypeChecker-owned narrowing code**

Run:

```sh
grep -R "Narrowing\|IfNarrowing\|narrowings_\|ifNarrowing\|withNarrowings" -n include/TypeChecker.hpp src/TypeChecker.cpp include/FlowFacts.hpp src/FlowFacts.cpp
```

Expected: matches appear in `include/FlowFacts.hpp` and `src/FlowFacts.cpp`; `include/TypeChecker.hpp` and `src/TypeChecker.cpp` should only contain `FlowNarrowing`, `BranchFlowFacts`, `nonNilNarrowingForVariable`, `factsForIfCondition`, and `flowFacts_.withNarrowings` references.

- [ ] **Step 8: Commit the TypeChecker integration**

Run:

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "refactor: delegate narrowing facts from type checker"
```

Expected: commit succeeds.

---

### Task 4: Run full verification and clean generated files

**Files:**
- Verify: whole repository
- Clean: `tests/__pycache__/` if Python creates it

- [ ] **Step 1: Run the full required verification suite**

Run these commands from the repository root:

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
```

Expected: every test command exits with code 0.

- [ ] **Step 2: Confirm no golden files or generated files changed**

Run:

```sh
git status --short
```

Expected: no modified golden files, no `tests/__pycache__/`, and no build outputs listed. If `git status --short` shows only intentional source/doc changes from a missed commit, commit them. If it shows golden output changes, inspect them as regressions rather than refreshing expected files.

- [ ] **Step 3: Record final verification in the implementation response**

When reporting completion, include the exact commands run and their pass/fail results. Do not claim the work is complete until the verification commands in Step 1 have succeeded.

---

## Self-Review Notes

- Spec coverage: Task 2 creates the standalone flow-fact subsystem; Task 3 removes active narrowing stack and condition-shape analysis from `TypeChecker`; Task 4 runs the full behavior-preserving verification suite.
- Behavior scope: no new nullable behavior is introduced; the focused tests mirror the existing `if`, grouping, `&&`, and `||` narrowing rules.
- Type consistency: the plan uses `FlowNarrowing`, `BranchFlowFacts`, `FlowFacts::factsForIfCondition`, `FlowFacts::narrowedTypeFor`, and `FlowFacts::withNarrowings` consistently across header, implementation, tests, and `TypeChecker` integration.
