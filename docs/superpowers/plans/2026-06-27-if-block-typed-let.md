# If Block Typed Let Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `if`/`else`, block statements, and syntax-only typed `let` declarations in the compiler pipeline.

**Architecture:** Extend the lexer/parser/AST to represent the new syntax, then extend the register IR with patchable `jump` and `jump_if_false` instructions so `if` executes through the existing IR interpreter. Blocks compile by compiling contained statements in order and do not create lexical scopes; typed lets preserve the annotation in AST output but IR generation ignores it.

**Tech Stack:** C++17, CMake/CTest, Python golden CLI test runner, existing register IR and IR interpreter.

---

## File Structure

- Modify `include/Token.hpp` and `src/Lexer.cpp`: add `{`, `}`, `:`, `if`, and `else` tokens.
- Modify `include/Ast.hpp` and `src/Ast.cpp`: add `BlockStmt`, `IfStmt`, and optional `LetStmt::typeName`.
- Modify `include/Parser.hpp` and `src/Parser.cpp`: parse typed lets, if/else, and block statements.
- Modify `include/IR.hpp` and `src/IR.cpp`: add `Jump` and `JumpIfFalse`, patch helpers, and jump printing.
- Modify `include/IRCompiler.hpp` and `src/IRCompiler.cpp`: compile block and if statements, ignore `LetStmt::typeName`.
- Modify `src/IRInterpreter.cpp`: execute jumps with an explicit instruction pointer loop.
- Create golden fixtures under `tests/golden/if_else`, `tests/golden/block`, and `tests/golden/typed_let`.
- Modify `docs/language-grammar.ebnf` and `README.md`: document supported syntax and current non-goals.

## Task 1: Add Failing Golden Runtime Fixtures

**Files:**
- Create: `tests/golden/if_else/input.cd`
- Create: `tests/golden/if_else/run.out`
- Create: `tests/golden/block/input.cd`
- Create: `tests/golden/block/run.out`
- Create: `tests/golden/typed_let/input.cd`
- Create: `tests/golden/typed_let/run.out`

- [ ] **Step 1: Add the new runtime fixture inputs and expected runtime outputs**

Run:

```bash
mkdir -p tests/golden/if_else tests/golden/block tests/golden/typed_let

cat > tests/golden/if_else/input.cd <<'CASE'
let ok = true;
if ok {
  print "then";
} else {
  print "else";
}

if false {
  print "bad";
} else {
  print "fallback";
}
CASE
cat > tests/golden/if_else/run.out <<'CASE'
then
fallback
CASE

cat > tests/golden/block/input.cd <<'CASE'
{
  let x = 10;
  print x;
}
print x;
CASE
cat > tests/golden/block/run.out <<'CASE'
10
10
CASE

cat > tests/golden/typed_let/input.cd <<'CASE'
let answer: number = 42;
let label: string = "answer";
print label;
print answer;
CASE
cat > tests/golden/typed_let/run.out <<'CASE'
answer
42
CASE
```

- [ ] **Step 2: Run CTest and verify the new fixtures fail before implementation**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `golden` fails. The failure should mention parse/lex errors around `if`, `{`, or `:` because the syntax is not implemented yet.

- [ ] **Step 3: Leave these fixtures uncommitted until the implementation is green**

Run:

```bash
git status --short tests/golden/if_else tests/golden/block tests/golden/typed_let
```

Expected output includes the three new fixture directories as untracked or modified files. Do not commit yet because the suite is intentionally red.

## Task 2: Implement Lexer, AST, and Parser Support

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add token types**

In `include/Token.hpp`, add the new punctuation near existing punctuation and keywords near existing keywords:

```cpp
enum class TokenType {
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Colon,
    Semicolon,
    ...
    If,
    Else,
    Let,
    Print,
    True,
    False,
    Nil,
    ...
};
```

- [ ] **Step 2: Lex braces, colon, and keywords**

In `src/Lexer.cpp`, add scan cases:

```cpp
case '{':
    addToken(TokenType::LeftBrace);
    break;
case '}':
    addToken(TokenType::RightBrace);
    break;
case ':':
    addToken(TokenType::Colon);
    break;
```

Extend keyword lookup:

```cpp
{"if", TokenType::If},
{"else", TokenType::Else},
```

Extend `tokenTypeName` with exact names:

```cpp
case TokenType::LeftBrace:
    return "LeftBrace";
case TokenType::RightBrace:
    return "RightBrace";
case TokenType::Colon:
    return "Colon";
case TokenType::If:
    return "If";
case TokenType::Else:
    return "Else";
```

- [ ] **Step 3: Extend AST declarations**

In `include/Ast.hpp`, include `<optional>` and update/add statement nodes:

```cpp
#include <optional>
```

Replace `LetStmt` declaration with:

```cpp
struct LetStmt final : Stmt {
    LetStmt(Token name, std::optional<Token> typeName, ExprPtr initializer);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::optional<Token> typeName;
    ExprPtr initializer;
};
```

Add after `ExpressionStmt` or before `Program`:

```cpp
struct BlockStmt final : Stmt {
    explicit BlockStmt(std::vector<StmtPtr> statements);
    void print(std::ostream& out, int indent) const override;

    std::vector<StmtPtr> statements;
};

struct IfStmt final : Stmt {
    IfStmt(ExprPtr condition, StmtPtr thenBranch, StmtPtr elseBranch);
    void print(std::ostream& out, int indent) const override;

    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
};
```

- [ ] **Step 4: Implement AST constructors and printing**

In `src/Ast.cpp`, update `LetStmt` and add block/if printing:

```cpp
LetStmt::LetStmt(Token name, std::optional<Token> typeName, ExprPtr initializer)
    : name(std::move(name))
    , typeName(std::move(typeName))
    , initializer(std::move(initializer))
{
}

void LetStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Let " << name.lexeme;
    if (typeName) {
        out << ": " << typeName->lexeme;
    }
    out << " = ";
    writeExpr(out, initializer);
    out << '\n';
}

BlockStmt::BlockStmt(std::vector<StmtPtr> statements)
    : statements(std::move(statements))
{
}

void BlockStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Block\n";
    for (const auto& statement : statements) {
        statement->print(out, indent + 1);
    }
}

IfStmt::IfStmt(ExprPtr condition, StmtPtr thenBranch, StmtPtr elseBranch)
    : condition(std::move(condition))
    , thenBranch(std::move(thenBranch))
    , elseBranch(std::move(elseBranch))
{
}

void IfStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "If ";
    writeExpr(out, condition);
    out << '\n';

    writeIndent(out, indent + 1);
    out << "Then\n";
    if (thenBranch) {
        thenBranch->print(out, indent + 2);
    }

    if (elseBranch) {
        writeIndent(out, indent + 1);
        out << "Else\n";
        elseBranch->print(out, indent + 2);
    }
}
```

- [ ] **Step 5: Extend parser interface**

In `include/Parser.hpp`, add helpers:

```cpp
StmtPtr ifStatement();
StmtPtr blockStatement();
std::vector<StmtPtr> blockStatements();
```

- [ ] **Step 6: Parse typed lets, if statements, and blocks**

In `src/Parser.cpp`, update `letDeclaration()` so initializer is required and type name is optional:

```cpp
StmtPtr Parser::letDeclaration()
{
    Token name = consume(TokenType::Identifier, "expected variable name after `let`");

    std::optional<Token> typeName;
    if (match(TokenType::Colon)) {
        typeName = consume(TokenType::Identifier, "expected type name after `:`");
    }

    consume(TokenType::Equal, "expected `=` after variable declaration");
    ExprPtr initializer = expression();

    consume(TokenType::Semicolon, "expected `;` after variable declaration");
    return std::make_unique<LetStmt>(std::move(name), std::move(typeName), std::move(initializer));
}
```

Update `statement()`:

```cpp
StmtPtr Parser::statement()
{
    if (match(TokenType::Print)) {
        return printStatement();
    }
    if (match(TokenType::If)) {
        return ifStatement();
    }
    if (match(TokenType::LeftBrace)) {
        return blockStatement();
    }
    return expressionStatement();
}
```

Add parser methods:

```cpp
StmtPtr Parser::ifStatement()
{
    ExprPtr condition = expression();
    consume(TokenType::LeftBrace, "expected `{` after if condition");
    StmtPtr thenBranch = blockStatement();

    StmtPtr elseBranch;
    if (match(TokenType::Else)) {
        consume(TokenType::LeftBrace, "expected `{` after `else`");
        elseBranch = blockStatement();
    }

    return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

StmtPtr Parser::blockStatement()
{
    return std::make_unique<BlockStmt>(blockStatements());
}

std::vector<StmtPtr> Parser::blockStatements()
{
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        statements.push_back(declaration());
    }
    consume(TokenType::RightBrace, "expected `}` after block");
    return statements;
}
```

- [ ] **Step 7: Build and inspect parser-only progress**

Run:

```bash
cmake --build build
printf 'let answer: number = 42;\nif true { print answer; } else { print 0; }\n' | ./build/compiler_demo
```

Expected: build succeeds and default AST output contains `Let answer: number`, `If true`, `Then`, and `Else`. `ctest` may still fail because IR control-flow support is not complete yet.

## Task 3: Add Jump IR and Interpreter Support

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `src/IRInterpreter.cpp`

- [ ] **Step 1: Add jump opcodes and helpers to IR interface**

In `include/IR.hpp`, add opcodes:

```cpp
Jump,
JumpIfFalse,
```

Add public helpers to `IRProgram`:

```cpp
std::size_t emitJump();
std::size_t emitJumpIfFalse(IRRegister condition);
void patchJump(std::size_t jumpInstruction);
std::size_t instructionCount() const;
```

- [ ] **Step 2: Implement jump helpers and printing**

In `src/IR.cpp`, update `isBinary()` switch to return false for `Jump` and `JumpIfFalse`.

Implement helpers:

```cpp
std::size_t IRProgram::emitJump()
{
    const std::size_t instruction = instructions_.size();
    emit(IRInstruction{IROp::Jump, std::nullopt, std::nullopt, std::nullopt, 0});
    return instruction;
}

std::size_t IRProgram::emitJumpIfFalse(IRRegister condition)
{
    const std::size_t instruction = instructions_.size();
    emit(IRInstruction{IROp::JumpIfFalse, std::nullopt, condition, std::nullopt, 0});
    return instruction;
}

void IRProgram::patchJump(std::size_t jumpInstruction)
{
    if (jumpInstruction >= instructions_.size()) {
        return;
    }
    instructions_[jumpInstruction].operand = instructions_.size();
}

std::size_t IRProgram::instructionCount() const
{
    return instructions_.size();
}
```

In `IRProgram::print`, add before unary/binary branches:

```cpp
} else if (instruction.op == IROp::Jump) {
    out << " " << std::setw(4) << std::setfill('0') << instruction.operand << std::setfill(' ');
} else if (instruction.op == IROp::JumpIfFalse) {
    if (instruction.left) {
        out << " " << *instruction.left << ", ";
    } else {
        out << " <missing>, ";
    }
    out << std::setw(4) << std::setfill('0') << instruction.operand << std::setfill(' ');
```

Extend `irOpName()`:

```cpp
case IROp::Jump:
    return "jump";
case IROp::JumpIfFalse:
    return "jump_if_false";
```

- [ ] **Step 3: Execute with an explicit instruction pointer**

In `src/IRInterpreter.cpp`, add a local helper near the anonymous namespace:

```cpp
void validateJumpTarget(std::size_t target, std::size_t instructionCount)
{
    if (target > instructionCount) {
        throw IRRuntimeError("jump target out of range");
    }
}
```

Replace the `for` loop in `IRInterpreter::execute` with an instruction pointer loop. The pattern should be:

```cpp
std::size_t ip = 0;
while (ip < instructions.size()) {
    const IRInstruction& instruction = instructions[ip];
    switch (instruction.op) {
    case IROp::Constant:
        writeRegister(readDest(instruction), readConstant(program, instruction.operand));
        ++ip;
        break;
    ...
    case IROp::Jump:
        validateJumpTarget(instruction.operand, instructions.size());
        ip = instruction.operand;
        break;
    case IROp::JumpIfFalse:
        validateJumpTarget(instruction.operand, instructions.size());
        if (!isTruthy(readRegister(readLeft(instruction)))) {
            ip = instruction.operand;
        } else {
            ++ip;
        }
        break;
    }
}
```

Every existing non-jump instruction must increment `ip` exactly once after performing its existing behavior.

- [ ] **Step 4: Build after IR/interpreter changes**

Run:

```bash
cmake --build build
```

Expected: build succeeds or fails only because `IRCompiler` does not yet handle `BlockStmt`/`IfStmt`. If compiler errors mention missing `Jump` handling in switches, update the switch as described above.

## Task 4: Compile Block and If Statements to IR

**Files:**
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Compile block statements**

In `IRCompiler::compileStatement`, after expression statements or before the unsupported fallback, add:

```cpp
if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
    for (const auto& child : block->statements) {
        compileStatement(*child);
    }
    return;
}
```

- [ ] **Step 2: Compile if statements**

In `IRCompiler::compileStatement`, add:

```cpp
if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&statement)) {
    const IRRegister condition = compileExpression(*ifStmt->condition);
    const std::size_t jumpIfFalse = ir_.emitJumpIfFalse(condition);

    compileStatement(*ifStmt->thenBranch);

    if (ifStmt->elseBranch) {
        const std::size_t jumpOverElse = ir_.emitJump();
        ir_.patchJump(jumpIfFalse);
        compileStatement(*ifStmt->elseBranch);
        ir_.patchJump(jumpOverElse);
    } else {
        ir_.patchJump(jumpIfFalse);
    }
    return;
}
```

- [ ] **Step 3: Confirm typed let is ignored by IR generation**

No code is needed beyond using `let->initializer` as before. Confirm the let branch still compiles:

```cpp
if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
    const IRRegister value = compileExpression(*let->initializer);
    ir_.emitStoreVar(let->name.lexeme, value);
    return;
}
```

Do not reference `let->typeName` in IR generation.

- [ ] **Step 4: Build and run targeted samples**

Run:

```bash
cmake --build build
printf 'let ok = true;\nif ok { print "then"; } else { print "else"; }\n' | ./build/compiler_demo --run
printf '{ let x = 10; print x; } print x;\n' | ./build/compiler_demo --run
printf 'let answer: number = 42; print answer;\n' | ./build/compiler_demo --run
```

Expected outputs:

```text
then
```

```text
10
10
```

```text
42
```

## Task 5: Generate Golden Outputs and Commit Implementation

**Files:**
- Modify: generated files under `tests/golden/if_else/`
- Modify: generated files under `tests/golden/block/`
- Modify: generated files under `tests/golden/typed_let/`
- Commit all language implementation files from Tasks 2-4.

- [ ] **Step 1: Generate golden AST and IR outputs**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Expected: output includes `golden tests: 33 passed, 0 failed` because existing 24 checks plus 3 new successful cases with 3 checks each equals 33.

- [ ] **Step 2: Run all tests**

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
100% tests passed, 0 tests failed out of 2
golden tests: 33 passed, 0 failed
Ran 5 tests
OK
```

- [ ] **Step 3: Inspect new IR and runtime outputs**

Run:

```bash
printf '%s\n' '--- if_else run.out ---'
cat tests/golden/if_else/run.out
printf '%s\n' '--- if_else ir.out jumps ---'
grep -n 'jump' tests/golden/if_else/ir.out
printf '%s\n' '--- typed_let ast.out ---'
cat tests/golden/typed_let/ast.out
```

Expected:

```text
then
fallback
```

`if_else/ir.out` contains `jump_if_false` and `jump` lines. `typed_let/ast.out` contains `Let answer: number` and `Let label: string`.

- [ ] **Step 4: Commit implementation and golden tests**

Run:

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp include/IR.hpp src/IR.cpp src/IRInterpreter.cpp src/IRCompiler.cpp tests/golden/if_else tests/golden/block tests/golden/typed_let
git commit -m "feat: add if blocks and typed lets"
```

Expected: commit succeeds.

## Task 6: Update Language Documentation

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`

- [ ] **Step 1: Update grammar document to target grammar**

Replace `docs/language-grammar.ebnf` with grammar from `docs/superpowers/specs/2026-06-27-if-block-typed-let-design.md`, including `ifStmt`, `block`, and typed `let` as implemented syntax. Remove the old comment listing those constructs as not implemented.

- [ ] **Step 2: Update README language section**

In `README.md`, update supported statements to:

```text
let name = expression;
let name: type = expression;
print expression;
if expression { statement* } else { statement* }
{ statement* }
expression;
```

Add this paragraph below the statement list:

```markdown
Type annotations on `let` declarations are currently syntax-only: they are parsed and shown in the AST, but they are not type-checked yet. Blocks group statements for control flow but do not introduce lexical scope yet.
```

- [ ] **Step 3: Run documentation verification tests**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
rm -rf tests/__pycache__
```

Expected:

```text
100% tests passed, 0 tests failed out of 2
golden tests: 33 passed, 0 failed
```

- [ ] **Step 4: Commit documentation updates**

Run:

```bash
git add docs/language-grammar.ebnf README.md
git commit -m "docs: describe if blocks and typed lets"
```

Expected: commit succeeds.

## Task 7: Final Verification

**Files:**
- Verify only.

- [ ] **Step 1: Run full verification**

Run:

```bash
rm -rf tests/__pycache__
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
test ! -e tests/__pycache__
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
git status --short
```

Expected:

```text
100% tests passed, 0 tests failed out of 2
golden tests: 33 passed, 0 failed
Ran 5 tests
OK
```

`git status --short` should produce no output.

- [ ] **Step 2: Review recent commits**

Run:

```bash
git log --oneline -5
```

Expected recent commits include:

```text
docs: describe if blocks and typed lets
feat: add if blocks and typed lets
Add if block typed let design spec
```

## Self-Review Notes

- Spec coverage: Tasks 2-4 implement lexer/parser/AST/IR/compiler/interpreter behavior. Task 5 adds golden runtime/AST/IR tests. Task 6 updates grammar and README. Task 7 verifies everything.
- Scope: The plan does not add type checking, lexical scopes, loops, functions, CFG construction, or SSA.
- Type consistency: The plan consistently uses `BlockStmt`, `IfStmt`, `LetStmt::typeName`, `IROp::Jump`, `IROp::JumpIfFalse`, `emitJump`, `emitJumpIfFalse`, `patchJump`, and `instructionCount`.
