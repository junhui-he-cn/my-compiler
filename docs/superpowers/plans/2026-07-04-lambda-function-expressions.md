# Lambda / Function Expressions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `fun (params) { body }` function expressions that behave like existing functions and closures in both the IR interpreter and bytecode VM.

**Architecture:** Add a new `FunctionExpr` AST node parsed in primary-expression position. Type checking records anonymous function metadata in `ResolvedNames`, IR lowering emits existing function-table plus `make_function` operations, and bytecode execution works through the already implemented `MakeFunction`/`Call`/closure machinery.

**Tech Stack:** C++17, recursive-descent parser, existing AST/TypeChecker/IR/Bytecode pipeline, Python golden tests.

---

## File Structure

Modify:

- `include/Ast.hpp` — add `FunctionExpr` and move/introduce `Stmt` forward declaration so expression nodes can own function bodies.
- `src/Ast.cpp` — implement `FunctionExpr` constructor and compact AST printing.
- `include/Parser.hpp` — add `functionExpression()` helper declaration.
- `src/Parser.cpp` — parse `fun (params) { body }` in `primary()`.
- `include/TypeChecker.hpp` — add `ResolvedNames` entries for `FunctionExpr`, and `checkFunctionExpression()`.
- `src/TypeChecker.cpp` — resolve lambda parameters/body and return `StaticType::Function`.
- `include/IRCompiler.hpp` — add `emitFunctionExpr()`.
- `src/IRCompiler.cpp` — lower `FunctionExpr` to existing `IRProgram::beginFunction()` / `emitMakeFunction()`.
- `docs/language-grammar.ebnf` — add `functionExpr`.
- `README.md` — document function-expression syntax and remove stale “not implemented” note.
- `docs/roadmap.md` — mark Phase 6C implemented after implementation.
- `AGENTS.md` — update current language semantics.

Create success fixtures:

- `tests/golden/lambda_basic/input.cd`
- `tests/golden/lambda_basic/ast.out`
- `tests/golden/lambda_basic/ir.out`
- `tests/golden/lambda_basic/bytecode.out`
- `tests/golden/lambda_basic/run.out`
- `tests/golden/lambda_basic/run_bytecode.out`
- `tests/golden/lambda_immediate_call/input.cd`
- `tests/golden/lambda_immediate_call/run.out`
- `tests/golden/lambda_immediate_call/run_bytecode.out`
- `tests/golden/lambda_closure/input.cd`
- `tests/golden/lambda_closure/run.out`
- `tests/golden/lambda_closure/run_bytecode.out`
- `tests/golden/lambda_mutable_closure/input.cd`
- `tests/golden/lambda_mutable_closure/run.out`
- `tests/golden/lambda_mutable_closure/run_bytecode.out`

Create error fixtures:

- `tests/golden/parse_errors/lambda_missing_parameter_paren.cd`
- `tests/golden/parse_errors/lambda_missing_parameter_paren.err`
- `tests/golden/parse_errors/lambda_missing_parameter_paren.exit`
- `tests/golden/parse_errors/lambda_missing_body.cd`
- `tests/golden/parse_errors/lambda_missing_body.err`
- `tests/golden/parse_errors/lambda_missing_body.exit`
- `tests/golden/type_errors/lambda_duplicate_parameter.cd`
- `tests/golden/type_errors/lambda_duplicate_parameter.err`
- `tests/golden/type_errors/lambda_duplicate_parameter.exit`

Reference:

- `docs/superpowers/specs/2026-07-04-lambda-function-expressions-design.md`
- `tests/golden/function_call_add/ast.out`
- `tests/golden/closure_counter/ir.out`
- `src/IRCompiler.cpp::compileFunctionStatement()`
- `src/TypeChecker.cpp::checkFunction()`

---

### Task 1: Add Lambda AST, Parser, and TypeChecker Support

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: `tests/golden/lambda_basic/input.cd`
- Test: `tests/golden/lambda_basic/ast.out`

- [ ] **Step 1: Add a failing frontend fixture**

Create `tests/golden/lambda_basic/input.cd`:

```text
let inc = fun (x) {
  return x + 1;
};
print inc(41);
```

Create `tests/golden/lambda_basic/ast.out`:

```text
Program
  Let inc = (fun (x) (return (+ x 1)))
  Print (call inc 41)
```

- [ ] **Step 2: Run the failing golden check**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: failure for `lambda_basic default(ast)` with a parse error such as `expected expression`, because `fun` is not yet valid in primary-expression position.

- [ ] **Step 3: Add `FunctionExpr` to `include/Ast.hpp`**

Move the `Stmt` forward declaration before expression nodes. The top of the file should have these aliases before expression classes that need them:

```cpp
struct Expr {
    virtual ~Expr() = default;
    virtual void print(std::ostream& out) const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
```

Remove the later duplicate `using StmtPtr = std::unique_ptr<Stmt>;` after `struct Stmt`.

Add this expression node after `CallExpr` or before `ArrayExpr`:

```cpp
struct FunctionExpr final : Expr {
    FunctionExpr(Token keyword, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out) const override;

    Token keyword;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};
```

- [ ] **Step 4: Implement `FunctionExpr` printing in `src/Ast.cpp`**

Add compact inline statement printing helpers in the anonymous namespace near `writeExpr()`:

```cpp
void writeStmtInline(std::ostream& out, const StmtPtr& statement);

void writeStmtInline(std::ostream& out, const Stmt& statement)
{
    if (const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        out << "(return";
        if (returnStmt->value) {
            out << ' ';
            writeExpr(out, returnStmt->value);
        }
        out << ')';
        return;
    }

    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        out << "(expr ";
        writeExpr(out, expression->expression);
        out << ')';
        return;
    }

    if (const auto* print = dynamic_cast<const PrintStmt*>(&statement)) {
        out << "(print ";
        writeExpr(out, print->expression);
        out << ')';
        return;
    }

    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        out << "(let " << let->name.lexeme << ' ';
        writeExpr(out, let->initializer);
        out << ')';
        return;
    }

    out << "(stmt)";
}

void writeStmtInline(std::ostream& out, const StmtPtr& statement)
{
    if (!statement) {
        out << "(nil)";
        return;
    }
    writeStmtInline(out, *statement);
}
```

Add the constructor and printer after `CallExpr::print()`:

```cpp
FunctionExpr::FunctionExpr(Token keyword, std::vector<Token> parameters, std::vector<StmtPtr> body)
    : keyword(std::move(keyword))
    , parameters(std::move(parameters))
    , body(std::move(body))
{
}

void FunctionExpr::print(std::ostream& out) const
{
    out << "(fun (";
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << parameters[i].lexeme;
    }
    out << ')';
    for (const auto& statement : body) {
        out << ' ';
        writeStmtInline(out, statement);
    }
    out << ')';
}
```

- [ ] **Step 5: Add parser declarations**

In `include/Parser.hpp`, add:

```cpp
ExprPtr functionExpression();
```

near `arrayLiteral()` / `primary()`.

- [ ] **Step 6: Parse function expressions in `src/Parser.cpp`**

Add:

```cpp
ExprPtr Parser::functionExpression()
{
    Token keyword = previous();
    consume(TokenType::LeftParen, "expected `(` after `fun`");

    std::vector<Token> parameters;
    if (!check(TokenType::RightParen)) {
        do {
            parameters.push_back(consume(TokenType::Identifier, "expected parameter name"));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RightParen, "expected `)` after function parameters");
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionExpr>(std::move(keyword), std::move(parameters), blockStatements());
}
```

At the start of `Parser::primary()`, before literal checks, add:

```cpp
    if (match(TokenType::Fun)) {
        return functionExpression();
    }
```

- [ ] **Step 7: Extend `ResolvedNames` in `include/TypeChecker.hpp`**

Add public overloads:

```cpp
const std::string& functionName(const FunctionExpr& expression) const;
const std::vector<std::string>& parameterNames(const FunctionExpr& expression) const;
```

Add private recorders:

```cpp
void recordFunction(const FunctionExpr& expression, std::string name);
void recordParameters(const FunctionExpr& expression, std::vector<std::string> names);
```

Add maps:

```cpp
std::unordered_map<const FunctionExpr*, std::string> functionExpressionNames_;
std::unordered_map<const FunctionExpr*, std::vector<std::string>> functionExpressionParameterNames_;
```

Add a checker declaration:

```cpp
StaticType checkFunctionExpression(const FunctionExpr& expression);
```

- [ ] **Step 8: Implement `ResolvedNames` lambda support in `src/TypeChecker.cpp`**

Add getter implementations after the `FunctionStmt` getters:

```cpp
const std::string& ResolvedNames::functionName(const FunctionExpr& expression) const
{
    const auto found = functionExpressionNames_.find(&expression);
    if (found == functionExpressionNames_.end()) {
        throw std::logic_error("missing resolved function expression name");
    }
    return found->second;
}

const std::vector<std::string>& ResolvedNames::parameterNames(const FunctionExpr& expression) const
{
    const auto found = functionExpressionParameterNames_.find(&expression);
    if (found == functionExpressionParameterNames_.end()) {
        throw std::logic_error("missing resolved function expression parameter names");
    }
    return found->second;
}
```

In `ResolvedNames::clear()`, clear the two new maps:

```cpp
functionExpressionNames_.clear();
functionExpressionParameterNames_.clear();
```

Add recorders:

```cpp
void ResolvedNames::recordFunction(const FunctionExpr& expression, std::string name)
{
    functionExpressionNames_.emplace(&expression, std::move(name));
}

void ResolvedNames::recordParameters(const FunctionExpr& expression, std::vector<std::string> names)
{
    functionExpressionParameterNames_.emplace(&expression, std::move(names));
}
```

- [ ] **Step 9: Type-check function expressions**

In `TypeChecker::checkExpression()`, after grouping or before call handling, add:

```cpp
    if (const auto* function = dynamic_cast<const FunctionExpr*>(&expression)) {
        return checkFunctionExpression(*function);
    }
```

Implement:

```cpp
StaticType TypeChecker::checkFunctionExpression(const FunctionExpr& expression)
{
    resolvedNames_.recordFunction(expression, "<lambda>");

    beginScope();
    ++functionDepth_;

    std::vector<std::string> parameterNames;
    for (const Token& parameter : expression.parameters) {
        Binding parameterBinding = declareVariable(parameter, StaticType::Unknown);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordParameters(expression, std::move(parameterNames));

    for (const auto& child : expression.body) {
        checkStatement(*child);
    }

    --functionDepth_;
    endScope();

    return StaticType::Function;
}
```

- [ ] **Step 10: Build and run the frontend fixture**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `lambda_basic default(ast)` passes; checks for `--ir`/`--run` are not present yet for this fixture.

- [ ] **Step 11: Commit frontend support**

```bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/lambda_basic
git commit -m "feat: parse lambda function expressions"
```

---

### Task 2: Lower Lambdas to Existing IR and Bytecode

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Test: `tests/golden/lambda_basic/ir.out`
- Test: `tests/golden/lambda_basic/bytecode.out`
- Test: `tests/golden/lambda_basic/run.out`
- Test: `tests/golden/lambda_basic/run_bytecode.out`

- [ ] **Step 1: Add failing backend expectations**

Create `tests/golden/lambda_basic/run.out`:

```text
42
```

Create `tests/golden/lambda_basic/run_bytecode.out`:

```text
42
```

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `lambda_basic --run` fails with `Compile error: unsupported expression node` because IR lowering does not support `FunctionExpr`.

- [ ] **Step 2: Add `emitFunctionExpr()` declaration**

In `include/IRCompiler.hpp`, add:

```cpp
IRRegister emitFunctionExpr(const FunctionExpr& expression);
```

near `emitCall()`.

- [ ] **Step 3: Lower `FunctionExpr` in `src/IRCompiler.cpp`**

In `IRCompiler::compileExpression()`, before `CallExpr` handling, add:

```cpp
    if (const auto* function = dynamic_cast<const FunctionExpr*>(&expression)) {
        return emitFunctionExpr(*function);
    }
```

Implement:

```cpp
IRRegister IRCompiler::emitFunctionExpr(const FunctionExpr& expression)
{
    std::vector<std::string> parameters = resolvedNames_->parameterNames(expression);
    ir_.beginFunction(resolvedNames_->functionName(expression), std::move(parameters));

    for (const auto& statement : expression.body) {
        compileStatement(*statement);
    }
    IRRegister nilValue = ir_.emitConstant(Value::nil());
    ir_.emitReturn(nilValue);

    const std::size_t functionIndex = ir_.endFunction();
    return ir_.emitMakeFunction(functionIndex);
}
```

- [ ] **Step 4: Generate IR and bytecode goldens intentionally**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Then inspect only `lambda_basic` generated files:

```bash
cat tests/golden/lambda_basic/ir.out
cat tests/golden/lambda_basic/bytecode.out
cat tests/golden/lambda_basic/run.out
cat tests/golden/lambda_basic/run_bytecode.out
```

Expected:

- `run.out` is `42`.
- `run_bytecode.out` is `42`.
- `ir.out` contains `make_function` and a function named `<lambda>/1`.
- `bytecode.out` contains `make_function` and a function named `<lambda>/1`.

- [ ] **Step 5: Run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden checks pass.

- [ ] **Step 6: Commit IR/backend support**

```bash
git add include/IRCompiler.hpp src/IRCompiler.cpp tests/golden/lambda_basic
git commit -m "feat: compile lambda function expressions"
```

---

### Task 3: Add Lambda Call and Closure Coverage

**Files:**
- Test: `tests/golden/lambda_immediate_call/input.cd`
- Test: `tests/golden/lambda_immediate_call/run.out`
- Test: `tests/golden/lambda_immediate_call/run_bytecode.out`
- Test: `tests/golden/lambda_closure/input.cd`
- Test: `tests/golden/lambda_closure/run.out`
- Test: `tests/golden/lambda_closure/run_bytecode.out`
- Test: `tests/golden/lambda_mutable_closure/input.cd`
- Test: `tests/golden/lambda_mutable_closure/run.out`
- Test: `tests/golden/lambda_mutable_closure/run_bytecode.out`

- [ ] **Step 1: Add immediate-call fixture**

Create `tests/golden/lambda_immediate_call/input.cd`:

```text
print (fun (x) {
  return x * 2;
})(3);
```

Create `tests/golden/lambda_immediate_call/run.out`:

```text
6
```

Create `tests/golden/lambda_immediate_call/run_bytecode.out`:

```text
6
```

- [ ] **Step 2: Add closure fixture**

Create `tests/golden/lambda_closure/input.cd`:

```text
fun makeAdder(n) {
  return fun (x) {
    return x + n;
  };
}

let add10 = makeAdder(10);
print add10(5);
```

Create `tests/golden/lambda_closure/run.out`:

```text
15
```

Create `tests/golden/lambda_closure/run_bytecode.out`:

```text
15
```

- [ ] **Step 3: Add mutable closure fixture**

Create `tests/golden/lambda_mutable_closure/input.cd`:

```text
fun counter() {
  let n = 0;
  return fun () {
    n = n + 1;
    return n;
  };
}

let next = counter();
print next();
print next();
```

Create `tests/golden/lambda_mutable_closure/run.out`:

```text
1
2
```

Create `tests/golden/lambda_mutable_closure/run_bytecode.out`:

```text
1
2
```

- [ ] **Step 4: Run closure fixtures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all new lambda call/closure fixtures pass. If any fail, fix the relevant type-checker name resolution or IR lowering before continuing.

- [ ] **Step 5: Add optional AST/IR/bytecode goldens for closure fixture**

Generate only if useful for review:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

If this updates many unrelated files, revert unrelated changes and keep only intended lambda fixture outputs.

At minimum, keep `run.out` and `run_bytecode.out` for these fixtures. Add `ast.out`, `ir.out`, or `bytecode.out` only if they provide useful regression coverage without excessive churn.

- [ ] **Step 6: Commit closure coverage**

```bash
git add tests/golden/lambda_immediate_call tests/golden/lambda_closure tests/golden/lambda_mutable_closure
git commit -m "test: cover lambda calls and closures"
```

---

### Task 4: Add Lambda Parse and Type Error Coverage

**Files:**
- Test: `tests/golden/parse_errors/lambda_missing_parameter_paren.cd`
- Test: `tests/golden/parse_errors/lambda_missing_parameter_paren.err`
- Test: `tests/golden/parse_errors/lambda_missing_parameter_paren.exit`
- Test: `tests/golden/parse_errors/lambda_missing_body.cd`
- Test: `tests/golden/parse_errors/lambda_missing_body.err`
- Test: `tests/golden/parse_errors/lambda_missing_body.exit`
- Test: `tests/golden/type_errors/lambda_duplicate_parameter.cd`
- Test: `tests/golden/type_errors/lambda_duplicate_parameter.err`
- Test: `tests/golden/type_errors/lambda_duplicate_parameter.exit`

- [ ] **Step 1: Add missing-parameter-paren parse fixture**

Create `tests/golden/parse_errors/lambda_missing_parameter_paren.cd`:

```text
let f = fun (x { return x; };
```

Create `tests/golden/parse_errors/lambda_missing_parameter_paren.err`:

```text
Parse error at 1:16: expected `)` after function parameters
```

Create `tests/golden/parse_errors/lambda_missing_parameter_paren.exit`:

```text
1
```

- [ ] **Step 2: Add missing-body parse fixture**

Create `tests/golden/parse_errors/lambda_missing_body.cd`:

```text
let f = fun (x) return x;
```

Create `tests/golden/parse_errors/lambda_missing_body.err`:

```text
Parse error at 1:17: expected `{` before function body
```

Create `tests/golden/parse_errors/lambda_missing_body.exit`:

```text
1
```

- [ ] **Step 3: Add duplicate-parameter type fixture**

Create `tests/golden/type_errors/lambda_duplicate_parameter.cd`:

```text
let f = fun (x, x) {
  return x;
};
```

Create `tests/golden/type_errors/lambda_duplicate_parameter.err`:

```text
Type error at 1:17: duplicate declaration `x`
```

Create `tests/golden/type_errors/lambda_duplicate_parameter.exit`:

```text
1
```

- [ ] **Step 4: Run error fixtures and correct exact locations if needed**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

If column numbers differ, inspect actual stderr and update only the new lambda error goldens to match the current diagnostic location behavior.

- [ ] **Step 5: Commit diagnostics coverage**

```bash
git add tests/golden/parse_errors/lambda_missing_parameter_paren.* tests/golden/parse_errors/lambda_missing_body.* tests/golden/type_errors/lambda_duplicate_parameter.*
git commit -m "test: cover lambda diagnostics"
```

---

### Task 5: Update Grammar and User Documentation

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar**

In `docs/language-grammar.ebnf`, change `primary` to include `functionExpr` first:

```text
primary     = functionExpr
            | "false"
            | "true"
            | "nil"
            | number
            | string
            | array
            | identifier
            | "(", expression, ")" ;
```

Add:

```text
functionExpr = "fun", "(", [ parameters ], ")", block ;
```

near `array` / `primary`.

- [ ] **Step 2: Update README language section**

In `README.md`, replace the function paragraph:

```markdown
Functions are named values declared with `fun`. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive calls are supported. Nested functions are closures: they capture enclosing local variables by reference, so reads and assignments share the same variable cell even after the outer function returns. Lambda/function-expression syntax and function type annotations are not implemented yet.
```

with:

```markdown
Functions are values. Named functions are declared with `fun name(...) { ... }`, and anonymous function expressions use `fun (...) { ... }`. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported. Nested functions and function expressions are closures: they capture enclosing local variables by reference, so reads and assignments share the same variable cell even after the outer function returns. Function type annotations are not implemented yet.
```

In the supported expressions list, add:

```markdown
- Function expressions: `fun (parameter*) { declaration* }`
```

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, replace the Phase 6C section with:

```markdown
### Phase 6C: Function Expressions / Lambdas — Implemented

Status: implemented. The language supports anonymous function expressions with `fun (parameter*) { declaration* }`. Function expressions produce ordinary function values, support `return`, and reuse the existing by-reference closure capture model.
```

Keep future lambda features such as expression-body syntax and function type annotations out of implemented wording.

- [ ] **Step 4: Update AGENTS current semantics**

In `AGENTS.md`, update current semantics bullets so expressions include function expressions:

```markdown
- Supported expressions include literals, arrays, indexing, variables, calls, function expressions, grouping, unary operators, binary/logical operators, and assignment expressions.
```

Update function semantics:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Nested functions and function expressions are closures and capture enclosing local variables by reference through shared runtime cells.
```

- [ ] **Step 5: Run full verification**

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

- `ctest`: all tests pass.
- Golden tests: all checks pass.
- Selftests: OK.

- [ ] **Step 6: Commit documentation**

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document lambda function expressions"
```

---

## Self-Review Checklist

- Spec coverage:
  - `fun (params) { body }` expression syntax: Task 1.
  - AST node and printing: Task 1.
  - TypeChecker and resolved names: Task 1.
  - IR lowering with existing `make_function`: Task 2.
  - Bytecode parity without new opcode: Task 2 and Task 3.
  - Closure by-reference behavior: Task 3.
  - Parse/type diagnostics: Task 4.
  - Grammar/README/roadmap/AGENTS: Task 5.
- No new runtime value kind is planned.
- No new IR or bytecode opcode is planned.
- Full verification command is included.
- Worktree isolation is required at execution time via `superpowers:using-git-worktrees`.

