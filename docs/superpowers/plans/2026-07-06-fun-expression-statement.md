# Function Expression Statement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow anonymous function expressions beginning with `fun (` to be parsed directly as expression statements while preserving named `fun name(...)` declarations.

**Architecture:** Make a focused parser-dispatch change: add one-token lookahead to `Parser`, use it in `declaration()` to route `fun identifier` to `functionDeclaration()`, let `fun (` fall through to normal expression parsing, and emit a clearer parse error for malformed `fun` starts. No AST, IR, bytecode, interpreter, or VM semantic changes are needed because function expressions and expression statements already exist.

**Tech Stack:** C++17 recursive-descent parser, existing AST/IR/codegen pipeline, Python golden tests, Rust VM golden parity allowlist, CMake/CTest.

---

## File Structure

- Modify `include/Parser.hpp`: declare `checkNext(TokenType type) const`.
- Modify `src/Parser.cpp`: update `declaration()` `fun` dispatch, add `checkNext()`, and add a small helper for unexpected-token parse messages.
- Create `tests/golden/lambda_expression_statement/`: top-level bare anonymous function expression statement fixture with `input.cd`, `ast.out`, and `run.out`.
- Create `tests/golden/lambda_expression_statement_block/`: block-level bare anonymous function expression statement fixture with `input.cd`, `ast.out`, and `run.out`.
- Create `tests/golden/parse_errors/fun_invalid_start.*`: malformed `fun ;` parse-error fixture with Phase 15A snippet/caret.
- Modify `tests/run_rust_vm_tests.py`: add `lambda_expression_statement` to the golden parity allowlist.
- Modify `README.md`: document that anonymous function expressions can appear directly as expression statements.
- Modify `AGENTS.md`: update current semantics to mention bare anonymous function expression statements.
- Modify `docs/roadmap.md`: mark Phase 15B implemented.

---

### Task 1: RED fixtures for bare anonymous function expression statements

**Files:**
- Create: `tests/golden/lambda_expression_statement/input.cd`
- Create: `tests/golden/lambda_expression_statement/ast.out`
- Create: `tests/golden/lambda_expression_statement/run.out`
- Create: `tests/golden/lambda_expression_statement_block/input.cd`
- Create: `tests/golden/lambda_expression_statement_block/ast.out`
- Create: `tests/golden/lambda_expression_statement_block/run.out`
- Create: `tests/golden/parse_errors/fun_invalid_start.cd`
- Create: `tests/golden/parse_errors/fun_invalid_start.err`
- Create: `tests/golden/parse_errors/fun_invalid_start.exit`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Add top-level anonymous function expression statement fixture**

Create `tests/golden/lambda_expression_statement/input.cd`:

```cd
fun (x) {
  return x + 1;
};
print "ok";
```

Create `tests/golden/lambda_expression_statement/ast.out`:

```text
Program
  Expr (fun (x) (return (+ x 1)))
  Print "ok"
```

Create `tests/golden/lambda_expression_statement/run.out`:

```text
ok
```

- [ ] **Step 2: Add block-level anonymous function expression statement fixture**

Create `tests/golden/lambda_expression_statement_block/input.cd`:

```cd
{
  fun () {
    return 1;
  };
}
print "done";
```

Create `tests/golden/lambda_expression_statement_block/ast.out`:

```text
Program
  Block
    Expr (fun () (return 1))
  Print "done"
```

Create `tests/golden/lambda_expression_statement_block/run.out`:

```text
done
```

- [ ] **Step 3: Add malformed `fun ;` parse-error fixture**

Create `tests/golden/parse_errors/fun_invalid_start.cd` with no trailing newline:

```cd
fun ;
```

Create `tests/golden/parse_errors/fun_invalid_start.err`:

```text
Parse error at 1:5: expected function name after `fun` declaration or `(` for function expression, found Semicolon `;`
  fun ;
      ^
```

Create `tests/golden/parse_errors/fun_invalid_start.exit`:

```text
1
```

- [ ] **Step 4: Add Rust VM golden parity allowlist entry**

In `tests/run_rust_vm_tests.py`, add `"lambda_expression_statement",` after the existing `"lambda_basic",` allowlist entry:

```python
            "lambda_basic",
            "lambda_expression_statement",
            "lambda_closure",
```

- [ ] **Step 5: Verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case lambda_expression_statement
```

Expected before implementation:

- Golden tests fail for `lambda_expression_statement` and `lambda_expression_statement_block` because `fun (` is still parsed as a named declaration and reports `expected function name after `fun``.
- `parse_errors/fun_invalid_start` may fail with the old diagnostic wording/caret until the parser dispatch error is updated.
- Rust VM case fails at compile time for the new allowlisted fixture for the same parser reason.

- [ ] **Step 6: Commit RED tests**

```bash
git add tests/golden/lambda_expression_statement tests/golden/lambda_expression_statement_block tests/golden/parse_errors/fun_invalid_start.* tests/run_rust_vm_tests.py
git commit -m "test: add function expression statement coverage"
```

---

### Task 2: Implement parser lookahead for `fun` declaration vs expression dispatch

**Files:**
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Declare parser lookahead helper**

In `include/Parser.hpp`, add this private method after `check(TokenType type) const;`:

```cpp
    bool checkNext(TokenType type) const;
```

The surrounding private helper section should look like:

```cpp
    bool match(TokenType type);
    bool check(TokenType type) const;
    bool checkNext(TokenType type) const;
    Token advance();
```

- [ ] **Step 2: Add local unexpected-token formatter for parser dispatch errors**

In `src/Parser.cpp`, add this helper in the existing anonymous namespace area. If the file has no anonymous namespace, add it after the `#include` block and before `ParseError::ParseError(...)`:

```cpp
namespace {

std::string expectedFunStartMessage(const Token& token)
{
    std::ostringstream message;
    message << "expected function name after `fun` declaration or `(` for function expression, found "
            << tokenTypeName(token.type);
    if (!token.lexeme.empty()) {
        message << " `" << token.lexeme << "`";
    }
    return message.str();
}

} // namespace
```

`src/Parser.cpp` already includes `<sstream>`, so no new include is needed.

- [ ] **Step 3: Update `Parser::declaration()` `fun` dispatch**

Replace the current `fun` branch in `Parser::declaration()`:

```cpp
    if (match(TokenType::Fun)) {
        return functionDeclaration();
    }
```

with:

```cpp
    if (check(TokenType::Fun) && checkNext(TokenType::Identifier)) {
        advance();
        return functionDeclaration();
    }
    if (check(TokenType::Fun) && !checkNext(TokenType::LeftParen)) {
        advance();
        throw ParseError(peek(), expectedFunStartMessage(peek()));
    }
```

This preserves named declarations, lets `fun (` fall through to `statement()` and `expressionStatement()`, and reports malformed `fun ;` at the semicolon after consuming `fun`.

- [ ] **Step 4: Implement `Parser::checkNext()`**

In `src/Parser.cpp`, add this method after `Parser::check(TokenType type) const`:

```cpp
bool Parser::checkNext(TokenType type) const
{
    if (current_ + 1 >= tokens_.size()) {
        return false;
    }
    return tokens_[current_ + 1].type == type;
}
```

- [ ] **Step 5: Verify GREEN for focused tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case lambda_expression_statement
```

Expected:

- Golden tests pass, including new lambda expression statement fixtures and parse-error fixture.
- Rust VM focused parity passes for `lambda_expression_statement`.
- Existing named function declaration and existing lambda fixtures still pass.

- [ ] **Step 6: Commit parser implementation**

```bash
git add include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse function expression statements"
```

---

### Task 3: Documentation and roadmap updates

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README function-expression text**

In `README.md`, find the paragraph starting with `Functions are values.` and replace the first two sentences:

```markdown
Functions are values. Named functions use `fun name(parameter[: type]*) [: type] { declaration* }`, and anonymous function expressions use `fun (parameter[: type]*) [: type] { declaration* }`.
```

with:

```markdown
Functions are values. Named functions use `fun name(parameter[: type]*) [: type] { declaration* }`, and anonymous function expressions use `fun (parameter[: type]*) [: type] { declaration* }`. Anonymous function expressions may appear in expression positions, including direct expression statements such as `fun () { return nil; };`.
```

In the supported expressions list, replace:

```markdown
- Function expressions: `fun (parameter[: type]*) [: type] { declaration* }`
```

with:

```markdown
- Function expressions: `fun (parameter[: type]*) [: type] { declaration* }`, including direct expression statements such as `fun () { return nil; };`
```

- [ ] **Step 2: Update AGENTS current semantics**

In `AGENTS.md`, replace this bullet:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Parameters and returns may be annotated with `number`, `bool`, `string`, `nil`, or function types such as `fun(number): string`; known function values carry arity, annotated parameter types, and conservative or annotated return types for static checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

with:

```markdown
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Anonymous function expressions may appear directly as expression statements, for example `fun () { return nil; };`. Parameters and returns may be annotated with `number`, `bool`, `string`, `nil`, or function types such as `fun(number): string`; known function values carry arity, annotated parameter types, and conservative or annotated return types for static checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
```

- [ ] **Step 3: Update roadmap Phase 15 status**

In `docs/roadmap.md`, update the Phase 15 status paragraph from:

```markdown
Status: in progress. Phase 15A is implemented: located front-end diagnostics print the combined-source line and a caret while keeping the existing first diagnostic line stable. File-aware diagnostic remapping remains future work.
```

into:

```markdown
Status: in progress. Phase 15A is implemented: located front-end diagnostics print the combined-source line and a caret while keeping the existing first diagnostic line stable. Phase 15B is implemented: anonymous function expressions beginning with `fun (` can appear directly as expression statements while named `fun name(...)` declarations keep their existing behavior. File-aware diagnostic remapping remains future work.
```

In the suggested features list, replace:

```markdown
- Clear handling for lambda expression statements that begin with `fun`, either by documenting parenthesized form or changing parser disambiguation.
```

with:

```markdown
- Clear handling for lambda expression statements that begin with `fun`. Implemented by parser disambiguation between `fun name` declarations and `fun (` expressions.
```

- [ ] **Step 4: Verify docs and golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
```

Expected: both pass.

- [ ] **Step 5: Commit docs**

```bash
git add README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document function expression statements"
```

---

### Task 4: Full verification and push

**Files:**
- No planned source edits.
- Remove generated `tests/__pycache__/` if present.

- [ ] **Step 1: Check status before full verification**

Run:

```bash
git status --short --branch
```

Expected: branch is `master`; only intentional committed work is ahead of `origin/master`.

- [ ] **Step 2: Run full verification suite**

Run exactly:

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

Expected: every command before `rm -rf` exits 0.

- [ ] **Step 3: Inspect final state**

Run:

```bash
git status --short --branch
git log --oneline --decorate -8
```

Expected: worktree is clean and `master` is ahead of `origin/master` by the Phase 15B spec/plan/test/implementation/docs commits.

- [ ] **Step 4: Push to master**

Run:

```bash
git push origin master
```

Expected: push succeeds.

- [ ] **Step 5: Final report**

Report these items to the user:

```text
完成 Phase 15B function expression statements 并已 push。

实现：
- `fun name(...) { ... }` 继续解析为 named function declaration。
- `fun (...) { ... };` 可直接作为 expression statement。
- block/if/while body 中同样支持 bare anonymous function expression statement。
- `fun ;` 给出清晰 Parse error + source snippet。
- 无 AST/IR/bytecode/VM 新语义；复用现有 FunctionExpr + ExpressionStmt。

验证：
- cmake -S . -B build ✅
- cmake --build build ✅
- ctest --test-dir build --output-on-failure ✅
- python3 tests/run_golden_tests.py ./build/compiler_design ✅
- python3 tests/run_golden_tests_selftest.py ✅
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs ✅
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens ✅
- cargo test --manifest-path vm-rs/Cargo.toml ✅
- git push origin master ✅
```

---

## Self-Review

**Spec coverage:** The plan covers top-level and block-level bare anonymous function expression statements, preserves named function declarations, adds a clear malformed `fun ;` parse error with snippet, updates Rust VM golden parity allowlist for one success fixture, and documents the behavior.

**Placeholder scan:** No placeholder markers, deferred-implementation notes, or unstated test-writing steps remain.

**Type consistency:** `checkNext(TokenType) const`, `expectedFunStartMessage(const Token&)`, and `Parser::declaration()` dispatch logic use consistent names and signatures across tasks.
