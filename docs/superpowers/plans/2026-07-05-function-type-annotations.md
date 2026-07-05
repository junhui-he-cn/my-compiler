# Function Type Annotations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add first-class `fun(...): ...` function type annotations for `let`, function parameters, and function returns.

**Architecture:** Replace raw annotation tokens with a recursive AST `TypeAnnotation`, parse recursive type expressions at existing annotation sites, and replace ad hoc function metadata in the type checker with a recursive `TypeInfo`. Keep runtime, IR, bytecode, and Rust VM unchanged; this is a static type-system slice with golden coverage.

**Tech Stack:** C++17 recursive-descent parser, AST printer, static TypeChecker, Python golden tests, CMake, existing Rust VM parity checks for unchanged backend behavior.

---

## File Structure

- Modify `include/Ast.hpp`: add recursive `TypeAnnotation`; replace annotation fields on `Parameter`, `LetStmt`, `FunctionStmt`, and `FunctionExpr`.
- Modify `src/Ast.cpp`: add type annotation constructors/helpers and print recursive annotations.
- Modify `include/Parser.hpp`: add `typeAnnotation()` and `typeArguments()` parser helpers; update annotation helper return types.
- Modify `src/Parser.cpp`: parse simple and function type annotations at `let`, parameter, and return annotation sites.
- Modify `include/TypeChecker.hpp`: add recursive `TypeInfo`; replace `CheckedExpression`, `Binding`, and return context type fields.
- Modify `src/TypeChecker.cpp`: resolve recursive annotations, check function signature compatibility, check call arguments, and preserve existing simple type behavior.
- Add success fixtures under `tests/golden/function_type_*`.
- Add parse-error fixtures under `tests/golden/parse_errors/function_type_*`.
- Add type-error fixtures under `tests/golden/type_errors/function_type_*`.
- Modify `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, and `AGENTS.md` after behavior lands.

---

### Task 1: RED test for parsing and printing function type annotations

**Files:**
- Create: `tests/golden/function_type_annotations/input.cd`
- Create: `tests/golden/function_type_annotations/ast.out`

- [ ] **Step 1: Add the failing AST fixture**

Create `tests/golden/function_type_annotations/input.cd`:

```cd
let pred: fun(number): bool = fun (x: number): bool {
  return x > 0;
};

fun apply(f: fun(number): number, x: number): number {
  return f(x);
}

fun makeAdder(): fun(number): number {
  return fun (x: number): number {
    return x + 1;
  };
}
```

Create `tests/golden/function_type_annotations/ast.out`:

```text
Program
  Let pred: fun(number): bool = (fun (x: number): bool (return (> x 0)))
  Fun apply(f: fun(number): number, x: number): number
    Return (call f x)
  Fun makeAdder(): fun(number): number
    Return (fun (x: number): number (return (+ x 1)))
```

- [ ] **Step 2: Run the golden tests to verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. The new fixture should fail during parsing near `fun(number): bool` because annotation parsing currently expects an identifier token after `:`.

- [ ] **Step 3: Commit the RED fixture**

```bash
git add tests/golden/function_type_annotations/input.cd tests/golden/function_type_annotations/ast.out
git commit -m "test: cover function type annotation AST"
```

---

### Task 2: Parse and print recursive type annotations

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Test: `tests/golden/function_type_annotations/ast.out`

- [ ] **Step 1: Add recursive `TypeAnnotation` to `include/Ast.hpp`**

Replace the current `Parameter` definition:

```cpp
struct Parameter {
    Token name;
    std::optional<Token> typeName;
};
```

with:

```cpp
struct TypeAnnotation {
    enum class Kind {
        Simple,
        Function,
    };

    static TypeAnnotation simple(Token token);
    static TypeAnnotation function(Token token, std::vector<TypeAnnotation> parameterTypes, TypeAnnotation returnType);

    Kind kind = Kind::Simple;
    Token token;
    std::vector<TypeAnnotation> parameterTypes;
    std::shared_ptr<TypeAnnotation> returnType;
};

struct Parameter {
    Token name;
    std::optional<TypeAnnotation> typeName;
};
```

Add `#include <memory>` if it is not already present.

Update constructor declarations:

```cpp
struct FunctionExpr final : Expr {
    FunctionExpr(Token keyword, std::vector<Parameter> parameters, std::optional<TypeAnnotation> returnTypeName, std::vector<StmtPtr> body);
    // fields unchanged except:
    std::optional<TypeAnnotation> returnTypeName;
};

struct LetStmt final : Stmt {
    LetStmt(Token name, std::optional<TypeAnnotation> typeName, ExprPtr initializer);
    std::optional<TypeAnnotation> typeName;
};

struct FunctionStmt final : Stmt {
    FunctionStmt(Token name, std::vector<Parameter> parameters, std::optional<TypeAnnotation> returnTypeName, std::vector<StmtPtr> body);
    std::optional<TypeAnnotation> returnTypeName;
};
```

- [ ] **Step 2: Implement AST type annotation printing in `src/Ast.cpp`**

Add these helpers near `writeExpr`:

```cpp
void writeTypeAnnotation(std::ostream& out, const TypeAnnotation& annotation)
{
    if (annotation.kind == TypeAnnotation::Kind::Simple) {
        out << annotation.token.lexeme;
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

void writeOptionalTypeAnnotation(std::ostream& out, const std::optional<TypeAnnotation>& annotation)
{
    if (annotation) {
        out << ": ";
        writeTypeAnnotation(out, *annotation);
    }
}
```

Update `writeParameterList`, inline `LetStmt`, inline `FunctionStmt`, `FunctionExpr::print`, and `LetStmt::print` to call `writeOptionalTypeAnnotation(...)` instead of writing `typeName->lexeme`.

Add the `TypeAnnotation` factory methods:

```cpp
TypeAnnotation TypeAnnotation::simple(Token token)
{
    TypeAnnotation result;
    result.kind = Kind::Simple;
    result.token = std::move(token);
    return result;
}

TypeAnnotation TypeAnnotation::function(Token token, std::vector<TypeAnnotation> parameterTypes, TypeAnnotation returnType)
{
    TypeAnnotation result;
    result.kind = Kind::Function;
    result.token = std::move(token);
    result.parameterTypes = std::move(parameterTypes);
    result.returnType = std::make_shared<TypeAnnotation>(std::move(returnType));
    return result;
}
```

Update constructor definitions to accept `std::optional<TypeAnnotation>`.

- [ ] **Step 3: Add parser declarations in `include/Parser.hpp`**

Replace:

```cpp
std::optional<Token> optionalReturnType();
```

with:

```cpp
TypeAnnotation typeAnnotation();
std::vector<TypeAnnotation> typeArguments();
std::optional<TypeAnnotation> optionalReturnType();
```

- [ ] **Step 4: Parse recursive annotations in `src/Parser.cpp`**

In `letDeclaration()`, replace the annotation parse with:

```cpp
std::optional<TypeAnnotation> typeName;
if (match(TokenType::Colon)) {
    typeName = typeAnnotation();
}
```

In `parameter()`, replace the annotation parse with:

```cpp
std::optional<TypeAnnotation> typeName;
if (match(TokenType::Colon)) {
    typeName = typeAnnotation();
}
return Parameter{std::move(name), std::move(typeName)};
```

Replace `optionalReturnType()` with:

```cpp
std::optional<TypeAnnotation> Parser::optionalReturnType()
{
    if (!match(TokenType::Colon)) {
        return std::nullopt;
    }
    return typeAnnotation();
}
```

Add:

```cpp
TypeAnnotation Parser::typeAnnotation()
{
    if (match(TokenType::Fun)) {
        Token keyword = previous();
        consume(TokenType::LeftParen, "expected `(` after `fun` in function type");
        std::vector<TypeAnnotation> parameterTypes = typeArguments();
        consume(TokenType::RightParen, "expected `)` after function type parameters");
        consume(TokenType::Colon, "expected `:` before function type return");
        TypeAnnotation returnType = typeAnnotation();
        return TypeAnnotation::function(std::move(keyword), std::move(parameterTypes), std::move(returnType));
    }

    return TypeAnnotation::simple(consume(TokenType::Identifier, "expected type name"));
}

std::vector<TypeAnnotation> Parser::typeArguments()
{
    std::vector<TypeAnnotation> arguments;
    if (!check(TokenType::RightParen)) {
        do {
            arguments.push_back(typeAnnotation());
        } while (match(TokenType::Comma));
    }
    return arguments;
}
```

- [ ] **Step 5: Build and verify the AST fixture now fails in type checking, not parsing**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: build succeeds. The new fixture may still fail with a type-check limitation, but the parse error about `fun` after `:` must be gone. If the new AST fixture passes already, continue to Task 3.

- [ ] **Step 6: Commit parser and AST support**

```bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse function type annotations"
```

---

### Task 3: Introduce recursive `TypeInfo` without changing behavior

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: existing golden tests

- [ ] **Step 1: Replace ad hoc function metadata with `TypeInfo` declarations**

In `include/TypeChecker.hpp`, keep `enum class StaticType` unchanged and add before `class TypeChecker`:

```cpp
struct TypeInfo {
    StaticType kind = StaticType::Unknown;
    std::vector<TypeInfo> parameterTypes;
    std::shared_ptr<TypeInfo> returnType;
};
```

Add `#include <memory>`.

Update `CheckedExpression` to:

```cpp
struct CheckedExpression {
    TypeInfo type;
};
```

Update `Binding` to:

```cpp
struct Binding {
    TypeInfo type;
    std::string resolvedName;
    std::size_t scopeDepth = 0;
    std::size_t functionDepth = 0;
};
```

Update `FunctionReturnContext` to:

```cpp
struct FunctionReturnContext {
    bool sawReturn = false;
    TypeInfo returnType;
    std::optional<TypeInfo> expectedReturnType;
};
```

Update declarations:

```cpp
Binding declareVariable(const Token& name, TypeInfo type);
Binding declareVariable(const LetStmt& statement, TypeInfo type);
TypeInfo checkFunctionBody(const std::vector<StmtPtr>& body, std::optional<TypeInfo> expectedReturnType, const Token& functionToken, const std::string& functionLabel);
void recordReturn(const Token& keyword, TypeInfo type);
void checkImplicitNilReturn(const Token& functionToken, const std::string& functionLabel, const TypeInfo& expectedReturnType) const;
TypeInfo checkExpression(const Expr& expression);
CheckedExpression checkExpressionInfo(const Expr& expression);
CheckedExpression checkFunctionExpression(const FunctionExpr& expression);
CheckedExpression checkCall(const CallExpr& expression);
CheckedExpression checkBuiltinLenCall(const CallExpr& expression);
TypeInfo checkIndex(const IndexExpr& expression);
CheckedExpression checkIndexAssignment(const IndexAssignExpr& expression);
CheckedExpression checkLetInitializer(const LetStmt& statement);
TypeInfo resolveAnnotation(const TypeAnnotation& typeName) const;
void checkAssignable(const Token& token, const std::string& context, const TypeInfo& expected, const TypeInfo& actual) const;
TypeInfo checkUnary(const UnaryExpr& expression);
TypeInfo checkBinary(const BinaryExpr& expression);
```

- [ ] **Step 2: Add `TypeInfo` helpers in `src/TypeChecker.cpp`**

Inside the anonymous namespace, add:

```cpp
TypeInfo unknownType()
{
    return TypeInfo{StaticType::Unknown, {}, nullptr};
}

TypeInfo simpleType(StaticType kind)
{
    return TypeInfo{kind, {}, nullptr};
}

TypeInfo functionType(std::vector<TypeInfo> parameterTypes, TypeInfo returnType)
{
    TypeInfo result;
    result.kind = StaticType::Function;
    result.parameterTypes = std::move(parameterTypes);
    result.returnType = std::make_shared<TypeInfo>(std::move(returnType));
    return result;
}

bool isKnown(const TypeInfo& type)
{
    return type.kind != StaticType::Unknown;
}

bool hasFunctionSignature(const TypeInfo& type)
{
    return type.kind == StaticType::Function && type.returnType != nullptr;
}

std::string typeInfoName(const TypeInfo& type)
{
    if (type.kind != StaticType::Function || !type.returnType) {
        return staticTypeName(type.kind);
    }

    std::string result = "fun(";
    for (std::size_t i = 0; i < type.parameterTypes.size(); ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += typeInfoName(type.parameterTypes[i]);
    }
    result += "): ";
    result += typeInfoName(*type.returnType);
    return result;
}

bool compatible(const TypeInfo& expected, const TypeInfo& actual)
{
    if (!isKnown(expected) || !isKnown(actual)) {
        return true;
    }
    if (expected.kind != actual.kind) {
        return false;
    }
    if (expected.kind != StaticType::Function) {
        return true;
    }
    if (!hasFunctionSignature(expected) || !hasFunctionSignature(actual)) {
        return true;
    }
    if (expected.parameterTypes.size() != actual.parameterTypes.size()) {
        return false;
    }
    for (std::size_t i = 0; i < expected.parameterTypes.size(); ++i) {
        if (!compatible(expected.parameterTypes[i], actual.parameterTypes[i])) {
            return false;
        }
    }
    return compatible(*expected.returnType, *actual.returnType);
}

TypeInfo mergeReturnTypes(const TypeInfo& current, const TypeInfo& next)
{
    if (compatible(current, next) && compatible(next, current) && current.kind == next.kind) {
        if (current.kind != StaticType::Function || typeInfoName(current) == typeInfoName(next)) {
            return current;
        }
    }
    if (!isKnown(current) || !isKnown(next)) {
        return unknownType();
    }
    return unknownType();
}
```

Keep the existing `isKnown(StaticType)` helper only if needed by transitional code. Remove it after all call sites use `TypeInfo`.

- [ ] **Step 3: Convert simple expression checks to return `TypeInfo`**

In `checkExpressionInfo`, replace simple returns such as:

```cpp
return CheckedExpression{StaticType::Number, std::nullopt, StaticType::Unknown};
```

with:

```cpp
return CheckedExpression{simpleType(StaticType::Number)};
```

Use these mappings:

- nil literal -> `simpleType(StaticType::Nil)`
- bool literal -> `simpleType(StaticType::Bool)`
- string literal -> `simpleType(StaticType::String)`
- number literal -> `simpleType(StaticType::Number)`
- array literal -> `simpleType(StaticType::Array)`
- unknown result -> `unknownType()`
- known function with no signature -> `functionType(parameterUnknowns, returnType)` where unannotated parameters are `unknownType()`.

- [ ] **Step 4: Update `declareVariable` implementations**

Replace the two `declareVariable` definitions with:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(const Token& name, TypeInfo type)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError(name, "variable `" + name.lexeme + "` already declared in this scope");
    }

    Binding binding{std::move(type), makeResolvedName(name.lexeme), scopes_.size() - 1, functionDepth_};
    scope.emplace(name.lexeme, binding);
    return binding;
}

TypeChecker::Binding TypeChecker::declareVariable(const LetStmt& statement, TypeInfo type)
{
    Binding binding = declareVariable(statement.name, std::move(type));
    resolvedNames_.recordLet(statement, binding.resolvedName);
    return binding;
}
```

- [ ] **Step 5: Update error messages to use `typeInfoName`**

Change messages that currently concatenate `staticTypeName(type)` to use `typeInfoName(type)` when the value is a `TypeInfo`.

Examples:

```cpp
throw TypeError(expression.paren, "len expects array or string, got " + typeInfoName(argument.type));
```

```cpp
throw TypeError(assign->name, "cannot assign " + typeInfoName(value.type) + " to `" + assign->name.lexeme
    + "` of type " + typeInfoName(target->type));
```

- [ ] **Step 6: Run existing tests to keep behavior green**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: existing fixtures pass except tests intentionally added in Task 1 if this task has not yet completed all function-type compatibility. No existing golden output should change.

- [ ] **Step 7: Commit recursive type model scaffolding**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "refactor: use recursive static type info"
```

---

### Task 4: Resolve function type annotations and check declarations/assignments

**Files:**
- Modify: `src/TypeChecker.cpp`
- Add: `tests/golden/function_type_annotations/run.out`
- Add: `tests/golden/type_errors/function_type_initializer_param_mismatch.cd`
- Add: `tests/golden/type_errors/function_type_initializer_param_mismatch.err`
- Add: `tests/golden/type_errors/function_type_initializer_param_mismatch.exit`
- Add: `tests/golden/type_errors/function_type_initializer_return_mismatch.cd`
- Add: `tests/golden/type_errors/function_type_initializer_return_mismatch.err`
- Add: `tests/golden/type_errors/function_type_initializer_return_mismatch.exit`
- Add: `tests/golden/type_errors/function_type_assignment_param_mismatch.cd`
- Add: `tests/golden/type_errors/function_type_assignment_param_mismatch.err`
- Add: `tests/golden/type_errors/function_type_assignment_param_mismatch.exit`

- [ ] **Step 1: Add success runtime output**

Create `tests/golden/function_type_annotations/run.out`:

```text
true
42
6
```

Update `tests/golden/function_type_annotations/input.cd` by appending:

```cd
print pred(1);
print apply(fun (n: number): number { return n + 1; }, 41);
let addOne: fun(number): number = makeAdder();
print addOne(5);
```

Update `tests/golden/function_type_annotations/ast.out` by appending these lines after the `Fun makeAdder...` block:

```text
  Print (call pred 1)
  Print (call apply (fun (n: number): number (return (+ n 1))) 41)
  Let addOne: fun(number): number = (call makeAdder)
  Print (call addOne 5)
```

- [ ] **Step 2: Add initializer and assignment mismatch fixtures**

Create `tests/golden/type_errors/function_type_initializer_param_mismatch.cd`:

```cd
let f: fun(number): number = fun (s: string): number {
  return 1;
};
```

Create `tests/golden/type_errors/function_type_initializer_param_mismatch.err`:

```text
Type error at 1:5: cannot initialize `f` of type fun(number): number with fun(string): number
```

Create `tests/golden/type_errors/function_type_initializer_param_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/function_type_initializer_return_mismatch.cd`:

```cd
let f: fun(number): number = fun (x: number): string {
  return "no";
};
```

Create `tests/golden/type_errors/function_type_initializer_return_mismatch.err`:

```text
Type error at 1:5: cannot initialize `f` of type fun(number): number with fun(number): string
```

Create `tests/golden/type_errors/function_type_initializer_return_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/function_type_assignment_param_mismatch.cd`:

```cd
let f: fun(number): number = fun (x: number): number {
  return x;
};
f = fun (s: string): number {
  return 1;
};
```

Create `tests/golden/type_errors/function_type_assignment_param_mismatch.err`:

```text
Type error at 4:1: cannot assign fun(string): number to `f` of type fun(number): number
```

Create `tests/golden/type_errors/function_type_assignment_param_mismatch.exit`:

```text
1
```

- [ ] **Step 3: Verify RED for type checking**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. At least the mismatch fixtures should not yet produce the expected static errors.

- [ ] **Step 4: Resolve recursive annotations**

Replace `resolveAnnotation` in `src/TypeChecker.cpp` with:

```cpp
TypeInfo TypeChecker::resolveAnnotation(const TypeAnnotation& typeName) const
{
    if (typeName.kind == TypeAnnotation::Kind::Function) {
        std::vector<TypeInfo> parameterTypes;
        parameterTypes.reserve(typeName.parameterTypes.size());
        for (const TypeAnnotation& parameter : typeName.parameterTypes) {
            parameterTypes.push_back(resolveAnnotation(parameter));
        }
        return functionType(std::move(parameterTypes), resolveAnnotation(*typeName.returnType));
    }

    if (typeName.token.lexeme == "number") {
        return simpleType(StaticType::Number);
    }
    if (typeName.token.lexeme == "bool") {
        return simpleType(StaticType::Bool);
    }
    if (typeName.token.lexeme == "string") {
        return simpleType(StaticType::String);
    }
    if (typeName.token.lexeme == "nil") {
        return simpleType(StaticType::Nil);
    }
    throw TypeError(typeName.token, "unknown type `" + typeName.token.lexeme + "`");
}
```

- [ ] **Step 5: Build function types for function declarations and expressions**

Add helper in the anonymous namespace:

```cpp
std::vector<TypeInfo> parameterTypesFromAnnotations(
    const std::vector<Parameter>& parameters,
    const TypeChecker& checker)
{
    std::vector<TypeInfo> types;
    types.reserve(parameters.size());
    for (const Parameter& parameter : parameters) {
        types.push_back(parameter.typeName ? checker.resolveAnnotation(*parameter.typeName) : unknownType());
    }
    return types;
}
```

If private access prevents this helper from calling `resolveAnnotation`, instead implement the same loop directly in `TypeChecker::checkFunction` and `TypeChecker::checkFunctionExpression`.

In `checkFunction`, build the declaration type before declaring the name:

```cpp
std::vector<TypeInfo> declaredParameterTypes;
for (const Parameter& parameter : statement.parameters) {
    declaredParameterTypes.push_back(parameter.typeName ? resolveAnnotation(*parameter.typeName) : unknownType());
}
std::optional<TypeInfo> expectedReturnType;
if (statement.returnTypeName) {
    expectedReturnType = resolveAnnotation(*statement.returnTypeName);
}
TypeInfo initialReturnType = expectedReturnType ? *expectedReturnType : unknownType();
Binding functionBinding = declareVariable(statement.name, functionType(declaredParameterTypes, initialReturnType));
```

Then declare parameter bindings using `declaredParameterTypes[i]`.

After `checkFunctionBody`, update `storedFunction->type` to:

```cpp
storedFunction->type = functionType(std::move(declaredParameterTypes), returnType);
```

In `checkFunctionExpression`, return:

```cpp
return CheckedExpression{functionType(std::move(declaredParameterTypes), returnType)};
```

- [ ] **Step 6: Use recursive compatibility for let initialization and assignment**

In `checkLetInitializer`, after resolving an annotation:

```cpp
const TypeInfo declared = resolveAnnotation(*statement.typeName);
checkAssignable(
    statement.name,
    "cannot initialize `" + statement.name.lexeme + "` of type " + typeInfoName(declared)
        + " with " + typeInfoName(initializer.type),
    declared,
    initializer.type);
return CheckedExpression{declared};
```

In assignment checking, replace all StaticType/arity-specific function checks with one compatibility check:

```cpp
if (!compatible(target->type, value.type)) {
    throw TypeError(assign->name, "cannot assign " + typeInfoName(value.type) + " to `" + assign->name.lexeme
        + "` of type " + typeInfoName(target->type));
}
target->type = isKnown(target->type) ? target->type : value.type;
resolvedNames_.recordAssignment(*assign, target->resolvedName);
return CheckedExpression{target->type};
```

- [ ] **Step 7: Verify GREEN for declaration and assignment tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: the new initializer/assignment fixtures pass, and previous function arity assignment tests still pass. If old function assignment arity wording changes, prefer preserving the old error fixture by making `compatible` detect function arity mismatch before general mismatch; otherwise refresh only intentional wording changes with `--update` and review.

- [ ] **Step 8: Commit declaration/assignment function type checking**

```bash
git add src/TypeChecker.cpp tests/golden/function_type_annotations tests/golden/type_errors/function_type_initializer_param_mismatch.* tests/golden/type_errors/function_type_initializer_return_mismatch.* tests/golden/type_errors/function_type_assignment_param_mismatch.*
git commit -m "feat: check function type annotations"
```

---

### Task 5: Check call argument types through function signatures

**Files:**
- Modify: `src/TypeChecker.cpp`
- Add: `tests/golden/type_errors/function_type_call_arg_mismatch.cd`
- Add: `tests/golden/type_errors/function_type_call_arg_mismatch.err`
- Add: `tests/golden/type_errors/function_type_call_arg_mismatch.exit`
- Add: `tests/golden/type_errors/function_type_call_arity_mismatch.cd`
- Add: `tests/golden/type_errors/function_type_call_arity_mismatch.err`
- Add: `tests/golden/type_errors/function_type_call_arity_mismatch.exit`

- [ ] **Step 1: Add call mismatch fixtures**

Create `tests/golden/type_errors/function_type_call_arg_mismatch.cd`:

```cd
fun apply(f: fun(number): number): number {
  return f("bad");
}
```

Create `tests/golden/type_errors/function_type_call_arg_mismatch.err`:

```text
Type error at 2:11: argument 1 expects number, got string
```

Create `tests/golden/type_errors/function_type_call_arg_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/function_type_call_arity_mismatch.cd`:

```cd
fun apply(f: fun(number): number): number {
  return f(1, 2);
}
```

Create `tests/golden/type_errors/function_type_call_arity_mismatch.err`:

```text
Type error at 2:15: expected 1 arguments but got 2
```

Create `tests/golden/type_errors/function_type_call_arity_mismatch.exit`:

```text
1
```

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. `function_type_call_arg_mismatch` should not yet be rejected at the intended call argument location.

- [ ] **Step 3: Update `checkCall` to validate signature arguments**

Replace the current arity/return logic in `checkCall` with:

```cpp
const CheckedExpression callee = checkExpressionInfo(*expression.callee);
std::vector<CheckedExpression> arguments;
arguments.reserve(expression.arguments.size());
for (const auto& argument : expression.arguments) {
    arguments.push_back(checkExpressionInfo(*argument));
}

if (callee.type.kind != StaticType::Unknown && callee.type.kind != StaticType::Function) {
    throw TypeError(expression.paren, "can only call functions");
}

if (callee.type.kind == StaticType::Function && hasFunctionSignature(callee.type)) {
    if (callee.type.parameterTypes.size() != expression.arguments.size()) {
        throw TypeError(expression.paren, "expected " + std::to_string(callee.type.parameterTypes.size())
            + " arguments but got " + std::to_string(expression.arguments.size()));
    }
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const TypeInfo& expected = callee.type.parameterTypes[i];
        const TypeInfo& actual = arguments[i].type;
        if (!compatible(expected, actual)) {
            throw TypeError(expression.paren,
                "argument " + std::to_string(i + 1) + " expects " + typeInfoName(expected)
                    + ", got " + typeInfoName(actual));
        }
    }
    return CheckedExpression{*callee.type.returnType};
}

return CheckedExpression{unknownType()};
```

Keep the `len` builtin fast path before this block.

- [ ] **Step 4: Verify GREEN for call signature tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass.

- [ ] **Step 5: Commit call argument checking**

```bash
git add src/TypeChecker.cpp tests/golden/type_errors/function_type_call_arg_mismatch.* tests/golden/type_errors/function_type_call_arity_mismatch.*
git commit -m "feat: check function type call arguments"
```

---

### Task 6: Check function-typed returns and nested signatures

**Files:**
- Modify: `src/TypeChecker.cpp`
- Add: `tests/golden/function_type_nested/input.cd`
- Add: `tests/golden/function_type_nested/ast.out`
- Add: `tests/golden/function_type_nested/run.out`
- Add: `tests/golden/type_errors/function_type_return_non_function.cd`
- Add: `tests/golden/type_errors/function_type_return_non_function.err`
- Add: `tests/golden/type_errors/function_type_return_non_function.exit`
- Add: `tests/golden/type_errors/function_type_missing_return.cd`
- Add: `tests/golden/type_errors/function_type_missing_return.err`
- Add: `tests/golden/type_errors/function_type_missing_return.exit`

- [ ] **Step 1: Add nested success fixture**

Create `tests/golden/function_type_nested/input.cd`:

```cd
let factory: fun(): fun(number): number = fun (): fun(number): number {
  return fun (x: number): number {
    return x + 2;
  };
};

let f: fun(number): number = factory();
print f(5);
```

Create `tests/golden/function_type_nested/ast.out`:

```text
Program
  Let factory: fun(): fun(number): number = (fun (): fun(number): number (return (fun (x: number): number (return (+ x 2)))))
  Let f: fun(number): number = (call factory)
  Print (call f 5)
```

Create `tests/golden/function_type_nested/run.out`:

```text
7
```

- [ ] **Step 2: Add return mismatch fixtures**

Create `tests/golden/type_errors/function_type_return_non_function.cd`:

```cd
fun make(): fun(number): number {
  return 123;
}
```

Create `tests/golden/type_errors/function_type_return_non_function.err`:

```text
Type error at 2:3: cannot return number from function returning fun(number): number
```

Create `tests/golden/type_errors/function_type_return_non_function.exit`:

```text
1
```

Create `tests/golden/type_errors/function_type_missing_return.cd`:

```cd
fun make(): fun(number): number {
}
```

Create `tests/golden/type_errors/function_type_missing_return.err`:

```text
Type error at 1:1: function `make` may return nil but is annotated fun(number): number
```

Create `tests/golden/type_errors/function_type_missing_return.exit`:

```text
1
```

- [ ] **Step 3: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL if function-typed return paths are not fully checked yet.

- [ ] **Step 4: Update return tracking to use `TypeInfo`**

In `checkStatement` for `ReturnStmt`, replace returned type calculation with:

```cpp
const TypeInfo returned = returnStmt->value
    ? checkExpression(*returnStmt->value)
    : simpleType(StaticType::Nil);
recordReturn(returnStmt->keyword, returned);
```

In `recordReturn`, use recursive compatibility:

```cpp
if (context.expectedReturnType && !compatible(*context.expectedReturnType, type)) {
    throw TypeError(keyword, "cannot return " + typeInfoName(type)
        + " from function returning " + typeInfoName(*context.expectedReturnType));
}
```

Initialize return context with nil as:

```cpp
returnContexts_.push_back(FunctionReturnContext{false, simpleType(StaticType::Nil), expectedReturnType});
```

In `checkImplicitNilReturn`, use:

```cpp
if (!compatible(expectedReturnType, simpleType(StaticType::Nil))) {
    throw TypeError(functionToken,
        "function `" + functionLabel + "` may return nil but is annotated " + typeInfoName(expectedReturnType));
}
```

- [ ] **Step 5: Verify GREEN for nested and return tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass.

- [ ] **Step 6: Commit return signature checking**

```bash
git add src/TypeChecker.cpp tests/golden/function_type_nested tests/golden/type_errors/function_type_return_non_function.* tests/golden/type_errors/function_type_missing_return.*
git commit -m "feat: check function type returns"
```

---

### Task 7: Add parse-error coverage for malformed function type annotations

**Files:**
- Add: `tests/golden/parse_errors/function_type_missing_paren.cd`
- Add: `tests/golden/parse_errors/function_type_missing_paren.err`
- Add: `tests/golden/parse_errors/function_type_missing_paren.exit`
- Add: `tests/golden/parse_errors/function_type_missing_colon.cd`
- Add: `tests/golden/parse_errors/function_type_missing_colon.err`
- Add: `tests/golden/parse_errors/function_type_missing_colon.exit`
- Add: `tests/golden/parse_errors/function_type_missing_return.cd`
- Add: `tests/golden/parse_errors/function_type_missing_return.err`
- Add: `tests/golden/parse_errors/function_type_missing_return.exit`

- [ ] **Step 1: Add malformed function type parse fixtures**

Create `tests/golden/parse_errors/function_type_missing_paren.cd`:

```cd
let f: fun(number: number = fun (x: number): number {
  return x;
};
```

Create `tests/golden/parse_errors/function_type_missing_paren.err`:

```text
Parse error at 1:18: expected `)` after function type parameters, found Colon `:`
```

Create `tests/golden/parse_errors/function_type_missing_paren.exit`:

```text
1
```

Create `tests/golden/parse_errors/function_type_missing_colon.cd`:

```cd
let f: fun(number) number = fun (x: number): number {
  return x;
};
```

Create `tests/golden/parse_errors/function_type_missing_colon.err`:

```text
Parse error at 1:20: expected `:` before function type return, found Identifier `number`
```

Create `tests/golden/parse_errors/function_type_missing_colon.exit`:

```text
1
```

Create `tests/golden/parse_errors/function_type_missing_return.cd`:

```cd
let f: fun(number): = fun (x: number): number {
  return x;
};
```

Create `tests/golden/parse_errors/function_type_missing_return.err`:

```text
Parse error at 1:21: expected type name
```

Create `tests/golden/parse_errors/function_type_missing_return.exit`:

```text
1
```

- [ ] **Step 2: Run parse-error fixtures**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: parse-error fixtures pass. If token columns differ, inspect actual output and update only the new `.err` files to match the parser's exact token locations.

- [ ] **Step 3: Commit parse-error coverage**

```bash
git add tests/golden/parse_errors/function_type_missing_*.cd tests/golden/parse_errors/function_type_missing_*.err tests/golden/parse_errors/function_type_missing_*.exit
git commit -m "test: cover malformed function type annotations"
```

---

### Task 8: Update documentation and roadmap

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar documentation**

In `docs/language-grammar.ebnf`, replace annotation references from `typeName` to `typeExpr` and add:

```ebnf
typeExpr    = simpleType
            | functionType ;

simpleType  = identifier ;

functionType = "fun", "(", [ typeArguments ], ")", ":", typeExpr ;

typeArguments = typeExpr,
                { ",", typeExpr } ;
```

Update `funDecl`, `parameter`, `letDecl`, and `functionExpr` to use `typeExpr`.

- [ ] **Step 2: Update README language summary**

In `README.md`, update the annotation section to state:

```markdown
Type annotations support `number`, `bool`, `string`, `nil`, and function types such as `fun(number): string`. Function type annotations may be used on `let` bindings, function parameters, and function returns. Known function signatures are checked for assignment, calls, and returns. Array element types, generic types, and nullable type syntax are not implemented yet.
```

Update examples near function syntax to include:

```cd
let f: fun(number): number = fun (x: number): number { return x + 1; };
fun apply(f: fun(number): number, x: number): number { return f(x); }
```

- [ ] **Step 3: Update roadmap Phase 9 status**

In `docs/roadmap.md`, update Phase 9 status to add:

```markdown
Phase 9E is implemented: function type annotations use `fun(...): ...` syntax and support static signature checks for annotated variables, parameters, returns, assignments, and calls.
```

Move or edit the future-feature bullets so function type annotations are no longer listed as future work. Keep array element types and nil compatibility as future work.

- [ ] **Step 4: Update AGENTS current language semantics**

In `AGENTS.md`, replace the current type-system note with wording that includes:

```markdown
Function type annotations use `fun(type, ...): type` and may appear in `let`, parameter, and return annotations. Known function signatures are checked for assignment compatibility, call argument types, and function returns. Array element types are not fully inferred yet.
```

- [ ] **Step 5: Run docs-aware golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass.

- [ ] **Step 6: Commit documentation updates**

```bash
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document function type annotations"
```

---

### Task 9: Full verification and cleanup

**Files:**
- No source edits expected unless verification reveals a defect.

- [ ] **Step 1: Run the full required verification set**

Run from repository root:

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

Expected: every command exits 0. The Python and Rust VM parity tests should still pass because runtime/backend behavior is unchanged.

- [ ] **Step 2: Check workspace status**

Run:

```bash
git status --short
```

Expected: no untracked `tests/__pycache__/` or build artifacts. Only intentional source, docs, and test fixture files should be present if any fix was made after the last commit.

- [ ] **Step 3: Commit any verification fixes**

If Step 1 required fixes, commit them with a focused message:

```bash
git add <changed-files>
git commit -m "fix: stabilize function type annotations"
```

If Step 1 required no fixes, do not create an empty commit.

- [ ] **Step 4: Prepare completion summary**

Use `superpowers:verification-before-completion` before claiming success. Report exact command results from Step 1.

---

## Self-Review Notes

- Spec coverage: the plan covers recursive parse grammar, AST printing, recursive static types, assignment compatibility, calls, returns, parser errors, docs, and full verification.
- Runtime/backend scope: no IR, bytecode, `.cdbc`, or Rust VM semantic change is planned; full backend parity tests still run at the end.
- TDD order: each behavior task adds a failing fixture before implementation.
