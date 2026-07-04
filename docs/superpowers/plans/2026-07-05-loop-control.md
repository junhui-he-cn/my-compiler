# Loop Control Statements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `break;` and `continue;` statements for existing `while` loops, with type-checking and IR/bytecode/Rust VM parity.

**Architecture:** Add `BreakStmt` and `ContinueStmt` AST nodes parsed from new reserved keywords. Type checking tracks loop depth and resets it for function bodies. IR lowering converts loop control to existing `Jump` instructions, so bytecode and `.cdbc` formats need no new opcodes.

**Tech Stack:** C++17 lexer/parser/AST/type checker/IR compiler, existing IR interpreter, existing bytecode emitter/compiler, Rust VM `.cdbc` parity runner, Python golden tests.

---

## File Structure

- Modify `include/Token.hpp`: add `Break` and `Continue` token kinds.
- Modify `src/Lexer.cpp`: reserve `break` and `continue`; print token names.
- Modify `include/Ast.hpp`: add `BreakStmt` and `ContinueStmt` statement nodes.
- Modify `src/Ast.cpp`: implement constructors and AST printing for `(break)` / `(continue)`.
- Modify `include/Parser.hpp`: declare `breakStatement()` and `continueStatement()`.
- Modify `src/Parser.cpp`: parse `break;` and `continue;` as statements.
- Modify `include/TypeChecker.hpp`: add loop-depth state.
- Modify `src/TypeChecker.cpp`: reject loop control outside loops and reset loop depth inside function bodies.
- Modify `include/IRCompiler.hpp`: add loop-context state and helpers.
- Modify `src/IRCompiler.cpp`: lower `break;` / `continue;` to jumps.
- Modify `tests/run_rust_vm_tests.py`: add at least one loop-control golden to Rust VM parity allowlist.
- Add golden fixtures under `tests/golden/` and `tests/golden/type_errors/`.
- Modify `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, and `AGENTS.md` after behavior is implemented.

---

### Task 1: Lexer, parser, and AST support

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Test fixtures: `tests/golden/loop_break/input.cd`, `tests/golden/loop_break/ast.out`

- [ ] **Step 1: Write a failing parser/AST golden fixture**

Create `tests/golden/loop_break/input.cd`:

```cd
let i = 0;
while true {
  break;
}
print i;
```

Create `tests/golden/loop_break/ast.out` with the desired AST output:

```text
Let i = 0
While true
  Block
    Break
Print i
```

- [ ] **Step 2: Run the golden test and verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case loop_break
```

Expected: FAIL. The parser should currently reject `break` because it is still an identifier/expression context, with a parse error near `;` or missing expected expression syntax.

- [ ] **Step 3: Add token kinds**

In `include/Token.hpp`, add `Break` and `Continue` among statement keywords:

```cpp
    Break,
    Continue,
    If,
    Else,
    Fun,
    Let,
    Print,
    Return,
    While,
```

- [ ] **Step 4: Reserve lexer keywords and token names**

In `src/Lexer.cpp`, add keyword mappings inside `Lexer::identifier()`:

```cpp
        {"break", TokenType::Break},
        {"continue", TokenType::Continue},
```

In `tokenTypeName(TokenType type)`, add cases near the other keywords:

```cpp
    case TokenType::Break:
        return "Break";
    case TokenType::Continue:
        return "Continue";
```

- [ ] **Step 5: Add AST statement structs**

In `include/Ast.hpp`, after `WhileStmt` and before `FunctionStmt`, add:

```cpp
struct BreakStmt final : Stmt {
    explicit BreakStmt(Token keyword);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
};

struct ContinueStmt final : Stmt {
    explicit ContinueStmt(Token keyword);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
};
```

- [ ] **Step 6: Print inline loop-control AST nodes**

In `src/Ast.cpp`, inside `writeInlineStmt()` after the `WhileStmt` branch and before `FunctionStmt`, add:

```cpp
    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        out << "(break)";
        return;
    }

    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        out << "(continue)";
        return;
    }
```

- [ ] **Step 7: Implement BreakStmt and ContinueStmt constructors/printing**

In `src/Ast.cpp`, after `WhileStmt::print(...)` and before `FunctionStmt::FunctionStmt(...)`, add:

```cpp
BreakStmt::BreakStmt(Token keyword)
    : keyword(std::move(keyword))
{
}

void BreakStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Break\n";
}

ContinueStmt::ContinueStmt(Token keyword)
    : keyword(std::move(keyword))
{
}

void ContinueStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Continue\n";
}
```

Use `std::move`; `src/Ast.cpp` already includes `<utility>`.

- [ ] **Step 8: Declare parser methods**

In `include/Parser.hpp`, after `whileStatement()` add:

```cpp
    StmtPtr breakStatement();
    StmtPtr continueStatement();
```

- [ ] **Step 9: Parse loop-control statements**

In `src/Parser.cpp`, update `Parser::statement()` after the `while` branch and before `return`:

```cpp
    if (match(TokenType::Break)) {
        return breakStatement();
    }
    if (match(TokenType::Continue)) {
        return continueStatement();
    }
```

Add method implementations after `whileStatement()`:

```cpp
StmtPtr Parser::breakStatement()
{
    Token keyword = previous();
    consume(TokenType::Semicolon, "expected `;` after break");
    return std::make_unique<BreakStmt>(std::move(keyword));
}

StmtPtr Parser::continueStatement()
{
    Token keyword = previous();
    consume(TokenType::Semicolon, "expected `;` after continue");
    return std::make_unique<ContinueStmt>(std::move(keyword));
}
```

`src/Parser.cpp` already includes `<utility>` transitively is not guaranteed; if the compiler complains about `std::move`, add `#include <utility>`.

- [ ] **Step 10: Run the AST golden and verify GREEN for parsing**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case loop_break
```

Expected: still may fail in `--run` only if `run.out` exists. With only `ast.out`, it should pass once AST output matches:

```text
Let i = 0
While true
  Block
    Break
Print i
```

- [ ] **Step 11: Commit parser/AST slice**

Run:

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/loop_break/input.cd tests/golden/loop_break/ast.out
git commit -m "feat: parse loop control statements"
```

---

### Task 2: Type-check loop-control placement

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test fixtures: `tests/golden/type_errors/break_outside_loop.cd`, `.err`, `.exit`
- Test fixtures: `tests/golden/type_errors/continue_outside_loop.cd`, `.err`, `.exit`
- Test fixtures: `tests/golden/type_errors/break_inside_function_in_loop.cd`, `.err`, `.exit`

- [ ] **Step 1: Write failing type-error fixtures**

Create `tests/golden/type_errors/break_outside_loop.cd`:

```cd
break;
```

Create `tests/golden/type_errors/break_outside_loop.err`:

```text
Type error at 1:1: `break` can only be used inside a loop
```

Create `tests/golden/type_errors/break_outside_loop.exit`:

```text
1
```

Create `tests/golden/type_errors/continue_outside_loop.cd`:

```cd
continue;
```

Create `tests/golden/type_errors/continue_outside_loop.err`:

```text
Type error at 1:1: `continue` can only be used inside a loop
```

Create `tests/golden/type_errors/continue_outside_loop.exit`:

```text
1
```

Create `tests/golden/type_errors/break_inside_function_in_loop.cd`:

```cd
while true {
  fun f() {
    break;
  }
}
```

Create `tests/golden/type_errors/break_inside_function_in_loop.err`:

```text
Type error at 3:5: `break` can only be used inside a loop
```

Create `tests/golden/type_errors/break_inside_function_in_loop.exit`:

```text
1
```

- [ ] **Step 2: Run type-error fixtures and verify RED**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case break_outside_loop --case continue_outside_loop --case break_inside_function_in_loop
```

Expected: FAIL. Before type-check support, `break;` and `continue;` parse but TypeChecker should report `unsupported statement node` or a later compile error rather than the desired messages.

- [ ] **Step 3: Add loop-depth state**

In `include/TypeChecker.hpp`, add a private member after `functionDepth_`:

```cpp
    std::size_t loopDepth_ = 0;
```

- [ ] **Step 4: Reset loop depth at program start**

In `src/TypeChecker.cpp`, inside `TypeChecker::check(...)`, after resetting `functionDepth_ = 0;`, add:

```cpp
    loopDepth_ = 0;
```

- [ ] **Step 5: Check break and continue statements**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkStatement(...)`, after the `ReturnStmt` branch and before the `BlockStmt` branch, add:

```cpp
    if (const auto* breakStmt = dynamic_cast<const BreakStmt*>(&statement)) {
        if (loopDepth_ == 0) {
            throw TypeError(breakStmt->keyword, "`break` can only be used inside a loop");
        }
        return;
    }

    if (const auto* continueStmt = dynamic_cast<const ContinueStmt*>(&statement)) {
        if (loopDepth_ == 0) {
            throw TypeError(continueStmt->keyword, "`continue` can only be used inside a loop");
        }
        return;
    }
```

- [ ] **Step 6: Increment loop depth for while bodies**

Replace the current `WhileStmt` branch in `TypeChecker::checkStatement(...)`:

```cpp
    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        checkExpression(*whileStmt->condition);
        checkStatement(*whileStmt->body);
        return;
    }
```

with:

```cpp
    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        checkExpression(*whileStmt->condition);
        ++loopDepth_;
        checkStatement(*whileStmt->body);
        --loopDepth_;
        return;
    }
```

- [ ] **Step 7: Reset loop depth while checking function statements**

In `TypeChecker::checkFunction(const FunctionStmt& statement)`, after `++functionDepth_;`, add save/reset logic:

```cpp
    const std::size_t enclosingLoopDepth = loopDepth_;
    loopDepth_ = 0;
```

After `const StaticType returnType = checkFunctionBody(statement.body);`, before `--functionDepth_;`, restore:

```cpp
    loopDepth_ = enclosingLoopDepth;
```

The function body section should look like:

```cpp
    beginScope();
    ++functionDepth_;
    const std::size_t enclosingLoopDepth = loopDepth_;
    loopDepth_ = 0;

    std::vector<std::string> parameterNames;
    for (const Token& parameter : statement.parameters) {
        Binding parameterBinding = declareVariable(parameter, StaticType::Unknown);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordParameters(statement, std::move(parameterNames));

    const StaticType returnType = checkFunctionBody(statement.body);

    loopDepth_ = enclosingLoopDepth;
    --functionDepth_;
    endScope();
```

- [ ] **Step 8: Reset loop depth while checking function expressions**

In `TypeChecker::checkFunctionExpression(const FunctionExpr& expression)`, apply the same save/reset/restore pattern after `++functionDepth_;` and before `--functionDepth_;`:

```cpp
    const std::size_t enclosingLoopDepth = loopDepth_;
    loopDepth_ = 0;
```

and:

```cpp
    loopDepth_ = enclosingLoopDepth;
```

- [ ] **Step 9: Run type-error fixtures and verify GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case break_outside_loop --case continue_outside_loop --case break_inside_function_in_loop
```

Expected: all selected fixtures pass and diagnostics match the `.err` files.

- [ ] **Step 10: Commit type-checking slice**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/break_outside_loop.cd tests/golden/type_errors/break_outside_loop.err tests/golden/type_errors/break_outside_loop.exit tests/golden/type_errors/continue_outside_loop.cd tests/golden/type_errors/continue_outside_loop.err tests/golden/type_errors/continue_outside_loop.exit tests/golden/type_errors/break_inside_function_in_loop.cd tests/golden/type_errors/break_inside_function_in_loop.err tests/golden/type_errors/break_inside_function_in_loop.exit
git commit -m "feat: type check loop control placement"
```

---

### Task 3: IR lowering for break and continue

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Test fixtures: `tests/golden/loop_break/run.out`, `tests/golden/loop_break/ir.out`, `tests/golden/loop_break/bytecode.out`
- Test fixtures: `tests/golden/loop_continue/input.cd`, `run.out`
- Test fixtures: `tests/golden/loop_control_nested/input.cd`, `run.out`

- [ ] **Step 1: Add failing runtime fixtures**

Update `tests/golden/loop_break/input.cd` to exercise an actual early exit:

```cd
let i = 0;
while true {
  i = i + 1;
  if i == 3 {
    break;
  }
}
print i;
```

Update `tests/golden/loop_break/ast.out` to:

```text
Let i = 0
While true
  Block
    Expr (= i (+ i 1))
    If (== i 3)
      Block
        Break
Print i
```

Create `tests/golden/loop_break/run.out`:

```text
3
```

Create `tests/golden/loop_continue/input.cd`:

```cd
let i = 0;
while i < 5 {
  i = i + 1;
  if i == 3 {
    continue;
  }
  print i;
}
```

Create `tests/golden/loop_continue/run.out`:

```text
1
2
4
5
```

Create `tests/golden/loop_control_nested/input.cd`:

```cd
let outer = 0;
while outer < 3 {
  outer = outer + 1;
  let inner = 0;
  while inner < 3 {
    inner = inner + 1;
    if inner == 2 {
      break;
    }
    print outer * 10 + inner;
  }
}
```

Create `tests/golden/loop_control_nested/run.out`:

```text
11
21
31
```

- [ ] **Step 2: Run selected fixtures and verify RED**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case loop_break --case loop_continue --case loop_control_nested
```

Expected: FAIL. Type checking should pass, but IR compilation should still report `unsupported statement node` for `BreakStmt` or `ContinueStmt`.

- [ ] **Step 3: Add IRCompiler loop context type and helpers**

In `include/IRCompiler.hpp`, add includes:

```cpp
#include <cstddef>
#include <vector>
```

Then add a private struct and helper declarations before `IRProgram ir_;`:

```cpp
    struct LoopContext {
        std::size_t continueTarget = 0;
        std::vector<std::size_t> breakJumps;
    };

    void compileBreak(const BreakStmt& statement);
    void compileContinue(const ContinueStmt& statement);
```

Add a member after `const ResolvedNames* resolvedNames_ = nullptr;`:

```cpp
    std::vector<LoopContext> loopContexts_;
```

- [ ] **Step 4: Reset loop contexts for each compile**

In `src/IRCompiler.cpp`, inside `IRCompiler::compile(...)`, after `resolvedNames_ = &resolvedNames;`, add:

```cpp
    loopContexts_.clear();
```

Before returning, after `resolvedNames_ = nullptr;`, add another clear for hygiene:

```cpp
    loopContexts_.clear();
```

The ending should be:

```cpp
    resolvedNames_ = nullptr;
    loopContexts_.clear();
    return std::move(ir_);
```

- [ ] **Step 5: Lower while with a loop context**

Replace the current `WhileStmt` branch in `IRCompiler::compileStatement(...)`:

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

with:

```cpp
    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        const std::size_t loopStart = ir_.instructionCount();
        const IRRegister condition = compileExpression(*whileStmt->condition);
        const std::size_t exitJump = ir_.emitJumpIfFalse(condition);

        loopContexts_.push_back(LoopContext{loopStart, {}});
        compileStatement(*whileStmt->body);
        LoopContext loop = std::move(loopContexts_.back());
        loopContexts_.pop_back();

        ir_.emitJumpTo(loopStart);
        ir_.patchJump(exitJump);
        for (const std::size_t breakJump : loop.breakJumps) {
            ir_.patchJump(breakJump);
        }
        return;
    }
```

- [ ] **Step 6: Dispatch break and continue statements**

In `IRCompiler::compileStatement(...)`, after the `ReturnStmt` branch and before the `BlockStmt` branch, add:

```cpp
    if (const auto* breakStmt = dynamic_cast<const BreakStmt*>(&statement)) {
        compileBreak(*breakStmt);
        return;
    }

    if (const auto* continueStmt = dynamic_cast<const ContinueStmt*>(&statement)) {
        compileContinue(*continueStmt);
        return;
    }
```

- [ ] **Step 7: Implement compileBreak and compileContinue**

In `src/IRCompiler.cpp`, after `compileReturn(...)`, add:

```cpp
void IRCompiler::compileBreak(const BreakStmt&)
{
    if (loopContexts_.empty()) {
        throw IRCompileError("`break` can only be used inside a loop");
    }
    loopContexts_.back().breakJumps.push_back(ir_.emitJump());
}

void IRCompiler::compileContinue(const ContinueStmt&)
{
    if (loopContexts_.empty()) {
        throw IRCompileError("`continue` can only be used inside a loop");
    }
    ir_.emitJumpTo(loopContexts_.back().continueTarget);
}
```

- [ ] **Step 8: Run runtime fixtures and inspect IR/bytecode output**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case loop_break --case loop_continue --case loop_control_nested
```

Expected: `run.out` checks pass, but `loop_break` may fail because `ir.out` and `bytecode.out` are not created yet.

Generate the missing expected files intentionally:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case loop_break --update
```

Review `tests/golden/loop_break/ir.out` and `tests/golden/loop_break/bytecode.out`. Confirm they include `jump` instructions for break and while back-edge, and no new opcodes.

- [ ] **Step 9: Run selected fixtures and verify GREEN**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case loop_break --case loop_continue --case loop_control_nested
```

Expected: selected fixtures pass.

- [ ] **Step 10: Commit IR lowering slice**

Run:

```bash
git add include/IRCompiler.hpp src/IRCompiler.cpp tests/golden/loop_break tests/golden/loop_continue tests/golden/loop_control_nested
git commit -m "feat: lower loop control to jumps"
```

---

### Task 4: Rust VM parity coverage and parse-error coverage

**Files:**
- Modify: `tests/run_rust_vm_tests.py`
- Test fixtures: `tests/golden/parse_errors/break_missing_semicolon.cd`, `.err`, `.exit`
- Test fixtures: `tests/golden/parse_errors/continue_missing_semicolon.cd`, `.err`, `.exit`

- [ ] **Step 1: Add parse-error fixtures for missing semicolons**

Create `tests/golden/parse_errors/break_missing_semicolon.cd`:

```cd
while true {
  break
}
```

Create `tests/golden/parse_errors/break_missing_semicolon.err`:

```text
Parse error at 3:1: expected `;` after break, found RightBrace
```

Create `tests/golden/parse_errors/break_missing_semicolon.exit`:

```text
1
```

Create `tests/golden/parse_errors/continue_missing_semicolon.cd`:

```cd
while true {
  continue
}
```

Create `tests/golden/parse_errors/continue_missing_semicolon.err`:

```text
Parse error at 3:1: expected `;` after continue, found RightBrace
```

Create `tests/golden/parse_errors/continue_missing_semicolon.exit`:

```text
1
```

- [ ] **Step 2: Run parse-error fixtures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case break_missing_semicolon --case continue_missing_semicolon
```

Expected: pass if parser messages match. If column numbers differ, inspect actual stderr and update only the `.err` files to the exact stable parser output.

- [ ] **Step 3: Add loop_break to Rust VM golden allowlist**

In `tests/run_rust_vm_tests.py`, inside `golden_allowlist`, add:

```python
            "loop_break",
```

Place it near other language feature fixtures, for example after `len_builtin_shadowing` or near the `bytecode_*` control-flow cases.

- [ ] **Step 4: Run Rust VM parity for loop_break**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case loop_break
```

Expected:

```text
rust vm tests: 2 passed, 0 failed
```

- [ ] **Step 5: Run all current golden and Rust VM tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
rm -rf tests/__pycache__
```

Expected: both commands pass.

- [ ] **Step 6: Commit parity and parse-error coverage**

Run:

```bash
git add tests/run_rust_vm_tests.py tests/golden/parse_errors/break_missing_semicolon.cd tests/golden/parse_errors/break_missing_semicolon.err tests/golden/parse_errors/break_missing_semicolon.exit tests/golden/parse_errors/continue_missing_semicolon.cd tests/golden/parse_errors/continue_missing_semicolon.err tests/golden/parse_errors/continue_missing_semicolon.exit
git commit -m "test: cover loop control diagnostics and rust vm parity"
```

---

### Task 5: Documentation updates

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar docs**

In `docs/language-grammar.ebnf`, update `statement` to include loop-control statements:

```ebnf
statement   = printStmt
            | ifStmt
            | whileStmt
            | breakStmt
            | continueStmt
            | returnStmt
            | block
            | exprStmt ;
```

Add definitions after `whileStmt`:

```ebnf
breakStmt   = "break", ";" ;

continueStmt = "continue", ";" ;
```

- [ ] **Step 2: Update README language summary**

In `README.md`, update the statements block to include:

```text
break;
continue;
```

Replace the old sentence:

```markdown
`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. `break` and `continue` are not implemented yet.
```

with:

```markdown
`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. `break;` exits the nearest enclosing `while`, and `continue;` skips to that loop's next condition check. Loop-control statements outside loops are type errors; nested function bodies cannot break or continue an enclosing loop.
```

Also fix the project overview if it still mentions a bytecode VM execution backend in C++:

```markdown
- Bytecode compiler: lowers register IR into a bytecode program and `.cdbc` artifacts for the Rust VM.
```

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, update the current baseline statements bullet from:

```markdown
- Statements: `let`, `print`, `if`/`else`, `while`, `fun`, `return`, blocks, and expression statements.
```

to:

```markdown
- Statements: `let`, `print`, `if`/`else`, `while`, `break`, `continue`, `fun`, `return`, blocks, and expression statements.
```

Update Phase 11 status by adding a status sentence under `## Phase 11: Loop Control and For Loops`:

```markdown
Status: in progress. Phase 11A is implemented: `break;` exits the nearest `while`, and `continue;` skips to the nearest `while` condition check. `for` loop syntax remains future work.
```

Update the recommended split:

```markdown
- Phase 11A: `break` / `continue` for existing `while` loops. Implemented.
- Phase 11B: `for` loop syntax and lowering.
```

- [ ] **Step 4: Update AGENTS language memory**

In `AGENTS.md`, update current semantics:

```markdown
- Supported statements include `let`, `print`, `if`/`else`, `while`, `break`, `continue`, `fun`, `return`, blocks, and expression statements.
```

Add a bullet near the `while` or assignment bullets:

```markdown
- `break;` exits the nearest enclosing `while`, and `continue;` skips to the next condition check of the nearest enclosing `while`; both are type errors outside loops, including inside nested function bodies that are lexically inside a loop.
```

- [ ] **Step 5: Verify docs contain no stale 11A wording**

Run:

```bash
grep -R -n "break.*not implemented\|continue.*not implemented\|Phase 11A:.*$" README.md AGENTS.md docs/roadmap.md docs/language-grammar.ebnf || true
```

Expected: no stale “not implemented” wording. `Phase 11A` may appear only with `Implemented.`.

- [ ] **Step 6: Run doc-adjacent tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
rm -rf tests/__pycache__
```

Expected: both commands pass.

- [ ] **Step 7: Commit docs**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document loop control statements"
```

---

### Task 6: Full verification and branch completion

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

- [ ] **Step 2: Verify no C++ VM stale references returned**

Run:

```bash
grep -R -n -e '--run-bytecode' -e 'BytecodeVM' -e 'run_bytecode' CMakeLists.txt include src tests README.md AGENTS.md docs/roadmap.md docs/bytecode-text-format.md vm-rs || true
```

Expected: no output.

- [ ] **Step 3: Verify loop-control docs and fixtures are present**

Run:

```bash
grep -R -n "breakStmt\|continueStmt" docs/language-grammar.ebnf
grep -R -n "break;.*nearest\|continue;.*nearest" README.md AGENTS.md docs/roadmap.md
find tests/golden -path '*loop*' -type f | sort
```

Expected: grammar and docs mention implemented loop control; loop fixtures include `loop_break`, `loop_continue`, and `loop_control_nested` files.

- [ ] **Step 4: Review diff and status**

Run:

```bash
git diff --stat HEAD~5..HEAD
git diff --name-status HEAD~5..HEAD
git status --short --branch
```

Expected: diff includes loop-control source/tests/docs changes and no generated build artifacts. Working tree is clean.

- [ ] **Step 5: Complete branch**

Use `superpowers:verification-before-completion` to report exact verification results, then use `superpowers:finishing-a-development-branch` to present merge/push/keep/discard options.
