# Function Return Type Inference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Infer known function return types internally so calls to known function values produce useful static types for existing `let` inference and assignment checks.

**Architecture:** Extend `TypeChecker` metadata from `{type, arity}` to `{type, arity, returnType}` for function-valued expressions and bindings. Add a small return-context stack used only while checking function bodies; return statements contribute static types to the current context, and calls return the callee's known return type. No parser, AST, grammar, IR, bytecode, VM, or runtime value representation changes are required.

**Tech Stack:** C++17, existing recursive-descent compiler frontend, existing `TypeChecker`, Python golden tests.

---

## File Structure

Modify:

- `include/TypeChecker.hpp` — add return metadata to `CheckedExpression` and `Binding`, add return-context declarations, and extend `declareVariable()` signatures.
- `src/TypeChecker.cpp` — merge return types, track return statements in function contexts, preserve function return metadata through variables/lets/assignments/calls.
- `README.md` — document known function return type inference and remaining function typing limitations.
- `docs/roadmap.md` — mark Phase 9C as implemented after code lands.
- `AGENTS.md` — update current language semantics with return type metadata.

Create type-error fixtures:

- `tests/golden/type_errors/function_return_assignment_type_mismatch.cd`
- `tests/golden/type_errors/function_return_assignment_type_mismatch.err`
- `tests/golden/type_errors/function_return_assignment_type_mismatch.exit`
- `tests/golden/type_errors/lambda_return_assignment_type_mismatch.cd`
- `tests/golden/type_errors/lambda_return_assignment_type_mismatch.err`
- `tests/golden/type_errors/lambda_return_assignment_type_mismatch.exit`
- `tests/golden/type_errors/function_assignment_updates_return_type.cd`
- `tests/golden/type_errors/function_assignment_updates_return_type.err`
- `tests/golden/type_errors/function_assignment_updates_return_type.exit`

Create success fixtures:

- `tests/golden/function_return_type_success/input.cd`
- `tests/golden/function_return_type_success/run.out`
- `tests/golden/function_return_type_success/run_bytecode.out`
- `tests/golden/function_return_type_unknown_preserved/input.cd`
- `tests/golden/function_return_type_unknown_preserved/run.out`
- `tests/golden/function_return_type_unknown_preserved/run_bytecode.out`

Do not modify:

- `include/Ast.hpp`, `src/Ast.cpp`
- `include/Parser.hpp`, `src/Parser.cpp`
- `docs/language-grammar.ebnf`
- `include/IR.hpp`, `src/IR.cpp`, `include/IRCompiler.hpp`, `src/IRCompiler.cpp`
- bytecode compiler/VM files
- `include/Value.hpp`, `src/Value.cpp`

Reference:

- `docs/superpowers/specs/2026-07-04-function-return-type-inference-design.md`
- `include/TypeChecker.hpp`
- `src/TypeChecker.cpp::checkFunction()`
- `src/TypeChecker.cpp::checkFunctionExpression()`
- `src/TypeChecker.cpp::checkExpressionInfo()`
- `src/TypeChecker.cpp::checkCall()`
- `tests/golden/type_errors/inferred_let_assignment_mismatch.*`
- `tests/golden/function_value_arity_success/`

---

### Task 1: Add Failing Type-Error Coverage for Known Function Return Types

**Files:**
- Create: `tests/golden/type_errors/function_return_assignment_type_mismatch.cd`
- Create: `tests/golden/type_errors/function_return_assignment_type_mismatch.err`
- Create: `tests/golden/type_errors/function_return_assignment_type_mismatch.exit`
- Create: `tests/golden/type_errors/lambda_return_assignment_type_mismatch.cd`
- Create: `tests/golden/type_errors/lambda_return_assignment_type_mismatch.err`
- Create: `tests/golden/type_errors/lambda_return_assignment_type_mismatch.exit`
- Create: `tests/golden/type_errors/function_assignment_updates_return_type.cd`
- Create: `tests/golden/type_errors/function_assignment_updates_return_type.err`
- Create: `tests/golden/type_errors/function_assignment_updates_return_type.exit`

- [ ] **Step 1: Add named function return mismatch fixture**

Create `tests/golden/type_errors/function_return_assignment_type_mismatch.cd`:

```cd
fun one() {
  return 1;
}
let x = one();
x = "bad";
```

Create `tests/golden/type_errors/function_return_assignment_type_mismatch.err`:

```text
Type error at 5:1: cannot assign string to `x` of type number
```

Create `tests/golden/type_errors/function_return_assignment_type_mismatch.exit`:

```text
1
```

- [ ] **Step 2: Add lambda return mismatch fixture**

Create `tests/golden/type_errors/lambda_return_assignment_type_mismatch.cd`:

```cd
let make = fun () {
  return 42;
};
let n = make();
n = true;
```

Create `tests/golden/type_errors/lambda_return_assignment_type_mismatch.err`:

```text
Type error at 5:1: cannot assign bool to `n` of type number
```

Create `tests/golden/type_errors/lambda_return_assignment_type_mismatch.exit`:

```text
1
```

- [ ] **Step 3: Add function assignment return metadata update fixture**

Create `tests/golden/type_errors/function_assignment_updates_return_type.cd`:

```cd
let f = fun () {
  return 1;
};
f = fun () {
  return "x";
};
let s = f();
s = 2;
```

Create `tests/golden/type_errors/function_assignment_updates_return_type.err`:

```text
Type error at 8:1: cannot assign number to `s` of type string
```

Create `tests/golden/type_errors/function_assignment_updates_return_type.exit`:

```text
1
```

- [ ] **Step 4: Run golden tests and observe expected failures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

If `./build/compiler_demo` does not exist yet in this worktree, first run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected before implementation: the three new type-error fixtures fail because the compiler currently lets these programs type-check and run or reach a later mode without the new static diagnostics. Existing tests should still have their previous behavior.

- [ ] **Step 5: Commit failing fixtures**

Run:

```bash
git add tests/golden/type_errors/function_return_assignment_type_mismatch.* \
        tests/golden/type_errors/lambda_return_assignment_type_mismatch.* \
        tests/golden/type_errors/function_assignment_updates_return_type.*
git commit -m "test: cover function return type errors"
```

---

### Task 2: Add Return Type Metadata to TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Extend metadata structs and declarations in `include/TypeChecker.hpp`**

In `class TypeChecker`, change `CheckedExpression` from:

```cpp
    struct CheckedExpression {
        StaticType type;
        std::optional<std::size_t> arity;
    };
```

to:

```cpp
    struct CheckedExpression {
        StaticType type;
        std::optional<std::size_t> arity;
        StaticType returnType = StaticType::Unknown;
    };
```

Change `Binding` from:

```cpp
    struct Binding {
        StaticType type;
        std::string resolvedName;
        std::optional<std::size_t> arity;
        std::size_t scopeDepth = 0;
        std::size_t functionDepth = 0;
    };
```

to:

```cpp
    struct Binding {
        StaticType type;
        std::string resolvedName;
        std::optional<std::size_t> arity;
        StaticType returnType = StaticType::Unknown;
        std::size_t scopeDepth = 0;
        std::size_t functionDepth = 0;
    };
```

Add this return context struct after `Binding`:

```cpp
    struct FunctionReturnContext {
        bool sawReturn = false;
        StaticType returnType = StaticType::Nil;
    };
```

Change `declareVariable` declarations from:

```cpp
    Binding declareVariable(const Token& name, StaticType type, std::optional<std::size_t> arity = std::nullopt);
    Binding declareVariable(
        const LetStmt& statement,
        StaticType type,
        std::optional<std::size_t> arity = std::nullopt);
```

to:

```cpp
    Binding declareVariable(
        const Token& name,
        StaticType type,
        std::optional<std::size_t> arity = std::nullopt,
        StaticType returnType = StaticType::Unknown);
    Binding declareVariable(
        const LetStmt& statement,
        StaticType type,
        std::optional<std::size_t> arity = std::nullopt,
        StaticType returnType = StaticType::Unknown);
```

Add private helper declarations near the expression helpers:

```cpp
    StaticType checkFunctionBody(const std::vector<StmtPtr>& body);
    void recordReturn(StaticType type);
```

Add the context stack field after `functionDepth_`:

```cpp
    std::vector<FunctionReturnContext> returnContexts_;
```

- [ ] **Step 2: Add return type merge helper in `src/TypeChecker.cpp`**

In the anonymous namespace near `logicalResultType()`, add:

```cpp
StaticType mergeReturnTypes(StaticType current, StaticType next)
{
    if (current == next) {
        return current;
    }
    if (!isKnown(current) || !isKnown(next)) {
        return StaticType::Unknown;
    }
    return StaticType::Unknown;
}
```

This implements the conservative rule: same type remains precise, unknown or conflicting known types degrade to `Unknown`.

- [ ] **Step 3: Clear return contexts at top-level check start**

In `TypeChecker::check()`, after:

```cpp
    functionDepth_ = 0;
```

add:

```cpp
    returnContexts_.clear();
```

- [ ] **Step 4: Update `declareVariable()` definitions**

Change the token overload definition from:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(const Token& name, StaticType type, std::optional<std::size_t> arity)
```

to:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(
    const Token& name,
    StaticType type,
    std::optional<std::size_t> arity,
    StaticType returnType)
```

Replace its binding construction:

```cpp
    Binding binding{type, makeResolvedName(name.lexeme), arity, scopes_.size() - 1, functionDepth_};
```

with:

```cpp
    Binding binding{type, makeResolvedName(name.lexeme), arity,
        type == StaticType::Function ? returnType : StaticType::Unknown,
        scopes_.size() - 1, functionDepth_};
```

Change the `LetStmt` overload definition from:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(
    const LetStmt& statement,
    StaticType type,
    std::optional<std::size_t> arity)
{
    Binding binding = declareVariable(statement.name, type, arity);
```

to:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(
    const LetStmt& statement,
    StaticType type,
    std::optional<std::size_t> arity,
    StaticType returnType)
{
    Binding binding = declareVariable(statement.name, type, arity, returnType);
```

- [ ] **Step 5: Preserve return metadata in let declarations**

In `TypeChecker::checkStatement()`, replace the `let` branch body:

```cpp
        const CheckedExpression declared = checkLetInitializer(*let);
        declareVariable(*let, declared.type, declared.arity);
```

with:

```cpp
        const CheckedExpression declared = checkLetInitializer(*let);
        declareVariable(*let, declared.type, declared.arity, declared.returnType);
```

- [ ] **Step 6: Track return statements**

In the `ReturnStmt` branch of `TypeChecker::checkStatement()`, replace:

```cpp
        if (returnStmt->value) {
            checkExpression(*returnStmt->value);
        }
        return;
```

with:

```cpp
        const StaticType returned = returnStmt->value
            ? checkExpression(*returnStmt->value)
            : StaticType::Nil;
        recordReturn(returned);
        return;
```

Add this helper definition before `checkFunction()`:

```cpp
void TypeChecker::recordReturn(StaticType type)
{
    if (returnContexts_.empty()) {
        throw TypeError("return context stack is empty");
    }

    FunctionReturnContext& context = returnContexts_.back();
    if (!context.sawReturn) {
        context.sawReturn = true;
        context.returnType = type;
        return;
    }

    context.returnType = mergeReturnTypes(context.returnType, type);
}
```

- [ ] **Step 7: Extract body checking with return context**

Add this helper definition before `checkFunction()`:

```cpp
StaticType TypeChecker::checkFunctionBody(const std::vector<StmtPtr>& body)
{
    returnContexts_.push_back(FunctionReturnContext{});

    for (const auto& child : body) {
        checkStatement(*child);
    }

    const FunctionReturnContext context = returnContexts_.back();
    returnContexts_.pop_back();
    return context.sawReturn ? context.returnType : StaticType::Nil;
}
```

This helper must be called only after function parameters have been declared and before the function scope is popped.

- [ ] **Step 8: Infer named function return types**

In `TypeChecker::checkFunction()`, replace:

```cpp
    for (const auto& child : statement.body) {
        checkStatement(*child);
    }

    --functionDepth_;
    endScope();
```

with:

```cpp
    const StaticType returnType = checkFunctionBody(statement.body);

    --functionDepth_;
    endScope();

    Binding* storedFunction = findVariable(statement.name.lexeme);
    if (!storedFunction) {
        throw TypeError(statement.name, "undefined function `" + statement.name.lexeme + "`");
    }
    storedFunction->returnType = returnType;
```

Recursive calls during body checking still see the provisional `Unknown` return type, which is intentional for this phase.

- [ ] **Step 9: Infer function expression return types**

In `TypeChecker::checkFunctionExpression()`, replace:

```cpp
    for (const auto& child : expression.body) {
        checkStatement(*child);
    }

    --functionDepth_;
    endScope();

    return CheckedExpression{StaticType::Function, expression.parameters.size()};
```

with:

```cpp
    const StaticType returnType = checkFunctionBody(expression.body);

    --functionDepth_;
    endScope();

    return CheckedExpression{StaticType::Function, expression.parameters.size(), returnType};
```

- [ ] **Step 10: Update all `CheckedExpression` construction sites**

In `TypeChecker::checkLetInitializer()`, replace the explicit annotation return:

```cpp
    return CheckedExpression{declared, std::nullopt};
```

with:

```cpp
    return CheckedExpression{declared, std::nullopt, StaticType::Unknown};
```

In `TypeChecker::checkExpressionInfo()`, update literal and non-function returns to include `StaticType::Unknown` as the third field. Examples:

```cpp
return CheckedExpression{StaticType::Nil, std::nullopt, StaticType::Unknown};
return CheckedExpression{StaticType::Bool, std::nullopt, StaticType::Unknown};
return CheckedExpression{StaticType::String, std::nullopt, StaticType::Unknown};
return CheckedExpression{StaticType::Number, std::nullopt, StaticType::Unknown};
return CheckedExpression{StaticType::Array, std::nullopt, StaticType::Unknown};
return CheckedExpression{checkUnary(*unary), std::nullopt, StaticType::Unknown};
return CheckedExpression{checkBinary(*binary), std::nullopt, StaticType::Unknown};
return CheckedExpression{logicalResultType(left, right), std::nullopt, StaticType::Unknown};
return CheckedExpression{checkIndex(*index), std::nullopt, StaticType::Unknown};
```

In the variable branch, replace:

```cpp
return CheckedExpression{binding->type, binding->arity};
```

with:

```cpp
return CheckedExpression{binding->type, binding->arity, binding->returnType};
```

- [ ] **Step 11: Preserve and clear return metadata on assignment**

In the assignment branch of `TypeChecker::checkExpressionInfo()`, after the existing arity update:

```cpp
        if (target->type == StaticType::Function) {
            target->arity = value.type == StaticType::Function ? value.arity : std::nullopt;
        }
```

replace it with:

```cpp
        if (target->type == StaticType::Function) {
            target->arity = value.type == StaticType::Function ? value.arity : std::nullopt;
            target->returnType = value.type == StaticType::Function ? value.returnType : StaticType::Unknown;
        }
```

Then replace the assignment expression return:

```cpp
        return CheckedExpression{resultType, resultType == StaticType::Function ? target->arity : std::nullopt};
```

with:

```cpp
        return CheckedExpression{resultType,
            resultType == StaticType::Function ? target->arity : std::nullopt,
            resultType == StaticType::Function ? target->returnType : StaticType::Unknown};
```

- [ ] **Step 12: Return callee return type from calls**

In `TypeChecker::checkCall()`, replace:

```cpp
    return CheckedExpression{StaticType::Unknown, std::nullopt};
```

with:

```cpp
    if (callee.type == StaticType::Function) {
        return CheckedExpression{callee.returnType, std::nullopt, StaticType::Unknown};
    }

    return CheckedExpression{StaticType::Unknown, std::nullopt, StaticType::Unknown};
```

This preserves unknown call results for unknown callees and gives known function calls their inferred return type.

- [ ] **Step 13: Build and run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected after implementation:

- The three new type-error fixtures pass, except for possible source column differences in new `.err` files.
- Existing function arity tests still pass.
- Existing runtime and bytecode success tests still pass.

If a new expected column differs, update only that new `.err` file to match the actual diagnostic location.

- [ ] **Step 14: Commit implementation**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp \
        tests/golden/type_errors/function_return_assignment_type_mismatch.err \
        tests/golden/type_errors/lambda_return_assignment_type_mismatch.err \
        tests/golden/type_errors/function_assignment_updates_return_type.err
git commit -m "feat: infer function return types"
```

Only the `.err` files should be staged in this commit if source column corrections were needed.

---

### Task 3: Add Success and Unknown-Return Preservation Coverage

**Files:**
- Create: `tests/golden/function_return_type_success/input.cd`
- Create: `tests/golden/function_return_type_success/run.out`
- Create: `tests/golden/function_return_type_success/run_bytecode.out`
- Create: `tests/golden/function_return_type_unknown_preserved/input.cd`
- Create: `tests/golden/function_return_type_unknown_preserved/run.out`
- Create: `tests/golden/function_return_type_unknown_preserved/run_bytecode.out`

- [ ] **Step 1: Add success fixture for known return types**

Create `tests/golden/function_return_type_success/input.cd`:

```cd
fun greet() {
  return "hi";
}
let message = greet();
print message + "!";

let makeNumber = fun () {
  return 41;
};
let n = makeNumber();
print n + 1;

fun noReturn() {
}
let z = noReturn();
z = nil;
print z;
```

Create `tests/golden/function_return_type_success/run.out`:

```text
hi!
42
nil
```

Create `tests/golden/function_return_type_success/run_bytecode.out` with the same content:

```text
hi!
42
nil
```

- [ ] **Step 2: Add mixed-return unknown preservation fixture**

Create `tests/golden/function_return_type_unknown_preserved/input.cd`:

```cd
fun maybe(flag) {
  if flag {
    return 1;
  } else {
    return "x";
  }
}

let v = maybe(true);
v = false;
print v;
```

Create `tests/golden/function_return_type_unknown_preserved/run.out`:

```text
false
```

Create `tests/golden/function_return_type_unknown_preserved/run_bytecode.out` with the same content:

```text
false
```

This fixture proves mixed known return types degrade to `Unknown`; otherwise `v = false;` would be rejected statically after `let v = maybe(true);`.

- [ ] **Step 3: Run golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass.

- [ ] **Step 4: Commit success fixtures**

Run:

```bash
git add tests/golden/function_return_type_success tests/golden/function_return_type_unknown_preserved
git commit -m "test: cover function return type success cases"
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
Functions are values. Named functions use `fun name(parameter*) { declaration* }`, and anonymous function expressions use `fun (parameter*) { declaration* }`. Known function values carry arity for static argument-count checks, including variables initialized from named functions or function expressions. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Function parameter types, return types, and function type annotations are not implemented yet.
```

with:

```markdown
Functions are values. Named functions use `fun name(parameter*) { declaration* }`, and anonymous function expressions use `fun (parameter*) { declaration* }`. Known function values carry arity and inferred return types for static checks, including variables initialized from named functions or function expressions. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported, though recursive return inference remains conservative. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Function parameter types, return type annotations, and function type annotations are not implemented yet.
```

- [ ] **Step 2: Update roadmap Phase 9 status**

In `docs/roadmap.md`, replace the Phase 9 status paragraph:

```markdown
Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Phase 9B is implemented: known function values carry arity for static argument-count checks. Function parameter types, function return types, function type annotations, and array element types remain future work.
```

with:

```markdown
Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Phase 9B is implemented: known function values carry arity for static argument-count checks. Phase 9C is implemented: known function values carry conservative inferred return types for call-result checking. Function parameter types, return type annotations, function type annotations, and array element types remain future work.
```

After the existing completed second slice sentence, add:

```markdown
Completed third slice: function bodies infer conservative return types, and calls through known function values feed those types into `let` inference and assignment checks.
```

- [ ] **Step 3: Update AGENTS current semantics**

In `AGENTS.md`, replace this bullet:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Known function values carry arity for static argument-count checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

with:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Known function values carry arity and conservative inferred return types for static checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

- [ ] **Step 4: Verify docs mention return type boundary**

Run:

```bash
grep -n "Known function values carry arity and inferred return types" README.md
grep -n "Phase 9C is implemented" docs/roadmap.md
grep -n "Known function values carry arity and conservative inferred return types" AGENTS.md
```

Expected: each command prints one matching line.

- [ ] **Step 5: Commit docs**

Run:

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document function return type inference"
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
git diff --stat d0abedf..HEAD
git diff --name-status d0abedf..HEAD
git status --short --branch
```

Expected:

- Changes include `include/TypeChecker.hpp`, `src/TypeChecker.cpp`, new return-type fixtures, README, roadmap, AGENTS, and the design/plan docs.
- No parser, grammar, AST, IR, bytecode, VM, or runtime value representation changes.
- Worktree is clean.

---

## Self-Review Checklist

- Spec coverage:
  - Named function return inference: Task 2.
  - Function expression return inference: Task 2.
  - Variable reads preserve function return metadata: Task 2.
  - Unannotated `let` preserves function metadata: Task 2.
  - Calls return known function return type: Task 2.
  - Multiple same-type returns, unknown returns, conflicting returns: Task 2 and Task 3.
  - No-return functions infer `nil`: Task 2 and Task 3.
  - Function assignment updates and clears metadata: Task 2 and Task 1.
  - Recursive fixed-point inference excluded: documented in spec and README wording.
  - Docs updated: Task 4.
- No parser, grammar, AST, IR, bytecode, VM, or runtime value representation changes are planned.
- Full verification command is included.
