# Logical Operators Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add short-circuit `&&` and `||` logical expression operators with truthiness and operand-value result semantics.

**Architecture:** Add lexer tokens, a dedicated `LogicalExpr` AST node, parser precedence levels between assignment and equality, type-checker support, and IR lowering that uses `copy`, `jump_if_true`, and `jump_if_false` to preserve short-circuit behavior. Extend the IR interpreter with `Copy` and `JumpIfTrue`, then document the implemented grammar and roadmap status.

**Tech Stack:** C++17, CMake, recursive-descent parser, AST side tables in `TypeChecker`, register IR, IR interpreter, Python golden tests, CTest.

---

## File Structure

- Modify: `include/Token.hpp` — add token kinds for `&&` and `||`.
- Modify: `src/Lexer.cpp` — scan `&&` and `||`, leave lone `&` and `|` as lexical errors, print token names.
- Modify: `include/Ast.hpp` — add `LogicalExpr` declaration.
- Modify: `src/Ast.cpp` — implement `LogicalExpr` constructor and prefix printer.
- Modify: `include/Parser.hpp` — add `logicalOr()` and `logicalAnd()` parser methods.
- Modify: `src/Parser.cpp` — insert logical precedence levels between assignment and equality.
- Modify: `src/TypeChecker.cpp` — type-check and resolve operands of `LogicalExpr`.
- Modify: `include/IR.hpp` — add `IROp::Copy`, `IROp::JumpIfTrue`, `emitCopy()`, `emitCopyTo()`, and `emitJumpIfTrue()`.
- Modify: `src/IR.cpp` — construct and print new IR operations.
- Modify: `include/IRCompiler.hpp` — add `emitLogical()` helper declaration.
- Modify: `src/IRCompiler.cpp` — lower `LogicalExpr` with short-circuit jumps and a stable result register.
- Modify: `src/IRInterpreter.cpp` — execute `Copy` and `JumpIfTrue`.
- Create: new success fixtures under `tests/golden/logical_*`.
- Create: new parse-error fixtures under `tests/golden/parse_errors`.
- Modify: `docs/language-grammar.ebnf` — document logical precedence.
- Modify: `README.md` — document logical operators and semantics.
- Modify: `docs/roadmap.md` — mark Phase 3 implemented after verification.

## Task 0: Baseline Verification

**Files:**
- Verify only.

- [ ] **Step 1: Check workspace state**

Run:

```bash
git status --short
```

Expected: only this plan file may be uncommitted before implementation begins. If other user changes are present, inspect them and do not overwrite them.

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

Expected baseline at the time this plan was written:

```text
100% tests passed, 0 tests failed out of 2
golden tests: all existing cases passed
Ran 9 tests
OK
```

If baseline fails before logical-operator edits, stop and report the exact failing command and output.

## Task 1: Add RED Front-End Golden Tests

**Files:**
- Create: `tests/golden/logical_precedence/input.cd`
- Create: `tests/golden/logical_precedence/ast.out`
- Create: `tests/golden/logical_precedence/run.out`
- Create: `tests/golden/parse_errors/logical_and_missing_rhs.cd`
- Create: `tests/golden/parse_errors/logical_and_missing_rhs.err`
- Create: `tests/golden/parse_errors/logical_and_missing_rhs.exit`
- Create: `tests/golden/parse_errors/logical_or_missing_rhs.cd`
- Create: `tests/golden/parse_errors/logical_or_missing_rhs.err`
- Create: `tests/golden/parse_errors/logical_or_missing_rhs.exit`

- [ ] **Step 1: Create logical precedence fixture**

Run:

```bash
mkdir -p tests/golden/logical_precedence
cat > tests/golden/logical_precedence/input.cd <<'CASE'
print true || false && false;
CASE
cat > tests/golden/logical_precedence/ast.out <<'CASE'
Program
  Print (|| true (&& false false))
CASE
cat > tests/golden/logical_precedence/run.out <<'CASE'
true
CASE
```

- [ ] **Step 2: Create malformed `&&` parse-error fixture**

Run:

```bash
cat > tests/golden/parse_errors/logical_and_missing_rhs.cd <<'CASE'
print true && ;
CASE
cat > tests/golden/parse_errors/logical_and_missing_rhs.err <<'CASE'
Parse error at line 1, column 15: expected expression
CASE
cat > tests/golden/parse_errors/logical_and_missing_rhs.exit <<'CASE'
1
CASE
```

- [ ] **Step 3: Create malformed `||` parse-error fixture**

Run:

```bash
cat > tests/golden/parse_errors/logical_or_missing_rhs.cd <<'CASE'
print false || ;
CASE
cat > tests/golden/parse_errors/logical_or_missing_rhs.err <<'CASE'
Parse error at line 1, column 16: expected expression
CASE
cat > tests/golden/parse_errors/logical_or_missing_rhs.exit <<'CASE'
1
CASE
```

- [ ] **Step 4: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL. The new logical fixtures should fail because the lexer does not recognize `|` or `&` yet. The failure output should mention an unexpected character for `|` or `&` in the new cases.

- [ ] **Step 5: Commit red fixtures**

Run:

```bash
git add tests/golden/logical_precedence tests/golden/parse_errors/logical_and_missing_rhs.* tests/golden/parse_errors/logical_or_missing_rhs.*
git commit -m "test: add logical operator parser goldens"
```

Expected: commit succeeds with only golden fixture files.

## Task 2: Implement Lexer, AST, Parser, and TypeChecker Support

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add logical token kinds**

Edit `include/Token.hpp` so the operator section contains `AmpersandAmpersand` and `PipePipe` after the comparison/equality tokens:

```cpp
    Bang,
    BangEqual,
    Equal,
    EqualEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    AmpersandAmpersand,
    PipePipe,
```

- [ ] **Step 2: Scan `&&` and `||`**

Edit `src/Lexer.cpp` inside `Lexer::scanToken()` and add these cases after the existing `>` case:

```cpp
    case '&':
        if (match('&')) {
            addToken(TokenType::AmpersandAmpersand);
        } else {
            throw std::runtime_error("Unexpected character at line "
                + std::to_string(line_) + ", column " + std::to_string(tokenColumn_)
                + ": '&'");
        }
        break;
    case '|':
        if (match('|')) {
            addToken(TokenType::PipePipe);
        } else {
            throw std::runtime_error("Unexpected character at line "
                + std::to_string(line_) + ", column " + std::to_string(tokenColumn_)
                + ": '|'");
        }
        break;
```

Then edit `tokenTypeName()` in `src/Lexer.cpp` and add these cases after `GreaterEqual`:

```cpp
    case TokenType::AmpersandAmpersand:
        return "AmpersandAmpersand";
    case TokenType::PipePipe:
        return "PipePipe";
```

- [ ] **Step 3: Add `LogicalExpr` declaration**

Edit `include/Ast.hpp` and add this node between `BinaryExpr` and `GroupingExpr`:

```cpp
struct LogicalExpr final : Expr {
    LogicalExpr(ExprPtr left, Token op, ExprPtr right);
    void print(std::ostream& out) const override;

    ExprPtr left;
    Token op;
    ExprPtr right;
};
```

- [ ] **Step 4: Implement `LogicalExpr` printing**

Edit `src/Ast.cpp` and add this implementation between `BinaryExpr::print()` and `GroupingExpr::GroupingExpr()`:

```cpp
LogicalExpr::LogicalExpr(ExprPtr left, Token op, ExprPtr right)
    : left(std::move(left))
    , op(std::move(op))
    , right(std::move(right))
{
}

void LogicalExpr::print(std::ostream& out) const
{
    out << '(' << op.lexeme << ' ';
    writeExpr(out, left);
    out << ' ';
    writeExpr(out, right);
    out << ')';
}
```

- [ ] **Step 5: Declare parser logical precedence methods**

Edit `include/Parser.hpp` so the expression grammar declarations become:

```cpp
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr primary();
```

- [ ] **Step 6: Parse logical precedence levels**

Edit `src/Parser.cpp` so `Parser::assignment()` starts from `logicalOr()` instead of `equality()`:

```cpp
ExprPtr Parser::assignment()
{
    ExprPtr expr = logicalOr();

    if (match(TokenType::Equal)) {
        Token equals = previous();
        ExprPtr value = assignment();

        if (const auto* variable = dynamic_cast<const VariableExpr*>(expr.get())) {
            return std::make_unique<AssignExpr>(variable->name, std::move(value));
        }

        throw ParseError(equals, "invalid assignment target");
    }

    return expr;
}
```

Add these methods between `assignment()` and `equality()`:

```cpp
ExprPtr Parser::logicalOr()
{
    ExprPtr expr = logicalAnd();
    while (match(TokenType::PipePipe)) {
        Token op = previous();
        ExprPtr right = logicalAnd();
        expr = std::make_unique<LogicalExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
}

ExprPtr Parser::logicalAnd()
{
    ExprPtr expr = equality();
    while (match(TokenType::AmpersandAmpersand)) {
        Token op = previous();
        ExprPtr right = equality();
        expr = std::make_unique<LogicalExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
}
```

- [ ] **Step 7: Type-check `LogicalExpr`**

Edit `src/TypeChecker.cpp` and add this helper in the anonymous namespace after `compatible()`:

```cpp
StaticType logicalResultType(StaticType left, StaticType right)
{
    if (!isKnown(left) || !isKnown(right)) {
        return StaticType::Unknown;
    }
    if (left == right) {
        return left;
    }
    return StaticType::Unknown;
}
```

Then edit `TypeChecker::checkExpression()` and add this branch after the `BinaryExpr` branch or immediately before it:

```cpp
    if (const auto* logical = dynamic_cast<const LogicalExpr*>(&expression)) {
        const StaticType left = checkExpression(*logical->left);
        const StaticType right = checkExpression(*logical->right);
        return logicalResultType(left, right);
    }
```

- [ ] **Step 8: Run front-end golden tests and verify partial GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: the new `logical_precedence default(ast)` and parse-error fixtures pass. `logical_precedence --run` still fails with an IR compile error such as `IR compile error: unsupported expression node`, because IR lowering for `LogicalExpr` is not implemented yet.

- [ ] **Step 9: Commit front-end implementation**

Run:

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp src/TypeChecker.cpp
git commit -m "feat: parse logical expressions"
```

Expected: commit succeeds with lexer, AST, parser, and type-checker changes.

## Task 3: Add RED Runtime and IR Golden Tests

**Files:**
- Create: `tests/golden/logical_short_circuit_or/input.cd`
- Create: `tests/golden/logical_short_circuit_or/run.out`
- Create: `tests/golden/logical_short_circuit_and/input.cd`
- Create: `tests/golden/logical_short_circuit_and/run.out`
- Create: `tests/golden/logical_values/input.cd`
- Create: `tests/golden/logical_values/run.out`
- Create: `tests/golden/logical_assignment_side_effects/input.cd`
- Create: `tests/golden/logical_assignment_side_effects/run.out`
- Create: `tests/golden/logical_ir/input.cd`
- Create: `tests/golden/logical_ir/ir.out`

- [ ] **Step 1: Create `||` short-circuit runtime fixture**

Run:

```bash
mkdir -p tests/golden/logical_short_circuit_or
cat > tests/golden/logical_short_circuit_or/input.cd <<'CASE'
print true || (1 / 0);
CASE
cat > tests/golden/logical_short_circuit_or/run.out <<'CASE'
true
CASE
```

- [ ] **Step 2: Create `&&` short-circuit runtime fixture**

Run:

```bash
mkdir -p tests/golden/logical_short_circuit_and
cat > tests/golden/logical_short_circuit_and/input.cd <<'CASE'
print false && (1 / 0);
CASE
cat > tests/golden/logical_short_circuit_and/run.out <<'CASE'
false
CASE
```

- [ ] **Step 3: Create operand-value runtime fixture**

Run:

```bash
mkdir -p tests/golden/logical_values
cat > tests/golden/logical_values/input.cd <<'CASE'
print "fallback" || "x";
print nil || "x";
print "x" && 42;
CASE
cat > tests/golden/logical_values/run.out <<'CASE'
fallback
x
42
CASE
```

- [ ] **Step 4: Create assignment side-effect runtime fixture**

Run:

```bash
mkdir -p tests/golden/logical_assignment_side_effects
cat > tests/golden/logical_assignment_side_effects/input.cd <<'CASE'
let x = "start";
true || (x = "or-right");
print x;
false && (x = "and-right");
print x;
false || (x = "or-ran");
print x;
true && (x = "and-ran");
print x;
CASE
cat > tests/golden/logical_assignment_side_effects/run.out <<'CASE'
start
start
or-ran
and-ran
CASE
```

- [ ] **Step 5: Create IR fixture for `copy`, `jump_if_true`, and `jump_if_false`**

Run:

```bash
mkdir -p tests/golden/logical_ir
cat > tests/golden/logical_ir/input.cd <<'CASE'
print true || false;
print true && false;
CASE
cat > tests/golden/logical_ir/ir.out <<'CASE'
IR
0000  v0 = constant #0 true
0001  v1 = copy v0
0002  jump_if_true v1, 0005
0003  v2 = constant #1 false
0004  v1 = copy v2
0005  print v1
0006  v3 = constant #2 true
0007  v4 = copy v3
0008  jump_if_false v4, 0011
0009  v5 = constant #3 false
0010  v4 = copy v5
0011  print v4
CASE
```

- [ ] **Step 6: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL. The new runtime and IR fixtures should fail because `IRCompiler::compileExpression()` does not lower `LogicalExpr` yet.

- [ ] **Step 7: Commit red runtime and IR fixtures**

Run:

```bash
git add tests/golden/logical_short_circuit_or tests/golden/logical_short_circuit_and tests/golden/logical_values tests/golden/logical_assignment_side_effects tests/golden/logical_ir
git commit -m "test: add logical operator runtime goldens"
```

Expected: commit succeeds with only golden fixture files.

## Task 4: Implement IR Operations and Logical Lowering

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `src/IRInterpreter.cpp`

- [ ] **Step 1: Extend IR declarations**

Edit `include/IR.hpp` so `IROp` includes `Copy` after `Constant` and `JumpIfTrue` after `JumpIfFalse`:

```cpp
enum class IROp {
    Constant,
    Copy,
    LoadVar,
    StoreVar,
    AssignVar,
    Print,
    Negate,
    Not,
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Jump,
    JumpIfFalse,
    JumpIfTrue,
};
```

Add these methods to `IRProgram` after `emitConstant(Value value)`:

```cpp
    IRRegister emitCopy(IRRegister value);
    void emitCopyTo(IRRegister dest, IRRegister value);
```

Add this method after `emitJumpIfFalse(IRRegister condition)`:

```cpp
    std::size_t emitJumpIfTrue(IRRegister condition);
```

- [ ] **Step 2: Implement IR construction and printing**

Edit `src/IR.cpp`.

Update `isUnary()` so `Copy` is not treated as unary:

```cpp
bool isUnary(IROp op)
{
    return op == IROp::Negate || op == IROp::Not;
}
```

Update `isBinary()` non-binary cases to include `Copy` and `JumpIfTrue`:

```cpp
    case IROp::Constant:
    case IROp::Copy:
    case IROp::LoadVar:
    case IROp::StoreVar:
    case IROp::AssignVar:
    case IROp::Print:
    case IROp::Negate:
    case IROp::Not:
    case IROp::Jump:
    case IROp::JumpIfFalse:
    case IROp::JumpIfTrue:
        return false;
```

Add implementations after `IRProgram::emitConstant()`:

```cpp
IRRegister IRProgram::emitCopy(IRRegister value)
{
    IRRegister dest = makeRegister();
    emitCopyTo(dest, value);
    return dest;
}

void IRProgram::emitCopyTo(IRRegister dest, IRRegister value)
{
    emit(IRInstruction{IROp::Copy, dest, value, std::nullopt, 0});
}
```

Add implementation after `IRProgram::emitJumpIfFalse()`:

```cpp
std::size_t IRProgram::emitJumpIfTrue(IRRegister condition)
{
    const std::size_t instruction = instructions_.size();
    emit(IRInstruction{IROp::JumpIfTrue, std::nullopt, condition, std::nullopt, 0});
    return instruction;
}
```

Update `IRProgram::patchJump()` condition:

```cpp
    if (instruction.op != IROp::Jump && instruction.op != IROp::JumpIfFalse
        && instruction.op != IROp::JumpIfTrue) {
        throw std::logic_error("cannot patch non-jump instruction");
    }
```

Update `IRProgram::print()` by adding a `Copy` print branch after `Constant`:

```cpp
        if (instruction.op == IROp::Constant) {
            printConstantOperand(out, *this, instruction.operand);
        } else if (instruction.op == IROp::Copy) {
            if (instruction.left) {
                out << " " << *instruction.left;
            }
        } else if (instruction.op == IROp::LoadVar) {
```

Update the jump printing branch to include `JumpIfTrue`:

```cpp
        } else if (instruction.op == IROp::JumpIfFalse || instruction.op == IROp::JumpIfTrue) {
            if (instruction.left) {
                out << " " << *instruction.left << ", ";
            } else {
                out << " ";
            }
            out << std::setw(4) << std::setfill('0') << instruction.operand << std::setfill(' ');
        }
```

Update `irOpName()` with:

```cpp
    case IROp::Copy:
        return "copy";
```

and:

```cpp
    case IROp::JumpIfTrue:
        return "jump_if_true";
```

- [ ] **Step 3: Declare logical lowering helper**

Edit `include/IRCompiler.hpp` and add this private method after `emitBinary(...)`:

```cpp
    IRRegister emitLogical(const LogicalExpr& expression);
```

- [ ] **Step 4: Lower `LogicalExpr` in the IR compiler**

Edit `src/IRCompiler.cpp` and add this branch in `IRCompiler::compileExpression()` after the grouping/unary/binary branches begin to keep it close to expression handling. Use this exact branch before the final unsupported-expression throw:

```cpp
    if (const auto* logical = dynamic_cast<const LogicalExpr*>(&expression)) {
        return emitLogical(*logical);
    }
```

Add this method after `IRCompiler::emitBinary()`:

```cpp
IRRegister IRCompiler::emitLogical(const LogicalExpr& expression)
{
    const IRRegister left = compileExpression(*expression.left);
    const IRRegister result = ir_.emitCopy(left);

    std::size_t jump = 0;
    switch (expression.op.type) {
    case TokenType::PipePipe:
        jump = ir_.emitJumpIfTrue(result);
        break;
    case TokenType::AmpersandAmpersand:
        jump = ir_.emitJumpIfFalse(result);
        break;
    default:
        throw IRCompileError("unsupported logical operator: " + tokenTypeName(expression.op.type));
    }

    const IRRegister right = compileExpression(*expression.right);
    ir_.emitCopyTo(result, right);
    ir_.patchJump(jump);
    return result;
}
```

- [ ] **Step 5: Execute new IR operations**

Edit `src/IRInterpreter.cpp`.

Add this case after `IROp::Constant`:

```cpp
        case IROp::Copy:
            writeRegister(readDest(instruction), readRegister(readLeft(instruction)));
            break;
```

Add this case after `IROp::JumpIfFalse`:

```cpp
        case IROp::JumpIfTrue:
            validateJumpTarget(instruction.operand, instructions.size());
            if (isTruthy(readRegister(readLeft(instruction)))) {
                ip = instruction.operand;
                continue;
            }
            break;
```

- [ ] **Step 6: Run golden tests and verify GREEN for logical behavior**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass, including these new checks:

```text
logical_precedence default(ast)
logical_precedence --run
logical_short_circuit_or --run
logical_short_circuit_and --run
logical_values --run
logical_assignment_side_effects --run
logical_ir --ir
parse_errors/logical_and_missing_rhs default(ast)
parse_errors/logical_or_missing_rhs default(ast)
```

If `logical_ir --ir` differs only because register numbers or jump targets are different, inspect the actual IR. Keep the implementation if it preserves the planned short-circuit shape, then update `tests/golden/logical_ir/ir.out` with:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

After updating, inspect:

```bash
git diff -- tests/golden/logical_ir/ir.out
```

- [ ] **Step 7: Commit IR implementation**

Run:

```bash
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp src/IRInterpreter.cpp tests/golden/logical_ir/ir.out
git commit -m "feat: lower logical operators to IR"
```

Expected: commit succeeds with IR and interpreter changes plus any reviewed IR golden refresh.

## Task 5: Update Documentation

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update grammar document**

Edit `docs/language-grammar.ebnf` so the expression section reads:

```ebnf
expression  = assignment ;

assignment  = identifier, "=", assignment
            | logicalOr ;

logicalOr   = logicalAnd,
              { "||", logicalAnd } ;

logicalAnd  = equality,
              { "&&", equality } ;

equality    = comparison,
              { ( "==" | "!=" ), comparison } ;
```

Keep the existing `comparison`, `term`, `factor`, `unary`, and `primary` productions unchanged.

- [ ] **Step 2: Update README language section**

Edit `README.md` supported expressions list so it includes logical operators after assignment and before grouping:

```markdown
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
- Logical operators: `left || right` and `left && right` short-circuit using the same truthiness rules as `if` and `!`. They return the selected operand value rather than forcing a boolean.
- Grouping: `(expression)`
```

Edit the binary-operators bullet so it remains only ordinary non-short-circuit binary operators:

```markdown
- Binary operators: `*`, `/`, `+`, `-`, `<`, `<=`, `>`, `>=`, `==`, `!=`
```

- [ ] **Step 3: Update roadmap status**

Edit `docs/roadmap.md` Phase 3 section from:

```markdown
## Phase 3: Logical Operators

Goal: add common boolean/control-flow expression operators.
```

to:

```markdown
## Phase 3: Logical Operators — Implemented

Status: implemented. The language supports `&&` and `||` short-circuit expressions using existing truthiness rules and returning the selected operand value.
```

Then replace the old suggested syntax block:

```text
a and b
a or b
```

with:

```text
a && b
a || b
```

- [ ] **Step 4: Run documentation diff review**

Run:

```bash
git diff -- docs/language-grammar.ebnf README.md docs/roadmap.md
```

Expected: docs describe implemented `&&` and `||` behavior only, with no keyword aliases and no single-character operators.

- [ ] **Step 5: Commit documentation**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md
git commit -m "docs: document logical operators"
```

Expected: commit succeeds with documentation changes only.

## Task 6: Full Verification and Cleanup

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
Implemented `&&` and `||` with short-circuit truthiness semantics and operand-value results. Added `LogicalExpr`, parser precedence, type-checker support, IR `copy`/`jump_if_true`, interpreter execution, golden coverage, and docs.
```
