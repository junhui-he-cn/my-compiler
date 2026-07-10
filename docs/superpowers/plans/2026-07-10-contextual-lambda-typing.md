# Contextual Lambda Typing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add contextual typing for anonymous function expressions when an expected function type is already available.

**Architecture:** Reuse the existing `checkExpressionInfo(expr, expectedType)` data flow. Teach `TypeChecker::checkFunctionExpression` to read expected function signatures for lambda parameter and return checking, and fix the existing field-assignment expected-type propagation gap so all planned contexts are covered. Runtime lowering remains unchanged.

**Tech Stack:** C++17 TypeChecker, existing golden tests, CMake, Python golden runner, Rust VM parity tests.

---

## File Structure

- Modify `include/TypeChecker.hpp`: update the private function-expression checker declaration and add a contextual function-type helper declaration.
- Modify `src/TypeChecker.cpp`: implement contextual function-type extraction, contextual lambda parameter/return checking, pass `expectedType` into function expressions, and pass known struct field types into field assignment RHS checking.
- Add success fixtures under `tests/golden/contextual_lambda_*`: cover typed `let`, call argument, function return, array element, and field assignment contexts.
- Add type-error fixtures under `tests/golden/type_errors/contextual_lambda_*`: cover arity mismatch, explicit parameter conflict, return annotation conflict, return value conflict, and implicit nil fallthrough.
- Modify `README.md`: document contextual typing for function expressions and keep global inference unsupported.
- Modify `docs/roadmap.md`: remove the completed M1 contextual lambda typing item after implementation.

---

### Task 1: Add failing contextual lambda golden fixtures

**Files:**
- Create: `tests/golden/contextual_lambda_let_struct_method/input.cd`
- Create: `tests/golden/contextual_lambda_let_struct_method/run.out`
- Create: `tests/golden/contextual_lambda_call_argument/input.cd`
- Create: `tests/golden/contextual_lambda_call_argument/run.out`
- Create: `tests/golden/contextual_lambda_return_function/input.cd`
- Create: `tests/golden/contextual_lambda_return_function/run.out`
- Create: `tests/golden/contextual_lambda_array_element/input.cd`
- Create: `tests/golden/contextual_lambda_array_element/run.out`
- Create: `tests/golden/contextual_lambda_field_assignment/input.cd`
- Create: `tests/golden/contextual_lambda_field_assignment/run.out`
- Create: `tests/golden/type_errors/contextual_lambda_arity_mismatch.cd`
- Create: `tests/golden/type_errors/contextual_lambda_arity_mismatch.err`
- Create: `tests/golden/type_errors/contextual_lambda_arity_mismatch.exit`
- Create: `tests/golden/type_errors/contextual_lambda_parameter_conflict.cd`
- Create: `tests/golden/type_errors/contextual_lambda_parameter_conflict.err`
- Create: `tests/golden/type_errors/contextual_lambda_parameter_conflict.exit`
- Create: `tests/golden/type_errors/contextual_lambda_return_annotation_conflict.cd`
- Create: `tests/golden/type_errors/contextual_lambda_return_annotation_conflict.err`
- Create: `tests/golden/type_errors/contextual_lambda_return_annotation_conflict.exit`
- Create: `tests/golden/type_errors/contextual_lambda_return_value_conflict.cd`
- Create: `tests/golden/type_errors/contextual_lambda_return_value_conflict.err`
- Create: `tests/golden/type_errors/contextual_lambda_return_value_conflict.exit`
- Create: `tests/golden/type_errors/contextual_lambda_fallthrough.cd`
- Create: `tests/golden/type_errors/contextual_lambda_fallthrough.err`
- Create: `tests/golden/type_errors/contextual_lambda_fallthrough.exit`

- [ ] **Step 1: Confirm workspace state**

Run:

```sh
git status --short
```

Expected: no output.

- [ ] **Step 2: Add typed-let success fixture**

Create `tests/golden/contextual_lambda_let_struct_method/input.cd`:

```text
struct Point { x: number, y: number }

impl Point {
  fun sum(): number {
    return this.x + this.y;
  }
}

let score: fun(Point): number = fun (p) {
  return p.sum();
};
print score(Point { x: 2, y: 5 });
```

Create `tests/golden/contextual_lambda_let_struct_method/run.out`:

```text
7
```

- [ ] **Step 3: Add call-argument success fixture**

Create `tests/golden/contextual_lambda_call_argument/input.cd`:

```text
struct Point { x: number, y: number }

impl Point {
  fun sum(): number {
    return this.x + this.y;
  }
}

fun apply(callback: fun(Point): number, point: Point): number {
  return callback(point);
}

print apply(fun (p) {
  return p.sum();
}, Point { x: 3, y: 4 });
```

Create `tests/golden/contextual_lambda_call_argument/run.out`:

```text
7
```

- [ ] **Step 4: Add return-function success fixture**

Create `tests/golden/contextual_lambda_return_function/input.cd`:

```text
struct Point { x: number, y: number }

impl Point {
  fun sum(): number {
    return this.x + this.y;
  }
}

fun makeScorer(): fun(Point): number {
  return fun (p) {
    return p.sum();
  };
}

let score = makeScorer();
print score(Point { x: 6, y: 1 });
```

Create `tests/golden/contextual_lambda_return_function/run.out`:

```text
7
```

- [ ] **Step 5: Add array-element success fixture**

Create `tests/golden/contextual_lambda_array_element/input.cd`:

```text
struct Point { x: number, y: number }

impl Point {
  fun sum(): number {
    return this.x + this.y;
  }
}

let scorers: [fun(Point): number] = [fun (p) {
  return p.sum();
}];
print scorers[0](Point { x: 1, y: 6 });
```

Create `tests/golden/contextual_lambda_array_element/run.out`:

```text
7
```

- [ ] **Step 6: Add field-assignment success fixture**

Create `tests/golden/contextual_lambda_field_assignment/input.cd`:

```text
struct Point { x: number, y: number }
struct Holder { score: fun(Point): number }

impl Point {
  fun sum(): number {
    return this.x + this.y;
  }
}

let holder: Holder = Holder { score: fun (p: Point): number {
  return p.x;
} };
holder.score = fun (p) {
  return p.sum();
};
print holder.score(Point { x: 3, y: 4 });
```

Create `tests/golden/contextual_lambda_field_assignment/run.out`:

```text
7
```

- [ ] **Step 7: Add arity mismatch type-error fixture**

Create `tests/golden/type_errors/contextual_lambda_arity_mismatch.cd`:

```text
struct Point { x: number }

let score: fun(Point): number = fun () {
  return 0;
};
```

Create `tests/golden/type_errors/contextual_lambda_arity_mismatch.err`:

```text
Type error at <repo>/tests/golden/type_errors/contextual_lambda_arity_mismatch.cd:3:33: expected 1 parameters but got 0
```

Create `tests/golden/type_errors/contextual_lambda_arity_mismatch.exit`:

```text
1
```

- [ ] **Step 8: Add explicit parameter conflict type-error fixture**

Create `tests/golden/type_errors/contextual_lambda_parameter_conflict.cd`:

```text
struct Point { x: number }

let score: fun(Point): number = fun (p: string): number {
  return 0;
};
```

Create `tests/golden/type_errors/contextual_lambda_parameter_conflict.err`:

```text
Type error at <repo>/tests/golden/type_errors/contextual_lambda_parameter_conflict.cd:3:34: parameter `p` expects Point, got string
```

Create `tests/golden/type_errors/contextual_lambda_parameter_conflict.exit`:

```text
1
```

- [ ] **Step 9: Add return annotation conflict type-error fixture**

Create `tests/golden/type_errors/contextual_lambda_return_annotation_conflict.cd`:

```text
let f: fun(number): number = fun (x): string {
  return "bad";
};
```

Create `tests/golden/type_errors/contextual_lambda_return_annotation_conflict.err`:

```text
Type error at <repo>/tests/golden/type_errors/contextual_lambda_return_annotation_conflict.cd:1:39: function `<lambda>` expects return number, got string
```

Create `tests/golden/type_errors/contextual_lambda_return_annotation_conflict.exit`:

```text
1
```

- [ ] **Step 10: Add return value conflict type-error fixture**

Create `tests/golden/type_errors/contextual_lambda_return_value_conflict.cd`:

```text
let f: fun(number): number = fun (x) {
  return "bad";
};
```

Create `tests/golden/type_errors/contextual_lambda_return_value_conflict.err`:

```text
Type error at <repo>/tests/golden/type_errors/contextual_lambda_return_value_conflict.cd:2:3: cannot return string from function returning number
```

Create `tests/golden/type_errors/contextual_lambda_return_value_conflict.exit`:

```text
1
```

- [ ] **Step 11: Add contextual fallthrough type-error fixture**

Create `tests/golden/type_errors/contextual_lambda_fallthrough.cd`:

```text
let f: fun(number): number = fun (x) {
  print x;
};
```

Create `tests/golden/type_errors/contextual_lambda_fallthrough.err`:

```text
Type error at <repo>/tests/golden/type_errors/contextual_lambda_fallthrough.cd:1:29: function `<lambda>` may return nil but is annotated number
```

Create `tests/golden/type_errors/contextual_lambda_fallthrough.exit`:

```text
1
```

- [ ] **Step 12: Run focused tests to verify RED**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design --case contextual_lambda
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case contextual_lambda_let_struct_method --case contextual_lambda_call_argument --case contextual_lambda_return_function --case contextual_lambda_array_element --case contextual_lambda_field_assignment
```

Expected: at least one new contextual lambda success fixture fails with `can only call methods on known named structs`, and new type-error fixtures fail because the current checker reports later assignment/initialization errors or no contextual return error. This is the required RED state.

---

### Task 2: Implement contextual function-expression checking

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: contextual lambda fixtures from Task 1

- [ ] **Step 1: Update TypeChecker declarations**

In `include/TypeChecker.hpp`, replace this declaration:

```cpp
    CheckedExpression checkFunctionExpression(const FunctionExpr& expression);
```

with:

```cpp
    const TypeInfo* contextualFunctionType(const TypeInfo* expectedType) const;
    CheckedExpression checkFunctionExpression(const FunctionExpr& expression, const TypeInfo* expectedType);
```

- [ ] **Step 2: Add contextual function helper**

In `src/TypeChecker.cpp`, add this definition immediately before `TypeChecker::CheckedExpression TypeChecker::checkFunctionExpression`:

```cpp
const TypeInfo* TypeChecker::contextualFunctionType(const TypeInfo* expectedType) const
{
    if (!expectedType || expectedType->kind != StaticType::Function || !hasFunctionSignature(*expectedType)) {
        return nullptr;
    }
    return expectedType;
}
```

- [ ] **Step 3: Replace function-expression checker implementation**

In `src/TypeChecker.cpp`, replace the whole existing `TypeChecker::CheckedExpression TypeChecker::checkFunctionExpression(const FunctionExpr& expression)` definition with:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkFunctionExpression(const FunctionExpr& expression, const TypeInfo* expectedType)
{
    const TypeInfo* context = contextualFunctionType(expectedType);
    if (context && context->parameterTypes.size() != expression.parameters.size()) {
        throw TypeError(expression.keyword,
            "expected " + std::to_string(context->parameterTypes.size())
                + " parameters but got " + std::to_string(expression.parameters.size()));
    }

    resolvedNames_.recordFunction(expression, "<lambda>");

    std::vector<TypeInfo> declaredParameterTypes;
    declaredParameterTypes.reserve(expression.parameters.size());
    for (std::size_t i = 0; i < expression.parameters.size(); ++i) {
        const Parameter& parameter = expression.parameters[i];
        TypeInfo parameterType = parameter.typeName
            ? resolveAnnotation(*parameter.typeName)
            : (context ? context->parameterTypes[i] : unknownType());

        if (context && parameter.typeName && !compatible(context->parameterTypes[i], parameterType)) {
            throw TypeError(parameter.name,
                "parameter `" + parameter.name.lexeme + "` expects " + typeInfoName(context->parameterTypes[i])
                    + ", got " + typeInfoName(parameterType));
        }

        declaredParameterTypes.push_back(std::move(parameterType));
    }

    std::optional<TypeInfo> expectedReturnType;
    if (expression.returnTypeName) {
        expectedReturnType = resolveAnnotation(*expression.returnTypeName);
    }
    if (context && context->returnType) {
        if (expectedReturnType && !compatible(*context->returnType, *expectedReturnType)) {
            throw TypeError(expression.returnTypeName->token,
                "function `<lambda>` expects return " + typeInfoName(*context->returnType)
                    + ", got " + typeInfoName(*expectedReturnType));
        }
        if (!expectedReturnType) {
            expectedReturnType = *context->returnType;
        }
    }

    beginScope();
    ++functionDepth_;
    const std::size_t enclosingLoopDepth = loopDepth_;
    loopDepth_ = 0;

    std::vector<std::string> parameterNames;
    for (std::size_t i = 0; i < expression.parameters.size(); ++i) {
        const Parameter& parameter = expression.parameters[i];
        Binding parameterBinding = declareVariable(parameter.name, declaredParameterTypes[i], parameter.typeName.has_value() || context != nullptr);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordParameters(expression, std::move(parameterNames));

    const TypeInfo returnType = checkFunctionBody(
        expression.body,
        expectedReturnType,
        expression.keyword,
        "<lambda>");

    loopDepth_ = enclosingLoopDepth;
    --functionDepth_;
    endScope();

    return CheckedExpression{functionType(std::move(declaredParameterTypes), returnType)};
}
```

- [ ] **Step 4: Pass expected type into function expressions**

In `src/TypeChecker.cpp`, inside `TypeChecker::CheckedExpression TypeChecker::checkExpressionInfo(const Expr& expression, const TypeInfo* expectedType)`, replace:

```cpp
    if (const auto* function = dynamic_cast<const FunctionExpr*>(&expression)) {
        return checkFunctionExpression(*function);
    }
```

with:

```cpp
    if (const auto* function = dynamic_cast<const FunctionExpr*>(&expression)) {
        return checkFunctionExpression(*function, expectedType);
    }
```

- [ ] **Step 5: Propagate field assignment expected type**

In `src/TypeChecker.cpp`, inside `TypeChecker::CheckedExpression TypeChecker::checkFieldAssignment(const FieldAssignExpr& expression)`, replace:

```cpp
    const StructFieldType* structField = checkStructFieldTarget(
        *expression.object, expression.name, "can only assign fields on structs");
    const CheckedExpression value = checkExpressionInfo(*expression.value);

    if (structField) {
```

with:

```cpp
    const StructFieldType* structField = checkStructFieldTarget(
        *expression.object, expression.name, "can only assign fields on structs");
    const TypeInfo* expectedFieldType = structField ? &structField->type : nullptr;
    const CheckedExpression value = checkExpressionInfo(*expression.value, expectedFieldType);

    if (structField) {
```

- [ ] **Step 6: Build compiler**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design
```

Expected: build succeeds.

- [ ] **Step 7: Run focused contextual lambda tests**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case contextual_lambda
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case contextual_lambda_let_struct_method --case contextual_lambda_call_argument --case contextual_lambda_return_function --case contextual_lambda_array_element --case contextual_lambda_field_assignment
```

Expected: contextual lambda tests pass. If a new `.err` file differs only by the actual stable diagnostic column or snippet form, update the `.err` file to the actual diagnostic and rerun this step.

- [ ] **Step 8: Commit implementation and fixtures**

Run:

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/contextual_lambda_let_struct_method tests/golden/contextual_lambda_call_argument tests/golden/contextual_lambda_return_function tests/golden/contextual_lambda_array_element tests/golden/contextual_lambda_field_assignment tests/golden/type_errors/contextual_lambda_arity_mismatch.cd tests/golden/type_errors/contextual_lambda_arity_mismatch.err tests/golden/type_errors/contextual_lambda_arity_mismatch.exit tests/golden/type_errors/contextual_lambda_parameter_conflict.cd tests/golden/type_errors/contextual_lambda_parameter_conflict.err tests/golden/type_errors/contextual_lambda_parameter_conflict.exit tests/golden/type_errors/contextual_lambda_return_annotation_conflict.cd tests/golden/type_errors/contextual_lambda_return_annotation_conflict.err tests/golden/type_errors/contextual_lambda_return_annotation_conflict.exit tests/golden/type_errors/contextual_lambda_return_value_conflict.cd tests/golden/type_errors/contextual_lambda_return_value_conflict.err tests/golden/type_errors/contextual_lambda_return_value_conflict.exit tests/golden/type_errors/contextual_lambda_fallthrough.cd tests/golden/type_errors/contextual_lambda_fallthrough.err tests/golden/type_errors/contextual_lambda_fallthrough.exit
git commit -m "feat: add contextual lambda typing"
```

Expected: commit succeeds.

---

### Task 3: Update docs and roadmap

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README function typing text**

In `README.md`, in the long type-annotations paragraph near the top, replace this sentence:

```text
Known function signatures are checked for assignment compatibility, call argument types, and function returns.
```

with:

```text
Known function signatures are checked for assignment compatibility, call argument types, and function returns; anonymous function expressions in expected function-typed positions use that context for unannotated parameter and return checking.
```

In the functions paragraph, replace this sentence:

```text
Known function values carry arity, parameter types when annotated, and inferred or annotated return types for static checks, including variables initialized from named functions or function expressions.
```

with:

```text
Known function values carry arity, parameter types when annotated or contextually typed, and inferred, annotated, or contextually checked return types for static checks, including variables initialized from named functions or function expressions.
```

- [ ] **Step 2: Update roadmap M1**

In `docs/roadmap.md`, remove the M1 contextual lambda item:

```text
1. Add contextual lambda typing from an expected function type; do not attempt
   global parameter-type inference as part of this slice.
```

Then replace the immediate dependency order block:

```text
contextual lambda typing
-> re-export
-> search paths
```

with:

```text
re-export
-> search paths
```

In the Phase 9 future work list, remove:

```text
- Add contextual typing for lambdas when an expected function type is available.
  Avoid global parameter-type inference until there is a stronger inference
  design.
```

In the Near-Term Recommendation block, replace:

```text
contextual lambda typing
-> Phase 14E re-export
-> Phase 14F search paths
```

with:

```text
Phase 14E re-export
-> Phase 14F search paths
```

- [ ] **Step 3: Verify docs diff**

Run:

```sh
git diff -- README.md docs/roadmap.md
```

Expected: diff only documents contextual lambda typing and removes the completed roadmap item.

- [ ] **Step 4: Commit docs**

Run:

```sh
git add README.md docs/roadmap.md
git commit -m "docs: document contextual lambda typing"
```

Expected: commit succeeds.

---

### Task 4: Full verification and cleanup

**Files:**
- Verify: whole repository
- Clean: `tests/__pycache__/`
- Clean: `build-sanitize/`

- [ ] **Step 1: Run standard full verification**

Run:

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

Expected: every command exits 0; CTest reports `10/10` passing; golden, bytecode artifact, Rust VM, golden runner selftest, and cargo tests report zero failures.

- [ ] **Step 2: Run sanitizer CTest because TypeChecker code changed**

Run:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
rm -rf build-sanitize
```

Expected: sanitizer configure/build/CTest exits 0 with `10/10` CTest pass. Existing non-fatal compiler warnings may be printed, but the command must exit 0.

- [ ] **Step 3: Confirm final workspace state**

Run:

```sh
git status --short
```

Expected: no output. If files remain modified, inspect them and either commit intentional changes or revert accidental changes.

- [ ] **Step 4: Report final verification evidence**

In the final response, list each verification command and observed pass counts. Do not claim completion without fresh verification evidence.

---

## Self-Review Notes

- Spec coverage: Task 1 tests all required success contexts and error categories. Task 2 implements contextual function type extraction, parameter selection, explicit parameter conflict, return checking, and expected-type propagation into function expressions. Task 2 also fixes field assignment expected-type propagation so the planned field assignment context is real. Task 3 updates README and roadmap. Task 4 runs required full and sanitizer verification.
- Completeness scan: the plan contains concrete file paths, fixture contents, implementation snippets, commands, expected RED/GREEN outcomes, and commit commands. Every test and implementation instruction is fully specified.
- Type consistency: the plan uses `contextualFunctionType`, `checkFunctionExpression(const FunctionExpr&, const TypeInfo*)`, `expectedType`, `expectedReturnType`, `declaredParameterTypes`, and existing `TypeInfo` / `TypeChecker::CheckedExpression` names consistently across declarations and definitions.
