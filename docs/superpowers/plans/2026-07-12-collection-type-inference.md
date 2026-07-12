# Collection Type Inference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Improve array element type inference for nullable literals, nested arrays, and direct unannotated array mutation while preserving mixed-array dynamic fallback.

**Architecture:** Add a focused `TypeUtils` merge helper for array element types, then use it in `TypeChecker` array literal inference and direct simple-variable array mutations. Use module-interface goldens to make inferred static types observable, while keeping IR, bytecode, runtime, parser, and grammar unchanged.

**Tech Stack:** C++17 frontend/type checker, existing golden test runner, Rust VM integration runner for runtime parity, Markdown docs.

---

## File Structure

Create:

- `tests/golden/collection_inference_nullable_literal_interface/input.cd` — imports a fixture module so module-interface mode prints exported inferred array types.
- `tests/golden/collection_inference_nullable_literal_interface/lib.cd` — declares and exports `[1, nil]`.
- `tests/golden/collection_inference_nullable_literal_interface/module-interface.out` — expected `[number?]` interface output.
- `tests/golden/collection_inference_member_push_interface/input.cd` — imports member-push fixture module.
- `tests/golden/collection_inference_member_push_interface/lib.cd` — refines `let xs = []` through `xs.push(1)`.
- `tests/golden/collection_inference_member_push_interface/module-interface.out` — expected `[number]` interface output.
- `tests/golden/collection_inference_function_push_interface/input.cd` — imports function-style push fixture module.
- `tests/golden/collection_inference_function_push_interface/lib.cd` — refines `let xs = []` through `push(xs, "a")`.
- `tests/golden/collection_inference_function_push_interface/module-interface.out` — expected `[string]` interface output.
- `tests/golden/collection_inference_nullable_widening_interface/input.cd` — imports nullable-widening fixture module.
- `tests/golden/collection_inference_nullable_widening_interface/lib.cd` — widens `[number]` to `[number?]` through `xs.push(nil)`.
- `tests/golden/collection_inference_nullable_widening_interface/module-interface.out` — expected `[number?]` interface output.
- `tests/golden/collection_inference_index_assignment_interface/input.cd` — imports index-assignment fixture module.
- `tests/golden/collection_inference_index_assignment_interface/lib.cd` — widens `[nil]` to `[number?]` through `xs[0] = 1`.
- `tests/golden/collection_inference_index_assignment_interface/module-interface.out` — expected `[number?]` interface output.
- `tests/golden/collection_inference_mixed_dynamic_interface/input.cd` — imports mixed-array fallback fixture module.
- `tests/golden/collection_inference_mixed_dynamic_interface/lib.cd` — degrades `[number]` to dynamic `array` through `xs.push("x")`.
- `tests/golden/collection_inference_mixed_dynamic_interface/module-interface.out` — expected dynamic `array` interface output.
- `tests/golden/collection_inference_readers/input.cd` — runtime fixture proving refined types still execute through `pop`, indexing, and `for-in`.
- `tests/golden/collection_inference_readers/run.out` — expected Rust VM output.
- `tests/golden/type_errors/typed_array_member_push_mismatch.cd` — explicit typed array member-push mismatch.
- `tests/golden/type_errors/typed_array_member_push_mismatch.err` — expected type error.
- `tests/golden/type_errors/typed_array_member_push_mismatch.exit` — expected exit code.

Modify:

- `include/TypeUtils.hpp` — declare `mergeArrayElementTypes`.
- `src/TypeUtils.cpp` — implement conservative nullable/nested element merge.
- `include/TypeChecker.hpp` — declare simple-variable binding lookup and mutation-refinement helpers.
- `src/TypeChecker.cpp` — use the merge helper in array literal inference, `push`, member `push`, and index assignment.
- `README.md` — document nullable-aware array literal inference and direct unannotated mutation refinement.
- `docs/roadmap.md` — mark collection inference complete and advance near-term recommendation to `.cdbc` compatibility policy.

Do not modify:

- `docs/language-grammar.ebnf` — syntax is unchanged.
- `include/Ast.hpp`, `src/Ast.cpp`, `include/Parser.hpp`, `src/Parser.cpp` — AST and grammar are unchanged.
- IR, bytecode, `.cdbc`, or Rust VM files — runtime representation and bytecode execution are unchanged.

---

### Task 1: Add failing observable type-inference goldens

**Files:**
- Create: `tests/golden/collection_inference_nullable_literal_interface/input.cd`
- Create: `tests/golden/collection_inference_nullable_literal_interface/lib.cd`
- Create: `tests/golden/collection_inference_nullable_literal_interface/module-interface.out`
- Create: `tests/golden/collection_inference_member_push_interface/input.cd`
- Create: `tests/golden/collection_inference_member_push_interface/lib.cd`
- Create: `tests/golden/collection_inference_member_push_interface/module-interface.out`
- Create: `tests/golden/collection_inference_function_push_interface/input.cd`
- Create: `tests/golden/collection_inference_function_push_interface/lib.cd`
- Create: `tests/golden/collection_inference_function_push_interface/module-interface.out`
- Create: `tests/golden/collection_inference_nullable_widening_interface/input.cd`
- Create: `tests/golden/collection_inference_nullable_widening_interface/lib.cd`
- Create: `tests/golden/collection_inference_nullable_widening_interface/module-interface.out`
- Create: `tests/golden/collection_inference_index_assignment_interface/input.cd`
- Create: `tests/golden/collection_inference_index_assignment_interface/lib.cd`
- Create: `tests/golden/collection_inference_index_assignment_interface/module-interface.out`
- Create: `tests/golden/collection_inference_mixed_dynamic_interface/input.cd`
- Create: `tests/golden/collection_inference_mixed_dynamic_interface/lib.cd`
- Create: `tests/golden/collection_inference_mixed_dynamic_interface/module-interface.out`
- Create: `tests/golden/collection_inference_readers/input.cd`
- Create: `tests/golden/collection_inference_readers/run.out`
- Create: `tests/golden/type_errors/typed_array_member_push_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_member_push_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_member_push_mismatch.exit`

- [ ] **Step 1: Create nullable literal interface fixture**

```sh
mkdir -p tests/golden/collection_inference_nullable_literal_interface
cat > tests/golden/collection_inference_nullable_literal_interface/input.cd <<'CASE'
import "./lib.cd";
CASE
cat > tests/golden/collection_inference_nullable_literal_interface/lib.cd <<'CASE'
let xs = [1, nil];
export xs;
CASE
cat > tests/golden/collection_inference_nullable_literal_interface/module-interface.out <<'CASE'
module 0 "/home/junhe/compiler/tests/golden/collection_inference_nullable_literal_interface/lib.cd"
  export value xs: [number?]

module 1 entry "/home/junhe/compiler/tests/golden/collection_inference_nullable_literal_interface/input.cd"
CASE
```

- [ ] **Step 2: Create member-push interface fixture**

```sh
mkdir -p tests/golden/collection_inference_member_push_interface
cat > tests/golden/collection_inference_member_push_interface/input.cd <<'CASE'
import "./lib.cd";
CASE
cat > tests/golden/collection_inference_member_push_interface/lib.cd <<'CASE'
let xs = [];
xs.push(1);
export xs;
CASE
cat > tests/golden/collection_inference_member_push_interface/module-interface.out <<'CASE'
module 0 "/home/junhe/compiler/tests/golden/collection_inference_member_push_interface/lib.cd"
  export value xs: [number]

module 1 entry "/home/junhe/compiler/tests/golden/collection_inference_member_push_interface/input.cd"
CASE
```

- [ ] **Step 3: Create function-style push interface fixture**

```sh
mkdir -p tests/golden/collection_inference_function_push_interface
cat > tests/golden/collection_inference_function_push_interface/input.cd <<'CASE'
import "./lib.cd";
CASE
cat > tests/golden/collection_inference_function_push_interface/lib.cd <<'CASE'
let xs = [];
push(xs, "a");
export xs;
CASE
cat > tests/golden/collection_inference_function_push_interface/module-interface.out <<'CASE'
module 0 "/home/junhe/compiler/tests/golden/collection_inference_function_push_interface/lib.cd"
  export value xs: [string]

module 1 entry "/home/junhe/compiler/tests/golden/collection_inference_function_push_interface/input.cd"
CASE
```

- [ ] **Step 4: Create nullable widening through push fixture**

```sh
mkdir -p tests/golden/collection_inference_nullable_widening_interface
cat > tests/golden/collection_inference_nullable_widening_interface/input.cd <<'CASE'
import "./lib.cd";
CASE
cat > tests/golden/collection_inference_nullable_widening_interface/lib.cd <<'CASE'
let xs = [1];
xs.push(nil);
export xs;
CASE
cat > tests/golden/collection_inference_nullable_widening_interface/module-interface.out <<'CASE'
module 0 "/home/junhe/compiler/tests/golden/collection_inference_nullable_widening_interface/lib.cd"
  export value xs: [number?]

module 1 entry "/home/junhe/compiler/tests/golden/collection_inference_nullable_widening_interface/input.cd"
CASE
```

- [ ] **Step 5: Create nullable widening through index assignment fixture**

```sh
mkdir -p tests/golden/collection_inference_index_assignment_interface
cat > tests/golden/collection_inference_index_assignment_interface/input.cd <<'CASE'
import "./lib.cd";
CASE
cat > tests/golden/collection_inference_index_assignment_interface/lib.cd <<'CASE'
let xs = [nil];
xs[0] = 1;
export xs;
CASE
cat > tests/golden/collection_inference_index_assignment_interface/module-interface.out <<'CASE'
module 0 "/home/junhe/compiler/tests/golden/collection_inference_index_assignment_interface/lib.cd"
  export value xs: [number?]

module 1 entry "/home/junhe/compiler/tests/golden/collection_inference_index_assignment_interface/input.cd"
CASE
```

- [ ] **Step 6: Create mixed dynamic fallback fixture**

```sh
mkdir -p tests/golden/collection_inference_mixed_dynamic_interface
cat > tests/golden/collection_inference_mixed_dynamic_interface/input.cd <<'CASE'
import "./lib.cd";
CASE
cat > tests/golden/collection_inference_mixed_dynamic_interface/lib.cd <<'CASE'
let xs = [1];
xs.push("x");
export xs;
CASE
cat > tests/golden/collection_inference_mixed_dynamic_interface/module-interface.out <<'CASE'
module 0 "/home/junhe/compiler/tests/golden/collection_inference_mixed_dynamic_interface/lib.cd"
  export value xs: array

module 1 entry "/home/junhe/compiler/tests/golden/collection_inference_mixed_dynamic_interface/input.cd"
CASE
```

- [ ] **Step 7: Create reader runtime fixture**

```sh
mkdir -p tests/golden/collection_inference_readers
cat > tests/golden/collection_inference_readers/input.cd <<'CASE'
let numbers = [];
numbers.push(1);
let popped: number = numbers.pop();
print popped;

let strings = [];
push(strings, "a");
let first: string = strings[0];
print first;

let maybe = [1];
maybe.push(nil);
let item: number? = maybe[1];
print item;

let total = 0;
for value in numbers {
  total = total + value;
}
print total;
CASE
cat > tests/golden/collection_inference_readers/run.out <<'CASE'
1
a
nil
0
CASE
```

- [ ] **Step 8: Create explicit typed member-push mismatch fixture**

```sh
cat > tests/golden/type_errors/typed_array_member_push_mismatch.cd <<'CASE'
let xs: [number] = [1];
xs.push("bad");
CASE
cat > tests/golden/type_errors/typed_array_member_push_mismatch.err <<'CASE'
Type error at 2:13: push value expects number, got string
CASE
cat > tests/golden/type_errors/typed_array_member_push_mismatch.exit <<'CASE'
1
CASE
```

- [ ] **Step 9: Run focused tests and verify expected failures**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case collection_inference_nullable_literal_interface \
  --case collection_inference_member_push_interface \
  --case collection_inference_function_push_interface \
  --case collection_inference_nullable_widening_interface \
  --case collection_inference_index_assignment_interface \
  --case collection_inference_mixed_dynamic_interface \
  --case typed_array_member_push_mismatch
```

Expected: command exits non-zero. The first three interface fixtures fail because current inferred output is `array` instead of `[number?]`, `[number]`, or `[string]`. The nullable-widening, index-assignment, and mixed-dynamic interface fixtures fail because current type checking reports `push value expects number, got nil`, `array index assignment expects nil, got number`, or `push value expects number, got string`. The explicit typed member-push mismatch fixture passes.

Run the runtime fixture through the Rust VM runner:

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case collection_inference_readers
```

Expected: this may already pass because current `unknown` types are permissive; keep it as runtime parity coverage for the final implementation.

- [ ] **Step 10: Commit failing tests**

```sh
git add \
  tests/golden/collection_inference_nullable_literal_interface \
  tests/golden/collection_inference_member_push_interface \
  tests/golden/collection_inference_function_push_interface \
  tests/golden/collection_inference_nullable_widening_interface \
  tests/golden/collection_inference_index_assignment_interface \
  tests/golden/collection_inference_mixed_dynamic_interface \
  tests/golden/collection_inference_readers \
  tests/golden/type_errors/typed_array_member_push_mismatch.cd \
  tests/golden/type_errors/typed_array_member_push_mismatch.err \
  tests/golden/type_errors/typed_array_member_push_mismatch.exit
git commit -m "test: add collection inference coverage"
```

---

### Task 2: Add merge helper and array literal inference

**Files:**
- Modify: `include/TypeUtils.hpp`
- Modify: `src/TypeUtils.cpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Declare the merge helper**

In `include/TypeUtils.hpp`, after `bool compatible(const TypeInfo& expected, const TypeInfo& actual);`, add:

```cpp
std::optional<TypeInfo> mergeArrayElementTypes(const TypeInfo& left, const TypeInfo& right);
```

`<optional>` is already included in this header.

- [ ] **Step 2: Implement the merge helper**

In `src/TypeUtils.cpp`, replace the include block:

```cpp
#include "TypeUtils.hpp"

#include <cstddef>
#include <utility>
```

with:

```cpp
#include "TypeUtils.hpp"

#include <cstddef>
#include <optional>
#include <utility>
```

Then add this function after `bool compatible(...)`:

```cpp
std::optional<TypeInfo> mergeArrayElementTypes(const TypeInfo& left, const TypeInfo& right)
{
    if (!isKnown(left) || !isKnown(right)) {
        return std::nullopt;
    }

    if (left.kind == StaticType::Nil && right.kind == StaticType::Nil) {
        return simpleType(StaticType::Nil);
    }

    if (isNullable(left)) {
        if (right.kind == StaticType::Nil) {
            return left;
        }
        if (isNullable(right)) {
            std::optional<TypeInfo> inner = mergeArrayElementTypes(*left.nullableOf, *right.nullableOf);
            if (!inner) {
                return std::nullopt;
            }
            return nullableType(std::move(*inner));
        }
        std::optional<TypeInfo> inner = mergeArrayElementTypes(*left.nullableOf, right);
        if (!inner) {
            return std::nullopt;
        }
        return nullableType(std::move(*inner));
    }

    if (isNullable(right)) {
        if (left.kind == StaticType::Nil) {
            return right;
        }
        std::optional<TypeInfo> inner = mergeArrayElementTypes(left, *right.nullableOf);
        if (!inner) {
            return std::nullopt;
        }
        return nullableType(std::move(*inner));
    }

    if (left.kind == StaticType::Nil) {
        return nullableType(right);
    }
    if (right.kind == StaticType::Nil) {
        return nullableType(left);
    }

    if (left.kind == StaticType::Array && right.kind == StaticType::Array) {
        if (!left.elementType || !right.elementType) {
            return simpleType(StaticType::Array);
        }
        std::optional<TypeInfo> element = mergeArrayElementTypes(*left.elementType, *right.elementType);
        if (!element) {
            return std::nullopt;
        }
        return arrayType(std::move(*element));
    }

    if (compatible(left, right) && compatible(right, left)) {
        return left;
    }

    return std::nullopt;
}
```

- [ ] **Step 3: Update array literal inference**

In `src/TypeChecker.cpp`, replace `TypeChecker::inferArrayElementType` with:

```cpp
TypeInfo TypeChecker::inferArrayElementType(const ArrayExpr& expression)
{
    std::optional<TypeInfo> current;
    for (const auto& element : expression.elements) {
        TypeInfo elementType = checkExpression(*element);
        if (!isKnown(elementType)) {
            return unknownType();
        }
        if (!current) {
            current = std::move(elementType);
            continue;
        }
        std::optional<TypeInfo> merged = mergeArrayElementTypes(*current, elementType);
        if (!merged) {
            return unknownType();
        }
        current = std::move(*merged);
    }
    return current ? *current : unknownType();
}
```

- [ ] **Step 4: Build and run the nullable literal interface test**

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case collection_inference_nullable_literal_interface
```

Expected: both commands pass. If the golden test fails, inspect the `module-interface.out` diff and fix only the merge or literal inference code.

- [ ] **Step 5: Run existing typed array tests that should stay stable**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case typed_array_assignment_mismatch \
  --case typed_array_index_assignment_mismatch \
  --case typed_array_push_mismatch \
  --case nullable_collections_structs
```

Expected: all selected checks pass.

- [ ] **Step 6: Commit array literal inference**

```sh
git add include/TypeUtils.hpp src/TypeUtils.cpp src/TypeChecker.cpp
git commit -m "feat: infer nullable array literals"
```

---

### Task 3: Refine unannotated arrays through push calls

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Declare direct variable mutation helpers**

In `include/TypeChecker.hpp`, after `Binding* findVariable(const std::string& name);`, add:

```cpp
    Binding* findSimpleVariableBinding(const Expr& expression);
    const Binding* findSimpleVariableBinding(const Expr& expression) const;
```

After `TypeInfo inferArrayElementType(const ArrayExpr& expression);`, add:

```cpp
    void refineArrayBindingFromMutation(Binding& target, const TypeInfo& valueType);
```

- [ ] **Step 2: Implement direct variable lookup helpers**

In `src/TypeChecker.cpp`, after the non-const and const `findVariable` overloads, add:

```cpp
TypeChecker::Binding* TypeChecker::findSimpleVariableBinding(const Expr& expression)
{
    const auto* variable = dynamic_cast<const VariableExpr*>(&expression);
    if (!variable) {
        return nullptr;
    }
    return findVariable(variable->name.lexeme);
}

const TypeChecker::Binding* TypeChecker::findSimpleVariableBinding(const Expr& expression) const
{
    const auto* variable = dynamic_cast<const VariableExpr*>(&expression);
    if (!variable) {
        return nullptr;
    }
    return findVariable(variable->name.lexeme);
}
```

- [ ] **Step 3: Implement mutation refinement helper**

In `src/TypeChecker.cpp`, after `TypeChecker::inferArrayElementType`, add:

```cpp
void TypeChecker::refineArrayBindingFromMutation(Binding& target, const TypeInfo& valueType)
{
    if (target.explicitType) {
        return;
    }

    if (!isKnown(valueType)) {
        target.type = simpleType(StaticType::Array);
        return;
    }

    if (!isKnown(target.type) || (target.type.kind == StaticType::Array && !target.type.elementType)) {
        target.type = arrayType(valueType);
        return;
    }

    if (target.type.kind != StaticType::Array) {
        return;
    }

    if (!target.type.elementType) {
        target.type = arrayType(valueType);
        return;
    }

    std::optional<TypeInfo> merged = mergeArrayElementTypes(*target.type.elementType, valueType);
    if (!merged) {
        target.type = simpleType(StaticType::Array);
        return;
    }

    target.type = arrayType(std::move(*merged));
}
```

- [ ] **Step 4: Update function-style `push(xs, value)`**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkNativeStdlibCall`, replace the `case NativeFunctionKind::Push` body with:

```cpp
    case NativeFunctionKind::Push: {
        const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
        if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "push expects array as first argument, got " + typeInfoName(arrayArgument.type));
        }

        Binding* target = findSimpleVariableBinding(*expression.arguments[0]);
        const bool strictElementCheck = target == nullptr || target->explicitType;
        const TypeInfo* expectedElement = strictElementCheck ? arrayArgument.type.elementType.get() : nullptr;
        const CheckedExpression valueArgument = checkExpressionInfo(*expression.arguments[1], expectedElement);
        if (strictElementCheck && expectedElement && !compatible(*expectedElement, valueArgument.type)) {
            throw TypeError(expression.paren,
                "push value expects " + typeInfoName(*expectedElement)
                    + ", got " + typeInfoName(valueArgument.type));
        }
        if (target && target->type.kind == StaticType::Array) {
            refineArrayBindingFromMutation(*target, valueArgument.type);
        }
        return CheckedExpression{simpleType(StaticType::Nil)};
    }
```

- [ ] **Step 5: Update member `xs.push(value)`**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkMemberCall`, replace the `if (name == "push")` block with:

```cpp
    if (name == "push") {
        expectArity(1);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::Array) {
            throw TypeError(expression.paren, "push expects array receiver, got " + typeInfoName(receiver.type));
        }

        Binding* target = findSimpleVariableBinding(*expression.receiver);
        const bool strictElementCheck = target == nullptr || target->explicitType;
        const TypeInfo* expectedElement = strictElementCheck ? receiver.type.elementType.get() : nullptr;
        const CheckedExpression value = checkExpressionInfo(*expression.arguments[0], expectedElement);
        if (strictElementCheck && expectedElement && !compatible(*expectedElement, value.type)) {
            throw TypeError(expression.paren,
                "push value expects " + typeInfoName(*expectedElement) + ", got " + typeInfoName(value.type));
        }
        if (target && target->type.kind == StaticType::Array) {
            refineArrayBindingFromMutation(*target, value.type);
        }
        return CheckedExpression{simpleType(StaticType::Nil)};
    }
```

- [ ] **Step 6: Build and run push-focused tests**

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case collection_inference_member_push_interface \
  --case collection_inference_function_push_interface \
  --case collection_inference_nullable_widening_interface \
  --case collection_inference_mixed_dynamic_interface \
  --case typed_array_push_mismatch \
  --case typed_array_member_push_mismatch
```

Expected: all selected checks pass. The explicit typed push fixtures must still report type errors, while unannotated push fixtures must produce the expected module-interface types.

- [ ] **Step 7: Run reader runtime fixture**

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case collection_inference_readers
```

Expected: emit and Rust run checks pass with output:

```text
1
a
nil
0
```

- [ ] **Step 8: Commit push refinement**

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: refine array types through push"
```

---

### Task 4: Refine unannotated arrays through index assignment

**Files:**
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Update index assignment value checking**

In `src/TypeChecker.cpp`, replace `TypeChecker::checkIndexAssignment` with:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkIndexAssignment(const IndexAssignExpr& expression)
{
    const IndexTargetTypes target = checkIndexTarget(
        *expression.collection, *expression.index, expression.bracket, "can only assign array elements");

    Binding* binding = findSimpleVariableBinding(*expression.collection);
    const bool strictElementCheck = binding == nullptr || binding->explicitType;
    const TypeInfo* expectedElement = strictElementCheck ? target.collection.elementType.get() : nullptr;
    const CheckedExpression value = checkExpressionInfo(*expression.value, expectedElement);
    if (strictElementCheck && expectedElement && !compatible(*expectedElement, value.type)) {
        throw TypeError(expression.bracket,
            "array index assignment expects " + typeInfoName(*expectedElement)
                + ", got " + typeInfoName(value.type));
    }

    if (binding && binding->type.kind == StaticType::Array) {
        refineArrayBindingFromMutation(*binding, value.type);
        return CheckedExpression{value.type};
    }

    return value;
}
```

- [ ] **Step 2: Build and run index assignment tests**

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case collection_inference_index_assignment_interface \
  --case typed_array_index_assignment_mismatch \
  --case array_assign_non_array \
  --case array_assign_non_number_index
```

Expected: all selected checks pass. The unannotated `[nil]` fixture exports `[number?]`; explicit typed array and non-array/index diagnostics remain unchanged.

- [ ] **Step 3: Run all collection inference focused tests**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case collection_inference_nullable_literal_interface \
  --case collection_inference_member_push_interface \
  --case collection_inference_function_push_interface \
  --case collection_inference_nullable_widening_interface \
  --case collection_inference_index_assignment_interface \
  --case collection_inference_mixed_dynamic_interface \
  --case typed_array_member_push_mismatch
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case collection_inference_readers
```

Expected: both commands pass.

- [ ] **Step 4: Commit index assignment refinement**

```sh
git add src/TypeChecker.cpp
git commit -m "feat: refine array types through index assignment"
```

---

### Task 5: Update documentation and roadmap

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README type inference text**

In `README.md`, replace the sentence that currently says:

```text
Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and anonymous `struct`; non-empty unannotated array literals infer an element type only when all known elements have the same type. Mixed unannotated arrays and empty unannotated arrays remain dynamic arrays with unknown element type.
```

with:

```text
Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and anonymous `struct`; non-empty unannotated array literals merge compatible element types, including nullable combinations such as `[1, nil]` becoming `[number?]` and nested homogeneous arrays preserving nested element types. Empty unannotated arrays start as dynamic arrays with unknown element type, then direct simple-variable mutations such as `push(xs, value)`, `xs.push(value)`, and `xs[index] = value` can refine the element type for later indexing, `pop`, and `for-in` reads. Mixed unannotated arrays and incompatible direct mutations fall back to dynamic arrays with unknown element type, while explicit array annotations remain strict.
```

- [ ] **Step 2: Update roadmap priority band**

In `docs/roadmap.md`, replace the M3 list:

```markdown
1. Add parser recovery and multiple diagnostics.
2. Improve collection type inference.
3. Define a `.cdbc` version-compatibility policy.
4. Treat GC as a dedicated backend project; continue to defer task scheduling
   and JIT exploration.
```

with:

```markdown
1. Define a `.cdbc` version-compatibility policy.
2. Treat GC as a dedicated backend project; continue to defer task scheduling
   and JIT exploration.
```

Replace the immediate dependency order block:

```markdown
```text
parser recovery and multiple diagnostics
```
```

with:

````markdown
```text
.cdbc version-compatibility policy
```
````

In `Phase 9: Richer Type System`, replace this future-work bullet:

```markdown
- Improve collection inference while preserving mixed-array dynamic escape
  hatches.
```

with:

```markdown
- No active near-term collection inference work remains after nullable-aware
  array literal merging and direct unannotated array mutation refinement.
```

In `Near-Term Recommendation`, replace the final text block:

```markdown
```text
parser recovery and multiple diagnostics
```
```

with:

````markdown
```text
.cdbc version-compatibility policy
```
````

- [ ] **Step 3: Run documentation-adjacent focused tests**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case collection_inference_nullable_literal_interface \
  --case collection_inference_member_push_interface \
  --case collection_inference_function_push_interface \
  --case collection_inference_nullable_widening_interface \
  --case collection_inference_index_assignment_interface \
  --case collection_inference_mixed_dynamic_interface
```

Expected: all selected checks pass.

- [ ] **Step 4: Commit docs**

```sh
git add README.md docs/roadmap.md
git commit -m "docs: document collection inference"
```

---

### Task 6: Full verification and cleanup

**Files:**
- No source changes expected.
- Remove: `tests/__pycache__/` if Python creates it.

- [ ] **Step 1: Check git status before verification**

```sh
git status --short
```

Expected: no output. If documentation or golden files are still modified, inspect them and commit intentional changes before continuing.

- [ ] **Step 2: Run full required verification**

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

Expected: every command exits 0. Record the exact output summary for the final response.

- [ ] **Step 3: Check final git status**

```sh
git status --short
```

Expected: no output after `rm -rf tests/__pycache__`. If any generated files remain, remove them or explain why they are intentional.

- [ ] **Step 4: Prepare final implementation summary**

Report:

```text
Implemented collection type inference:
- nullable/nested array literal element merging;
- direct unannotated array refinement through push and index assignment;
- mixed-array dynamic fallback;
- docs and roadmap updates.

Verification run:
- cmake -S . -B build: passed
- cmake --build build: passed
- ctest --test-dir build --output-on-failure: passed
- python3 tests/run_golden_tests.py ./build/compiler_design: passed
- python3 tests/run_golden_tests_selftest.py: passed
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: passed
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: passed
- cargo test --manifest-path vm-rs/Cargo.toml: passed
```
