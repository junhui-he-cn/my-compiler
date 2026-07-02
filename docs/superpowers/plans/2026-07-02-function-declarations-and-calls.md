# Function Declarations and Calls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Phase 6A named function declarations, call expressions, and explicit returns, without closures.

**Architecture:** Extend the existing vertical pipeline with `fun`/`return`/`,` tokens, `FunctionStmt`/`ReturnStmt`/`CallExpr` AST nodes, resolver/type-checker function bindings with arity and function-boundary checks, function values in `Value`, an IR function table with `make_function`/`call`/`return`, and interpreter call frames. Function bodies compile to separate IR functions; function values are stored in normal resolved variable bindings; calls execute in child frames and return explicit or implicit `nil` values.

**Tech Stack:** C++17, CMake, recursive-descent parser, AST side-table name resolution, register IR, IR interpreter call frames, Python golden tests, CTest.

---

## File Structure

- Modify: `include/Token.hpp`, `src/Lexer.cpp` — add `Comma`, `Fun`, and `Return`.
- Modify: `include/Ast.hpp`, `src/Ast.cpp` — add `CallExpr`, `FunctionStmt`, and `ReturnStmt` printing.
- Modify: `include/Parser.hpp`, `src/Parser.cpp` — parse function declarations, return statements, call expressions, parameters, and arguments.
- Modify: `include/TypeChecker.hpp`, `src/TypeChecker.cpp` — add function binding arity, function-depth/scope-depth tracking, function-name/parameter resolution, return validation, call validation, and no-closure checks.
- Modify: `include/Value.hpp`, `src/Value.cpp` — add function runtime values and printing/equality.
- Modify: `include/IR.hpp`, `src/IR.cpp` — add function table, `MakeFunction`, `Call`, `Return`, and argument-register printing.
- Modify: `include/IRCompiler.hpp`, `src/IRCompiler.cpp` — compile functions into IR function table, emit function values, calls, and returns.
- Modify: `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp` — replace single register/global execution with reusable frames and function calls.
- Create: success fixtures under `tests/golden/function_*`.
- Create: parse-error fixtures under `tests/golden/parse_errors`.
- Create: type-error fixtures under `tests/golden/type_errors`.
- Create: runtime-error fixtures under `tests/golden/runtime_errors`.
- Modify: `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, and possibly `AGENTS.md`.

## Task 0: Prepare Workspace and Baseline

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
function-declarations-and-calls
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

If baseline fails before function edits, stop and report the exact failing command and output.

## Task 1: Add RED Parser/AST Function Syntax Goldens

**Files:**
- Create: `tests/golden/function_call_add/input.cd`
- Create: `tests/golden/function_call_add/ast.out`
- Create: `tests/golden/function_call_add/run.out`
- Create: `tests/golden/function_bare_return/input.cd`
- Create: `tests/golden/function_bare_return/ast.out`
- Create: `tests/golden/function_bare_return/run.out`
- Create: `tests/golden/parse_errors/function_missing_parameter_paren.cd`
- Create: `tests/golden/parse_errors/function_missing_parameter_paren.err`
- Create: `tests/golden/parse_errors/function_missing_parameter_paren.exit`
- Create: `tests/golden/parse_errors/call_missing_argument.cd`
- Create: `tests/golden/parse_errors/call_missing_argument.err`
- Create: `tests/golden/parse_errors/call_missing_argument.exit`

- [ ] **Step 1: Create basic function AST/run fixture**

Run:

```bash
mkdir -p tests/golden/function_call_add
cat > tests/golden/function_call_add/input.cd <<'CASE'
fun add(a, b) {
  return a + b;
}
print add(1, 2);
CASE
cat > tests/golden/function_call_add/ast.out <<'CASE'
Program
  Fun add(a, b)
    Return (+ a b)
  Print (call add 1 2)
CASE
cat > tests/golden/function_call_add/run.out <<'CASE'
3
CASE
```

- [ ] **Step 2: Create bare return AST/run fixture**

Run:

```bash
mkdir -p tests/golden/function_bare_return
cat > tests/golden/function_bare_return/input.cd <<'CASE'
fun f() {
  return;
  print "no";
}
print f();
CASE
cat > tests/golden/function_bare_return/ast.out <<'CASE'
Program
  Fun f()
    Return nil
    Print "no"
  Print (call f)
CASE
cat > tests/golden/function_bare_return/run.out <<'CASE'
nil
CASE
```

- [ ] **Step 3: Create missing parameter closing-paren parse-error fixture**

Run:

```bash
cat > tests/golden/parse_errors/function_missing_parameter_paren.cd <<'CASE'
fun add(a, b {
  return a + b;
}
CASE
cat > tests/golden/parse_errors/function_missing_parameter_paren.err <<'CASE'
Parse error at 1:14: expected `)` after function parameters, found LeftBrace `{`
CASE
cat > tests/golden/parse_errors/function_missing_parameter_paren.exit <<'CASE'
1
CASE
```

- [ ] **Step 4: Create bad call argument comma parse-error fixture**

Run:

```bash
cat > tests/golden/parse_errors/call_missing_argument.cd <<'CASE'
print add(1, );
CASE
cat > tests/golden/parse_errors/call_missing_argument.err <<'CASE'
Parse error at 1:14: expected expression
CASE
cat > tests/golden/parse_errors/call_missing_argument.exit <<'CASE'
1
CASE
```

- [ ] **Step 5: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL. New fixtures fail because `fun`, `return`, comma, and call parsing do not exist yet. Failures should include parse errors such as expected semicolon or unexpected character `,`.

- [ ] **Step 6: Commit red parser fixtures**

Run:

```bash
git add tests/golden/function_call_add tests/golden/function_bare_return tests/golden/parse_errors/function_missing_parameter_paren.* tests/golden/parse_errors/call_missing_argument.*
git commit -m "test: add function parser goldens"
```

Expected: commit succeeds with only test fixture files.

## Task 2: Implement Lexer, AST, and Parser Support

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add tokens**

Edit `include/Token.hpp`.

Add `Comma` after `Semicolon`:

```cpp
    Colon,
    Semicolon,
    Comma,
```

Add `Fun` and `Return` to keyword tokens:

```cpp
    Else,
    Fun,
    Let,
    Print,
    Return,
    While,
```

- [ ] **Step 2: Scan comma and keywords**

Edit `src/Lexer.cpp`.

Add comma case after semicolon:

```cpp
    case ',':
        addToken(TokenType::Comma);
        break;
```

Add keyword entries:

```cpp
        {"fun", TokenType::Fun},
        {"return", TokenType::Return},
```

Add `tokenTypeName()` cases:

```cpp
    case TokenType::Comma:
        return "Comma";
    case TokenType::Fun:
        return "Fun";
    case TokenType::Return:
        return "Return";
```

- [ ] **Step 3: Add AST declarations**

Edit `include/Ast.hpp`.

Add `CallExpr` after `GroupingExpr`:

```cpp
struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, Token paren, std::vector<ExprPtr> arguments);
    void print(std::ostream& out) const override;

    ExprPtr callee;
    Token paren;
    std::vector<ExprPtr> arguments;
};
```

Add `FunctionStmt` and `ReturnStmt` before `Program`:

```cpp
struct FunctionStmt final : Stmt {
    FunctionStmt(Token name, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};

struct ReturnStmt final : Stmt {
    ReturnStmt(Token keyword, ExprPtr value);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
    ExprPtr value;
};
```

- [ ] **Step 4: Implement AST printing**

Edit `src/Ast.cpp`.

Add `CallExpr` implementation after `GroupingExpr::print()`:

```cpp
CallExpr::CallExpr(ExprPtr callee, Token paren, std::vector<ExprPtr> arguments)
    : callee(std::move(callee))
    , paren(std::move(paren))
    , arguments(std::move(arguments))
{
}

void CallExpr::print(std::ostream& out) const
{
    out << "(call ";
    writeExpr(out, callee);
    for (const auto& argument : arguments) {
        out << ' ';
        writeExpr(out, argument);
    }
    out << ')';
}
```

Add statement implementations after `WhileStmt::print()`:

```cpp
FunctionStmt::FunctionStmt(Token name, std::vector<Token> parameters, std::vector<StmtPtr> body)
    : name(std::move(name))
    , parameters(std::move(parameters))
    , body(std::move(body))
{
}

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

ReturnStmt::ReturnStmt(Token keyword, ExprPtr value)
    : keyword(std::move(keyword))
    , value(std::move(value))
{
}

void ReturnStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Return ";
    writeExpr(out, value);
    out << '\n';
}
```

- [ ] **Step 5: Add parser declarations**

Edit `include/Parser.hpp`.

Add:

```cpp
    StmtPtr functionDeclaration();
    StmtPtr returnStatement();
    ExprPtr call();
    ExprPtr finishCall(ExprPtr callee);
```

Place `functionDeclaration()` near `letDeclaration()`, `returnStatement()` near other statements, and `call()`/`finishCall()` between `unary()` and `primary()`.

- [ ] **Step 6: Parse function declarations and returns**

Edit `src/Parser.cpp`.

In `declaration()`, before `Let`:

```cpp
    if (match(TokenType::Fun)) {
        return functionDeclaration();
    }
```

Add `functionDeclaration()` after `letDeclaration()`:

```cpp
StmtPtr Parser::functionDeclaration()
{
    Token name = consume(TokenType::Identifier, "expected function name after `fun`");
    consume(TokenType::LeftParen, "expected `(` after function name");

    std::vector<Token> parameters;
    if (!check(TokenType::RightParen)) {
        do {
            parameters.push_back(consume(TokenType::Identifier, "expected parameter name"));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected `)` after function parameters");
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionStmt>(std::move(name), std::move(parameters), blockStatements());
}
```

In `statement()`, before block:

```cpp
    if (match(TokenType::Return)) {
        return returnStatement();
    }
```

Add `returnStatement()` after `printStatement()`:

```cpp
StmtPtr Parser::returnStatement()
{
    Token keyword = previous();
    ExprPtr value;
    if (!check(TokenType::Semicolon)) {
        value = expression();
    }
    consume(TokenType::Semicolon, "expected `;` after return value");
    return std::make_unique<ReturnStmt>(std::move(keyword), std::move(value));
}
```

- [ ] **Step 7: Parse call expressions**

Edit `src/Parser.cpp`.

Change `unary()` final line from:

```cpp
    return primary();
```

to:

```cpp
    return call();
```

Add `call()` and `finishCall()` before `primary()`:

```cpp
ExprPtr Parser::call()
{
    ExprPtr expr = primary();
    while (true) {
        if (match(TokenType::LeftParen)) {
            expr = finishCall(std::move(expr));
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::finishCall(ExprPtr callee)
{
    std::vector<ExprPtr> arguments;
    if (!check(TokenType::RightParen)) {
        do {
            arguments.push_back(expression());
        } while (match(TokenType::Comma));
    }
    Token paren = consume(TokenType::RightParen, "expected `)` after arguments");
    return std::make_unique<CallExpr>(std::move(callee), std::move(paren), std::move(arguments));
}
```

- [ ] **Step 8: Run golden tests and verify parser partial GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `function_call_add default(ast)`, `function_bare_return default(ast)`, and the new parse-error fixtures pass. Their `--run` checks still fail with type errors or unsupported nodes because type checking and runtime support are not complete.

- [ ] **Step 9: Commit parser implementation**

Run:

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse function declarations and calls"
```

Expected: commit succeeds with parser-layer changes.

## Task 3: Add RED Type Error Fixtures and TypeChecker Function Resolution

**Files:**
- Create: `tests/golden/type_errors/return_outside_function.cd`
- Create: `tests/golden/type_errors/return_outside_function.err`
- Create: `tests/golden/type_errors/return_outside_function.exit`
- Create: `tests/golden/type_errors/function_duplicate.cd`
- Create: `tests/golden/type_errors/function_duplicate.err`
- Create: `tests/golden/type_errors/function_duplicate.exit`
- Create: `tests/golden/type_errors/function_wrong_arity.cd`
- Create: `tests/golden/type_errors/function_wrong_arity.err`
- Create: `tests/golden/type_errors/function_wrong_arity.exit`
- Create: `tests/golden/type_errors/function_capture_local.cd`
- Create: `tests/golden/type_errors/function_capture_local.err`
- Create: `tests/golden/type_errors/function_capture_local.exit`
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add type-error fixtures**

Run:

```bash
cat > tests/golden/type_errors/return_outside_function.cd <<'CASE'
return 1;
CASE
cat > tests/golden/type_errors/return_outside_function.err <<'CASE'
Type error at 1:1: return outside function
CASE
cat > tests/golden/type_errors/return_outside_function.exit <<'CASE'
1
CASE
cat > tests/golden/type_errors/function_duplicate.cd <<'CASE'
fun f() {}
fun f() {}
CASE
cat > tests/golden/type_errors/function_duplicate.err <<'CASE'
Type error at 2:5: variable `f` already declared in this scope
CASE
cat > tests/golden/type_errors/function_duplicate.exit <<'CASE'
1
CASE
cat > tests/golden/type_errors/function_wrong_arity.cd <<'CASE'
fun add(a, b) {
  return a + b;
}
print add(1);
CASE
cat > tests/golden/type_errors/function_wrong_arity.err <<'CASE'
Type error at 4:12: expected 2 arguments but got 1
CASE
cat > tests/golden/type_errors/function_wrong_arity.exit <<'CASE'
1
CASE
cat > tests/golden/type_errors/function_capture_local.cd <<'CASE'
{
  let x = 1;
  fun f() {
    print x;
  }
}
CASE
cat > tests/golden/type_errors/function_capture_local.err <<'CASE'
Type error at 4:11: cannot capture local variable `x`
CASE
cat > tests/golden/type_errors/function_capture_local.exit <<'CASE'
1
CASE
```

- [ ] **Step 2: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL. New type fixtures fail because type checker does not support functions/returns/calls yet.

- [ ] **Step 3: Extend type-checker declarations**

Edit `include/TypeChecker.hpp`.

Add `Function` to `StaticType`:

```cpp
    String,
    Function,
```

Extend `ResolvedNames` public API:

```cpp
    const std::string& functionName(const FunctionStmt& statement) const;
    const std::vector<std::string>& parameterNames(const FunctionStmt& statement) const;
```

Extend private record API and maps:

```cpp
    void recordFunction(const FunctionStmt& statement, std::string name);
    void recordParameters(const FunctionStmt& statement, std::vector<std::string> names);

    std::unordered_map<const FunctionStmt*, std::string> functionNames_;
    std::unordered_map<const FunctionStmt*, std::vector<std::string>> parameterNames_;
```

Extend `Binding`:

```cpp
        std::optional<std::size_t> arity;
        std::size_t scopeDepth = 0;
        std::size_t functionDepth = 0;
```

Add private methods/data:

```cpp
    Binding declareVariable(const Token& name, StaticType type, std::optional<std::size_t> arity = std::nullopt);
    void checkFunction(const FunctionStmt& statement);
    StaticType checkCall(const CallExpr& expression);
    bool isGlobalBinding(const Binding& binding) const;
    bool isCurrentFunctionBinding(const Binding& binding) const;

    std::size_t functionDepth_ = 0;
```

Keep the existing `declareVariable(const LetStmt&, StaticType)` as a wrapper or replace its call sites.

- [ ] **Step 4: Implement ResolvedNames support**

Edit `src/TypeChecker.cpp`.

Add methods mirroring existing let/variable/assignment maps:

```cpp
const std::string& ResolvedNames::functionName(const FunctionStmt& statement) const
{
    const auto found = functionNames_.find(&statement);
    if (found == functionNames_.end()) {
        throw std::logic_error("missing resolved function name");
    }
    return found->second;
}

const std::vector<std::string>& ResolvedNames::parameterNames(const FunctionStmt& statement) const
{
    const auto found = parameterNames_.find(&statement);
    if (found == parameterNames_.end()) {
        throw std::logic_error("missing resolved parameter names");
    }
    return found->second;
}
```

Update `clear()`:

```cpp
    functionNames_.clear();
    parameterNames_.clear();
```

Add record methods:

```cpp
void ResolvedNames::recordFunction(const FunctionStmt& statement, std::string name)
{
    functionNames_.emplace(&statement, std::move(name));
}

void ResolvedNames::recordParameters(const FunctionStmt& statement, std::vector<std::string> names)
{
    parameterNames_.emplace(&statement, std::move(names));
}
```

Update `staticTypeName()` with:

```cpp
    case StaticType::Function:
        return "function";
```

- [ ] **Step 5: Implement function-aware declaration helpers**

Edit `src/TypeChecker.cpp`.

Set `functionDepth_ = 0;` in `TypeChecker::check()` before `beginScope()`.

Replace `declareVariable(const LetStmt&...)` implementation with a token-based helper plus wrapper:

```cpp
TypeChecker::Binding TypeChecker::declareVariable(const Token& name, StaticType type, std::optional<std::size_t> arity)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError(name, "variable `" + name.lexeme + "` already declared in this scope");
    }

    Binding binding{type, makeResolvedName(name.lexeme), arity, scopes_.size() - 1, functionDepth_};
    scope.emplace(name.lexeme, binding);
    return binding;
}

TypeChecker::Binding TypeChecker::declareVariable(const LetStmt& statement, StaticType type)
{
    Binding binding = declareVariable(statement.name, type);
    resolvedNames_.recordLet(statement, binding.resolvedName);
    return binding;
}
```

Add helper definitions:

```cpp
bool TypeChecker::isGlobalBinding(const Binding& binding) const
{
    return binding.scopeDepth == 0;
}

bool TypeChecker::isCurrentFunctionBinding(const Binding& binding) const
{
    return binding.functionDepth == functionDepth_;
}
```

- [ ] **Step 6: Type-check function and return statements**

Edit `TypeChecker::checkStatement()`.

Add before `LetStmt` branch:

```cpp
    if (const auto* function = dynamic_cast<const FunctionStmt*>(&statement)) {
        checkFunction(*function);
        return;
    }

    if (const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        if (functionDepth_ == 0) {
            throw TypeError(returnStmt->keyword, "return outside function");
        }
        if (returnStmt->value) {
            checkExpression(*returnStmt->value);
        }
        return;
    }
```

Add `checkFunction()`:

```cpp
void TypeChecker::checkFunction(const FunctionStmt& statement)
{
    Binding functionBinding = declareVariable(statement.name, StaticType::Function, statement.parameters.size());
    resolvedNames_.recordFunction(statement, functionBinding.resolvedName);

    beginScope();
    ++functionDepth_;

    std::vector<std::string> parameterNames;
    for (const Token& parameter : statement.parameters) {
        Binding parameterBinding = declareVariable(parameter, StaticType::Unknown);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordParameters(statement, std::move(parameterNames));

    for (const auto& child : statement.body) {
        checkStatement(*child);
    }

    --functionDepth_;
    endScope();
}
```

- [ ] **Step 7: Type-check calls and closure boundaries**

Edit `TypeChecker::checkExpression()`.

In the `VariableExpr` branch, after finding the binding and before recording the variable, add:

```cpp
        if (functionDepth_ > 0 && !isGlobalBinding(*binding) && !isCurrentFunctionBinding(*binding)) {
            throw TypeError(variable->name, "cannot capture local variable `" + variable->name.lexeme + "`");
        }
```

Add `CallExpr` branch before unsupported expression:

```cpp
    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        return checkCall(*call);
    }
```

Add `checkCall()`:

```cpp
StaticType TypeChecker::checkCall(const CallExpr& expression)
{
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
}
```

- [ ] **Step 8: Run golden tests and verify type GREEN, run still unsupported**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: new type-error fixtures pass. Function success fixtures still fail in `--run` because runtime/IR support is not implemented yet.

- [ ] **Step 9: Commit type-checker function support**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors
git commit -m "feat: resolve function declarations"
```

Expected: commit succeeds with type-checker changes and type-error fixtures.

## Task 4: Add Function Runtime Value Representation

**Files:**
- Modify: `include/Value.hpp`
- Modify: `src/Value.cpp`

- [ ] **Step 1: Add function value declarations**

Edit `include/Value.hpp`.

Add include:

```cpp
#include <cstddef>
```

Add public type:

```cpp
struct FunctionValue {
    std::string name;
    std::size_t functionIndex = 0;
    std::size_t arity = 0;
};
```

Add `Function` to `Value::Type`:

```cpp
        String,
        Function,
```

Add factory/accessor:

```cpp
    static Value function(FunctionValue value);
    const FunctionValue& asFunction() const;
```

Add private member:

```cpp
    FunctionValue function_;
```

- [ ] **Step 2: Implement function value behavior**

Edit `src/Value.cpp`.

Add factory:

```cpp
Value Value::function(FunctionValue value)
{
    Value result(Type::Function);
    result.function_ = std::move(value);
    return result;
}
```

Add accessor:

```cpp
const FunctionValue& Value::asFunction() const
{
    if (type_ != Type::Function) {
        throw std::runtime_error("value is not a function");
    }
    return function_;
}
```

Update `isTruthy()` behavior by leaving functions truthy through the existing final `return true`.

Update `valuesEqual()`:

```cpp
    case Value::Type::Function:
        return left.asFunction().functionIndex == right.asFunction().functionIndex;
```

Update `valueToString()`:

```cpp
    case Value::Type::Function:
        return "<fun " + value.asFunction().name + ">";
```

- [ ] **Step 3: Build and run existing tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: same current state as before this task; function success runtime fixtures still fail due missing IR/interpreter support, but no new failures are introduced by `Value` changes.

- [ ] **Step 4: Commit function values**

Run:

```bash
git add include/Value.hpp src/Value.cpp
git commit -m "feat: add function runtime values"
```

Expected: commit succeeds with value representation changes.

## Task 5: Extend IR with Function Table, MakeFunction, Call, and Return

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Create: `tests/golden/function_ir/input.cd`
- Create: `tests/golden/function_ir/ir.out`

- [ ] **Step 1: Add IR fixture expectation shell**

Run:

```bash
mkdir -p tests/golden/function_ir
cat > tests/golden/function_ir/input.cd <<'CASE'
fun add(a, b) {
  return a + b;
}
print add(1, 2);
CASE
cat > tests/golden/function_ir/ir.out <<'CASE'
IR
0000  v0 = make_function $0 add/2
0001  store_var @0 add#0, v0
0002  v1 = load_var @1 add#0
0003  v2 = constant #0 1
0004  v3 = constant #1 2
0005  v4 = call v1(v2, v3)
0006  print v4

function $0 add(a#1, b#2)
0000  v0 = load_var @0 a#1
0001  v1 = load_var @1 b#2
0002  v2 = add v0, v1
0003  return v2
0004  v3 = constant #2 nil
0005  return v3
CASE
```

- [ ] **Step 2: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL for `function_ir --ir` because IR compiler does not support functions.

- [ ] **Step 3: Extend IR data structures**

Edit `include/IR.hpp`.

Add operations:

```cpp
    MakeFunction,
    Call,
    Return,
```

Recommended placement: `MakeFunction` after `Constant`, `Call` after `AssignVar`, and `Return` after `Print`.

Add argument registers to `IRInstruction`:

```cpp
    std::vector<IRRegister> arguments;
```

Add function metadata before `IRProgram`:

```cpp
struct IRFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<IRInstruction> instructions;
    std::size_t registerCount = 0;
};
```

Add public methods:

```cpp
    void beginFunction(std::string name, std::vector<std::string> parameters);
    std::size_t endFunction();
    IRRegister emitMakeFunction(std::size_t functionIndex);
    IRRegister emitCall(IRRegister callee, std::vector<IRRegister> arguments);
    void emitReturn(IRRegister value);
    const std::vector<IRFunction>& functions() const;
```

Add private active function state:

```cpp
    bool buildingFunction_ = false;
    IRFunction currentFunction_;
    std::vector<IRFunction> functions_;
```

`makeRegister()`, `emit()`, and `instructionCount()` will use the active function when `buildingFunction_` is true; otherwise they use the main instruction stream.

- [ ] **Step 4: Update IR construction**

Edit `src/IR.cpp`.

Update all existing `IRInstruction{...}` initializers to include the new vector field before `operand`. For example:

```cpp
emit(IRInstruction{IROp::Constant, dest, std::nullopt, std::nullopt, {}, addConstant(std::move(value))});
```

Replace `IRProgram::makeRegister()` with:

```cpp
IRRegister IRProgram::makeRegister()
{
    if (buildingFunction_) {
        return IRRegister{currentFunction_.registerCount++};
    }
    return IRRegister{registerCount_++};
}
```

Add methods:

```cpp
void IRProgram::beginFunction(std::string name, std::vector<std::string> parameters)
{
    if (buildingFunction_) {
        throw std::logic_error("nested IR function build");
    }
    buildingFunction_ = true;
    currentFunction_ = IRFunction{std::move(name), std::move(parameters), {}, 0};
}

std::size_t IRProgram::endFunction()
{
    if (!buildingFunction_) {
        throw std::logic_error("not building IR function");
    }
    buildingFunction_ = false;
    functions_.push_back(std::move(currentFunction_));
    currentFunction_ = IRFunction{};
    return functions_.size() - 1;
}

IRRegister IRProgram::emitMakeFunction(std::size_t functionIndex)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::MakeFunction, dest, std::nullopt, std::nullopt, {}, functionIndex});
    return dest;
}

IRRegister IRProgram::emitCall(IRRegister callee, std::vector<IRRegister> arguments)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Call, dest, callee, std::nullopt, std::move(arguments), 0});
    return dest;
}

void IRProgram::emitReturn(IRRegister value)
{
    emit(IRInstruction{IROp::Return, std::nullopt, value, std::nullopt, {}, 0});
}

const std::vector<IRFunction>& IRProgram::functions() const
{
    return functions_;
}
```

- [ ] **Step 5: Update IR names and printing**

Edit `src/IR.cpp`.

Update `isUnary()`/`isBinary()` non-matching switch cases to include the new ops.

Add `irOpName()` cases:

```cpp
    case IROp::MakeFunction:
        return "make_function";
    case IROp::Call:
        return "call";
    case IROp::Return:
        return "return";
```

In `IRProgram::print()`, add branches:

```cpp
        } else if (instruction.op == IROp::MakeFunction) {
            out << " $" << instruction.operand;
            if (instruction.operand < functions_.size()) {
                const IRFunction& function = functions_[instruction.operand];
                out << " " << function.name << "/" << function.parameters.size();
            }
        } else if (instruction.op == IROp::Call) {
            if (instruction.left) {
                out << " " << *instruction.left << "(";
                for (std::size_t arg = 0; arg < instruction.arguments.size(); ++arg) {
                    if (arg != 0) {
                        out << ", ";
                    }
                    out << instruction.arguments[arg];
                }
                out << ")";
            }
        } else if (instruction.op == IROp::Return) {
            if (instruction.left) {
                out << " " << *instruction.left;
            }
```

After printing main instructions, print functions:

```cpp
    for (std::size_t functionIndex = 0; functionIndex < functions_.size(); ++functionIndex) {
        const IRFunction& function = functions_[functionIndex];
        out << '\n' << "function $" << functionIndex << " " << function.name << "(";
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << function.parameters[i];
        }
        out << ")\n";
        for (std::size_t i = 0; i < function.instructions.size(); ++i) {
            printInstruction(out, function.instructions[i], i);
        }
    }
```

If the current print loop is not easy to reuse, extract the body into a helper function in the anonymous namespace:

```cpp
void printInstruction(std::ostream& out, const IRProgram& program, const IRInstruction& instruction, std::size_t index)
```

Use that helper for both main and function instructions.

- [ ] **Step 6: Build to verify IR changes compile**

Run:

```bash
cmake --build build
```

Expected: build succeeds. Golden tests still fail for function fixtures because compiler lowering is not implemented yet.

- [ ] **Step 7: Commit IR model support**

Run:

```bash
git add include/IR.hpp src/IR.cpp tests/golden/function_ir
git commit -m "feat: add function IR model"
```

Expected: commit succeeds with IR model changes and red IR fixture.

## Task 6: Compile Function Declarations, Calls, and Returns

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Add compiler helpers for functions and calls**

Edit `include/IRCompiler.hpp`.

Add private helpers:

```cpp
    void compileFunctionStatement(const FunctionStmt& function);
    IRRegister emitCall(const CallExpr& expression);
    void compileReturn(const ReturnStmt& statement);
```

- [ ] **Step 2: Lower function declarations in statement compiler**

Edit `src/IRCompiler.cpp` in `compileStatement()` before `LetStmt`:

```cpp
    if (const auto* function = dynamic_cast<const FunctionStmt*>(&statement)) {
        compileFunctionStatement(*function);
        return;
    }

    if (const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        compileReturn(*returnStmt);
        return;
    }
```

Add `compileFunctionStatement()`:

```cpp
void IRCompiler::compileFunctionStatement(const FunctionStmt& function)
{
    std::vector<std::string> parameters = resolvedNames_->parameterNames(function);
    ir_.beginFunction(function.name.lexeme, std::move(parameters));

    for (const auto& statement : function.body) {
        compileStatement(*statement);
    }
    IRRegister nilValue = ir_.emitConstant(Value::nil());
    ir_.emitReturn(nilValue);

    const std::size_t functionIndex = ir_.endFunction();
    IRRegister value = ir_.emitMakeFunction(functionIndex);
    ir_.emitStoreVar(resolvedNames_->functionName(function), value);
}
```

- [ ] **Step 3: Lower returns**

Add `compileReturn()`:

```cpp
void IRCompiler::compileReturn(const ReturnStmt& statement)
{
    IRRegister value = statement.value ? compileExpression(*statement.value) : ir_.emitConstant(Value::nil());
    ir_.emitReturn(value);
}
```

- [ ] **Step 4: Lower calls**

In `compileExpression()`, add before unsupported expression:

```cpp
    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        return emitCall(*call);
    }
```

Add `emitCall()`:

```cpp
IRRegister IRCompiler::emitCall(const CallExpr& expression)
{
    IRRegister callee = compileExpression(*expression.callee);
    std::vector<IRRegister> arguments;
    for (const auto& argument : expression.arguments) {
        arguments.push_back(compileExpression(*argument));
    }
    return ir_.emitCall(callee, std::move(arguments));
}
```

- [ ] **Step 5: Run IR golden and inspect**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: function IR fixture may fail if exact register/name indexes differ. Inspect actual output. If it preserves `make_function`, `call`, `return`, and function table shape, refresh:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
git diff -- tests/golden/function_ir/ir.out
```

Function runtime fixtures still fail until interpreter support is implemented.

- [ ] **Step 6: Commit IR compiler support**

Run:

```bash
git add include/IRCompiler.hpp src/IRCompiler.cpp tests/golden/function_ir/ir.out
git commit -m "feat: compile function declarations and calls"
```

Expected: commit succeeds with compiler lowering and reviewed IR golden.

## Task 7: Implement Interpreter Call Frames and Function Calls

**Files:**
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Create: `tests/golden/function_return_nil/input.cd`
- Create: `tests/golden/function_return_nil/run.out`
- Create: `tests/golden/function_recursion/input.cd`
- Create: `tests/golden/function_recursion/run.out`
- Create: `tests/golden/function_scope_global/input.cd`
- Create: `tests/golden/function_scope_global/run.out`

- [ ] **Step 1: Add remaining success fixtures**

Run:

```bash
mkdir -p tests/golden/function_return_nil
cat > tests/golden/function_return_nil/input.cd <<'CASE'
fun noop() {
  print "hi";
}
print noop();
CASE
cat > tests/golden/function_return_nil/run.out <<'CASE'
hi
nil
CASE
mkdir -p tests/golden/function_recursion
cat > tests/golden/function_recursion/input.cd <<'CASE'
fun count(n) {
  if n <= 0 {
    return 0;
  }
  return count(n - 1) + 1;
}
print count(3);
CASE
cat > tests/golden/function_recursion/run.out <<'CASE'
3
CASE
mkdir -p tests/golden/function_scope_global
cat > tests/golden/function_scope_global/input.cd <<'CASE'
let g = "global";
fun show() {
  print g;
}
show();
CASE
cat > tests/golden/function_scope_global/run.out <<'CASE'
global
CASE
```

- [ ] **Step 2: Refactor interpreter header for frames**

Edit `include/IRInterpreter.hpp`.

Add private `Frame` and `ExecutionResult`:

```cpp
    struct Frame {
        std::vector<Value> registers;
        std::unordered_map<std::string, Value> locals;
    };

    struct ExecutionResult {
        bool returned = false;
        Value value = Value::nil();
    };
```

Replace register helpers with frame-aware helpers:

```cpp
    ExecutionResult executeInstructions(const IRProgram& program, const std::vector<IRInstruction>& instructions, Frame& frame);
    Value callFunction(const IRProgram& program, const FunctionValue& function, const std::vector<Value>& arguments);
    const Value& readRegister(const Frame& frame, IRRegister reg) const;
    void writeRegister(Frame& frame, IRRegister reg, Value value);
    Value loadVariable(const Frame& frame, const std::string& name) const;
    void storeVariable(Frame& frame, const std::string& name, Value value);
    void assignVariable(Frame& frame, const std::string& name, Value value);
```

Remove or stop using the single `registers_` member. Keep:

```cpp
    std::unordered_map<std::string, Value> globals_;
```

- [ ] **Step 3: Execute main through a frame**

Edit `src/IRInterpreter.cpp`.

Change `execute()` to:

```cpp
void IRInterpreter::execute(const IRProgram& program)
{
    globals_.clear();
    Frame mainFrame;
    mainFrame.registers.assign(program.registerCount(), Value::nil());
    executeInstructions(program, program.instructions(), mainFrame);
}
```

Move the old instruction loop into `executeInstructions(...)` and replace uses of `registers_` with `frame.registers` via helper calls.

- [ ] **Step 4: Implement variable helpers**

Add:

```cpp
Value IRInterpreter::loadVariable(const Frame& frame, const std::string& name) const
{
    const auto local = frame.locals.find(name);
    if (local != frame.locals.end()) {
        return local->second;
    }
    const auto global = globals_.find(name);
    if (global != globals_.end()) {
        return global->second;
    }
    throw IRRuntimeError("undefined variable `" + name + "`");
}

void IRInterpreter::storeVariable(Frame& frame, const std::string& name, Value value)
{
    if (&frame == nullptr) {
        globals_.insert_or_assign(name, std::move(value));
        return;
    }
    frame.locals.insert_or_assign(name, std::move(value));
}

void IRInterpreter::assignVariable(Frame& frame, const std::string& name, Value value)
{
    auto local = frame.locals.find(name);
    if (local != frame.locals.end()) {
        local->second = std::move(value);
        return;
    }
    auto global = globals_.find(name);
    if (global != globals_.end()) {
        global->second = std::move(value);
        return;
    }
    throw IRRuntimeError("undefined variable `" + name + "`");
}
```

Adjust `StoreVar` behavior so main-frame top-level stores populate `globals_`, while function-frame stores populate locals. A simple way is to add a `bool isMain` parameter to `executeInstructions()` and use it in `StoreVar`.

Required signature:

```cpp
ExecutionResult executeInstructions(const IRProgram& program, const std::vector<IRInstruction>& instructions, Frame& frame, bool isMain);
```

In `StoreVar`:

```cpp
if (isMain) {
    globals_.insert_or_assign(name, readRegister(frame, readLeft(instruction)));
} else {
    frame.locals.insert_or_assign(name, readRegister(frame, readLeft(instruction)));
}
```

- [ ] **Step 5: Implement MakeFunction, Call, and Return execution**

In the instruction switch:

```cpp
        case IROp::MakeFunction: {
            if (instruction.operand >= program.functions().size()) {
                throw IRRuntimeError("function index out of range");
            }
            const IRFunction& function = program.functions()[instruction.operand];
            writeRegister(frame, readDest(instruction), Value::function(FunctionValue{function.name, instruction.operand, function.parameters.size()}));
            break;
        }
        case IROp::Call: {
            const Value& callee = readRegister(frame, readLeft(instruction));
            if (callee.type() != Value::Type::Function) {
                throw IRRuntimeError("can only call functions");
            }
            std::vector<Value> arguments;
            for (IRRegister argument : instruction.arguments) {
                arguments.push_back(readRegister(frame, argument));
            }
            writeRegister(frame, readDest(instruction), callFunction(program, callee.asFunction(), arguments));
            break;
        }
        case IROp::Return:
            return ExecutionResult{true, readRegister(frame, readLeft(instruction))};
```

Add `callFunction()`:

```cpp
Value IRInterpreter::callFunction(const IRProgram& program, const FunctionValue& function, const std::vector<Value>& arguments)
{
    if (function.functionIndex >= program.functions().size()) {
        throw IRRuntimeError("function index out of range");
    }
    const IRFunction& irFunction = program.functions()[function.functionIndex];
    if (arguments.size() != irFunction.parameters.size()) {
        throw IRRuntimeError("expected " + std::to_string(irFunction.parameters.size())
            + " arguments but got " + std::to_string(arguments.size()));
    }

    Frame frame;
    frame.registers.assign(irFunction.registerCount, Value::nil());
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        frame.locals.emplace(irFunction.parameters[i], arguments[i]);
    }

    ExecutionResult result = executeInstructions(program, irFunction.instructions, frame, false);
    return result.returned ? result.value : Value::nil();
}
```

- [ ] **Step 6: Run golden tests and verify function runtime GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: function success fixtures pass. If IR fixture output changed due final compiler/interpreter design, update and review:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
git diff -- tests/golden/function_ir/ir.out
```

- [ ] **Step 7: Commit interpreter support**

Run:

```bash
git add include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/function_return_nil tests/golden/function_recursion tests/golden/function_scope_global tests/golden/function_ir/ir.out
git commit -m "feat: execute function calls"
```

Expected: commit succeeds with interpreter and additional success fixtures.

## Task 8: Add Runtime Error Fixtures for Calls

**Files:**
- Create: `tests/golden/runtime_errors/call_non_function.cd`
- Create: `tests/golden/runtime_errors/call_non_function.run.err`
- Create: `tests/golden/runtime_errors/call_non_function.exit`

- [ ] **Step 1: Add call-non-function runtime fixture**

Run:

```bash
cat > tests/golden/runtime_errors/call_non_function.cd <<'CASE'
let x = 1;
print x();
CASE
cat > tests/golden/runtime_errors/call_non_function.run.err <<'CASE'
Runtime error: can only call functions
CASE
cat > tests/golden/runtime_errors/call_non_function.exit <<'CASE'
1
CASE
```

- [ ] **Step 2: Run golden tests and verify runtime error GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: runtime error fixture passes. The variable `x` is unannotated, so its static type is `unknown`; the call is allowed statically and rejected at runtime.

- [ ] **Step 3: Commit runtime call errors**

Run:

```bash
git add tests/golden/runtime_errors/call_non_function.*
git commit -m "test: cover invalid function calls"
```

Expected: commit succeeds with whichever fixture category matches implementation behavior.

## Task 9: Update Documentation

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md` if a new convention is useful.

- [ ] **Step 1: Update grammar**

Edit `docs/language-grammar.ebnf`.

Add `funDecl` to declarations:

```ebnf
declaration = funDecl
            | letDecl
            | statement ;
```

Add productions:

```ebnf
funDecl     = "fun", identifier,
              "(", [ parameters ], ")",
              block ;

parameters  = identifier,
              { ",", identifier } ;
```

Add return to statements:

```ebnf
statement   = printStmt
            | ifStmt
            | whileStmt
            | returnStmt
            | block
            | exprStmt ;

returnStmt  = "return", [ expression ], ";" ;
```

Update expression precedence:

```ebnf
unary       = ( "!" | "-" ), unary
            | call ;

call        = primary,
              { "(", [ arguments ], ")" } ;

arguments   = expression,
              { ",", expression } ;
```

- [ ] **Step 2: Update README**

Edit `README.md` supported statements block to include:

```text
fun name(parameter*) { declaration* }
return [expression];
```

Edit supported expressions list to include:

```markdown
- Calls: `callee(argument*)`
```

Add a concise paragraph:

```markdown
Functions are named values declared with `fun`. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive calls are supported. Closures and function type annotations are not implemented yet.
```

- [ ] **Step 3: Update roadmap**

Edit `docs/roadmap.md` Phase 6 section so Phase 6A is marked implemented and closures remain pending:

```markdown
### Phase 6A: Function Declarations and Calls — Implemented

Status: implemented. The language supports named functions, calls, explicit `return`, bare `return;`, implicit `nil` returns, and recursion. Closures are not implemented yet.
```

Leave Phase 6B closures as a future subsection.

- [ ] **Step 4: Update AGENTS function convention**

Add this function-specific convention to `AGENTS.md`:

```markdown
- Phase 6A functions compile to an IR function table. Closures are intentionally unsupported; function bodies may use parameters, locals, recursive/global bindings, but not non-global enclosing locals.
```

- [ ] **Step 5: Run documentation diff review**

Run:

```bash
git diff -- docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
```

Expected: docs describe implemented Phase 6A only and clearly say closures are not implemented.

- [ ] **Step 6: Commit docs**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document function declarations and calls"
```

Expected: commit succeeds with documentation changes.

## Task 10: Full Verification and Cleanup

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
Implemented Phase 6A function declarations and calls with explicit/bare/implicit nil returns, recursion, function values, IR function table/call frames, arity diagnostics, no-closure checks, goldens, and docs.
```
