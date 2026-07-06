# Array Element Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `[T]` static array element type annotations and propagate known element types through literals, indexing, assignment, `push`, and `pop`.

**Architecture:** Extend existing `TypeAnnotation` and `TypeInfo` trees with one recursive array element child. Keep runtime arrays, IR, bytecode, and Rust VM semantics unchanged; typed arrays are erased after type checking. Use contextual type checking for array literals so explicit `[T]` destinations reject mismatched elements instead of degrading to dynamic `array`.

**Tech Stack:** C++17 recursive-descent parser, AST printer, TypeChecker, Python golden tests, CMake/CTest, Rust VM parity tests.

---

## File Structure

- Modify `include/Ast.hpp`: add `TypeAnnotation::Kind::Array`, `TypeAnnotation::array`, `elementType`, and an `ArrayExpr` bracket token for diagnostics.
- Modify `src/Ast.cpp`: print `[T]` type annotations and construct array annotations/literals.
- Modify `include/Parser.hpp`: keep current parser API, no new public API.
- Modify `src/Parser.cpp`: parse `[` typeExpr `]` in type annotations and pass the array literal bracket token to `ArrayExpr`.
- Modify `include/TypeChecker.hpp`: add `TypeInfo::elementType`; add contextual expression checking helpers.
- Modify `src/TypeChecker.cpp`: add array type helpers, recursive compatibility/name formatting, contextual array literal checks, index result typing, index assignment checks, `push` value checks, and `pop` result typing.
- Modify `docs/language-grammar.ebnf`: document `arrayType`.
- Modify `README.md`: document `[T]` annotations and dynamic mixed-array escape hatch.
- Modify `docs/roadmap.md`: mark Phase 9F implemented after code and tests pass.
- Create/modify `tests/golden/*`: success, parse-error, and type-error fixtures.
- Modify `tests/run_rust_vm_tests.py`: include one typed-array runtime parity case if no existing discovered golden covers it.

---

### Task 1: Add parser and AST support for `[T]` annotations

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `src/Parser.cpp`
- Create: `tests/golden/typed_array_annotations/input.cd`
- Create: `tests/golden/typed_array_annotations/ast.out`
- Create: `tests/golden/parse_errors/array_type_missing_bracket.cd`
- Create: `tests/golden/parse_errors/array_type_missing_bracket.err`
- Create: `tests/golden/parse_errors/array_type_missing_bracket.exit`

- [ ] **Step 1: Create parser/AST golden fixtures that fail before implementation**

Create `tests/golden/typed_array_annotations/input.cd`:

```cd
let numbers: [number] = [1, 2, 3];
let matrix: [[number]] = [[1], [2]];
let callbacks: [fun(number): number] = [fun (x: number): number { return x; }];

fun makeStrings(): [string] {
  return ["a", "b"];
}

struct Bag { values: [number] }
```

Create `tests/golden/typed_array_annotations/ast.out`:

```text
Program
  Let numbers: [number] = (array 1 2 3)
  Let matrix: [[number]] = (array (array 1) (array 2))
  Let callbacks: [fun(number): number] = (array (fun (x: number): number (return x)))
  Fun makeStrings(): [string]
    Return (array "a" "b")
  Struct Bag {values: [number]}
```

Create `tests/golden/parse_errors/array_type_missing_bracket.cd`:

```cd
let xs: [number = [];
```

Create `tests/golden/parse_errors/array_type_missing_bracket.err`:

```text
Parse error at 1:17: expected `]` after array element type, found =
```

Create `tests/golden/parse_errors/array_type_missing_bracket.exit`:

```text
1
```

- [ ] **Step 2: Run the focused golden tests and confirm parser failure**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. `typed_array_annotations` should fail with a parse error near `[` in a type annotation, and `array_type_missing_bracket` may fail with a different parse message until the new parser branch exists.

- [ ] **Step 3: Extend `TypeAnnotation` and `ArrayExpr` in `include/Ast.hpp`**

Edit `include/Ast.hpp` so the relevant declarations become:

```cpp
struct TypeAnnotation {
    enum class Kind {
        Simple,
        Function,
        Array,
    };

    static TypeAnnotation simple(Token token);
    static TypeAnnotation function(Token token, std::vector<TypeAnnotation> parameterTypes, TypeAnnotation returnType);
    static TypeAnnotation array(Token token, TypeAnnotation elementType);

    Kind kind = Kind::Simple;
    Token token{TokenType::Identifier, "", 0, 0};
    std::vector<TypeAnnotation> parameterTypes;
    std::shared_ptr<TypeAnnotation> returnType;
    std::shared_ptr<TypeAnnotation> elementType;
};
```

Change `ArrayExpr` to store its opening bracket:

```cpp
struct ArrayExpr final : Expr {
    ArrayExpr(Token bracket, std::vector<ExprPtr> elements);
    void print(std::ostream& out) const override;

    Token bracket;
    std::vector<ExprPtr> elements;
};
```

- [ ] **Step 4: Extend AST printing and constructors in `src/Ast.cpp`**

Replace `writeTypeAnnotation` with this structure:

```cpp
void writeTypeAnnotation(std::ostream& out, const TypeAnnotation& annotation)
{
    if (annotation.kind == TypeAnnotation::Kind::Simple) {
        out << annotation.token.lexeme;
        return;
    }

    if (annotation.kind == TypeAnnotation::Kind::Array) {
        out << '[';
        writeTypeAnnotation(out, *annotation.elementType);
        out << ']';
        return;
    }

    out << "fun(";
    for (std::size_t i = 0; i < annotation.parameterTypes.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        writeTypeAnnotation(out, annotation.parameterTypes[i]);
    }
    out << "): ";
    writeTypeAnnotation(out, *annotation.returnType);
}
```

Add the array annotation factory after `TypeAnnotation::function`:

```cpp
TypeAnnotation TypeAnnotation::array(Token token, TypeAnnotation elementType)
{
    TypeAnnotation result;
    result.kind = Kind::Array;
    result.token = std::move(token);
    result.elementType = std::make_shared<TypeAnnotation>(std::move(elementType));
    return result;
}
```

Replace the `ArrayExpr` constructor with:

```cpp
ArrayExpr::ArrayExpr(Token bracket, std::vector<ExprPtr> elements)
    : bracket(std::move(bracket))
    , elements(std::move(elements))
{
}
```

Leave `ArrayExpr::print` output unchanged.

- [ ] **Step 5: Parse array type annotations and preserve array literal brackets in `src/Parser.cpp`**

At the start of `Parser::typeAnnotation`, before the `Fun` branch, add:

```cpp
if (match(TokenType::LeftBracket)) {
    Token bracket = previous();
    TypeAnnotation elementType = typeAnnotation("expected array element type after `[`");
    consume(TokenType::RightBracket, "expected `]` after array element type");
    return TypeAnnotation::array(std::move(bracket), std::move(elementType));
}
```

In `Parser::primary`, change the array literal branch to pass the bracket token:

```cpp
if (match(TokenType::LeftBracket)) {
    Token bracket = previous();
    return arrayLiteral(std::move(bracket));
}
```

Update the `arrayLiteral` declaration in `include/Parser.hpp`:

```cpp
ExprPtr arrayLiteral(Token bracket);
```

Update the definition in `src/Parser.cpp`:

```cpp
ExprPtr Parser::arrayLiteral(Token bracket)
{
    std::vector<ExprPtr> elements;
    if (!check(TokenType::RightBracket)) {
        do {
            elements.push_back(expression());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBracket, "expected `]` after array elements");
    return std::make_unique<ArrayExpr>(std::move(bracket), std::move(elements));
}
```

- [ ] **Step 6: Build and run the focused golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS for parse/AST coverage added in this task, or FAIL only in type-checking behavior that later tasks intentionally add. If the parse error location differs, inspect `tests/golden/parse_errors/array_type_missing_bracket.err` and update it only if the parser points at the `=` token and the message remains `expected `]` after array element type`.

- [ ] **Step 7: Commit parser and AST support**

Run:

```bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/typed_array_annotations tests/golden/parse_errors/array_type_missing_bracket.*
git commit -m "feat: parse array type annotations"
```

---

### Task 2: Add recursive array type information and compatibility

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Extend `TypeInfo` in `include/TypeChecker.hpp`**

Change `TypeInfo` to include an element type pointer:

```cpp
struct TypeInfo {
    StaticType kind = StaticType::Unknown;
    std::vector<TypeInfo> parameterTypes;
    std::shared_ptr<TypeInfo> returnType;
    std::optional<std::string> structName;
    std::shared_ptr<TypeInfo> elementType;
};
```

- [ ] **Step 2: Add array helpers and update existing helper initializers in `src/TypeChecker.cpp`**

Replace the simple helper returns at the top of the file with designated field assignments to avoid constructor argument churn:

```cpp
TypeInfo unknownType()
{
    return TypeInfo{};
}

TypeInfo simpleType(StaticType kind)
{
    TypeInfo result;
    result.kind = kind;
    return result;
}
```

Add this helper after `namedStructType`:

```cpp
TypeInfo arrayType(TypeInfo elementType)
{
    TypeInfo result;
    result.kind = StaticType::Array;
    result.elementType = std::make_shared<TypeInfo>(std::move(elementType));
    return result;
}
```

Replace `functionWithoutSignature` with:

```cpp
TypeInfo functionWithoutSignature()
{
    TypeInfo result;
    result.kind = StaticType::Function;
    return result;
}
```

- [ ] **Step 3: Update type name formatting for arrays**

In `typeInfoName`, add the array branch before the function branch:

```cpp
if (type.kind == StaticType::Array && type.elementType) {
    return "[" + typeInfoName(*type.elementType) + "]";
}
```

Keep `staticTypeName(StaticType::Array)` returning `array` for arrays without known element type.

- [ ] **Step 4: Update recursive compatibility for arrays**

In `compatible`, add the array branch after the struct branch and before the function branch:

```cpp
if (expected.kind == StaticType::Array) {
    if (!expected.elementType || !actual.elementType) {
        return true;
    }
    return compatible(*expected.elementType, *actual.elementType);
}
```

Leave function compatibility unchanged after this branch.

- [ ] **Step 5: Resolve `[T]` annotations in `resolveAnnotation`**

At the top of `TypeChecker::resolveAnnotation`, before the function branch, add:

```cpp
if (typeName.kind == TypeAnnotation::Kind::Array) {
    return arrayType(resolveAnnotation(*typeName.elementType));
}
```

- [ ] **Step 6: Build and run the focused tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS. This task changes the internal type model and annotation resolution, while existing untyped array behavior remains unchanged.

- [ ] **Step 7: Commit recursive type model support**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: model array element types"
```

---

### Task 3: Infer and context-check array literals

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/typed_array_runtime/input.cd`
- Create: `tests/golden/typed_array_runtime/ast.out`
- Create: `tests/golden/typed_array_runtime/run.out`
- Create: `tests/golden/type_errors/typed_array_literal_element_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_literal_element_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_literal_element_mismatch.exit`
- Create: `tests/golden/type_errors/typed_array_function_return_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_function_return_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_function_return_mismatch.exit`
- Create: `tests/golden/type_errors/typed_array_assignment_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_assignment_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_assignment_mismatch.exit`

- [ ] **Step 1: Create success and contextual mismatch fixtures**

Create `tests/golden/typed_array_runtime/input.cd`:

```cd
let xs: [number] = [1, 2, 3];
print xs[1];
let empty: [string] = [];
push(empty, "ok");
print pop(empty);
```

Create `tests/golden/typed_array_runtime/ast.out`:

```text
Program
  Let xs: [number] = (array 1 2 3)
  Print (index xs 1)
  Let empty: [string] = (array)
  Expr (call push empty "ok")
  Print (call pop empty)
```

Create `tests/golden/typed_array_runtime/run.out`:

```text
2
ok
```

Create `tests/golden/type_errors/typed_array_literal_element_mismatch.cd`:

```cd
let bad: [number] = [1, "bad"];
```

Create `tests/golden/type_errors/typed_array_literal_element_mismatch.err`:

```text
Type error at 1:21: array element expects number, got string
```

Create `tests/golden/type_errors/typed_array_literal_element_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/typed_array_function_return_mismatch.cd`:

```cd
fun bad(): [number] {
  return [1, "bad"];
}
```

Create `tests/golden/type_errors/typed_array_function_return_mismatch.err`:

```text
Type error at 2:10: array element expects number, got string
```

Create `tests/golden/type_errors/typed_array_function_return_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/typed_array_assignment_mismatch.cd`:

```cd
let xs: [number] = [1];
let ys = ["bad"];
xs = ys;
```

Create `tests/golden/type_errors/typed_array_assignment_mismatch.err`:

```text
Type error at 3:1: cannot assign [string] to `xs` of type [number]
```

Create `tests/golden/type_errors/typed_array_assignment_mismatch.exit`:

```text
1
```

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL because array literals are not yet inferred or context-checked by element type.

- [ ] **Step 2: Add contextual checking declarations in `include/TypeChecker.hpp`**

Add these private declarations near `checkExpressionInfo`:

```cpp
CheckedExpression checkExpressionInfo(const Expr& expression, const TypeInfo* expectedType);
CheckedExpression checkArrayLiteral(const ArrayExpr& expression, const TypeInfo* expectedType);
TypeInfo inferArrayElementType(const ArrayExpr& expression);
```

Keep the existing single-argument declaration as the public private wrapper:

```cpp
CheckedExpression checkExpressionInfo(const Expr& expression);
```

- [ ] **Step 3: Add the wrapper and contextual overload in `src/TypeChecker.cpp`**

Replace the start of expression checking with:

```cpp
TypeInfo TypeChecker::checkExpression(const Expr& expression)
{
    return checkExpressionInfo(expression).type;
}

TypeChecker::CheckedExpression TypeChecker::checkExpressionInfo(const Expr& expression)
{
    return checkExpressionInfo(expression, nullptr);
}

TypeChecker::CheckedExpression TypeChecker::checkExpressionInfo(const Expr& expression, const TypeInfo* expectedType)
{
```

Keep the existing function body contents under the new overload.

- [ ] **Step 4: Implement array literal inference helpers**

Before `checkExpressionInfo`, add:

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
        if (!(compatible(*current, elementType) && compatible(elementType, *current))) {
            return unknownType();
        }
    }
    return current ? *current : unknownType();
}

TypeChecker::CheckedExpression TypeChecker::checkArrayLiteral(const ArrayExpr& expression, const TypeInfo* expectedType)
{
    if (expectedType && expectedType->kind == StaticType::Array && expectedType->elementType) {
        for (const auto& element : expression.elements) {
            const CheckedExpression actual = checkExpressionInfo(*element, expectedType->elementType.get());
            if (!compatible(*expectedType->elementType, actual.type)) {
                throw TypeError(expression.bracket,
                    "array element expects " + typeInfoName(*expectedType->elementType)
                        + ", got " + typeInfoName(actual.type));
            }
        }
        return CheckedExpression{*expectedType};
    }

    const TypeInfo element = inferArrayElementType(expression);
    if (isKnown(element)) {
        return CheckedExpression{arrayType(element)};
    }
    return CheckedExpression{simpleType(StaticType::Array)};
}
```

- [ ] **Step 5: Route array expressions through the helper**

Inside the new contextual `checkExpressionInfo` overload, replace the array literal branch with:

```cpp
if (const auto* array = dynamic_cast<const ArrayExpr*>(&expression)) {
    return checkArrayLiteral(*array, expectedType);
}
```

- [ ] **Step 6: Pass expected types from typed `let`, assignment, return, calls, and struct fields**

In `checkLetInitializer`, replace the non-struct initializer check with:

```cpp
const CheckedExpression initializer = checkExpressionInfo(*statement.initializer, &declared);
```

In the `AssignExpr` branch, check the target before checking the value. Use this structure:

```cpp
if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
    Binding* target = findVariable(assign->name.lexeme);
    if (!target) {
        throw TypeError(assign->name, "undefined variable `" + assign->name.lexeme + "`");
    }

    const CheckedExpression value = checkExpressionInfo(*assign->value, &target->type);
```

Keep the existing function-specific and compatibility checks after that line.

In the return statement branch in `checkStatement`, replace returned expression checking with:

```cpp
const TypeInfo* expectedReturn = nullptr;
if (!returnContexts_.empty() && returnContexts_.back().expectedReturnType) {
    expectedReturn = &*returnContexts_.back().expectedReturnType;
}
const TypeInfo returned = returnStmt->value
    ? checkExpressionInfo(*returnStmt->value, expectedReturn).type
    : simpleType(StaticType::Nil);
```

In `checkNamedStructLiteralInitializer`, replace field value checking with:

```cpp
const CheckedExpression actual = checkExpressionInfo(*found->second->value, &expectedField.type);
```

In `checkCall`, when the callee has a signature, check arguments with their expected parameter types. Replace the existing pre-check loop plus signature branch with this shape:

```cpp
std::vector<CheckedExpression> arguments;
arguments.reserve(expression.arguments.size());

if (callee.type.kind == StaticType::Function && hasFunctionSignature(callee.type)) {
    if (callee.type.parameterTypes.size() != expression.arguments.size()) {
        throw TypeError(expression.paren, "expected " + std::to_string(callee.type.parameterTypes.size())
            + " arguments but got " + std::to_string(expression.arguments.size()));
    }
    for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
        arguments.push_back(checkExpressionInfo(*expression.arguments[i], &callee.type.parameterTypes[i]));
    }
} else {
    for (const auto& argument : expression.arguments) {
        arguments.push_back(checkExpressionInfo(*argument));
    }
}
```

Then keep the existing non-function check and argument compatibility loop using `arguments`.

- [ ] **Step 7: Run focused golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS for typed array literal success and mismatch fixtures. The assignment mismatch fixture in this task should now pass with `[string]` versus `[number]` in the diagnostic.

- [ ] **Step 8: Commit contextual literal checking**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/typed_array_runtime tests/golden/type_errors/typed_array_literal_element_mismatch.* tests/golden/type_errors/typed_array_function_return_mismatch.* tests/golden/type_errors/typed_array_assignment_mismatch.*
git commit -m "feat: check typed array literals"
```

---

### Task 4: Propagate element types through indexing, index assignment, `push`, and `pop`

**Files:**
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/typed_array_index_read_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_index_read_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_index_read_mismatch.exit`
- Create: `tests/golden/type_errors/typed_array_index_assignment_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_index_assignment_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_index_assignment_mismatch.exit`
- Create: `tests/golden/type_errors/typed_array_push_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_push_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_push_mismatch.exit`
- Create: `tests/golden/type_errors/typed_array_pop_result_mismatch.cd`
- Create: `tests/golden/type_errors/typed_array_pop_result_mismatch.err`
- Create: `tests/golden/type_errors/typed_array_pop_result_mismatch.exit`

- [ ] **Step 1: Create element operation fixtures**

Create `tests/golden/type_errors/typed_array_index_read_mismatch.cd`:

```cd
let xs: [number] = [1];
let s: string = xs[0];
```

Create `tests/golden/type_errors/typed_array_index_read_mismatch.err`:

```text
Type error at 2:5: cannot initialize `s` of type string with number
```

Create `tests/golden/type_errors/typed_array_index_read_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/typed_array_index_assignment_mismatch.cd`:

```cd
let xs: [number] = [1];
xs[0] = "bad";
```

Create `tests/golden/type_errors/typed_array_index_assignment_mismatch.err`:

```text
Type error at 2:3: array index assignment expects number, got string
```

Create `tests/golden/type_errors/typed_array_index_assignment_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/typed_array_push_mismatch.cd`:

```cd
let xs: [number] = [1];
push(xs, "bad");
```

Create `tests/golden/type_errors/typed_array_push_mismatch.err`:

```text
Type error at 2:5: push value expects number, got string
```

Create `tests/golden/type_errors/typed_array_push_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/typed_array_pop_result_mismatch.cd`:

```cd
let xs: [number] = [1];
let s: string = pop(xs);
```

Create `tests/golden/type_errors/typed_array_pop_result_mismatch.err`:

```text
Type error at 2:5: cannot initialize `s` of type string with number
```

Create `tests/golden/type_errors/typed_array_pop_result_mismatch.exit`:

```text
1
```

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL because index, push, and pop still return or accept unknown element types.

- [ ] **Step 2: Return element types from index reads**

In `TypeChecker::checkIndex`, replace the final return with:

```cpp
if (collection.kind == StaticType::Array && collection.elementType) {
    return *collection.elementType;
}
return unknownType();
```

- [ ] **Step 3: Check index assignment values against element types**

In `TypeChecker::checkIndexAssignment`, check collection and index first, then check the assigned value with the element type when known. Use this structure:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkIndexAssignment(const IndexAssignExpr& expression)
{
    const TypeInfo collection = checkExpression(*expression.collection);
    const TypeInfo index = checkExpression(*expression.index);

    if (collection.kind != StaticType::Unknown && collection.kind != StaticType::Array) {
        throw TypeError(expression.bracket, "can only assign array elements");
    }

    if (index.kind != StaticType::Unknown && index.kind != StaticType::Number) {
        throw TypeError(expression.bracket, "array index must be number");
    }

    const TypeInfo* expectedElement = collection.elementType.get();
    const CheckedExpression value = checkExpressionInfo(*expression.value, expectedElement);
    if (expectedElement && !compatible(*expectedElement, value.type)) {
        throw TypeError(expression.bracket,
            "array index assignment expects " + typeInfoName(*expectedElement)
                + ", got " + typeInfoName(value.type));
    }

    return value;
}
```

- [ ] **Step 4: Check `push` value and return `pop` element type**

In `checkNativeStdlibCall`, replace the current eager argument checking loop with kind-specific checking. Use this structure after arity validation:

```cpp
switch (function->kind) {
case NativeFunctionKind::Push: {
    const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
    if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
        throw TypeError(expression.paren,
            "push expects array as first argument, got " + typeInfoName(arrayArgument.type));
    }
    const TypeInfo* expectedElement = arrayArgument.type.elementType.get();
    const CheckedExpression valueArgument = checkExpressionInfo(*expression.arguments[1], expectedElement);
    if (expectedElement && !compatible(*expectedElement, valueArgument.type)) {
        throw TypeError(expression.paren,
            "push value expects " + typeInfoName(*expectedElement)
                + ", got " + typeInfoName(valueArgument.type));
    }
    return CheckedExpression{simpleType(StaticType::Nil)};
}
case NativeFunctionKind::Pop: {
    const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
    if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
        throw TypeError(expression.paren,
            "pop expects array as first argument, got " + typeInfoName(arrayArgument.type));
    }
    if (arrayArgument.type.kind == StaticType::Array && arrayArgument.type.elementType) {
        return CheckedExpression{*arrayArgument.type.elementType};
    }
    return CheckedExpression{unknownType()};
}
case NativeFunctionKind::Floor:
case NativeFunctionKind::Ceil:
case NativeFunctionKind::Sqrt: {
    const CheckedExpression argument = checkExpressionInfo(*expression.arguments[0]);
    if (argument.type.kind != StaticType::Unknown && argument.type.kind != StaticType::Number) {
        throw TypeError(expression.paren,
            std::string(function->name) + " expects number, got " + typeInfoName(argument.type));
    }
    return CheckedExpression{simpleType(StaticType::Number)};
}
}
```

Keep the existing final `throw TypeError(...)` after the switch.

- [ ] **Step 5: Run focused golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS for all typed array type-error fixtures and existing push/pop/math static error fixtures.

- [ ] **Step 6: Commit element operation checking**

Run:

```bash
git add src/TypeChecker.cpp tests/golden/type_errors/typed_array_index_read_mismatch.* tests/golden/type_errors/typed_array_index_assignment_mismatch.* tests/golden/type_errors/typed_array_push_mismatch.* tests/golden/type_errors/typed_array_pop_result_mismatch.*
git commit -m "feat: type check array element operations"
```

---

### Task 5: Update docs, roadmap, and Rust VM parity coverage

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Update grammar documentation**

In `docs/language-grammar.ebnf`, replace the `typeExpr` section with:

```ebnf
typeExpr    = arrayType
            | functionType
            | simpleType ;

arrayType   = "[", typeExpr, "]" ;

simpleType  = identifier ;

functionType = "fun", "(", [ typeArguments ], ")", ":", typeExpr ;
```

- [ ] **Step 2: Update README type annotation text**

In `README.md`, update the type annotation paragraph so it states:

```markdown
Type annotations support `number`, `bool`, `string`, `nil`, named struct types, array types such as `[number]` and `[[string]]`, and function types such as `fun(number): string`. Function type annotations may be used on `let` bindings, function parameters, and function returns. Array type annotations may be used in the same positions and carry static element types through indexing, index assignment, `push`, and `pop`. Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and anonymous `struct`; non-empty unannotated array literals infer an element type only when all known elements have the same type. Mixed unannotated arrays and empty unannotated arrays remain dynamic arrays with unknown element type. Generic types and nullable type syntax are not implemented yet.
```

If the existing paragraph also documents scope rules, keep the existing scope sentences immediately after this replacement.

- [ ] **Step 3: Update roadmap Phase 9 status**

In `docs/roadmap.md`, update the Phase 9 status paragraph to include:

```markdown
Phase 9F is implemented: array type annotations use `[type]` syntax and known element types flow through array literals, indexing, index assignment, `push`, and `pop`.
```

Move `Array element types` out of the future feature bullet list or reword that bullet to mention deeper collection inference instead.

- [ ] **Step 4: Add Rust VM parity coverage if needed**

Open `tests/run_rust_vm_tests.py`. If it discovers all success goldens with `run.out`, no code change is needed because `tests/golden/typed_array_runtime/run.out` already covers this feature. If it uses an explicit allow-list, add `typed_array_runtime` to that list. The added line should match the surrounding style, for example:

```python
"typed_array_runtime",
```

- [ ] **Step 5: Run docs-sensitive and parity tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: PASS. The Rust VM should print the same output as `typed_array_runtime/run.out` because type annotations are erased before bytecode execution.

- [ ] **Step 6: Commit docs and parity updates**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md tests/run_rust_vm_tests.py tests/golden/typed_array_runtime
git commit -m "docs: document array element types"
```

---

### Task 6: Run full verification and cleanup

**Files:**
- No planned source edits unless verification exposes a concrete failure.

- [ ] **Step 1: Run the full verification set from `AGENTS.md`**

Run from the repository root:

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
```

Expected: every command exits with status 0.

- [ ] **Step 2: Inspect git status**

Run:

```bash
git status --short
```

Expected: no untracked `tests/__pycache__/` and no unintended build artifacts. Source, docs, and golden changes should either be committed by prior tasks or intentionally staged for a final fix commit.

- [ ] **Step 3: Commit any verification-only fixes**

If Step 1 revealed a concrete issue and you made a fix, commit it with a focused message:

```bash
git add <changed-files>
git commit -m "test: finalize array element type coverage"
```

If there are no changes, do not create an empty commit.

- [ ] **Step 4: Report exact verification results**

In the handoff message, list each command from Step 1 and whether it passed. Mention that runtime representation, IR, bytecode format, and Rust VM semantics were unchanged.
