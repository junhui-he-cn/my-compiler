# Assignment Expression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add assignment expressions (`name = expression`) that update existing variables, evaluate to the assigned value, and support chained right-associative assignment.

**Architecture:** Add parse-error golden fixture support first so invalid assignment targets are regression-tested. Then add `AssignExpr` to the AST/parser, add an `AssignVar` IR operation distinct from `StoreVar`, and teach the IR interpreter to reject assignment to undefined variables. Update golden fixtures and language documentation after the implementation is green.

**Tech Stack:** C++17, recursive-descent parser, register IR, IR interpreter, Python golden CLI test runner, CMake/CTest.

---

## File Structure

- Modify `tests/run_golden_tests.py`: add parse-error fixture discovery/checking for `tests/golden/parse_errors/*.cd`.
- Modify `tests/run_golden_tests_selftest.py`: add selftest coverage for parse-error fixtures.
- Create `tests/golden/assignment/`: success golden for normal assignment and assignment expression value.
- Create `tests/golden/chained_assignment/`: success golden for right-associative chained assignment.
- Create `tests/golden/runtime_errors/assign_undefined.cd`, `.run.err`, and `.exit`: runtime error for assigning to an undefined variable.
- Create `tests/golden/parse_errors/invalid_assignment_target.cd`, `.err`, and `.exit`: parse error for a non-variable assignment target.
- Modify `include/Ast.hpp` and `src/Ast.cpp`: add `AssignExpr` and AST printing.
- Modify `include/Parser.hpp` and `src/Parser.cpp`: add `assignment()` below `expression()` and above `equality()`.
- Modify `include/IR.hpp` and `src/IR.cpp`: add `IROp::AssignVar`, `emitAssignVar`, IR naming, and IR printing.
- Modify `src/IRCompiler.cpp`: compile `AssignExpr` by emitting `AssignVar` and returning the RHS register.
- Modify `src/IRInterpreter.cpp`: execute `AssignVar` with an existing-variable check.
- Modify `docs/language-grammar.ebnf`: document assignment precedence.
- Modify `README.md`: document assignment expressions and parse-error golden fixtures.

## Task 0: Prepare an Isolated Workspace

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
assignment-expression
```

Expected worktree path if using the project convention:

```text
/home/junhe/compiler/.worktrees/assignment-expression
```

- [ ] **Step 2: Run baseline verification in the worktree**

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

If this fails before any assignment-expression edits, stop and report the baseline failure.

## Task 1: Add Parse-Error Golden Runner Support

**Files:**
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`

- [ ] **Step 1: Write the failing selftest**

Append this test method to `GoldenRunnerQualityTests` in `tests/run_golden_tests_selftest.py` before the `if __name__ == "__main__"` block:

```python
    def test_parse_error_case_checks_default_mode_stderr_exit_and_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            parse_dir = golden_dir / "parse_errors"
            parse_dir.mkdir(parents=True)
            (parse_dir / "bad_assignment.cd").write_text("(x + 1) = 2;\n", encoding="utf-8")
            (parse_dir / "bad_assignment.err").write_text("parse error\n", encoding="utf-8")
            (parse_dir / "bad_assignment.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stdout="unexpected output\n",
                stderr="parse error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("parse_errors/bad_assignment default(ast)", results[0].message)
        self.assertIn("unexpected stdout", results[0].message)
        self.assertIn("unexpected output", results[0].message)
```

- [ ] **Step 2: Run the selftest and verify RED**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: the new test fails because `tests/golden/parse_errors` is not discovered. The failure should show `0 != 1` for the result count or otherwise prove the parse-error fixture was ignored.

- [ ] **Step 3: Add parse-error discovery and checking**

In `tests/run_golden_tests.py`, add this function after `discover_runtime_error_cases`:

```python
def discover_parse_error_cases(golden_dir: Path) -> list[Path]:
    parse_dir = golden_dir / "parse_errors"
    if not parse_dir.is_dir():
        return []
    return sorted(parse_dir.glob("*.cd"))
```

Add this helper after `unexpected_runtime_stdout_result`:

```python
def unexpected_parse_stdout_result(case_name: str, stdout: str) -> CheckResult:
    return CheckResult(
        case_name,
        False,
        (
            f"FAIL {case_name} produced unexpected stdout for parse error\n\n"
            f"STDOUT:\n{stdout}"
        ),
    )
```

Add this checker after `check_runtime_error_case`:

```python
def check_parse_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    stem = source.with_suffix("")
    err_path = stem.with_suffix(".err")
    exit_path = stem.with_suffix(".exit")
    case_name = f"parse_errors/{source.stem} default(ast)"

    completed = run_compiler(compiler, (), source)

    if update:
        write_text(err_path, completed.stderr)
        write_text(exit_path, f"{completed.returncode}\n")
        if completed.stdout:
            return [unexpected_parse_stdout_result(case_name, completed.stdout)]
        return [CheckResult(case_name, True)]

    results: list[CheckResult] = []

    if completed.stdout:
        results.append(unexpected_parse_stdout_result(case_name, completed.stdout))

    if not err_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected stderr file: {err_path}"))
    else:
        expected_err = read_text(err_path)
        actual_err = completed.stderr
        if actual_err != expected_err:
            diff = unified_diff(expected_err, actual_err, "expected stderr", "actual stderr")
            results.append(CheckResult(case_name, False, f"FAIL {case_name} stderr mismatch\n\n{diff}"))

    if not exit_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected exit file: {exit_path}"))
    else:
        expected_exit_text = read_text(exit_path).strip()
        actual_exit_text = str(completed.returncode)
        if actual_exit_text != expected_exit_text:
            results.append(
                CheckResult(
                    case_name,
                    False,
                    f"FAIL {case_name} exit code mismatch\nexpected: {expected_exit_text}\nactual: {actual_exit_text}",
                )
            )

    if not results:
        results.append(CheckResult(case_name, True))

    return results
```

Update `run_all` to include parse-error cases after runtime-error cases:

```python
    for source in discover_parse_error_cases(golden_dir):
        results.extend(check_parse_error_case(compiler, source, update))
```

- [ ] **Step 4: Run the selftest and verify GREEN**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

```text
Ran 6 tests
OK
```

- [ ] **Step 5: Run full tests**

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

- [ ] **Step 6: Commit parse-error runner support**

Run:

```bash
git add tests/run_golden_tests.py tests/run_golden_tests_selftest.py
git commit -m "test: add parse error golden support"
```

Expected: commit succeeds.

## Task 2: Add Red Assignment Golden Fixtures

**Files:**
- Create: `tests/golden/assignment/input.cd`
- Create: `tests/golden/assignment/run.out`
- Create: `tests/golden/chained_assignment/input.cd`
- Create: `tests/golden/chained_assignment/run.out`
- Create: `tests/golden/runtime_errors/assign_undefined.cd`
- Create: `tests/golden/runtime_errors/assign_undefined.run.err`
- Create: `tests/golden/runtime_errors/assign_undefined.exit`
- Create: `tests/golden/parse_errors/invalid_assignment_target.cd`
- Create: `tests/golden/parse_errors/invalid_assignment_target.err`
- Create: `tests/golden/parse_errors/invalid_assignment_target.exit`

- [ ] **Step 1: Add success and error fixture files**

Run:

```bash
mkdir -p tests/golden/assignment tests/golden/chained_assignment tests/golden/parse_errors

cat > tests/golden/assignment/input.cd <<'CASE'
let x = 1;
x = x + 2;
print x;
print x = 5;
print x;
CASE
cat > tests/golden/assignment/run.out <<'CASE'
3
5
5
CASE

cat > tests/golden/chained_assignment/input.cd <<'CASE'
let a = 0;
let b = 0;
a = b = 7;
print a;
print b;
CASE
cat > tests/golden/chained_assignment/run.out <<'CASE'
7
7
CASE

cat > tests/golden/runtime_errors/assign_undefined.cd <<'CASE'
missing = 1;
CASE
cat > tests/golden/runtime_errors/assign_undefined.run.err <<'CASE'
IR runtime error: undefined variable `missing`
CASE
cat > tests/golden/runtime_errors/assign_undefined.exit <<'CASE'
1
CASE

cat > tests/golden/parse_errors/invalid_assignment_target.cd <<'CASE'
(x + 1) = 2;
CASE
cat > tests/golden/parse_errors/invalid_assignment_target.err <<'CASE'
Parse error at line 1, column 9: invalid assignment target
CASE
cat > tests/golden/parse_errors/invalid_assignment_target.exit <<'CASE'
1
CASE
```

- [ ] **Step 2: Run golden tests and verify RED**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: fails. The failure output should include parse errors for assignment syntax, because `x = x + 2;`, `print x = 5;`, and `missing = 1;` are not implemented yet. The invalid assignment target parse-error fixture may fail with a different parser message until `assignment()` is implemented.

- [ ] **Step 3: Commit red fixtures**

Run:

```bash
git add tests/golden/assignment tests/golden/chained_assignment tests/golden/runtime_errors/assign_undefined.cd tests/golden/runtime_errors/assign_undefined.run.err tests/golden/runtime_errors/assign_undefined.exit tests/golden/parse_errors
git commit -m "test: add assignment expression fixtures"
```

Expected: commit succeeds. The branch may have failing golden tests until implementation tasks are complete.

## Task 3: Parse Assignment Expressions and Print AST

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add `AssignExpr` declaration**

In `include/Ast.hpp`, add this struct after `VariableExpr` and before `UnaryExpr`:

```cpp
struct AssignExpr final : Expr {
    AssignExpr(Token name, ExprPtr value);
    void print(std::ostream& out) const override;

    Token name;
    ExprPtr value;
};
```

- [ ] **Step 2: Implement `AssignExpr` construction and printing**

In `src/Ast.cpp`, add `#include <utility>` with the other includes if it is not already present:

```cpp
#include <string>
#include <utility>
```

Add this implementation after `VariableExpr::print` and before `UnaryExpr::UnaryExpr`:

```cpp
AssignExpr::AssignExpr(Token name, ExprPtr value)
    : name(std::move(name))
    , value(std::move(value))
{
}

void AssignExpr::print(std::ostream& out) const
{
    out << "(= " << name.lexeme << ' ';
    writeExpr(out, value);
    out << ')';
}
```

- [ ] **Step 3: Add parser method declaration**

In `include/Parser.hpp`, change the expression comment section from:

```cpp
    ExprPtr expression();
    ExprPtr equality();
```

to:

```cpp
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr equality();
```

- [ ] **Step 4: Implement right-associative assignment parsing**

In `src/Parser.cpp`, replace `Parser::expression()` with:

```cpp
ExprPtr Parser::expression()
{
    return assignment();
}
```

Add this method between `expression()` and `equality()`:

```cpp
ExprPtr Parser::assignment()
{
    ExprPtr expr = equality();

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

- [ ] **Step 5: Build and verify AST parsing behavior**

Run:

```bash
cmake --build build
./build/compiler_demo tests/golden/assignment/input.cd
./build/compiler_demo tests/golden/chained_assignment/input.cd
./build/compiler_demo tests/golden/parse_errors/invalid_assignment_target.cd >/tmp/invalid_assignment.out 2>/tmp/invalid_assignment.err; printf '%s\n' "$?"; cat /tmp/invalid_assignment.err
```

Expected AST snippets:

```text
Expr (= x (+ x 2))
Print (= x 5)
Expr (= a (= b 7))
```

Expected parse-error command output:

```text
1
Parse error at line 1, column 9: invalid assignment target
```

The full golden suite is still expected to fail at this point because IR compilation does not support `AssignExpr` yet.

- [ ] **Step 6: Commit parser and AST support**

Run:

```bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse assignment expressions"
```

Expected: commit succeeds.

## Task 4: Add AssignVar IR and Runtime Semantics

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `src/IRInterpreter.cpp`

- [ ] **Step 1: Add `AssignVar` to the IR interface**

In `include/IR.hpp`, add `AssignVar` after `StoreVar`:

```cpp
enum class IROp {
    Constant,
    LoadVar,
    StoreVar,
    AssignVar,
    Print,
    Negate,
```

Add the emitter declaration after `emitStoreVar`:

```cpp
    void emitStoreVar(std::string name, IRRegister value);
    void emitAssignVar(std::string name, IRRegister value);
    void emitPrint(IRRegister value);
```

- [ ] **Step 2: Add `AssignVar` to IR utility switches and emitter**

In `src/IR.cpp`, update `isBinary` so `AssignVar` returns false:

```cpp
    case IROp::StoreVar:
    case IROp::AssignVar:
    case IROp::Print:
```

Add this emitter after `IRProgram::emitStoreVar`:

```cpp
void IRProgram::emitAssignVar(std::string name, IRRegister value)
{
    emit(IRInstruction{IROp::AssignVar, std::nullopt, value, std::nullopt, addName(std::move(name))});
}
```

In `IRProgram::print`, change the `StoreVar` branch:

```cpp
        } else if (instruction.op == IROp::StoreVar) {
            printNameOperand(out, *this, instruction.operand);
            if (instruction.left) {
                out << ", " << *instruction.left;
            }
```

to handle both variable-write operations:

```cpp
        } else if (instruction.op == IROp::StoreVar || instruction.op == IROp::AssignVar) {
            printNameOperand(out, *this, instruction.operand);
            if (instruction.left) {
                out << ", " << *instruction.left;
            }
```

In `irOpName`, add:

```cpp
    case IROp::AssignVar:
        return "assign_var";
```

immediately after the `StoreVar` case.

- [ ] **Step 3: Compile `AssignExpr`**

In `src/IRCompiler.cpp`, add this block to `IRCompiler::compileExpression` after the `VariableExpr` handling and before the `GroupingExpr` handling:

```cpp
    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const IRRegister value = compileExpression(*assign->value);
        ir_.emitAssignVar(assign->name.lexeme, value);
        return value;
    }
```

- [ ] **Step 4: Execute `AssignVar` with an existing-variable check**

In `src/IRInterpreter.cpp`, add this switch case immediately after the `StoreVar` case:

```cpp
        case IROp::AssignVar: {
            const std::string name = readName(program, instruction.operand);
            auto found = globals_.find(name);
            if (found == globals_.end()) {
                throw IRRuntimeError("undefined variable `" + name + "`");
            }
            found->second = readRegister(readLeft(instruction));
            break;
        }
```

- [ ] **Step 5: Build and verify runtime behavior manually**

Run:

```bash
cmake --build build
./build/compiler_demo --run tests/golden/assignment/input.cd
./build/compiler_demo --run tests/golden/chained_assignment/input.cd
./build/compiler_demo --run tests/golden/runtime_errors/assign_undefined.cd >/tmp/assign_undefined.out 2>/tmp/assign_undefined.err; printf '%s\n' "$?"; cat /tmp/assign_undefined.err
./build/compiler_demo --ir tests/golden/assignment/input.cd | grep assign_var
```

Expected outputs:

```text
3
5
5
```

```text
7
7
```

```text
1
IR runtime error: undefined variable `missing`
```

The `grep assign_var` command should print at least two `assign_var` instructions.

- [ ] **Step 6: Run golden tests and verify GREEN for existing expected files**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected:

```text
golden tests: 37 passed, 0 failed
```

At this point the two new success fixtures only have `run.out`, so their AST and IR checks are not counted until Task 5 generates `ast.out` and `ir.out`.

- [ ] **Step 7: Commit IR and runtime support**

Run:

```bash
git add include/IR.hpp src/IR.cpp src/IRCompiler.cpp src/IRInterpreter.cpp
git commit -m "feat: compile assignment expressions"
```

Expected: commit succeeds.

## Task 5: Generate Full Golden Outputs

**Files:**
- Create: `tests/golden/assignment/ast.out`
- Create: `tests/golden/assignment/ir.out`
- Create: `tests/golden/chained_assignment/ast.out`
- Create: `tests/golden/chained_assignment/ir.out`
- Modify: `tests/golden/assignment/run.out` if current output formatting requires regeneration.
- Modify: `tests/golden/chained_assignment/run.out` if current output formatting requires regeneration.

- [ ] **Step 1: Generate missing golden files**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Expected:

```text
golden tests: 41 passed, 0 failed
```

- [ ] **Step 2: Inspect generated assignment goldens**

Run:

```bash
printf '%s\n' '--- assignment ast ---'
cat tests/golden/assignment/ast.out
printf '%s\n' '--- assignment ir assign_var ---'
grep -n 'assign_var' tests/golden/assignment/ir.out
printf '%s\n' '--- chained ast ---'
cat tests/golden/chained_assignment/ast.out
printf '%s\n' '--- parse error expected ---'
cat tests/golden/parse_errors/invalid_assignment_target.err
```

Expected snippets:

```text
Expr (= x (+ x 2))
Print (= x 5)
Expr (= a (= b 7))
Parse error at line 1, column 9: invalid assignment target
```

The `grep` output should include `assign_var` instructions.

- [ ] **Step 3: Run full tests**

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
golden tests: 41 passed, 0 failed
Ran 6 tests
OK
```

- [ ] **Step 4: Commit generated golden outputs**

Run:

```bash
git add tests/golden/assignment tests/golden/chained_assignment tests/golden/runtime_errors/assign_undefined.cd tests/golden/runtime_errors/assign_undefined.run.err tests/golden/runtime_errors/assign_undefined.exit tests/golden/parse_errors
git commit -m "test: add assignment expression goldens"
```

Expected: commit succeeds. If the previous red fixture commit already tracked some of these files, this commit should contain the generated `ast.out` and `ir.out` files plus any regenerated outputs.

## Task 6: Update Language Documentation

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`

- [ ] **Step 1: Update grammar document**

In `docs/language-grammar.ebnf`, replace:

```ebnf
expression  = equality ;

equality    = comparison,
```

with:

```ebnf
expression  = assignment ;

assignment  = identifier, "=", assignment
            | equality ;

equality    = comparison,
```

- [ ] **Step 2: Update README language section**

In `README.md`, add assignment to the supported expressions list. Replace:

```markdown
- Variables: `name`
```

with:

```markdown
- Variables: `name`
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
```

- [ ] **Step 3: Update README test section for parse errors**

In the README test section, replace the sentence:

```markdown
Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, or `run.out` files to cover new syntax. Runtime-error fixtures live in `tests/golden/runtime_errors`: for `example.cd`, add matching `example.run.err` and `example.exit` files.
```

with:

```markdown
Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, or `run.out` files to cover successful syntax. Runtime-error fixtures live in `tests/golden/runtime_errors`: for `example.cd`, add matching `example.run.err` and `example.exit` files. Parse-error fixtures live in `tests/golden/parse_errors`: for `example.cd`, add matching `example.err` and `example.exit` files.
```

- [ ] **Step 4: Run documentation verification tests**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

```text
100% tests passed, 0 tests failed out of 2
golden tests: 41 passed, 0 failed
Ran 6 tests
OK
```

- [ ] **Step 5: Commit documentation updates**

Run:

```bash
git add docs/language-grammar.ebnf README.md
git commit -m "docs: describe assignment expressions"
```

Expected: commit succeeds.

## Task 7: Final Verification and Review

**Files:**
- Verify only.

- [ ] **Step 1: Run complete verification**

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
git log --oneline -10
```

Expected:

```text
100% tests passed, 0 tests failed out of 2
golden tests: 41 passed, 0 failed
Ran 6 tests
OK
```

`git status --short` should produce no output.

Recent commits should include:

```text
docs: describe assignment expressions
test: add assignment expression goldens
feat: compile assignment expressions
feat: parse assignment expressions
test: add assignment expression fixtures
test: add parse error golden support
docs: add assignment expression implementation plan
docs: add assignment expression design
```

- [ ] **Step 2: Request code review before merge**

Invoke `superpowers:requesting-code-review` and review the full assignment-expression branch against:

```text
docs/superpowers/specs/2026-06-27-assignment-expression-design.md
docs/superpowers/plans/2026-06-27-assignment-expression.md
```

Require the reviewer to check:

- Parser right-associativity for `a = b = 7`.
- Invalid target errors use the `=` token location.
- `AssignVar` cannot create variables.
- Assignment expression returns RHS register for `print x = 5;`.
- Parse-error golden support does not affect success or runtime-error fixture behavior.
- Documentation matches implemented behavior.

- [ ] **Step 3: Handle review feedback**

If review returns Critical or Important issues, invoke `superpowers:receiving-code-review`, fix each issue, and rerun the complete verification command from Step 1.

If review returns only Minor issues, either fix low-risk documentation/test clarity items or record them as non-blocking before finishing.

- [ ] **Step 4: Finish branch**

After review approval and clean verification, invoke `superpowers:finishing-a-development-branch` and present the standard finish options.

## Self-Review Notes

- Spec coverage: Task 1 implements parse-error fixture support. Task 2 creates red assignment, chain, runtime-error, and parse-error fixtures. Task 3 implements AST/parser support. Task 4 implements IR/compiler/interpreter runtime semantics. Task 5 generates full AST/IR/run goldens. Task 6 updates grammar and README. Task 7 verifies and reviews the complete branch.
- TDD order: The plan writes failing selftests before parse-error runner changes and writes failing golden fixtures before assignment implementation. Implementation tasks use targeted red/green checks before full suite verification.
- Scope: The plan does not add compound assignment, increment/decrement, destructuring, property/index assignment, lexical scope changes, static analysis, or `while` loops.
- Type consistency: The plan consistently uses `AssignExpr`, `IROp::AssignVar`, `emitAssignVar`, `assign_var`, `assignment()`, and `tests/golden/parse_errors`.
