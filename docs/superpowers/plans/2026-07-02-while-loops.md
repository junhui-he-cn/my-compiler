# While Loops Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add block-bodied `while` loops that repeatedly execute while a truthy condition holds.

**Architecture:** Extend the existing vertical compiler pipeline: lexer keyword, parser statement, `WhileStmt` AST node, type-checker traversal, IR lowering with existing `jump_if_false` plus a new `emitJumpTo(target)` helper for backward jumps, and golden/documentation updates. The IR interpreter already supports arbitrary valid jump targets, so no new runtime opcode is needed.

**Tech Stack:** C++17, CMake, recursive-descent parser, AST side-table name resolution in `TypeChecker`, register IR, IR interpreter, Python golden tests, CTest.

---

## File Structure

- Modify: `include/Token.hpp` — add `TokenType::While`.
- Modify: `src/Lexer.cpp` — recognize `while` as a keyword and print token name `While`.
- Modify: `include/Ast.hpp` — add `WhileStmt`.
- Modify: `src/Ast.cpp` — implement `WhileStmt` constructor and tree printer.
- Modify: `include/Parser.hpp` — declare `whileStatement()`.
- Modify: `src/Parser.cpp` — parse `while expression { ... }` as a statement.
- Modify: `src/TypeChecker.cpp` — check while condition and body.
- Modify: `include/IR.hpp` — add `IRProgram::emitJumpTo(std::size_t target)`.
- Modify: `src/IR.cpp` — implement known-target jump emission.
- Modify: `src/IRCompiler.cpp` — lower `WhileStmt` to condition, exit jump, body, backward jump.
- Create: success fixtures under `tests/golden/while_*`.
- Create: parse-error fixture under `tests/golden/parse_errors`.
- Create: type-error fixture under `tests/golden/type_errors`.
- Modify: `docs/language-grammar.ebnf` — document `whileStmt`.
- Modify: `README.md` — document supported while statements and truthiness.
- Modify: `docs/roadmap.md` — mark Phase 4 implemented after verification.

## Task 0: Prepare Workspace and Baseline

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
while-loops
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
golden tests: 63 passed, 0 failed
Ran 9 tests
OK
```

If baseline fails before while-loop edits, stop and report the exact failing command and output.

## Task 1: Add RED Parser and Scope Golden Fixtures

**Files:**
- Create: `tests/golden/while_zero_iterations/input.cd`
- Create: `tests/golden/while_zero_iterations/ast.out`
- Create: `tests/golden/while_zero_iterations/run.out`
- Create: `tests/golden/parse_errors/while_missing_block.cd`
- Create: `tests/golden/parse_errors/while_missing_block.err`
- Create: `tests/golden/parse_errors/while_missing_block.exit`
- Create: `tests/golden/type_errors/while_body_scope_escape.cd`
- Create: `tests/golden/type_errors/while_body_scope_escape.err`
- Create: `tests/golden/type_errors/while_body_scope_escape.exit`

- [ ] **Step 1: Write zero-iteration fixture**

Run:

```bash
mkdir -p tests/golden/while_zero_iterations
cat > tests/golden/while_zero_iterations/input.cd <<'CASE'
while false {
  print "skipped";
}
print "done";
CASE
cat > tests/golden/while_zero_iterations/ast.out <<'CASE'
Program
  While false
    Body
      Block
        Print "skipped"
  Print "done"
CASE
cat > tests/golden/while_zero_iterations/run.out <<'CASE'
done
CASE
```

- [ ] **Step 2: Write missing-block parse-error fixture**

Run:

```bash
cat > tests/golden/parse_errors/while_missing_block.cd <<'CASE'
while true print 1;
CASE
cat > tests/golden/parse_errors/while_missing_block.err <<'CASE'
Parse error at line 1, column 12: expected `{` after while condition, found Print `print`
CASE
cat > tests/golden/parse_errors/while_missing_block.exit <<'CASE'
1
CASE
```

- [ ] **Step 3: Write body-scope type-error fixture**

Run:

```bash
cat > tests/golden/type_errors/while_body_scope_escape.cd <<'CASE'
while true {
  let x = 1;
}
print x;
CASE
cat > tests/golden/type_errors/while_body_scope_escape.err <<'CASE'
Type error: undefined variable `x`
CASE
cat > tests/golden/type_errors/while_body_scope_escape.exit <<'CASE'
1
CASE
```

- [ ] **Step 4: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL. The new while fixtures should fail because `while` is still lexed as an identifier and the parser does not recognize a while statement. Failures should include parse errors or unexpected output for the new `while_*` and `while_missing_block` cases.

- [ ] **Step 5: Commit red fixtures**

Run:

```bash
git add tests/golden/while_zero_iterations tests/golden/parse_errors/while_missing_block.* tests/golden/type_errors/while_body_scope_escape.*
git commit -m "test: add while loop parser goldens"
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

- [ ] **Step 1: Add `While` token kind**

Edit `include/Token.hpp` and add `While` after `Print` in the keyword section:

```cpp
    If,
    Else,
    Let,
    Print,
    While,
    True,
    False,
    Nil,
```

- [ ] **Step 2: Recognize and print `while` tokens**

Edit `src/Lexer.cpp` keyword map in `Lexer::identifier()` and add:

```cpp
        {"while", TokenType::While},
```

Place it after the `print` keyword entry.

Then edit `tokenTypeName()` and add this case after `Print`:

```cpp
    case TokenType::While:
        return "While";
```

- [ ] **Step 3: Add `WhileStmt` declaration**

Edit `include/Ast.hpp` and add this node after `IfStmt`:

```cpp
struct WhileStmt final : Stmt {
    WhileStmt(ExprPtr condition, StmtPtr body);
    void print(std::ostream& out, int indent) const override;

    ExprPtr condition;
    StmtPtr body;
};
```

- [ ] **Step 4: Implement `WhileStmt` printing**

Edit `src/Ast.cpp` and add this implementation after `IfStmt::print()`:

```cpp
WhileStmt::WhileStmt(ExprPtr condition, StmtPtr body)
    : condition(std::move(condition))
    , body(std::move(body))
{
}

void WhileStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "While ";
    writeExpr(out, condition);
    out << '\n';

    writeIndent(out, indent + 1);
    out << "Body\n";
    if (body) {
        body->print(out, indent + 2);
    }
}
```

- [ ] **Step 5: Declare and dispatch parser method**

Edit `include/Parser.hpp` and add this private method after `ifStatement()`:

```cpp
    StmtPtr whileStatement();
```

Edit `src/Parser.cpp` in `Parser::statement()` and add this branch after the `If` branch:

```cpp
    if (match(TokenType::While)) {
        return whileStatement();
    }
```

- [ ] **Step 6: Parse while statements**

Edit `src/Parser.cpp` and add this method after `Parser::ifStatement()`:

```cpp
StmtPtr Parser::whileStatement()
{
    ExprPtr condition = expression();
    consume(TokenType::LeftBrace, "expected `{` after while condition");
    StmtPtr body = blockStatement();
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
}
```

- [ ] **Step 7: Type-check while statements**

Edit `src/TypeChecker.cpp` in `TypeChecker::checkStatement()` and add this branch after the `IfStmt` branch:

```cpp
    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        checkExpression(*whileStmt->condition);
        checkStatement(*whileStmt->body);
        return;
    }
```

- [ ] **Step 8: Run golden tests and verify partial GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `while_zero_iterations default(ast)`, `parse_errors/while_missing_block default(ast)`, and `type_errors/while_body_scope_escape default(ast)` pass. `while_zero_iterations --run` still fails with an IR compile error such as:

```text
IR compile error: unsupported statement node
```

because `IRCompiler::compileStatement()` does not lower `WhileStmt` yet.

- [ ] **Step 9: Commit front-end implementation**

Run:

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp src/TypeChecker.cpp
git commit -m "feat: parse while statements"
```

Expected: commit succeeds with lexer, AST, parser, and type-checker changes.

## Task 3: Add RED Runtime and IR Golden Fixtures

**Files:**
- Create: `tests/golden/while_counting/input.cd`
- Create: `tests/golden/while_counting/ast.out`
- Create: `tests/golden/while_counting/run.out`
- Create: `tests/golden/while_scope_shadow/input.cd`
- Create: `tests/golden/while_scope_shadow/run.out`
- Create: `tests/golden/while_nested_if/input.cd`
- Create: `tests/golden/while_nested_if/run.out`
- Create: `tests/golden/while_ir/input.cd`
- Create: `tests/golden/while_ir/ir.out`

- [ ] **Step 1: Create counting-loop fixture**

Run:

```bash
mkdir -p tests/golden/while_counting
cat > tests/golden/while_counting/input.cd <<'CASE'
let i: number = 0;
while i < 3 {
  print i;
  i = i + 1;
}
CASE
cat > tests/golden/while_counting/ast.out <<'CASE'
Program
  Let i: number = 0
  While (< i 3)
    Body
      Block
        Print i
        Expr (= i (+ i 1))
CASE
cat > tests/golden/while_counting/run.out <<'CASE'
0
1
2
CASE
```

- [ ] **Step 2: Create scope-shadow fixture**

Run:

```bash
mkdir -p tests/golden/while_scope_shadow
cat > tests/golden/while_scope_shadow/input.cd <<'CASE'
let x = "outer";
let i = 0;
while i < 1 {
  let x = "inner";
  print x;
  i = i + 1;
}
print x;
CASE
cat > tests/golden/while_scope_shadow/run.out <<'CASE'
inner
outer
CASE
```

- [ ] **Step 3: Create nested-control-flow fixture**

Run:

```bash
mkdir -p tests/golden/while_nested_if
cat > tests/golden/while_nested_if/input.cd <<'CASE'
let i = 0;
while i < 3 {
  if i == 1 || i == 2 {
    print i;
  }
  i = i + 1;
}
CASE
cat > tests/golden/while_nested_if/run.out <<'CASE'
1
2
CASE
```

- [ ] **Step 4: Create IR fixture with backward jump**

Run:

```bash
mkdir -p tests/golden/while_ir
cat > tests/golden/while_ir/input.cd <<'CASE'
let i = 0;
while i < 2 {
  print i;
  i = i + 1;
}
CASE
cat > tests/golden/while_ir/ir.out <<'CASE'
IR
0000  v0 = constant #0 0
0001  store_var @0 i#0, v0
0002  v1 = load_var @1 i#0
0003  v2 = constant #1 2
0004  v3 = less v1, v2
0005  jump_if_false v3, 0013
0006  v4 = load_var @2 i#0
0007  print v4
0008  v5 = load_var @3 i#0
0009  v6 = constant #2 1
0010  v7 = add v5, v6
0011  assign_var @4 i#0, v7
0012  jump 0002
CASE
```

- [ ] **Step 5: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL. New runtime and IR fixtures should fail because `IRCompiler::compileStatement()` does not lower `WhileStmt` yet.

- [ ] **Step 6: Commit red runtime and IR fixtures**

Run:

```bash
git add tests/golden/while_counting tests/golden/while_scope_shadow tests/golden/while_nested_if tests/golden/while_ir
git commit -m "test: add while loop runtime goldens"
```

Expected: commit succeeds with only golden fixture files.

## Task 4: Implement IR Backward Jump and While Lowering

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Add known-target jump API**

Edit `include/IR.hpp` and add this method after `emitJump()`:

```cpp
    void emitJumpTo(std::size_t target);
```

- [ ] **Step 2: Implement known-target jump emission**

Edit `src/IR.cpp` and add this method after `IRProgram::emitJump()`:

```cpp
void IRProgram::emitJumpTo(std::size_t target)
{
    emit(IRInstruction{IROp::Jump, std::nullopt, std::nullopt, std::nullopt, target});
}
```

- [ ] **Step 3: Lower `WhileStmt`**

Edit `src/IRCompiler.cpp` in `IRCompiler::compileStatement()` and add this branch after the `IfStmt` branch:

```cpp
    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        const std::size_t loopStart = ir_.instructionCount();
        const IRRegister condition = compileExpression(*whileStmt->condition);
        const std::size_t exitJump = ir_.emitJumpIfFalse(condition);
        compileStatement(*whileStmt->body);
        ir_.emitJumpTo(loopStart);
        ir_.patchJump(exitJump);
        return;
    }
```

- [ ] **Step 4: Run golden tests and verify GREEN for while behavior**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass, including these new checks:

```text
while_zero_iterations default(ast)
while_zero_iterations --run
while_counting default(ast)
while_counting --run
while_scope_shadow --run
while_nested_if --run
while_ir --ir
parse_errors/while_missing_block default(ast)
type_errors/while_body_scope_escape default(ast)
```

If `while_ir --ir` differs only because register numbers, name indexes, or jump targets are different, inspect the actual IR. Keep the implementation if it preserves the planned loop shape and backward `jump`, then update `tests/golden/while_ir/ir.out` with:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
git diff -- tests/golden/while_ir/ir.out
```

- [ ] **Step 5: Commit IR implementation**

Run:

```bash
git add include/IR.hpp src/IR.cpp src/IRCompiler.cpp tests/golden/while_ir/ir.out
git commit -m "feat: lower while loops to IR"
```

Expected: commit succeeds with IR changes plus any reviewed IR golden refresh.

## Task 5: Update Documentation

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update grammar document**

Edit `docs/language-grammar.ebnf` so the statement section includes `whileStmt`:

```ebnf
statement   = printStmt
            | ifStmt
            | whileStmt
            | block
            | exprStmt ;
```

Add this production after `ifStmt`:

```ebnf
whileStmt   = "while", expression, block ;
```

- [ ] **Step 2: Update README supported statements**

Edit `README.md` supported statements block so it includes while:

```text
let name = expression;
let name: type = expression;
print expression;
if expression { declaration* } [else { declaration* }]
while expression { declaration* }
{ declaration* }
expression;
```

Add this paragraph after the existing block-scope/type-annotation paragraph:

```markdown
`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. `break` and `continue` are not implemented yet.
```

- [ ] **Step 3: Update roadmap status**

Edit `docs/roadmap.md` Phase 4 heading and first lines from:

```markdown
## Phase 4: While Loops

Goal: support basic repeated control flow.
```

to:

```markdown
## Phase 4: While Loops — Implemented

Status: implemented. The language supports block-bodied `while` loops with truthy conditions. `break` and `continue` are not implemented yet.

Goal: support basic repeated control flow.
```

- [ ] **Step 4: Review documentation diff**

Run:

```bash
git diff -- docs/language-grammar.ebnf README.md docs/roadmap.md
```

Expected: docs describe implemented block-bodied `while` behavior only, with truthiness and no `break`/`continue`.

- [ ] **Step 5: Commit documentation**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md
git commit -m "docs: document while loops"
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
Implemented block-bodied `while` loops with truthy conditions. Added `WhileStmt`, parser/type-checker support, IR backward jump lowering, golden coverage, and docs.
```
