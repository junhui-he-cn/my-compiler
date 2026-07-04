# Function Value Arity Inference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Track known function arity through function-valued expressions and bindings so variable calls, direct lambda calls, and function-value assignments can be checked statically when arity is known.

**Architecture:** Extend the type checker with a small internal `CheckedExpression` result containing `StaticType` plus optional function arity. Keep `StaticType::Function` unchanged, keep `checkExpression()` as a compatibility wrapper returning only the type, and use the richer result only in let declarations, assignments, and call checking. No parser, grammar, IR, bytecode, VM, or runtime value changes are required.

**Tech Stack:** C++17, existing AST/type checker, existing IR and bytecode backends, Python golden tests.

---

## File Structure

Modify:

- `include/TypeChecker.hpp` — add `CheckedExpression`, richer expression-checking declarations, and `declareVariable()` arity support for `LetStmt`.
- `src/TypeChecker.cpp` — propagate function arity through function expressions, variable reads, let declarations, assignments, and calls.
- `README.md` — document known function value arity checking and remaining function type limitations.
- `docs/roadmap.md` — mark Phase 9B function value arity inference as implemented/progress.
- `AGENTS.md` — update current language semantics for function value arity checks.

Create type-error fixtures:

- `tests/golden/type_errors/lambda_variable_wrong_arity.cd`
- `tests/golden/type_errors/lambda_variable_wrong_arity.err`
- `tests/golden/type_errors/lambda_variable_wrong_arity.exit`
- `tests/golden/type_errors/lambda_direct_wrong_arity.cd`
- `tests/golden/type_errors/lambda_direct_wrong_arity.err`
- `tests/golden/type_errors/lambda_direct_wrong_arity.exit`
- `tests/golden/type_errors/function_value_wrong_arity.cd`
- `tests/golden/type_errors/function_value_wrong_arity.err`
- `tests/golden/type_errors/function_value_wrong_arity.exit`
- `tests/golden/type_errors/function_assignment_arity_mismatch.cd`
- `tests/golden/type_errors/function_assignment_arity_mismatch.err`
- `tests/golden/type_errors/function_assignment_arity_mismatch.exit`

Create success fixtures:

- `tests/golden/function_value_arity_success/input.cd`
- `tests/golden/function_value_arity_success/run.out`
- `tests/golden/function_value_arity_success/run_bytecode.out`
- `tests/golden/function_value_unknown_arity_assignment/input.cd`
- `tests/golden/function_value_unknown_arity_assignment/run.out`
- `tests/golden/function_value_unknown_arity_assignment/run_bytecode.out`

Do not modify:

- Parser, AST, grammar, IR, bytecode, VM, runtime `Value`, or runtime function value representation.

Reference:

- `docs/superpowers/specs/2026-07-04-function-value-arity-inference-design.md`
- `include/TypeChecker.hpp`
- `src/TypeChecker.cpp::checkExpression()`
- `src/TypeChecker.cpp::checkCall()`
- `src/TypeChecker.cpp::checkLetInitializer()`
- `tests/golden/type_errors/function_wrong_arity.*`
- `tests/golden/lambda_basic/`

---

### Task 1: Add Failing Type-Error Coverage for Known Function Arity

**Files:**
- Create: `tests/golden/type_errors/lambda_variable_wrong_arity.cd`
- Create: `tests/golden/type_errors/lambda_variable_wrong_arity.err`
- Create: `tests/golden/type_errors/lambda_variable_wrong_arity.exit`
- Create: `tests/golden/type_errors/lambda_direct_wrong_arity.cd`
- Create: `tests/golden/type_errors/lambda_direct_wrong_arity.err`
- Create: `tests/golden/type_errors/lambda_direct_wrong_arity.exit`
- Create: `tests/golden/type_errors/function_value_wrong_arity.cd`
- Create: `tests/golden/type_errors/function_value_wrong_arity.err`
- Create: `tests/golden/type_errors/function_value_wrong_arity.exit`
- Create: `tests/golden/type_errors/function_assignment_arity_mismatch.cd`
- Create: `tests/golden/type_errors/function_assignment_arity_mismatch.err`
- Create: `tests/golden/type_errors/function_assignment_arity_mismatch.exit`

- [ ] **Step 1: Add lambda variable wrong-arity fixture**

Create `tests/golden/type_errors/lambda_variable_wrong_arity.cd`:

```cd
let f = fun (x) {
  return x;
};
print f();
```

Create `tests/golden/type_errors/lambda_variable_wrong_arity.err`:

```text
Type error at 4:9: expected 1 arguments but got 0
```

Create `tests/golden/type_errors/lambda_variable_wrong_arity.exit`:

```text
1
```

- [ ] **Step 2: Add direct lambda wrong-arity fixture**

Create `tests/golden/type_errors/lambda_direct_wrong_arity.cd`:

```cd
print (fun (x) {
  return x;
})(1, 2);
```

Create `tests/golden/type_errors/lambda_direct_wrong_arity.err`:

```text
Type error at 3:7: expected 1 arguments but got 2
```

Create `tests/golden/type_errors/lambda_direct_wrong_arity.exit`:

```text
1
```

- [ ] **Step 3: Add named function value wrong-arity fixture**

Create `tests/golden/type_errors/function_value_wrong_arity.cd`:

```cd
fun add(a, b) {
  return a + b;
}
let f = add;
print f(1);
```

Create `tests/golden/type_errors/function_value_wrong_arity.err`:

```text
Type error at 5:10: expected 2 arguments but got 1
```

Create `tests/golden/type_errors/function_value_wrong_arity.exit`:

```text
1
```

- [ ] **Step 4: Add function assignment arity mismatch fixture**

Create `tests/golden/type_errors/function_assignment_arity_mismatch.cd`:

```cd
let f = fun (x) {
  return x;
};
f = fun (x, y) {
  return x + y;
};
```

Create `tests/golden/type_errors/function_assignment_arity_mismatch.err`:

```text
Type error at 4:1: cannot assign function with 2 parameters to `f` of type function with 1 parameters
```

Create `tests/golden/type_errors/function_assignment_arity_mismatch.exit`:

```text
1
```

- [ ] **Step 5: Run golden tests and observe expected failures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected before implementation: the new type-error fixtures fail because the compiler currently allows these programs to reach runtime or finish without the new static diagnostics. If exact columns differ after implementation, update only these new `.err` files to match current diagnostic locations.

- [ ] **Step 6: Commit failing fixtures**

Run:

```bash
git add tests/golden/type_errors/lambda_variable_wrong_arity.* \
        tests/golden/type_errors/lambda_direct_wrong_arity.* \
        tests/golden/type_errors/function_value_wrong_arity.* \
        tests/golden/type_errors/function_assignment_arity_mismatch.*
git commit -m "test: cover function value arity errors"
```

---

### Task 2: Add Expression Arity Metadata to TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add `CheckedExpression` and declarations in `include/TypeChecker.hpp`**

In the private section of `class TypeChecker`, insert this struct before `struct Binding`:

```cpp
    struct CheckedExpression {
        StaticType type;
        std::optional<std::size_t> arity;
    };
```

Change the declarations:

```cpp
    Binding declareVariable(const LetStmt& statement, StaticType type);
```

to:

```cpp
    Binding declareVariable(
        const LetStmt& statement,
        StaticType type,
        std::optional<std::size_t> arity = std::nullopt);
```

Add a new expression-info helper declaration next to `checkExpression()`:

```cpp
    CheckedExpression checkExpressionInfo(const Expr& expression);
```

Change these declarations:

```cpp
    StaticType checkFunctionExpression(const FunctionExpr& expression);
    StaticType checkCall(const CallExpr& expression);
    StaticType checkLetInitializer(const LetStmt& statement);
```

to:

```cpp
    CheckedExpression checkFunctionExpression(const FunctionExpr& expression);
    CheckedExpression checkCall(const CallExpr& expression);
    CheckedExpression checkLetInitializer(const LetStmt& statement);
```

Keep `StaticType checkExpression(const Expr& expression);` as a public-to-private compatibility helper for callers that only need type.

- [ ] **Step 2: Update `declareVariable(const LetStmt&, ...)` in `src/TypeChecker.cpp`**

Replace:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(const LetStmt& statement, StaticType type)
{
    Binding binding = declareVariable(statement.name, type);
    resolvedNames_.recordLet(statement, binding.resolvedName);
    return binding;
}
```

with:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(
    const LetStmt& statement,
    StaticType type,
    std::optional<std::size_t> arity)
{
    Binding binding = declareVariable(statement.name, type, arity);
    resolvedNames_.recordLet(statement, binding.resolvedName);
    return binding;
}
```

- [ ] **Step 3: Update let statement checking to store arity**

In `TypeChecker::checkStatement()`, replace:

```cpp
    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const StaticType declared = checkLetInitializer(*let);
        declareVariable(*let, declared);
        return;
    }
```

with:

```cpp
    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const CheckedExpression declared = checkLetInitializer(*let);
        declareVariable(*let, declared.type, declared.arity);
        return;
    }
```

- [ ] **Step 4: Update `checkFunctionExpression()` return type**

Change the definition from `StaticType TypeChecker::checkFunctionExpression(...)` to `TypeChecker::CheckedExpression TypeChecker::checkFunctionExpression(...)`.

Replace its final return:

```cpp
    return StaticType::Function;
```

with:

```cpp
    return CheckedExpression{StaticType::Function, expression.parameters.size()};
```

- [ ] **Step 5: Update `checkLetInitializer()` to preserve arity**

Change the definition from `StaticType TypeChecker::checkLetInitializer(...)` to `TypeChecker::CheckedExpression TypeChecker::checkLetInitializer(...)`.

Replace the beginning of the function:

```cpp
    const StaticType initializer = checkExpression(*statement.initializer);
    if (!statement.typeName) {
        return initializer;
    }
```

with:

```cpp
    const CheckedExpression initializer = checkExpressionInfo(*statement.initializer);
    if (!statement.typeName) {
        return initializer;
    }
```

In the explicit annotation branch, replace uses of `initializer` as a type with `initializer.type`, and replace the final return:

```cpp
    return declared;
```

with:

```cpp
    return CheckedExpression{declared, std::nullopt};
```

- [ ] **Step 6: Add `checkExpression()` wrapper and rename the old body**

Change the current `StaticType TypeChecker::checkExpression(const Expr& expression)` body to a wrapper:

```cpp
StaticType TypeChecker::checkExpression(const Expr& expression)
{
    return checkExpressionInfo(expression).type;
}

TypeChecker::CheckedExpression TypeChecker::checkExpressionInfo(const Expr& expression)
{
    // old checkExpression body goes here, updated to return CheckedExpression values
}
```

In the moved body, update returns as follows:

```cpp
return StaticType::Nil;
return StaticType::Bool;
return StaticType::String;
return StaticType::Number;
```

become:

```cpp
return CheckedExpression{StaticType::Nil, std::nullopt};
return CheckedExpression{StaticType::Bool, std::nullopt};
return CheckedExpression{StaticType::String, std::nullopt};
return CheckedExpression{StaticType::Number, std::nullopt};
```

The function-expression branch should become:

```cpp
if (const auto* function = dynamic_cast<const FunctionExpr*>(&expression)) {
    return checkFunctionExpression(*function);
}
```

The variable branch should return binding type and arity:

```cpp
return CheckedExpression{binding->type, binding->arity};
```

Grouping should return `checkExpressionInfo(*grouping->expression)`.

Unary, binary, logical, array, and index branches should wrap their existing `StaticType` results with `std::nullopt` arity. For example:

```cpp
return CheckedExpression{checkUnary(*unary), std::nullopt};
```

Array branch:

```cpp
return CheckedExpression{StaticType::Array, std::nullopt};
```

Index branch:

```cpp
return CheckedExpression{checkIndex(*index), std::nullopt};
```

- [ ] **Step 7: Update assignment expression handling**

Inside the assignment branch in `checkExpressionInfo()`, replace:

```cpp
const StaticType value = checkExpression(*assign->value);
```

with:

```cpp
const CheckedExpression value = checkExpressionInfo(*assign->value);
```

Change static type checks from `value` to `value.type`.

After the existing type mismatch check and before `resolvedNames_.recordAssignment(...)`, add function arity checking and arity metadata update:

```cpp
        if (target->type == StaticType::Function && value.type == StaticType::Function
            && target->arity && value.arity && *target->arity != *value.arity) {
            throw TypeError(assign->name,
                "cannot assign function with " + std::to_string(*value.arity)
                    + " parameters to `" + assign->name.lexeme
                    + "` of type function with " + std::to_string(*target->arity) + " parameters");
        }

        if (target->type == StaticType::Function) {
            target->arity = value.type == StaticType::Function ? value.arity : std::nullopt;
        }
```

Return expression info instead of only type:

```cpp
        const StaticType resultType = isKnown(target->type) ? target->type : value.type;
        return CheckedExpression{resultType, resultType == StaticType::Function ? target->arity : std::nullopt};
```

This intentionally clears stale arity when assigning an unknown or non-function value to a function-typed binding that type checking still allows because the value type is unknown.

- [ ] **Step 8: Update `checkCall()` to use callee expression info**

Change the definition from `StaticType TypeChecker::checkCall(...)` to `TypeChecker::CheckedExpression TypeChecker::checkCall(...)`.

Replace:

```cpp
    const StaticType callee = checkExpression(*expression.callee);
    for (const auto& argument : expression.arguments) {
        checkExpression(*argument);
    }

    if (callee != StaticType::Unknown && callee != StaticType::Function) {
        throw TypeError(expression.paren, "can only call functions");
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get())) {
        const Binding* binding = findVariable(variable->name.lexeme);
        if (binding && binding->arity && *binding->arity != expression.arguments.size()) {
            throw TypeError(expression.paren, "expected " + std::to_string(*binding->arity)
                + " arguments but got " + std::to_string(expression.arguments.size()));
        }
    }

    return StaticType::Unknown;
```

with:

```cpp
    const CheckedExpression callee = checkExpressionInfo(*expression.callee);
    for (const auto& argument : expression.arguments) {
        checkExpressionInfo(*argument);
    }

    if (callee.type != StaticType::Unknown && callee.type != StaticType::Function) {
        throw TypeError(expression.paren, "can only call functions");
    }

    if (callee.arity && *callee.arity != expression.arguments.size()) {
        throw TypeError(expression.paren, "expected " + std::to_string(*callee.arity)
            + " arguments but got " + std::to_string(expression.arguments.size()));
    }

    return CheckedExpression{StaticType::Unknown, std::nullopt};
```

This replaces the old variable-only arity check with a general callee-expression arity check.

- [ ] **Step 9: Build and run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected after implementation:

- The four new type-error fixtures pass, except for possible source column differences in the new `.err` files.
- Existing `function_wrong_arity` continues to pass.
- Existing runtime and bytecode tests continue to pass.

If a new expected column differs, update only that new `.err` file to match the actual diagnostic location.

- [ ] **Step 10: Commit implementation**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp \
        tests/golden/type_errors/lambda_variable_wrong_arity.err \
        tests/golden/type_errors/lambda_direct_wrong_arity.err \
        tests/golden/type_errors/function_value_wrong_arity.err \
        tests/golden/type_errors/function_assignment_arity_mismatch.err
git commit -m "feat: infer function value arity"
```

Only the `.err` files should be staged in this commit if source column corrections were needed.

---

### Task 3: Add Success and Unknown-Arity Preservation Coverage

**Files:**
- Create: `tests/golden/function_value_arity_success/input.cd`
- Create: `tests/golden/function_value_arity_success/run.out`
- Create: `tests/golden/function_value_arity_success/run_bytecode.out`
- Create: `tests/golden/function_value_unknown_arity_assignment/input.cd`
- Create: `tests/golden/function_value_unknown_arity_assignment/run.out`
- Create: `tests/golden/function_value_unknown_arity_assignment/run_bytecode.out`

- [ ] **Step 1: Add success fixture for known function arity**

Create `tests/golden/function_value_arity_success/input.cd`:

```cd
let inc = fun (x) {
  return x + 1;
};
print inc(41);

fun add(a, b) {
  return a + b;
}
let op = add;
print op(2, 3);

inc = fun (y) {
  return y + 2;
};
print inc(40);
```

Create `tests/golden/function_value_arity_success/run.out`:

```text
42
5
42
```

Create `tests/golden/function_value_arity_success/run_bytecode.out` with the same content:

```text
42
5
42
```

- [ ] **Step 2: Add unknown-arity assignment preservation fixture**

Create `tests/golden/function_value_unknown_arity_assignment/input.cd`:

```cd
fun makeZero() {
  return fun () {
    return 42;
  };
}

let f = fun (x) {
  return x;
};
f = makeZero();
print f();
```

Create `tests/golden/function_value_unknown_arity_assignment/run.out`:

```text
42
```

Create `tests/golden/function_value_unknown_arity_assignment/run_bytecode.out` with the same content:

```text
42
```

This fixture proves assigning an unknown-arity function result clears stale arity 1 from `f`; otherwise `print f();` would be rejected statically.

- [ ] **Step 3: Run golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass.

- [ ] **Step 4: Commit success fixtures**

Run:

```bash
git add tests/golden/function_value_arity_success tests/golden/function_value_unknown_arity_assignment
git commit -m "test: cover function value arity success cases"
```

---

### Task 4: Update Documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README function/type wording**

In `README.md`, replace this paragraph:

```markdown
Functions are values. Named functions use `fun name(parameter*) { declaration* }`, and anonymous function expressions use `fun (parameter*) { declaration* }`. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Function type annotations are not implemented yet.
```

with:

```markdown
Functions are values. Named functions use `fun name(parameter*) { declaration* }`, and anonymous function expressions use `fun (parameter*) { declaration* }`. Known function values carry arity for static argument-count checks, including variables initialized from named functions or function expressions. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Function parameter types, return types, and function type annotations are not implemented yet.
```

- [ ] **Step 2: Update roadmap Phase 9 status**

In `docs/roadmap.md`, replace the Phase 9 status paragraph:

```markdown
Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Function parameter types, function return types, and array element types remain future work.
```

with:

```markdown
Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Phase 9B is implemented: known function values carry arity for static argument-count checks. Function parameter types, function return types, function type annotations, and array element types remain future work.
```

In the Phase 9 suggested future features list, replace:

```markdown
- Internal function signatures for named functions and function expressions.
```

with:

```markdown
- Full function signatures for named functions and function expressions, including parameter and return types.
```

After the existing completed first slice sentence, add:

```markdown
Completed second slice: known function values carry arity through function expressions, named function variables, same-arity assignments, and direct lambda calls.
```

- [ ] **Step 3: Update AGENTS current semantics**

In `AGENTS.md`, replace this bullet:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

with:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Known function values carry arity for static argument-count checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

- [ ] **Step 4: Verify docs mention arity boundary**

Run:

```bash
grep -n "Known function values carry arity" README.md
grep -n "Phase 9B is implemented" docs/roadmap.md
grep -n "Known function values carry arity" AGENTS.md
```

Expected: each command prints one matching line.

- [ ] **Step 5: Commit docs**

Run:

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document function value arity inference"
```

---

### Task 5: Full Verification and Final Review Prep

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
git diff --stat 5e176ec..HEAD
git diff --name-status 5e176ec..HEAD
git status --short --branch
```

Expected:

- Changes include `include/TypeChecker.hpp`, `src/TypeChecker.cpp`, new arity fixtures, README, roadmap, AGENTS, and the design/plan docs.
- No parser, grammar, AST, IR, bytecode, VM, or runtime value representation changes.
- Worktree is clean.

---

## Self-Review Checklist

- Spec coverage:
  - Function expressions expose known arity: Task 2.
  - Named functions assigned to variables preserve known arity: Task 2 and Task 3.
  - Calls through variables with known arity are checked: Task 1 and Task 2.
  - Direct lambda calls are checked: Task 1 and Task 2.
  - Same-arity function assignment succeeds: Task 3.
  - Different known arity function assignment fails: Task 1 and Task 2.
  - Unknown arity assignment clears stale arity: Task 3.
  - Runtime unknown arity behavior remains possible: Task 3.
  - Docs updated: Task 4.
- No parser, grammar, IR, bytecode, VM, or runtime value representation changes are planned.
- Full verification command is included.
