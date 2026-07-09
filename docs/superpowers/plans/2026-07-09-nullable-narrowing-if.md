# Nullable If Narrowing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add first-slice flow-sensitive nullable narrowing for direct `if` nil-checks on variables.

**Architecture:** Extend `TypeChecker` with a branch-local narrowing overlay keyed by resolved variable name. Recognize direct `x != nil`, `nil != x`, `x == nil`, and `nil == x` conditions while checking `IfStmt`; push narrowed `T` views only around the branch where non-nil is proven. No parser, AST, IR, bytecode, or runtime changes are needed.

**Tech Stack:** C++17 recursive-descent compiler front-end, Python golden tests, Rust VM parity tests for regression.

---

## File Structure

- `include/TypeChecker.hpp`: add private `Narrowing`, `IfNarrowing`, narrowing stack, and helper declarations.
- `src/TypeChecker.cpp`: implement if-condition narrowing recognition, scoped overlay push/pop, and variable-read lookup integration.
- `tests/golden/nullable_if_narrowing/`: success fixture for direct then-branch narrowing.
- `tests/golden/nullable_if_else_narrowing/`: success fixture for else-branch narrowing and nil-left conditions.
- `tests/golden/type_errors/nullable_narrowing_outside_branch.*`: boundary fixture proving narrowing does not escape.
- `tests/golden/type_errors/nullable_narrowing_field_unsupported.*`: boundary fixture proving field checks do not narrow.
- `tests/golden/type_errors/nullable_narrowing_logical_unsupported.*`: boundary fixture proving `&&` composition is out of scope.
- `README.md`, `AGENTS.md`, `docs/roadmap.md`: document implemented scope.

---

### Task 1: RED fixtures

**Files:**
- Create: `tests/golden/nullable_if_narrowing/input.cd`
- Create: `tests/golden/nullable_if_narrowing/run.out`
- Create: `tests/golden/nullable_if_narrowing/ast.out`
- Create: `tests/golden/nullable_if_narrowing/ir.out`
- Create: `tests/golden/nullable_if_else_narrowing/input.cd`
- Create: `tests/golden/nullable_if_else_narrowing/run.out`
- Create: `tests/golden/nullable_if_else_narrowing/ast.out`
- Create: `tests/golden/nullable_if_else_narrowing/ir.out`
- Create: `tests/golden/type_errors/nullable_narrowing_outside_branch.cd`
- Create: `tests/golden/type_errors/nullable_narrowing_outside_branch.err`
- Create: `tests/golden/type_errors/nullable_narrowing_outside_branch.exit`
- Create: `tests/golden/type_errors/nullable_narrowing_field_unsupported.cd`
- Create: `tests/golden/type_errors/nullable_narrowing_field_unsupported.err`
- Create: `tests/golden/type_errors/nullable_narrowing_field_unsupported.exit`
- Create: `tests/golden/type_errors/nullable_narrowing_logical_unsupported.cd`
- Create: `tests/golden/type_errors/nullable_narrowing_logical_unsupported.err`
- Create: `tests/golden/type_errors/nullable_narrowing_logical_unsupported.exit`

- [ ] **Step 1: Create then-branch success fixture**

Create `tests/golden/nullable_if_narrowing/input.cd`:

```cd
fun takesNumber(value: number): number {
  return value;
}

let maybe: number? = 3;
if (maybe != nil) {
  print takesNumber(maybe);
}
```

Create `tests/golden/nullable_if_narrowing/run.out`:

```text
3
```

Create empty expected files:

```bash
: > tests/golden/nullable_if_narrowing/ast.out
: > tests/golden/nullable_if_narrowing/ir.out
```

- [ ] **Step 2: Create else-branch and nil-left success fixture**

Create `tests/golden/nullable_if_else_narrowing/input.cd`:

```cd
fun takesString(value: string): string {
  return value;
}

let name: string? = "Ada";
if (name == nil) {
  print "missing";
} else {
  print takesString(name);
}

let title: string? = "Dr";
if (nil != title) {
  print takesString(title);
}
```

Create `tests/golden/nullable_if_else_narrowing/run.out`:

```text
Ada
Dr
```

Create empty expected files:

```bash
: > tests/golden/nullable_if_else_narrowing/ast.out
: > tests/golden/nullable_if_else_narrowing/ir.out
```

- [ ] **Step 3: Create boundary type-error fixtures**

Create `tests/golden/type_errors/nullable_narrowing_outside_branch.cd`:

```cd
fun takesNumber(value: number): number { return value; }
let maybe: number? = 1;
if (maybe != nil) {
  print takesNumber(maybe);
}
let value: number = maybe;
```

Create `tests/golden/type_errors/nullable_narrowing_outside_branch.err`:

```text
Type error at 6:5: cannot initialize `value` of type number with number?
```

Create `tests/golden/type_errors/nullable_narrowing_outside_branch.exit`:

```text
1
```

Create `tests/golden/type_errors/nullable_narrowing_field_unsupported.cd`:

```cd
struct Box { value: number? }
fun takesNumber(value: number): number { return value; }
let box: Box = Box { value: 1 };
if (box.value != nil) {
  print takesNumber(box.value);
}
```

Create `tests/golden/type_errors/nullable_narrowing_field_unsupported.err`:

```text
Type error at 5:21: argument 1 expects number, got number?
```

Create `tests/golden/type_errors/nullable_narrowing_field_unsupported.exit`:

```text
1
```

Create `tests/golden/type_errors/nullable_narrowing_logical_unsupported.cd`:

```cd
fun takesNumber(value: number): number { return value; }
let maybe: number? = 1;
let ok = true;
if (maybe != nil && ok) {
  print takesNumber(maybe);
}
```

Create `tests/golden/type_errors/nullable_narrowing_logical_unsupported.err`:

```text
Type error at 5:21: argument 1 expects number, got number?
```

Create `tests/golden/type_errors/nullable_narrowing_logical_unsupported.exit`:

```text
1
```

- [ ] **Step 4: Verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Success fixtures fail with type errors because nullable values are not narrowed yet; boundary fixtures should pass or remain compatible with existing behavior.

- [ ] **Step 5: Commit RED fixtures**

```bash
git add tests/golden/nullable_if_narrowing tests/golden/nullable_if_else_narrowing \
        tests/golden/type_errors/nullable_narrowing_*
git commit -m "test: add nullable if narrowing fixtures"
```

---

### Task 2: TypeChecker narrowing overlay

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add narrowing declarations**

In `include/TypeChecker.hpp`, inside private `TypeChecker`, add after `IndexTargetTypes`:

```cpp
    struct Narrowing {
        std::string resolvedName;
        TypeInfo type;
    };

    struct IfNarrowing {
        std::optional<Narrowing> thenNarrowing;
        std::optional<Narrowing> elseNarrowing;
    };
```

Add helper declarations near expression helpers:

```cpp
    TypeInfo variableType(const Binding& binding) const;
    std::optional<Narrowing> nonNilNarrowingForVariable(const VariableExpr& variable);
    IfNarrowing ifNarrowing(const Expr& condition);
    void withOptionalNarrowing(const std::optional<Narrowing>& narrowing, const std::function<void()>& body);
```

Add member storage near other stacks:

```cpp
    std::vector<Narrowing> narrowings_;
```

Add `#include <functional>` to the header.

- [ ] **Step 2: Use narrowed type for variable reads**

In `src/TypeChecker.cpp`, add:

```cpp
TypeInfo TypeChecker::variableType(const Binding& binding) const
{
    for (auto it = narrowings_.rbegin(); it != narrowings_.rend(); ++it) {
        if (it->resolvedName == binding.resolvedName) {
            return it->type;
        }
    }
    return binding.type;
}
```

Then change the `VariableExpr` branch in `checkExpressionInfo` from:

```cpp
return CheckedExpression{binding->type};
```

to:

```cpp
return CheckedExpression{variableType(*binding)};
```

- [ ] **Step 3: Recognize direct nil-check conditions**

Add helpers near `variableType`:

```cpp
std::optional<TypeChecker::Narrowing> TypeChecker::nonNilNarrowingForVariable(const VariableExpr& variable)
{
    const Binding* binding = findVariable(variable.name.lexeme);
    if (!binding || !isNullable(binding->type)) {
        return std::nullopt;
    }
    return Narrowing{binding->resolvedName, *binding->type.nullableOf};
}

TypeChecker::IfNarrowing TypeChecker::ifNarrowing(const Expr& condition)
{
    const auto* binary = dynamic_cast<const BinaryExpr*>(&condition);
    if (!binary || (binary->op.type != TokenType::BangEqual && binary->op.type != TokenType::EqualEqual)) {
        return IfNarrowing{};
    }

    auto nilCheckedVariable = [](const Expr& left, const Expr& right) -> const VariableExpr* {
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
    };

    const VariableExpr* variable = nilCheckedVariable(*binary->left, *binary->right);
    if (!variable) {
        return IfNarrowing{};
    }

    std::optional<Narrowing> narrowing = nonNilNarrowingForVariable(*variable);
    if (!narrowing) {
        return IfNarrowing{};
    }

    IfNarrowing result;
    if (binary->op.type == TokenType::BangEqual) {
        result.thenNarrowing = std::move(narrowing);
    } else {
        result.elseNarrowing = std::move(narrowing);
    }
    return result;
}
```

- [ ] **Step 4: Scope narrowing around if branches**

Add:

```cpp
void TypeChecker::withOptionalNarrowing(const std::optional<Narrowing>& narrowing, const std::function<void()>& body)
{
    if (!narrowing) {
        body();
        return;
    }
    narrowings_.push_back(*narrowing);
    body();
    narrowings_.pop_back();
}
```

Change the `IfStmt` branch in `checkStatement` from:

```cpp
checkExpression(*ifStmt->condition);
checkStatement(*ifStmt->thenBranch);
if (ifStmt->elseBranch) {
    checkStatement(*ifStmt->elseBranch);
}
```

to:

```cpp
checkExpression(*ifStmt->condition);
const IfNarrowing narrowing = ifNarrowing(*ifStmt->condition);
withOptionalNarrowing(narrowing.thenNarrowing, [&]() {
    checkStatement(*ifStmt->thenBranch);
});
if (ifStmt->elseBranch) {
    withOptionalNarrowing(narrowing.elseNarrowing, [&]() {
        checkStatement(*ifStmt->elseBranch);
    });
}
```

- [ ] **Step 5: Build and verify GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --update
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS. Only the new success fixture `ast.out` and `ir.out` files should be populated; no broad unrelated golden rewrites should be committed.

- [ ] **Step 6: Commit implementation**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp \
        tests/golden/nullable_if_narrowing tests/golden/nullable_if_else_narrowing \
        tests/golden/type_errors/nullable_narrowing_*
git commit -m "feat: narrow nullable variables in if branches"
```

---

### Task 3: Documentation and final verification

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README nullable paragraph**

In `README.md`, change the nullable sentence that says `T?` cannot be assigned back without a future narrowing feature to state:

```markdown
A value of type `T?` may be either `nil` or a `T`; `nil` is not assignable to non-nullable `T`, and `T?` is not assignable back to `T` except inside direct `if` nil-check branches such as `if (x != nil) { ... }` or the `else` branch of `if (x == nil) { ... } else { ... }` for simple variables.
```

Also replace `Generic types and flow-sensitive nullable narrowing are not implemented yet.` with:

```markdown
Generic types and broader flow-sensitive nullable narrowing beyond direct `if` nil-checks are not implemented yet.
```

- [ ] **Step 2: Update AGENTS.md semantics**

In `AGENTS.md`, update the nullable bullet to mention direct `if` nil-check narrowing for simple variables only, and keep limitations for fields, arrays, logical compositions, and loops explicit.

- [ ] **Step 3: Update roadmap status**

In `docs/roadmap.md`, update Phase 9 and Phase 15F status to say direct `if` nil-check nullable narrowing is implemented as the first slice, while broader narrowing remains future work.

- [ ] **Step 4: Commit docs**

```bash
git add README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document nullable if narrowing"
```

- [ ] **Step 5: Run full verification**

Run:

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
git status --short
```

Expected: every command exits 0 and final git status is clean.

---

## Self-Review

- Spec coverage: The plan covers direct `if` nil-check recognition, branch-local non-nil narrowing, success and boundary tests, docs, and full verification.
- Placeholder scan: No placeholder tasks remain; fixture contents and commands are explicit.
- Scope check: The plan excludes loops, fields, logical compositions, and broad dataflow as required by the design.
- Type consistency: Helper names and structs are consistently declared in `include/TypeChecker.hpp` and implemented in `src/TypeChecker.cpp`.
