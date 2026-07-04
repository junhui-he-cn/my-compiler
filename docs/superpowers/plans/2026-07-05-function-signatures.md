# Function Signatures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add optional parameter and return type annotations to named functions and function expressions.

**Architecture:** Introduce a shared AST `Parameter` struct with optional type tokens, plus optional function return type tokens on `FunctionStmt` and `FunctionExpr`. Parser accepts `name: type` parameters and `): type` return annotations while preserving existing untyped syntax. TypeChecker resolves annotations, assigns typed parameter bindings, checks explicit and implicit returns, and keeps existing IR/bytecode/Rust VM runtime behavior unchanged.

**Tech Stack:** C++17 recursive-descent parser, AST printer, TypeChecker static analysis, existing IR compiler/resolved-name plumbing, Python golden tests, Rust VM parity tests for unchanged runtime artifact behavior.

---

## File Structure

- Modify `include/Ast.hpp`: add `Parameter`, update `FunctionStmt` / `FunctionExpr` fields and constructors.
- Modify `src/Ast.cpp`: print typed parameters and optional return annotations for function declarations and expressions.
- Modify `include/Parser.hpp`: add `parameters()` and `parameter()` helpers.
- Modify `src/Parser.cpp`: parse typed parameters and optional return annotations for named functions and lambdas.
- Modify `include/TypeChecker.hpp`: add function-return expectation context and helper declarations.
- Modify `src/TypeChecker.cpp`: resolve parameter/return annotations, check return compatibility, check implicit nil fallthrough, and keep inferred return propagation.
- Modify `src/IRCompiler.cpp`: adapt parameter name access from `Token` to `Parameter` via `ResolvedNames`, with no runtime semantic changes.
- Add success fixtures under `tests/golden/`.
- Add type-error fixtures under `tests/golden/type_errors/`.
- Add parse-error fixtures under `tests/golden/parse_errors/`.
- Modify `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, and `AGENTS.md` after implementation.

---

### Task 1: Parser and AST support for typed function signatures

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Test fixture: `tests/golden/function_typed_params_success/input.cd`
- Test fixture: `tests/golden/function_typed_params_success/ast.out`

- [ ] **Step 1: Write a failing AST fixture for typed named functions**

Create `tests/golden/function_typed_params_success/input.cd`:

```cd
fun add(x: number, y: number): number {
  return x + y;
}
print add(2, 3);
```

Create `tests/golden/function_typed_params_success/ast.out`:

```text
Program
  Fun add(x: number, y: number): number
    Return (+ x y)
  Print (call add 2 3)
```

- [ ] **Step 2: Run golden tests and verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. `function_typed_params_success default(ast)` should fail with a parse error because parameters currently do not accept `: type`.

- [ ] **Step 3: Add shared Parameter struct and update function AST declarations**

In `include/Ast.hpp`, after `using StmtPtr = std::unique_ptr<Stmt>;`, add:

```cpp
struct Parameter {
    Token name;
    std::optional<Token> typeName;
};
```

Update `FunctionExpr` from:

```cpp
struct FunctionExpr final : Expr {
    FunctionExpr(Token keyword, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out) const override;

    Token keyword;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};
```

to:

```cpp
struct FunctionExpr final : Expr {
    FunctionExpr(Token keyword, std::vector<Parameter> parameters, std::optional<Token> returnTypeName, std::vector<StmtPtr> body);
    void print(std::ostream& out) const override;

    Token keyword;
    std::vector<Parameter> parameters;
    std::optional<Token> returnTypeName;
    std::vector<StmtPtr> body;
};
```

Update `FunctionStmt` from:

```cpp
struct FunctionStmt final : Stmt {
    FunctionStmt(Token name, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};
```

to:

```cpp
struct FunctionStmt final : Stmt {
    FunctionStmt(Token name, std::vector<Parameter> parameters, std::optional<Token> returnTypeName, std::vector<StmtPtr> body);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::vector<Parameter> parameters;
    std::optional<Token> returnTypeName;
    std::vector<StmtPtr> body;
};
```

- [ ] **Step 4: Update AST parameter printing helpers**

In `src/Ast.cpp`, replace `writeParameterList`:

```cpp
void writeParameterList(std::ostream& out, const std::vector<Token>& parameters)
{
    out << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << parameters[i].lexeme;
    }
    out << ')';
}
```

with:

```cpp
void writeParameterList(std::ostream& out, const std::vector<Parameter>& parameters)
{
    out << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << parameters[i].name.lexeme;
        if (parameters[i].typeName) {
            out << ": " << parameters[i].typeName->lexeme;
        }
    }
    out << ')';
}

void writeReturnAnnotation(std::ostream& out, const std::optional<Token>& returnTypeName)
{
    if (returnTypeName) {
        out << ": " << returnTypeName->lexeme;
    }
}
```

- [ ] **Step 5: Update inline AST printing for function declarations and expressions**

In `src/Ast.cpp`, update the `FunctionStmt` branch inside `writeInlineStmt()` from:

```cpp
    if (const auto* functionStmt = dynamic_cast<const FunctionStmt*>(&stmt)) {
        out << "(fun " << functionStmt->name.lexeme << ' ';
        writeParameterList(out, functionStmt->parameters);
        for (const auto& child : functionStmt->body) {
            out << ' ';
            writeInlineStmt(out, *child);
        }
        out << ')';
        return;
    }
```

to:

```cpp
    if (const auto* functionStmt = dynamic_cast<const FunctionStmt*>(&stmt)) {
        out << "(fun " << functionStmt->name.lexeme << ' ';
        writeParameterList(out, functionStmt->parameters);
        writeReturnAnnotation(out, functionStmt->returnTypeName);
        for (const auto& child : functionStmt->body) {
            out << ' ';
            writeInlineStmt(out, *child);
        }
        out << ')';
        return;
    }
```

Update `FunctionExpr::print(...)` from:

```cpp
void FunctionExpr::print(std::ostream& out) const
{
    out << "(fun ";
    writeParameterList(out, parameters);
    for (const auto& statement : body) {
        out << ' ';
        writeInlineStmt(out, *statement);
    }
    out << ')';
}
```

to:

```cpp
void FunctionExpr::print(std::ostream& out) const
{
    out << "(fun ";
    writeParameterList(out, parameters);
    writeReturnAnnotation(out, returnTypeName);
    for (const auto& statement : body) {
        out << ' ';
        writeInlineStmt(out, *statement);
    }
    out << ')';
}
```

- [ ] **Step 6: Update AST constructors and FunctionStmt pretty printer**

In `src/Ast.cpp`, replace `FunctionExpr::FunctionExpr(...)` with:

```cpp
FunctionExpr::FunctionExpr(Token keyword, std::vector<Parameter> parameters, std::optional<Token> returnTypeName, std::vector<StmtPtr> body)
    : keyword(std::move(keyword))
    , parameters(std::move(parameters))
    , returnTypeName(std::move(returnTypeName))
    , body(std::move(body))
{
}
```

Replace `FunctionStmt::FunctionStmt(...)` with:

```cpp
FunctionStmt::FunctionStmt(Token name, std::vector<Parameter> parameters, std::optional<Token> returnTypeName, std::vector<StmtPtr> body)
    : name(std::move(name))
    , parameters(std::move(parameters))
    , returnTypeName(std::move(returnTypeName))
    , body(std::move(body))
{
}
```

Update `FunctionStmt::print(...)` from:

```cpp
void FunctionStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Fun " << name.lexeme << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << parameters[i].lexeme;
    }
    out << ")\n";
    for (const auto& statement : body) {
        statement->print(out, indent + 1);
    }
}
```

to:

```cpp
void FunctionStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Fun " << name.lexeme;
    writeParameterList(out, parameters);
    writeReturnAnnotation(out, returnTypeName);
    out << '\n';
    for (const auto& statement : body) {
        statement->print(out, indent + 1);
    }
}
```

- [ ] **Step 7: Add parser helper declarations**

In `include/Parser.hpp`, add after `StmtPtr letDeclaration();`:

```cpp
    std::vector<Parameter> parameters();
    Parameter parameter();
    std::optional<Token> optionalReturnType();
```

`Parser.hpp` already includes `Ast.hpp`, which includes `<optional>`.

- [ ] **Step 8: Implement parser helpers**

In `src/Parser.cpp`, after `letDeclaration()` add:

```cpp
std::vector<Parameter> Parser::parameters()
{
    std::vector<Parameter> parsedParameters;
    if (!check(TokenType::RightParen)) {
        do {
            parsedParameters.push_back(parameter());
        } while (match(TokenType::Comma));
    }
    return parsedParameters;
}

Parameter Parser::parameter()
{
    Token name = consume(TokenType::Identifier, "expected parameter name");
    std::optional<Token> typeName;
    if (match(TokenType::Colon)) {
        typeName = consume(TokenType::Identifier, "expected parameter type after `:`");
    }
    return Parameter{std::move(name), std::move(typeName)};
}

std::optional<Token> Parser::optionalReturnType()
{
    if (!match(TokenType::Colon)) {
        return std::nullopt;
    }
    return consume(TokenType::Identifier, "expected return type after `:`");
}
```

Add `#include <utility>` to `src/Parser.cpp` if it is not already available through transitive includes. The file currently uses `std::move`, so adding it is correct and explicit:

```cpp
#include <utility>
```

- [ ] **Step 9: Parse function declarations with typed signatures**

In `src/Parser.cpp`, replace the parameter parsing block in `Parser::functionDeclaration()`:

```cpp
    std::vector<Token> parameters;
    if (!check(TokenType::RightParen)) {
        do {
            parameters.push_back(consume(TokenType::Identifier, "expected parameter name"));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected `)` after function parameters");
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionStmt>(std::move(name), std::move(parameters), blockStatements());
```

with:

```cpp
    std::vector<Parameter> parsedParameters = parameters();
    consume(TokenType::RightParen, "expected `)` after function parameters");
    std::optional<Token> returnTypeName = optionalReturnType();
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionStmt>(
        std::move(name),
        std::move(parsedParameters),
        std::move(returnTypeName),
        blockStatements());
```

- [ ] **Step 10: Parse function expressions with typed signatures**

In `src/Parser.cpp`, replace the parameter parsing block in `Parser::functionExpression()`:

```cpp
    std::vector<Token> parameters;
    if (!check(TokenType::RightParen)) {
        do {
            parameters.push_back(consume(TokenType::Identifier, "expected parameter name"));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected `)` after function parameters");
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionExpr>(std::move(keyword), std::move(parameters), blockStatements());
```

with:

```cpp
    std::vector<Parameter> parsedParameters = parameters();
    consume(TokenType::RightParen, "expected `)` after function parameters");
    std::optional<Token> returnTypeName = optionalReturnType();
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionExpr>(
        std::move(keyword),
        std::move(parsedParameters),
        std::move(returnTypeName),
        blockStatements());
```

- [ ] **Step 11: Update existing call sites that expect parameter tokens**

Run:

```bash
grep -R "parameters\[.*\]\.lexeme\|for (const Token& parameter" -n include src
```

Expected: references in `src/TypeChecker.cpp`, `src/IRCompiler.cpp`, and possibly `src/Ast.cpp` need later tasks. Do not change TypeChecker/IRCompiler yet in this task unless build requires a simple `.name` access to compile. This task's target is parser/AST shape.

- [ ] **Step 12: Build and observe expected compile failures**

Run:

```bash
cmake --build build
```

Expected: compile may fail in `src/TypeChecker.cpp` and `src/IRCompiler.cpp` because parameters changed from `Token` to `Parameter`. If it fails there, apply the minimal compatibility changes from Task 2 Step 3 and Task 4 Step 1 before committing Task 1. If it compiles, continue.

- [ ] **Step 13: Run golden tests and verify parser/AST GREEN**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: `function_typed_params_success default(ast)` passes once TypeChecker/IRCompiler compatibility is added. Runtime may still fail until type checker/runtime compatibility tasks are complete if `run.out` is added later; at this point only `ast.out` exists.

- [ ] **Step 14: Commit parser/AST slice**

Run:

```bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/function_typed_params_success/input.cd tests/golden/function_typed_params_success/ast.out
git commit -m "feat: parse typed function signatures"
```

If minimal build compatibility touched `src/TypeChecker.cpp` or `src/IRCompiler.cpp`, include those files in this commit and note in the commit body that runtime semantics are unchanged.

---

### Task 2: TypeChecker parameter annotations

**Files:**
- Modify: `src/TypeChecker.cpp`
- Test fixture: `tests/golden/type_errors/function_param_type_mismatch.cd`
- Test fixture: `tests/golden/type_errors/function_param_type_mismatch.err`
- Test fixture: `tests/golden/type_errors/function_param_type_mismatch.exit`
- Test fixture: `tests/golden/type_errors/function_unknown_parameter_type.cd`
- Test fixture: `tests/golden/type_errors/function_unknown_parameter_type.err`
- Test fixture: `tests/golden/type_errors/function_unknown_parameter_type.exit`

- [ ] **Step 1: Write failing parameter type-error fixtures**

Create `tests/golden/type_errors/function_param_type_mismatch.cd`:

```cd
fun bad(x: number) {
  print x + "s";
}
```

Create `tests/golden/type_errors/function_param_type_mismatch.err`:

```text
Type error at 2:11: binary `+` expects two numbers or two strings, got number and string
```

Create `tests/golden/type_errors/function_param_type_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/function_unknown_parameter_type.cd`:

```cd
fun bad(x: nope) {
  print x;
}
```

Create `tests/golden/type_errors/function_unknown_parameter_type.err`:

```text
Type error at 1:12: unknown type `nope`
```

Create `tests/golden/type_errors/function_unknown_parameter_type.exit`:

```text
1
```

- [ ] **Step 2: Run golden tests and verify RED**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. `function_param_type_mismatch` should not report the desired typed-parameter mismatch yet because parameters are still declared as unknown. `function_unknown_parameter_type` should not report unknown type yet.

- [ ] **Step 3: Update TypeChecker parameter loops for FunctionStmt**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkFunction(const FunctionStmt& statement)`, replace:

```cpp
    std::vector<std::string> parameterNames;
    for (const Token& parameter : statement.parameters) {
        Binding parameterBinding = declareVariable(parameter, StaticType::Unknown);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
```

with:

```cpp
    std::vector<std::string> parameterNames;
    for (const Parameter& parameter : statement.parameters) {
        const StaticType parameterType = parameter.typeName
            ? resolveAnnotation(*parameter.typeName)
            : StaticType::Unknown;
        Binding parameterBinding = declareVariable(parameter.name, parameterType);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
```

- [ ] **Step 4: Update TypeChecker parameter loops for FunctionExpr**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkFunctionExpression(const FunctionExpr& expression)`, replace:

```cpp
    std::vector<std::string> parameterNames;
    for (const Token& parameter : expression.parameters) {
        Binding parameterBinding = declareVariable(parameter, StaticType::Unknown);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
```

with:

```cpp
    std::vector<std::string> parameterNames;
    for (const Parameter& parameter : expression.parameters) {
        const StaticType parameterType = parameter.typeName
            ? resolveAnnotation(*parameter.typeName)
            : StaticType::Unknown;
        Binding parameterBinding = declareVariable(parameter.name, parameterType);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
```

- [ ] **Step 5: Build and run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all current tests pass, including the new parameter annotation type-error fixtures. If the reported column for `function_param_type_mismatch` differs, inspect actual stderr and update the `.err` file to the compiler's stable location.

- [ ] **Step 6: Commit parameter annotation checking**

Run:

```bash
git add src/TypeChecker.cpp tests/golden/type_errors/function_param_type_mismatch.cd tests/golden/type_errors/function_param_type_mismatch.err tests/golden/type_errors/function_param_type_mismatch.exit tests/golden/type_errors/function_unknown_parameter_type.cd tests/golden/type_errors/function_unknown_parameter_type.err tests/golden/type_errors/function_unknown_parameter_type.exit
git commit -m "feat: type check function parameters"
```

---

### Task 3: Return annotations and return compatibility

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test fixture: `tests/golden/function_typed_return_inference/input.cd`
- Test fixture: `tests/golden/function_typed_return_inference/run.out`
- Test fixture: `tests/golden/lambda_typed_signature/input.cd`
- Test fixture: `tests/golden/lambda_typed_signature/run.out`
- Test fixture: `tests/golden/type_errors/function_return_type_mismatch.cd`
- Test fixture: `tests/golden/type_errors/function_return_type_mismatch.err`
- Test fixture: `tests/golden/type_errors/function_return_type_mismatch.exit`
- Test fixture: `tests/golden/type_errors/function_return_nil_mismatch.cd`
- Test fixture: `tests/golden/type_errors/function_return_nil_mismatch.err`
- Test fixture: `tests/golden/type_errors/function_return_nil_mismatch.exit`
- Test fixture: `tests/golden/type_errors/function_missing_return_mismatch.cd`
- Test fixture: `tests/golden/type_errors/function_missing_return_mismatch.err`
- Test fixture: `tests/golden/type_errors/function_missing_return_mismatch.exit`
- Test fixture: `tests/golden/type_errors/lambda_return_type_mismatch.cd`
- Test fixture: `tests/golden/type_errors/lambda_return_type_mismatch.err`
- Test fixture: `tests/golden/type_errors/lambda_return_type_mismatch.exit`

- [ ] **Step 1: Add success fixtures for return annotations**

Add `tests/golden/function_typed_return_inference/input.cd`:

```cd
fun value(): number {
  return 42;
}

let x = value();
x = x + 1;
print x;
```

Add `tests/golden/function_typed_return_inference/run.out`:

```text
43
```

Add `tests/golden/lambda_typed_signature/input.cd`:

```cd
let inc = fun (x: number): number {
  return x + 1;
};
print inc(4);
```

Add `tests/golden/lambda_typed_signature/run.out`:

```text
5
```

- [ ] **Step 2: Add failing return type-error fixtures**

Add `tests/golden/type_errors/function_return_type_mismatch.cd`:

```cd
fun bad(): number {
  return "nope";
}
```

Add `tests/golden/type_errors/function_return_type_mismatch.err`:

```text
Type error at 2:3: cannot return string from function returning number
```

Add `tests/golden/type_errors/function_return_type_mismatch.exit`:

```text
1
```

Add `tests/golden/type_errors/function_return_nil_mismatch.cd`:

```cd
fun bad(): number {
  return;
}
```

Add `tests/golden/type_errors/function_return_nil_mismatch.err`:

```text
Type error at 2:3: cannot return nil from function returning number
```

Add `tests/golden/type_errors/function_return_nil_mismatch.exit`:

```text
1
```

Add `tests/golden/type_errors/function_missing_return_mismatch.cd`:

```cd
fun bad(): number {
  print 1;
}
```

Add `tests/golden/type_errors/function_missing_return_mismatch.err`:

```text
Type error at 1:1: function `bad` may return nil but is annotated number
```

Add `tests/golden/type_errors/function_missing_return_mismatch.exit`:

```text
1
```

Add `tests/golden/type_errors/lambda_return_type_mismatch.cd`:

```cd
let bad = fun (): bool {
  return 1;
};
```

Add `tests/golden/type_errors/lambda_return_type_mismatch.err`:

```text
Type error at 2:3: cannot return number from function returning bool
```

Add `tests/golden/type_errors/lambda_return_type_mismatch.exit`:

```text
1
```

- [ ] **Step 3: Run golden tests and verify RED**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Return annotation syntax may parse after Task 1, but TypeChecker should not yet reject all return mismatches or implicit nil fallthrough correctly.

- [ ] **Step 4: Extend FunctionReturnContext**

In `include/TypeChecker.hpp`, replace:

```cpp
    struct FunctionReturnContext {
        bool sawReturn = false;
        StaticType returnType = StaticType::Nil;
    };
```

with:

```cpp
    struct FunctionReturnContext {
        bool sawReturn = false;
        StaticType returnType = StaticType::Nil;
        std::optional<StaticType> expectedReturnType;
    };
```

Change the declaration:

```cpp
    StaticType checkFunctionBody(const std::vector<StmtPtr>& body);
```

to:

```cpp
    StaticType checkFunctionBody(
        const std::vector<StmtPtr>& body,
        std::optional<StaticType> expectedReturnType,
        const Token& functionToken,
        const std::string& functionLabel);
```

Add helper declarations after `recordReturn(StaticType type);`:

```cpp
    bool bodyMayFallThrough(const std::vector<StmtPtr>& body) const;
    void checkImplicitNilReturn(const Token& functionToken, const std::string& functionLabel, StaticType expectedReturnType) const;
```

- [ ] **Step 5: Check explicit returns against expected return type**

In `src/TypeChecker.cpp`, update `recordReturn(StaticType type)` by adding an expected-return check after `FunctionReturnContext& context = returnContexts_.back();`:

```cpp
    if (context.expectedReturnType && !compatible(*context.expectedReturnType, type)) {
        throw TypeError("cannot return " + staticTypeName(type)
            + " from function returning " + staticTypeName(*context.expectedReturnType));
    }
```

Then change the method signature to keep location-aware diagnostics. Replace the declaration and implementation of `recordReturn` with:

In `include/TypeChecker.hpp`:

```cpp
    void recordReturn(const Token& keyword, StaticType type);
```

In `src/TypeChecker.cpp`, replace the full `recordReturn` implementation with:

```cpp
void TypeChecker::recordReturn(const Token& keyword, StaticType type)
{
    if (returnContexts_.empty()) {
        throw TypeError("return context stack is empty");
    }

    FunctionReturnContext& context = returnContexts_.back();
    if (context.expectedReturnType && !compatible(*context.expectedReturnType, type)) {
        throw TypeError(keyword, "cannot return " + staticTypeName(type)
            + " from function returning " + staticTypeName(*context.expectedReturnType));
    }

    if (!context.sawReturn) {
        context.sawReturn = true;
        context.returnType = type;
        return;
    }

    context.returnType = mergeReturnTypes(context.returnType, type);
}
```

Update the `ReturnStmt` branch in `checkStatement(...)` from:

```cpp
        recordReturn(returned);
```

to:

```cpp
        recordReturn(returnStmt->keyword, returned);
```

- [ ] **Step 6: Implement conservative implicit nil fallthrough check**

In `src/TypeChecker.cpp`, replace `checkFunctionBody(...)` with:

```cpp
bool TypeChecker::bodyMayFallThrough(const std::vector<StmtPtr>& body) const
{
    return body.empty() || dynamic_cast<const ReturnStmt*>(body.back().get()) == nullptr;
}

void TypeChecker::checkImplicitNilReturn(
    const Token& functionToken,
    const std::string& functionLabel,
    StaticType expectedReturnType) const
{
    if (!compatible(expectedReturnType, StaticType::Nil)) {
        throw TypeError(functionToken,
            "function `" + functionLabel + "` may return nil but is annotated " + staticTypeName(expectedReturnType));
    }
}

StaticType TypeChecker::checkFunctionBody(
    const std::vector<StmtPtr>& body,
    std::optional<StaticType> expectedReturnType,
    const Token& functionToken,
    const std::string& functionLabel)
{
    returnContexts_.push_back(FunctionReturnContext{false, StaticType::Nil, expectedReturnType});

    for (const auto& child : body) {
        checkStatement(*child);
    }

    const FunctionReturnContext context = returnContexts_.back();
    returnContexts_.pop_back();

    if (expectedReturnType) {
        if (bodyMayFallThrough(body)) {
            checkImplicitNilReturn(functionToken, functionLabel, *expectedReturnType);
        }
        return *expectedReturnType;
    }

    return context.sawReturn ? context.returnType : StaticType::Nil;
}
```

This deliberately uses a conservative rule: a function body is considered safe from implicit nil only when its final top-level statement is `return`. Richer control-flow-complete analysis is out of scope.

- [ ] **Step 7: Resolve return annotations for named functions**

In `TypeChecker::checkFunction(const FunctionStmt& statement)`, before `const StaticType returnType = checkFunctionBody(...)`, add:

```cpp
    std::optional<StaticType> expectedReturnType;
    if (statement.returnTypeName) {
        expectedReturnType = resolveAnnotation(*statement.returnTypeName);
    }
```

Replace:

```cpp
    const StaticType returnType = checkFunctionBody(statement.body);
```

with:

```cpp
    const StaticType returnType = checkFunctionBody(
        statement.body,
        expectedReturnType,
        statement.name,
        statement.name.lexeme);
```

- [ ] **Step 8: Resolve return annotations for function expressions**

In `TypeChecker::checkFunctionExpression(const FunctionExpr& expression)`, before `const StaticType returnType = checkFunctionBody(...)`, add:

```cpp
    std::optional<StaticType> expectedReturnType;
    if (expression.returnTypeName) {
        expectedReturnType = resolveAnnotation(*expression.returnTypeName);
    }
```

Replace:

```cpp
    const StaticType returnType = checkFunctionBody(expression.body);
```

with:

```cpp
    const StaticType returnType = checkFunctionBody(
        expression.body,
        expectedReturnType,
        expression.keyword,
        "<lambda>");
```

- [ ] **Step 9: Run golden tests and update expected columns only if needed**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all tests pass or only the new `.err` files need source-location/wording adjustment. If adjustment is needed, update expected files to the actual stable diagnostics and re-run the same command.

- [ ] **Step 10: Commit return annotation checking**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/function_typed_return_inference tests/golden/lambda_typed_signature tests/golden/type_errors/function_return_type_mismatch.cd tests/golden/type_errors/function_return_type_mismatch.err tests/golden/type_errors/function_return_type_mismatch.exit tests/golden/type_errors/function_return_nil_mismatch.cd tests/golden/type_errors/function_return_nil_mismatch.err tests/golden/type_errors/function_return_nil_mismatch.exit tests/golden/type_errors/function_missing_return_mismatch.cd tests/golden/type_errors/function_missing_return_mismatch.err tests/golden/type_errors/function_missing_return_mismatch.exit tests/golden/type_errors/lambda_return_type_mismatch.cd tests/golden/type_errors/lambda_return_type_mismatch.err tests/golden/type_errors/lambda_return_type_mismatch.exit
git commit -m "feat: type check function return annotations"
```

---

### Task 4: IR compiler compatibility and runtime success outputs

**Files:**
- Modify: `src/IRCompiler.cpp`
- Test fixture: `tests/golden/function_typed_params_success/run.out`
- Test fixture: `tests/golden/function_typed_params_success/ir.out`
- Test fixture: `tests/golden/lambda_typed_signature/ir.out`

- [ ] **Step 1: Update IRCompiler parameter name access for FunctionStmt if needed**

If not already done in Task 1 for build compatibility, update `src/IRCompiler.cpp` only where direct parameter token access remains. Search:

```bash
grep -R "for (const Token& parameter\|\.parameters\[.*\]\.lexeme" -n src include
```

Expected after parser/AST migration: there should be no direct `Token` parameter loops except where intentionally using resolved parameter names from `ResolvedNames`.

If `src/IRCompiler.cpp` needs changes, keep it using `ResolvedNames` as it currently does:

```cpp
std::vector<std::string> parameters = resolvedNames_->parameterNames(function);
ir_.beginFunction(function.name.lexeme, std::move(parameters));
```

and:

```cpp
std::vector<std::string> parameters = resolvedNames_->parameterNames(expression);
ir_.beginFunction(resolvedNames_->functionName(expression), std::move(parameters));
```

No IRCompiler runtime type metadata should be added.

- [ ] **Step 2: Add run output for typed params success**

Create `tests/golden/function_typed_params_success/run.out`:

```text
5
```

- [ ] **Step 3: Generate focused IR goldens for representative typed signatures**

Run:

```bash
./build/compiler_design --ir tests/golden/function_typed_params_success/input.cd > tests/golden/function_typed_params_success/ir.out
./build/compiler_design --ir tests/golden/lambda_typed_signature/input.cd > tests/golden/lambda_typed_signature/ir.out
```

Review both files. Expected: no type metadata appears in IR; parameter names are still resolved names and function arities are unchanged.

- [ ] **Step 4: Run full golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass.

- [ ] **Step 5: Commit runtime compatibility goldens**

Run:

```bash
git add src/IRCompiler.cpp tests/golden/function_typed_params_success/run.out tests/golden/function_typed_params_success/ir.out tests/golden/lambda_typed_signature/ir.out
git commit -m "test: cover function signature runtime lowering"
```

If `src/IRCompiler.cpp` was unchanged, omit it from `git add`.

---

### Task 5: Parse-error coverage for signature syntax boundaries

**Files:**
- Test fixture: `tests/golden/parse_errors/function_param_missing_type.cd`
- Test fixture: `tests/golden/parse_errors/function_param_missing_type.err`
- Test fixture: `tests/golden/parse_errors/function_param_missing_type.exit`
- Test fixture: `tests/golden/parse_errors/function_return_missing_type.cd`
- Test fixture: `tests/golden/parse_errors/function_return_missing_type.err`
- Test fixture: `tests/golden/parse_errors/function_return_missing_type.exit`

- [ ] **Step 1: Add parse-error fixtures**

Create `tests/golden/parse_errors/function_param_missing_type.cd`:

```cd
fun bad(x:) {
}
```

Create `tests/golden/parse_errors/function_param_missing_type.err`:

```text
Parse error at 1:11: expected parameter type after `:`, found RightParen `)`
```

Create `tests/golden/parse_errors/function_param_missing_type.exit`:

```text
1
```

Create `tests/golden/parse_errors/function_return_missing_type.cd`:

```cd
fun bad(): {
}
```

Create `tests/golden/parse_errors/function_return_missing_type.err`:

```text
Parse error at 1:12: expected return type after `:`, found LeftBrace `{`
```

Create `tests/golden/parse_errors/function_return_missing_type.exit`:

```text
1
```

- [ ] **Step 2: Run golden tests and update exact diagnostics if needed**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: pass or fail only due to exact line/column/token text in the new parse-error `.err` files. If needed, update the new `.err` files to actual stable stderr and re-run.

- [ ] **Step 3: Commit parse-error coverage**

Run:

```bash
git add tests/golden/parse_errors/function_param_missing_type.cd tests/golden/parse_errors/function_param_missing_type.err tests/golden/parse_errors/function_param_missing_type.exit tests/golden/parse_errors/function_return_missing_type.cd tests/golden/parse_errors/function_return_missing_type.err tests/golden/parse_errors/function_return_missing_type.exit
git commit -m "test: cover function signature parse errors"
```

---

### Task 6: Documentation updates

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar docs**

In `docs/language-grammar.ebnf`, replace function grammar:

```ebnf
funDecl     = "fun", identifier,
              "(", [ parameters ], ")",
              block ;

parameters  = identifier,
              { ",", identifier } ;
```

with:

```ebnf
funDecl     = "fun", identifier,
              "(", [ parameters ], ")",
              [ ":", typeName ],
              block ;

parameters  = parameter,
              { ",", parameter } ;

parameter   = identifier,
              [ ":", typeName ] ;
```

Replace:

```ebnf
functionExpr = "fun", "(", [ parameters ], ")", block ;
```

with:

```ebnf
functionExpr = "fun", "(", [ parameters ], ")",
               [ ":", typeName ],
               block ;
```

- [ ] **Step 2: Update README function syntax and semantics**

In `README.md`, update the supported statements block line:

```text
fun name(parameter*) { declaration* }
```

to:

```text
fun name(parameter[: type]*) [: type] { declaration* }
```

Update the function expression bullet from:

```markdown
- Function expressions: `fun (parameter*) { declaration* }`
```

to:

```markdown
- Function expressions: `fun (parameter[: type]*) [: type] { declaration* }`
```

Replace the paragraph sentence:

```markdown
Function parameter types, return type annotations, and function type annotations are not implemented yet.
```

with:

```markdown
Function parameters and function return values may be annotated with `number`, `bool`, `string`, or `nil`. Function type annotations for variables and parameters are not implemented yet.
```

Also update the earlier type paragraph sentence:

```markdown
Function parameters, function returns, and array element types are not fully inferred yet.
```

to:

```markdown
Annotated function parameters and return types are checked, while unannotated function parameters, unannotated function returns, and array element types are not fully inferred yet.
```

- [ ] **Step 3: Update roadmap Phase 9 status**

In `docs/roadmap.md`, update Phase 9 status paragraph to mention Phase 9D implemented:

```markdown
Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Phase 9B is implemented: known function values carry arity for static argument-count checks. Phase 9C is implemented: known function values carry conservative inferred return types for call-result checking. Phase 9D is implemented: named functions and function expressions support optional parameter and return annotations for `number`, `bool`, `string`, and `nil`. Function type annotations and array element types remain future work.
```

Add a completed slice after the existing third slice:

```markdown
Completed fourth slice: named functions and function expressions accept optional parameter and return annotations, use annotated parameter types in function bodies, and check explicit or implicit returns against annotated return types.
```

- [ ] **Step 4: Update AGENTS current language memory**

In `AGENTS.md`, replace:

```markdown
- `let name: type = expression;` checks explicit annotations for `number`, `bool`, `string`, and `nil`; unannotated `let` bindings infer known initializer types, while function parameters, function returns, and array element types are not fully inferred yet.
```

with:

```markdown
- `let name: type = expression;`, function parameter annotations, and function return annotations check explicit type names for `number`, `bool`, `string`, and `nil`; unannotated `let` bindings infer known initializer types, while unannotated function parameters, unannotated function returns, and array element types are not fully inferred yet.
```

Replace:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Known function values carry arity and conservative inferred return types for static checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

with:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Parameters and returns may be annotated with `number`, `bool`, `string`, or `nil`; known function values carry arity and conservative or annotated return types for static checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

- [ ] **Step 5: Run docs stale wording check**

Run:

```bash
grep -R -n "Function parameter types, return type annotations.*not implemented\|function returns.*not fully inferred\|fun name(parameter\*)" README.md AGENTS.md docs/roadmap.md docs/language-grammar.ebnf || true
```

Expected: no stale wording remains.

- [ ] **Step 6: Run golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
rm -rf tests/__pycache__
```

Expected: all golden tests pass.

- [ ] **Step 7: Commit docs**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document function signatures"
```

---

### Task 7: Full verification and branch completion

**Files:**
- Verify all source, tests, fixtures, and docs.

- [ ] **Step 1: Run full clean verification**

Run:

```bash
rm -rf build
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

Expected:

- CMake configure/build exits `0`.
- CTest passes all registered tests.
- Golden tests pass.
- Golden runner selftests pass.
- Bytecode artifact tests pass.
- Rust VM golden parity tests pass.
- Rust unit tests pass.

- [ ] **Step 2: Verify no runtime/backend format changes were introduced**

Run:

```bash
grep -R -n "FunctionSignature\|ParameterType\|ReturnType" include/IR.hpp include/Bytecode.hpp docs/bytecode-text-format.md vm-rs/src || true
```

Expected: no output. Function signatures should remain compile-time metadata and should not appear in IR/bytecode format definitions or Rust VM runtime code.

- [ ] **Step 3: Verify function signature docs and fixtures are present**

Run:

```bash
grep -R -n "parameter.*typeName\|returnTypeName" include/Ast.hpp src/Parser.cpp src/TypeChecker.cpp
grep -R -n "functionExpr.*typeName\|parameter" docs/language-grammar.ebnf
find tests/golden -path '*function_typed*' -type f | sort
find tests/golden -path '*lambda_typed*' -type f | sort
```

Expected: AST/parser/type checker mention typed parameters and return annotations; grammar includes typed parameters; fixtures include function and lambda signature coverage.

- [ ] **Step 4: Review diff and status**

Run:

```bash
git diff --stat HEAD~6..HEAD
git diff --name-status HEAD~6..HEAD
git status --short --branch
```

Expected: diff includes function signature source/tests/docs changes and no generated build artifacts. Working tree is clean.

- [ ] **Step 5: Complete branch**

Use `superpowers:verification-before-completion` to report exact verification results, then use `superpowers:finishing-a-development-branch` to present merge/push/keep/discard options.
