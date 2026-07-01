# Basic Type Checker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a static type-checking phase for explicit `let` annotations while preserving existing unannotated programs.

**Architecture:** Add `TypeChecker` as a new AST pass after parsing and before AST printing/IR compilation. Extend the golden runner with `tests/golden/type_errors`, then add red type-error fixtures, implement the checker, wire it into CMake and `main.cpp`, and update docs/goldens.

**Tech Stack:** C++17, CMake, recursive-descent AST, Python 3 golden test runner, CTest.

---

## File Structure

- Modify: `tests/run_golden_tests.py` — discover and check `tests/golden/type_errors/*.cd`.
- Modify: `tests/run_golden_tests_selftest.py` — selftests for type-error fixture behavior.
- Create: `tests/golden/type_errors/*.cd`, `.err`, `.exit` — red type-error fixtures.
- Delete: `tests/golden/runtime_errors/duplicate_declaration.*` — duplicate declarations become type errors.
- Create: `include/TypeChecker.hpp` — public checker API, `StaticType`, and `TypeError`.
- Create: `src/TypeChecker.cpp` — AST walker and type rules.
- Modify: `CMakeLists.txt` — compile `src/TypeChecker.cpp`.
- Modify: `src/main.cpp` — run `TypeChecker` after parsing before output/IR.
- Modify: `README.md`, `AGENTS.md`, `docs/roadmap.md` — document Phase 2 semantics.

## Task 0: Prepare Workspace and Baseline

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
basic-type-checker
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
golden tests: 49 passed, 0 failed
Ran 7 tests
OK
```

If baseline fails before type-checker edits, stop and report the failure.

## Task 1: Add Type-Error Golden Runner Support

**Files:**
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`

- [ ] **Step 1: Add failing selftests for type-error fixtures**

Append these methods to `GoldenRunnerQualityTests` before the `if __name__ == "__main__"` block in `tests/run_golden_tests_selftest.py`:

```python
    def test_type_error_case_checks_default_mode_stderr_exit_and_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            type_dir = golden_dir / "type_errors"
            type_dir.mkdir(parents=True)
            (type_dir / "bad_type.cd").write_text('let x: number = "bad";\n', encoding="utf-8")
            (type_dir / "bad_type.err").write_text("type error\n", encoding="utf-8")
            (type_dir / "bad_type.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stdout="unexpected output\n",
                stderr="type error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("type_errors/bad_type default(ast)", results[0].message)
        self.assertIn("unexpected stdout", results[0].message)
        self.assertIn("unexpected output", results[0].message)

    def test_type_error_input_cd_is_not_discovered_as_success_case(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            type_dir = golden_dir / "type_errors"
            type_dir.mkdir(parents=True)
            (type_dir / "input.cd").write_text('let x: number = "bad";\n', encoding="utf-8")
            (type_dir / "input.err").write_text("type error\n", encoding="utf-8")
            (type_dir / "input.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="type error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "type_errors/input default(ast)")
```

Expected: selftests refer to APIs not implemented yet.

- [ ] **Step 2: Run selftests and verify RED**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: the two new tests fail because `type_errors` is not discovered and is not excluded from success-case discovery.

- [ ] **Step 3: Update success discovery to exclude type errors**

In `tests/run_golden_tests.py`, replace:

```python
and case_dir.name not in {"runtime_errors", "parse_errors"}
```

with:

```python
and case_dir.name not in {"runtime_errors", "parse_errors", "type_errors"}
```

Expected: `tests/golden/type_errors/input.cd` will not be treated as a success fixture.

- [ ] **Step 4: Add type-error discovery and stdout helper**

Add this function after `discover_parse_error_cases`:

```python
def discover_type_error_cases(golden_dir: Path) -> list[Path]:
    type_dir = golden_dir / "type_errors"
    if not type_dir.is_dir():
        return []
    return sorted(type_dir.glob("*.cd"))
```

Add this helper after `unexpected_parse_stdout_result`:

```python
def unexpected_type_stdout_result(case_name: str, stdout: str) -> CheckResult:
    return CheckResult(
        case_name,
        False,
        (
            f"FAIL {case_name} produced unexpected stdout for type error\n\n"
            f"STDOUT:\n{stdout}"
        ),
    )
```

Expected: type-error fixtures can be found and unexpected stdout has a type-specific message.

- [ ] **Step 5: Add type-error checker**

Add this function after `check_parse_error_case`:

```python
def check_type_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    stem = source.with_suffix("")
    err_path = stem.with_suffix(".err")
    exit_path = stem.with_suffix(".exit")
    case_name = f"type_errors/{source.stem} default(ast)"

    completed = run_compiler(compiler, (), source)

    if update:
        write_text(err_path, completed.stderr)
        write_text(exit_path, f"{completed.returncode}\n")
        if completed.stdout:
            return [unexpected_type_stdout_result(case_name, completed.stdout)]
        return [CheckResult(case_name, True)]

    results: list[CheckResult] = []

    if completed.stdout:
        results.append(unexpected_type_stdout_result(case_name, completed.stdout))

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

Expected: checker mirrors parse-error behavior using `type_errors/<stem> default(ast)` names.

- [ ] **Step 6: Include type-error cases in `run_all`**

In `run_all`, add this loop after parse-error cases:

```python
    for source in discover_type_error_cases(golden_dir):
        results.extend(check_type_error_case(compiler, source, update))
```

Expected: type-error fixtures are checked after parse errors.

- [ ] **Step 7: Run selftests and verify GREEN**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

```text
Ran 9 tests
OK
```

- [ ] **Step 8: Commit runner support**

Run:

```bash
git add tests/run_golden_tests.py tests/run_golden_tests_selftest.py
git commit -m "test: add type error golden support"
```

Expected: commit succeeds with only runner/selftest changes.

## Task 2: Add Red Type-Error Fixtures

**Files:**
- Create: `tests/golden/type_errors/typed_let_number_mismatch.cd`, `.err`, `.exit`
- Create: `tests/golden/type_errors/typed_assignment_mismatch.cd`, `.err`, `.exit`
- Create: `tests/golden/type_errors/unknown_type_annotation.cd`, `.err`, `.exit`
- Create: `tests/golden/type_errors/duplicate_declaration.cd`, `.err`, `.exit`
- Create: `tests/golden/type_errors/unary_minus_non_number.cd`, `.err`, `.exit`
- Create: `tests/golden/type_errors/binary_number_operator_mismatch.cd`, `.err`, `.exit`
- Delete: `tests/golden/runtime_errors/duplicate_declaration.cd`, `.run.err`, `.exit`

- [ ] **Step 1: Create type-error fixtures**

Run:

```bash
mkdir -p tests/golden/type_errors

cat > tests/golden/type_errors/typed_let_number_mismatch.cd <<'CASE'
let x: number = "hello";
CASE
cat > tests/golden/type_errors/typed_let_number_mismatch.err <<'CASE'
Type error: cannot initialize `x` of type number with string
CASE
cat > tests/golden/type_errors/typed_let_number_mismatch.exit <<'CASE'
1
CASE

cat > tests/golden/type_errors/typed_assignment_mismatch.cd <<'CASE'
let x: number = 1;
x = "hello";
CASE
cat > tests/golden/type_errors/typed_assignment_mismatch.err <<'CASE'
Type error: cannot assign string to `x` of type number
CASE
cat > tests/golden/type_errors/typed_assignment_mismatch.exit <<'CASE'
1
CASE

cat > tests/golden/type_errors/unknown_type_annotation.cd <<'CASE'
let x: int = 1;
CASE
cat > tests/golden/type_errors/unknown_type_annotation.err <<'CASE'
Type error: unknown type `int`
CASE
cat > tests/golden/type_errors/unknown_type_annotation.exit <<'CASE'
1
CASE

cat > tests/golden/type_errors/duplicate_declaration.cd <<'CASE'
let x = 1;
let x = 2;
CASE
cat > tests/golden/type_errors/duplicate_declaration.err <<'CASE'
Type error: variable `x` already declared in this scope
CASE
cat > tests/golden/type_errors/duplicate_declaration.exit <<'CASE'
1
CASE

cat > tests/golden/type_errors/unary_minus_non_number.cd <<'CASE'
let x: number = -"hello";
CASE
cat > tests/golden/type_errors/unary_minus_non_number.err <<'CASE'
Type error: unary `-` expects number, got string
CASE
cat > tests/golden/type_errors/unary_minus_non_number.exit <<'CASE'
1
CASE

cat > tests/golden/type_errors/binary_number_operator_mismatch.cd <<'CASE'
let x: number = 1 * "hello";
CASE
cat > tests/golden/type_errors/binary_number_operator_mismatch.err <<'CASE'
Type error: binary `*` expects numbers, got number and string
CASE
cat > tests/golden/type_errors/binary_number_operator_mismatch.exit <<'CASE'
1
CASE
```

Expected: six type-error fixtures exist.

- [ ] **Step 2: Remove duplicate-declaration runtime fixture**

Run:

```bash
rm tests/golden/runtime_errors/duplicate_declaration.cd \
   tests/golden/runtime_errors/duplicate_declaration.run.err \
   tests/golden/runtime_errors/duplicate_declaration.exit
```

Expected: duplicate declarations are now represented as type errors, not runtime errors.

- [ ] **Step 3: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: type-error fixtures fail because the compiler still accepts them and prints AST output. Failures include unexpected stdout for `type_errors/* default(ast)`.

- [ ] **Step 4: Commit red fixtures**

Run:

```bash
git add tests/golden/type_errors tests/golden/runtime_errors
git commit -m "test: add basic type error goldens"
```

Expected: commit succeeds with fixture additions and duplicate runtime fixture removal.

## Task 3: Implement TypeChecker

**Files:**
- Create: `include/TypeChecker.hpp`
- Create: `src/TypeChecker.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`

- [ ] **Step 1: Add `TypeChecker.hpp`**

Run:

```bash
cat > include/TypeChecker.hpp <<'TYPECHECKER_HPP'
#pragma once

#include "Ast.hpp"
#include "Token.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class StaticType {
    Unknown,
    Nil,
    Number,
    Bool,
    String,
};

class TypeError final : public std::runtime_error {
public:
    explicit TypeError(const std::string& message);
};

class TypeChecker {
public:
    void check(const Program& program);

private:
    using Scope = std::unordered_map<std::string, StaticType>;

    void beginScope();
    void endScope();
    Scope& currentScope();
    const Scope& currentScope() const;
    StaticType* findVariable(const std::string& name);
    const StaticType* findVariable(const std::string& name) const;
    void declareVariable(const Token& name, StaticType type);

    void checkStatement(const Stmt& statement);
    StaticType checkExpression(const Expr& expression);
    StaticType checkLetInitializer(const LetStmt& statement);
    StaticType resolveAnnotation(const Token& typeName) const;
    void checkAssignable(const std::string& context, StaticType expected, StaticType actual) const;
    StaticType checkUnary(const UnaryExpr& expression);
    StaticType checkBinary(const BinaryExpr& expression);

    std::vector<Scope> scopes_;
};

std::string staticTypeName(StaticType type);
TYPECHECKER_HPP
```

Expected: header declares the type checker API and internal type enum.

- [ ] **Step 2: Add `TypeChecker.cpp`**

Run:

```bash
cat > src/TypeChecker.cpp <<'TYPECHECKER_CPP'
#include "TypeChecker.hpp"

#include <utility>

namespace {

bool isKnown(StaticType type)
{
    return type != StaticType::Unknown;
}

bool compatible(StaticType expected, StaticType actual)
{
    return !isKnown(expected) || !isKnown(actual) || expected == actual;
}

std::string binaryTypesMessage(const BinaryExpr& expression, StaticType left, StaticType right)
{
    return "binary `" + expression.op.lexeme + "` expects numbers, got "
        + staticTypeName(left) + " and " + staticTypeName(right);
}

} // namespace

TypeError::TypeError(const std::string& message)
    : std::runtime_error("Type error: " + message)
{
}

std::string staticTypeName(StaticType type)
{
    switch (type) {
    case StaticType::Unknown:
        return "unknown";
    case StaticType::Nil:
        return "nil";
    case StaticType::Number:
        return "number";
    case StaticType::Bool:
        return "bool";
    case StaticType::String:
        return "string";
    }

    return "unknown";
}

void TypeChecker::check(const Program& program)
{
    scopes_.clear();
    beginScope();
    for (const auto& statement : program.statements) {
        checkStatement(*statement);
    }
    endScope();
}

void TypeChecker::beginScope()
{
    scopes_.emplace_back();
}

void TypeChecker::endScope()
{
    if (scopes_.empty()) {
        throw TypeError("scope stack is empty");
    }
    scopes_.pop_back();
}

TypeChecker::Scope& TypeChecker::currentScope()
{
    if (scopes_.empty()) {
        throw TypeError("scope stack is empty");
    }
    return scopes_.back();
}

const TypeChecker::Scope& TypeChecker::currentScope() const
{
    if (scopes_.empty()) {
        throw TypeError("scope stack is empty");
    }
    return scopes_.back();
}

StaticType* TypeChecker::findVariable(const std::string& name)
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

const StaticType* TypeChecker::findVariable(const std::string& name) const
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

void TypeChecker::declareVariable(const Token& name, StaticType type)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError("variable `" + name.lexeme + "` already declared in this scope");
    }
    scope.emplace(name.lexeme, type);
}

void TypeChecker::checkStatement(const Stmt& statement)
{
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        beginScope();
        for (const auto& child : block->statements) {
            checkStatement(*child);
        }
        endScope();
        return;
    }

    if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&statement)) {
        checkExpression(*ifStmt->condition);
        checkStatement(*ifStmt->thenBranch);
        if (ifStmt->elseBranch) {
            checkStatement(*ifStmt->elseBranch);
        }
        return;
    }

    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const StaticType declared = checkLetInitializer(*let);
        declareVariable(let->name, declared);
        return;
    }

    if (const auto* print = dynamic_cast<const PrintStmt*>(&statement)) {
        checkExpression(*print->expression);
        return;
    }

    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        checkExpression(*expression->expression);
        return;
    }

    throw TypeError("unsupported statement node");
}

StaticType TypeChecker::checkLetInitializer(const LetStmt& statement)
{
    const StaticType initializer = checkExpression(*statement.initializer);
    if (!statement.typeName) {
        return StaticType::Unknown;
    }

    const StaticType declared = resolveAnnotation(*statement.typeName);
    checkAssignable(
        "cannot initialize `" + statement.name.lexeme + "` of type " + staticTypeName(declared)
            + " with " + staticTypeName(initializer),
        declared,
        initializer);
    return declared;
}

StaticType TypeChecker::checkExpression(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        if (literal->value == "nil") {
            return StaticType::Nil;
        }
        if (literal->value == "true" || literal->value == "false") {
            return StaticType::Bool;
        }
        if (literal->value.size() >= 2 && literal->value.front() == '"' && literal->value.back() == '"') {
            return StaticType::String;
        }
        return StaticType::Number;
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        const StaticType* type = findVariable(variable->name.lexeme);
        return type ? *type : StaticType::Unknown;
    }

    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const StaticType value = checkExpression(*assign->value);
        StaticType* target = findVariable(assign->name.lexeme);
        if (!target) {
            return value;
        }
        if (isKnown(*target) && isKnown(value) && *target != value) {
            throw TypeError("cannot assign " + staticTypeName(value) + " to `" + assign->name.lexeme
                + "` of type " + staticTypeName(*target));
        }
        return isKnown(*target) ? *target : value;
    }

    if (const auto* grouping = dynamic_cast<const GroupingExpr*>(&expression)) {
        return checkExpression(*grouping->expression);
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        return checkUnary(*unary);
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        return checkBinary(*binary);
    }

    throw TypeError("unsupported expression node");
}

StaticType TypeChecker::resolveAnnotation(const Token& typeName) const
{
    if (typeName.lexeme == "number") {
        return StaticType::Number;
    }
    if (typeName.lexeme == "bool") {
        return StaticType::Bool;
    }
    if (typeName.lexeme == "string") {
        return StaticType::String;
    }
    if (typeName.lexeme == "nil") {
        return StaticType::Nil;
    }
    throw TypeError("unknown type `" + typeName.lexeme + "`");
}

void TypeChecker::checkAssignable(const std::string& context, StaticType expected, StaticType actual) const
{
    if (!compatible(expected, actual)) {
        throw TypeError(context);
    }
}

StaticType TypeChecker::checkUnary(const UnaryExpr& expression)
{
    const StaticType right = checkExpression(*expression.right);
    switch (expression.op.type) {
    case TokenType::Minus:
        if (isKnown(right) && right != StaticType::Number) {
            throw TypeError("unary `-` expects number, got " + staticTypeName(right));
        }
        return StaticType::Number;
    case TokenType::Bang:
        return StaticType::Bool;
    default:
        throw TypeError("unsupported unary operator `" + expression.op.lexeme + "`");
    }
}

StaticType TypeChecker::checkBinary(const BinaryExpr& expression)
{
    const StaticType left = checkExpression(*expression.left);
    const StaticType right = checkExpression(*expression.right);

    switch (expression.op.type) {
    case TokenType::Plus:
        if (!isKnown(left) || !isKnown(right)) {
            return StaticType::Unknown;
        }
        if (left == StaticType::Number && right == StaticType::Number) {
            return StaticType::Number;
        }
        if (left == StaticType::String && right == StaticType::String) {
            return StaticType::String;
        }
        throw TypeError("binary `+` expects two numbers or two strings, got "
            + staticTypeName(left) + " and " + staticTypeName(right));
    case TokenType::Minus:
    case TokenType::Star:
    case TokenType::Slash:
        if (!isKnown(left) || !isKnown(right)) {
            return StaticType::Number;
        }
        if (left != StaticType::Number || right != StaticType::Number) {
            throw TypeError(binaryTypesMessage(expression, left, right));
        }
        return StaticType::Number;
    case TokenType::Greater:
    case TokenType::GreaterEqual:
    case TokenType::Less:
    case TokenType::LessEqual:
        if (!isKnown(left) || !isKnown(right)) {
            return StaticType::Bool;
        }
        if (left != StaticType::Number || right != StaticType::Number) {
            throw TypeError(binaryTypesMessage(expression, left, right));
        }
        return StaticType::Bool;
    case TokenType::EqualEqual:
    case TokenType::BangEqual:
        return StaticType::Bool;
    default:
        throw TypeError("unsupported binary operator `" + expression.op.lexeme + "`");
    }
}
TYPECHECKER_CPP
```

Expected: checker implementation compiles after CMake wiring.

- [ ] **Step 3: Add TypeChecker to CMake**

In `CMakeLists.txt`, add `src/TypeChecker.cpp` before `src/Value.cpp` in `add_executable`:

```cmake
    src/TypeChecker.cpp
```

Expected: the new source file is compiled into `compiler_demo`.

- [ ] **Step 4: Wire TypeChecker into `main.cpp`**

In `src/main.cpp`, add this include after `Parser.hpp`:

```cpp
#include "TypeChecker.hpp"
```

After:

```cpp
        Parser parser(tokens);
        Program program = parser.parse();
```

insert:

```cpp
        TypeChecker typeChecker;
        typeChecker.check(program);
```

Expected: all modes run type checking before output or IR compilation.

- [ ] **Step 5: Build and run type-error goldens**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: type-error fixtures now pass. Remaining failures, if any, should be intentional output changes or fixtures affected by duplicate declaration relocation.

- [ ] **Step 6: Commit implementation**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp CMakeLists.txt src/main.cpp
git commit -m "feat: add basic type checker"
```

Expected: commit succeeds with source/build changes only.

## Task 4: Refresh and Verify Goldens

**Files:**
- Modify: affected files under `tests/golden/**` only if required.

- [ ] **Step 1: Run golden update**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Expected: update mode succeeds with zero failures.

- [ ] **Step 2: Inspect type-error outputs**

Run:

```bash
for f in tests/golden/type_errors/*.err; do echo "--- $f"; cat "$f"; done
```

Expected output includes the six `Type error: ...` messages from Task 2.

- [ ] **Step 3: Run golden verification**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass with zero failures. Record the observed pass count in the final report.

- [ ] **Step 4: Commit golden updates if any**

Run:

```bash
if ! git diff --quiet -- tests/golden; then
  git add tests/golden
  git commit -m "test: refresh type checker goldens"
else
  echo "No golden refresh changes to commit"
fi
```

Expected: either a focused golden commit is created or the command prints that no golden changes were needed.

## Task 5: Update Documentation

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README type annotation wording**

In `README.md`, replace:

```text
Type annotations on `let` declarations are currently syntax-only: they are parsed and shown in the AST, but they are not type-checked yet. Blocks introduce lexical scope: variables declared inside a block are not visible outside it, and inner blocks may shadow outer variables. Re-declaring a variable in the same scope is a runtime error.
```

with:

```text
Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated variables are still accepted and are not fully inferred yet. Blocks introduce lexical scope: variables declared inside a block are not visible outside it, and inner blocks may shadow outer variables. Re-declaring a variable in the same scope is a type error.
```

Expected: README describes the explicit-annotation checker.

- [ ] **Step 2: Update README golden fixture wording**

In `README.md`, replace:

```text
Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, or `run.out` files to cover successful syntax. Runtime-error fixtures live in `tests/golden/runtime_errors`: for `example.cd`, add matching `example.run.err` and `example.exit` files. Parse-error fixtures live in `tests/golden/parse_errors`: for `example.cd`, add matching `example.err` and `example.exit` files.
```

with:

```text
Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, or `run.out` files to cover successful syntax. Runtime-error fixtures live in `tests/golden/runtime_errors`: for `example.cd`, add matching `example.run.err` and `example.exit` files. Parse-error fixtures live in `tests/golden/parse_errors`: for `example.cd`, add matching `example.err` and `example.exit` files. Type-error fixtures live in `tests/golden/type_errors`: for `example.cd`, add matching `example.err` and `example.exit` files.
```

Expected: README includes the new fixture category.

- [ ] **Step 3: Update AGENTS semantics and fixture conventions**

In `AGENTS.md`, replace:

```text
- `let name: type = expression;` parses and prints type annotations, but annotations are syntax-only and are not type-checked yet.
```

with:

```text
- `let name: type = expression;` checks explicit annotations for `number`, `bool`, `string`, and `nil`; unannotated variables are accepted without full inference.
```

After the parse-error fixture block, add this text:

```md
Type-error fixtures live under `tests/golden/type_errors`:

    tests/golden/type_errors/<case>.cd
    tests/golden/type_errors/<case>.err
    tests/golden/type_errors/<case>.exit
```

Expected: agent memory documents the type checker and type-error fixtures.

- [ ] **Step 4: Update roadmap Phase 2 status**

In `docs/roadmap.md`, replace:

```text
## Phase 2: Basic Type Checker

Goal: make `let name: type = expression;` meaningful instead of syntax-only.
```

with:

```text
## Phase 2: Basic Type Checker — Implemented

Status: implemented for explicit `let` annotations. Unannotated variables are still accepted without full inference.
```

Expected: roadmap marks Phase 2 complete without claiming full inference.

- [ ] **Step 5: Review documentation diff**

Run:

```bash
git diff -- README.md AGENTS.md docs/roadmap.md
```

Expected: diff only documents implemented type-checker behavior and fixture conventions.

- [ ] **Step 6: Commit docs**

Run:

```bash
git add README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document basic type checking"
```

Expected: commit succeeds with documentation changes only.

## Task 6: Final Verification

**Files:**
- Verify all touched files.

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
100% tests passed, 0 tests failed out of 2
golden tests: 0 failed
Ran 9 tests
OK
```

Record exact golden pass count in the final report.

- [ ] **Step 2: Check repository status**

Run:

```bash
git status --short
```

Expected: no output.

- [ ] **Step 3: Summarize recent commits**

Run:

```bash
git log --oneline -7
```

Expected: recent commits include runner support, red type-error fixtures, type-checker implementation, golden refresh if needed, and docs.

Final report should mention:

- explicit annotations `number`, `bool`, `string`, and `nil` are checked
- unannotated variables remain accepted without full inference
- same-scope duplicate declarations are type errors
- type-error goldens live under `tests/golden/type_errors`
- exact verification commands and results
