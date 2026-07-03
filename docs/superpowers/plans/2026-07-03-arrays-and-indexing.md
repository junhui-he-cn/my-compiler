# Arrays and Indexing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Phase 7 array literals and read-only indexing, including mixed element arrays, chained postfix indexing/calls, static type checks, runtime index errors, goldens, and docs.

**Architecture:** Extend the existing vertical compiler path: lexer tokens, AST nodes, parser postfix grammar, type checker, IR operations, IR compiler, runtime `Value`, and IR interpreter. Arrays are immutable-length runtime values with identity equality; indexing is read-only and uses runtime validation for dynamic values. Calls and indexing share one postfix precedence loop so expressions such as `makeArray()[0]` and `nested[0][1]` work naturally.

**Tech Stack:** C++17, CMake, recursive-descent parser, AST printer, type checker, register IR, IR interpreter, Python golden tests, CTest.

---

## File Structure

- Modify: `include/Token.hpp`, `src/Lexer.cpp` — add `LeftBracket` and `RightBracket` tokens and scanner cases.
- Modify: `include/Ast.hpp`, `src/Ast.cpp` — add `ArrayExpr` and `IndexExpr` AST nodes and printing.
- Modify: `include/Parser.hpp`, `src/Parser.cpp` — parse array literals and index suffixes in the postfix/call precedence level.
- Modify: `include/TypeChecker.hpp`, `src/TypeChecker.cpp` — add `StaticType::Array`, array literal checking, and index expression checking.
- Modify: `include/Value.hpp`, `src/Value.cpp` — add array runtime values, identity equality, and array string formatting.
- Modify: `include/IR.hpp`, `src/IR.cpp` — add `Array` and `Index` IR ops, emit helpers, and IR printing.
- Modify: `include/IRCompiler.hpp`, `src/IRCompiler.cpp` — lower array literals and index expressions.
- Modify: `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp` — execute array construction and indexing with runtime diagnostics.
- Create: success fixtures under `tests/golden/array_*`.
- Create: parse-error fixtures under `tests/golden/parse_errors/array_*`.
- Create: type-error fixtures under `tests/golden/type_errors/index_*`.
- Create: runtime-error fixtures under `tests/golden/runtime_errors/array_*`.
- Modify: `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, `AGENTS.md`.

## Task 0: Prepare Workspace and Baseline

**Files:**
- Verify only.

- [ ] **Step 1: Use the worktree skill before editing**

Invoke `superpowers:using-git-worktrees` before implementation. Use branch name:

```text
arrays-and-indexing
```

If already inside an isolated worktree on branch `arrays-and-indexing`, report that and continue. Do not implement on `master` unless the user explicitly authorizes it.

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

Expected baseline:

```text
100% tests passed, 0 tests failed out of 2
golden tests: 94 passed, 0 failed
Ran 9 tests
OK
```

If baseline fails before array edits, stop and report the exact failing command and output.

## Task 1: Add RED Array Golden Tests

**Files:**
- Create: `tests/golden/array_literal_print/input.cd`
- Create: `tests/golden/array_literal_print/ast.out`
- Create: `tests/golden/array_literal_print/run.out`
- Create: `tests/golden/array_index_basic/input.cd`
- Create: `tests/golden/array_index_basic/ast.out`
- Create: `tests/golden/array_index_basic/run.out`
- Create: `tests/golden/array_empty/input.cd`
- Create: `tests/golden/array_empty/ast.out`
- Create: `tests/golden/array_empty/run.out`
- Create: `tests/golden/array_nested/input.cd`
- Create: `tests/golden/array_nested/ast.out`
- Create: `tests/golden/array_nested/run.out`
- Create: `tests/golden/array_function_value/input.cd`
- Create: `tests/golden/array_function_value/run.out`
- Create: `tests/golden/parse_errors/array_literal_missing_bracket.cd`
- Create: `tests/golden/parse_errors/array_literal_missing_bracket.err`
- Create: `tests/golden/parse_errors/array_literal_missing_bracket.exit`
- Create: `tests/golden/parse_errors/array_index_missing_bracket.cd`
- Create: `tests/golden/parse_errors/array_index_missing_bracket.err`
- Create: `tests/golden/parse_errors/array_index_missing_bracket.exit`
- Create: `tests/golden/parse_errors/array_trailing_comma.cd`
- Create: `tests/golden/parse_errors/array_trailing_comma.err`
- Create: `tests/golden/parse_errors/array_trailing_comma.exit`
- Create: `tests/golden/type_errors/index_non_array.cd`
- Create: `tests/golden/type_errors/index_non_array.err`
- Create: `tests/golden/type_errors/index_non_array.exit`
- Create: `tests/golden/type_errors/index_non_number.cd`
- Create: `tests/golden/type_errors/index_non_number.err`
- Create: `tests/golden/type_errors/index_non_number.exit`
- Create: `tests/golden/runtime_errors/array_index_out_of_range.cd`
- Create: `tests/golden/runtime_errors/array_index_out_of_range.run.err`
- Create: `tests/golden/runtime_errors/array_index_out_of_range.exit`
- Create: `tests/golden/runtime_errors/array_index_non_integer.cd`
- Create: `tests/golden/runtime_errors/array_index_non_integer.run.err`
- Create: `tests/golden/runtime_errors/array_index_non_integer.exit`
- Create: `tests/golden/runtime_errors/array_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/array_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/array_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/array_dynamic_non_number_index.cd`
- Create: `tests/golden/runtime_errors/array_dynamic_non_number_index.run.err`
- Create: `tests/golden/runtime_errors/array_dynamic_non_number_index.exit`

- [ ] **Step 1: Add mixed array literal fixture**

Run:

```bash
mkdir -p tests/golden/array_literal_print
cat > tests/golden/array_literal_print/input.cd <<'CASE'
print [1, "two", true, nil];
CASE
cat > tests/golden/array_literal_print/ast.out <<'CASE'
Program
  Print (array 1 "two" true nil)
CASE
cat > tests/golden/array_literal_print/run.out <<'CASE'
[1, two, true, nil]
CASE
```

- [ ] **Step 2: Add basic indexing fixture**

Run:

```bash
mkdir -p tests/golden/array_index_basic
cat > tests/golden/array_index_basic/input.cd <<'CASE'
let xs = [10, 20, 30];
print xs[0];
print xs[1 + 1];
CASE
cat > tests/golden/array_index_basic/ast.out <<'CASE'
Program
  Let xs = (array 10 20 30)
  Print (index xs 0)
  Print (index xs (+ 1 1))
CASE
cat > tests/golden/array_index_basic/run.out <<'CASE'
10
30
CASE
```

- [ ] **Step 3: Add empty array fixture**

Run:

```bash
mkdir -p tests/golden/array_empty
cat > tests/golden/array_empty/input.cd <<'CASE'
print [];
CASE
cat > tests/golden/array_empty/ast.out <<'CASE'
Program
  Print (array)
CASE
cat > tests/golden/array_empty/run.out <<'CASE'
[]
CASE
```

- [ ] **Step 4: Add nested indexing fixture**

Run:

```bash
mkdir -p tests/golden/array_nested
cat > tests/golden/array_nested/input.cd <<'CASE'
let xs = [[1, 2], [3, 4]];
print xs[1][0];
CASE
cat > tests/golden/array_nested/ast.out <<'CASE'
Program
  Let xs = (array (array 1 2) (array 3 4))
  Print (index (index xs 1) 0)
CASE
cat > tests/golden/array_nested/run.out <<'CASE'
3
CASE
```

- [ ] **Step 5: Add function value in array fixture**

Run:

```bash
mkdir -p tests/golden/array_function_value
cat > tests/golden/array_function_value/input.cd <<'CASE'
fun makeAdder(base) {
  fun add(x) {
    return base + x;
  }
  return add;
}
let add10 = makeAdder(10);
let functions = [add10];
print functions[0](5);
CASE
cat > tests/golden/array_function_value/run.out <<'CASE'
15
CASE
```

- [ ] **Step 6: Add parse-error fixtures**

Run:

```bash
cat > tests/golden/parse_errors/array_literal_missing_bracket.cd <<'CASE'
print [1, 2;
CASE
cat > tests/golden/parse_errors/array_literal_missing_bracket.err <<'CASE'
Parse error at 1:12: expected `]` after array elements, found Semicolon `;`
CASE
cat > tests/golden/parse_errors/array_literal_missing_bracket.exit <<'CASE'
1
CASE
cat > tests/golden/parse_errors/array_index_missing_bracket.cd <<'CASE'
let xs = [1];
print xs[0;
CASE
cat > tests/golden/parse_errors/array_index_missing_bracket.err <<'CASE'
Parse error at 2:11: expected `]` after index, found Semicolon `;`
CASE
cat > tests/golden/parse_errors/array_index_missing_bracket.exit <<'CASE'
1
CASE
cat > tests/golden/parse_errors/array_trailing_comma.cd <<'CASE'
print [1,];
CASE
cat > tests/golden/parse_errors/array_trailing_comma.err <<'CASE'
Parse error at 1:10: expected expression
CASE
cat > tests/golden/parse_errors/array_trailing_comma.exit <<'CASE'
1
CASE
```

- [ ] **Step 7: Add type-error fixtures**

Run:

```bash
cat > tests/golden/type_errors/index_non_array.cd <<'CASE'
print 1[0];
CASE
cat > tests/golden/type_errors/index_non_array.err <<'CASE'
Type error at 1:8: can only index arrays
CASE
cat > tests/golden/type_errors/index_non_array.exit <<'CASE'
1
CASE
cat > tests/golden/type_errors/index_non_number.cd <<'CASE'
let xs = [1];
print xs["0"];
CASE
cat > tests/golden/type_errors/index_non_number.err <<'CASE'
Type error at 2:9: array index must be number
CASE
cat > tests/golden/type_errors/index_non_number.exit <<'CASE'
1
CASE
```

- [ ] **Step 8: Add runtime-error fixtures**

Run:

```bash
cat > tests/golden/runtime_errors/array_index_out_of_range.cd <<'CASE'
let xs = [1];
print xs[1];
CASE
cat > tests/golden/runtime_errors/array_index_out_of_range.run.err <<'CASE'
Runtime error: array index out of range
CASE
cat > tests/golden/runtime_errors/array_index_out_of_range.exit <<'CASE'
1
CASE
cat > tests/golden/runtime_errors/array_index_non_integer.cd <<'CASE'
let xs = [1, 2];
print xs[1.5];
CASE
cat > tests/golden/runtime_errors/array_index_non_integer.run.err <<'CASE'
Runtime error: array index must be integer
CASE
cat > tests/golden/runtime_errors/array_index_non_integer.exit <<'CASE'
1
CASE
cat > tests/golden/runtime_errors/array_dynamic_non_array.cd <<'CASE'
fun id(x) {
  return x;
}
print id(1)[0];
CASE
cat > tests/golden/runtime_errors/array_dynamic_non_array.run.err <<'CASE'
Runtime error: can only index arrays
CASE
cat > tests/golden/runtime_errors/array_dynamic_non_array.exit <<'CASE'
1
CASE
cat > tests/golden/runtime_errors/array_dynamic_non_number_index.cd <<'CASE'
fun id(x) {
  return x;
}
let xs = [1];
print xs[id("0")];
CASE
cat > tests/golden/runtime_errors/array_dynamic_non_number_index.run.err <<'CASE'
Runtime error: array index must be number
CASE
cat > tests/golden/runtime_errors/array_dynamic_non_number_index.exit <<'CASE'
1
CASE
```

- [ ] **Step 9: Run golden tests and verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL. Array fixtures should fail with lex errors for `[`/`]` or parse/type/runtime mismatches because arrays are not implemented yet.

- [ ] **Step 10: Commit RED array fixtures**

Run:

```bash
git add tests/golden/array_literal_print \
  tests/golden/array_index_basic \
  tests/golden/array_empty \
  tests/golden/array_nested \
  tests/golden/array_function_value \
  tests/golden/parse_errors/array_literal_missing_bracket.* \
  tests/golden/parse_errors/array_index_missing_bracket.* \
  tests/golden/parse_errors/array_trailing_comma.* \
  tests/golden/type_errors/index_non_array.* \
  tests/golden/type_errors/index_non_number.* \
  tests/golden/runtime_errors/array_index_out_of_range.* \
  tests/golden/runtime_errors/array_index_non_integer.* \
  tests/golden/runtime_errors/array_dynamic_non_array.* \
  tests/golden/runtime_errors/array_dynamic_non_number_index.*
git commit -m "test: add array and indexing goldens"
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

- [ ] **Step 1: Add bracket tokens**

Edit `include/Token.hpp`.

Add tokens after `RightParen`:

```cpp
    LeftBracket,
    RightBracket,
```

- [ ] **Step 2: Scan bracket tokens and names**

Edit `src/Lexer.cpp`.

Add scanner cases after the right-paren case:

```cpp
    case '[':
        addToken(TokenType::LeftBracket);
        break;
    case ']':
        addToken(TokenType::RightBracket);
        break;
```

Add `tokenTypeName()` cases after `RightParen`:

```cpp
    case TokenType::LeftBracket:
        return "LeftBracket";
    case TokenType::RightBracket:
        return "RightBracket";
```

- [ ] **Step 3: Add AST declarations**

Edit `include/Ast.hpp`.

Add `ArrayExpr` and `IndexExpr` after `CallExpr`:

```cpp
struct ArrayExpr final : Expr {
    explicit ArrayExpr(std::vector<ExprPtr> elements);
    void print(std::ostream& out) const override;

    std::vector<ExprPtr> elements;
};

struct IndexExpr final : Expr {
    IndexExpr(ExprPtr collection, Token bracket, ExprPtr index);
    void print(std::ostream& out) const override;

    ExprPtr collection;
    Token bracket;
    ExprPtr index;
};
```

- [ ] **Step 4: Implement AST printing**

Edit `src/Ast.cpp`.

Add after `CallExpr::print()`:

```cpp
ArrayExpr::ArrayExpr(std::vector<ExprPtr> elements)
    : elements(std::move(elements))
{
}

void ArrayExpr::print(std::ostream& out) const
{
    out << "(array";
    for (const auto& element : elements) {
        out << ' ';
        writeExpr(out, element);
    }
    out << ')';
}

IndexExpr::IndexExpr(ExprPtr collection, Token bracket, ExprPtr index)
    : collection(std::move(collection))
    , bracket(std::move(bracket))
    , index(std::move(index))
{
}

void IndexExpr::print(std::ostream& out) const
{
    out << "(index ";
    writeExpr(out, collection);
    out << ' ';
    writeExpr(out, index);
    out << ')';
}
```

- [ ] **Step 5: Add parser declarations**

Edit `include/Parser.hpp`.

Add these private methods near `finishCall()` and `primary()`:

```cpp
    ExprPtr finishIndex(ExprPtr collection);
    ExprPtr arrayLiteral();
```

- [ ] **Step 6: Parse index suffixes**

Edit `src/Parser.cpp`.

In `Parser::call()`, replace the loop body:

```cpp
        if (match(TokenType::LeftParen)) {
            expr = finishCall(std::move(expr));
        } else {
            break;
        }
```

with:

```cpp
        if (match(TokenType::LeftParen)) {
            expr = finishCall(std::move(expr));
        } else if (match(TokenType::LeftBracket)) {
            expr = finishIndex(std::move(expr));
        } else {
            break;
        }
```

Add `finishIndex()` after `finishCall()`:

```cpp
ExprPtr Parser::finishIndex(ExprPtr collection)
{
    Token bracket = previous();
    ExprPtr index = expression();
    consume(TokenType::RightBracket, "expected `]` after index");
    return std::make_unique<IndexExpr>(std::move(collection), std::move(bracket), std::move(index));
}
```

- [ ] **Step 7: Parse array literals**

In `Parser::primary()`, add before the identifier branch:

```cpp
    if (match(TokenType::LeftBracket)) {
        return arrayLiteral();
    }
```

Add `arrayLiteral()` before `primary()`:

```cpp
ExprPtr Parser::arrayLiteral()
{
    std::vector<ExprPtr> elements;
    if (!check(TokenType::RightBracket)) {
        do {
            elements.push_back(expression());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBracket, "expected `]` after array elements");
    return std::make_unique<ArrayExpr>(std::move(elements));
}
```

The parser will reject `[1,]` because after the comma it calls `expression()` and sees `]`, producing `expected expression`.

- [ ] **Step 8: Run parser-layer tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: parse-error fixtures pass. AST fixtures for arrays may still fail at type checking with `unsupported expression node` until Task 3 adds type checking.

- [ ] **Step 9: Commit parser support**

Run:

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse array literals and indexes"
```

Expected: commit succeeds.

## Task 3: Add TypeChecker Support

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add array static type declaration**

Edit `include/TypeChecker.hpp`.

Add `Array` after `Function`:

```cpp
    Function,
    Array,
```

Add private helper declaration near `checkCall()`:

```cpp
    StaticType checkIndex(const IndexExpr& expression);
```

- [ ] **Step 2: Add array type name**

Edit `src/TypeChecker.cpp`.

In `staticTypeName()`, add:

```cpp
    case StaticType::Array:
        return "array";
```

- [ ] **Step 3: Type-check array literals and indexes**

In `TypeChecker::checkExpression()`, add before the unsupported expression error:

```cpp
    if (const auto* array = dynamic_cast<const ArrayExpr*>(&expression)) {
        for (const auto& element : array->elements) {
            checkExpression(*element);
        }
        return StaticType::Array;
    }

    if (const auto* index = dynamic_cast<const IndexExpr*>(&expression)) {
        return checkIndex(*index);
    }
```

Add `checkIndex()` after `checkCall()`:

```cpp
StaticType TypeChecker::checkIndex(const IndexExpr& expression)
{
    const StaticType collection = checkExpression(*expression.collection);
    const StaticType index = checkExpression(*expression.index);

    if (collection != StaticType::Unknown && collection != StaticType::Array) {
        throw TypeError(expression.bracket, "can only index arrays");
    }

    if (index != StaticType::Unknown && index != StaticType::Number) {
        throw TypeError(expression.bracket, "array index must be number");
    }

    return StaticType::Unknown;
}
```

- [ ] **Step 4: Run type-checker tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: type-error fixtures pass. Success and runtime fixtures may fail with compile errors because IR lowering is not implemented yet.

- [ ] **Step 5: Commit type checker support**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: type check array indexing"
```

Expected: commit succeeds.

## Task 4: Add Array Runtime Value Support

**Files:**
- Modify: `include/Value.hpp`
- Modify: `src/Value.cpp`

- [ ] **Step 1: Add array value declarations**

Edit `include/Value.hpp`.

Ensure these includes exist:

```cpp
#include <memory>
#include <vector>
```

Forward declare `ArrayValue` before the `Value` class:

```cpp
struct ArrayValue;
```

Add `Array` to `Value::Type` after `Function`:

```cpp
        Function,
        Array,
```

Add factory and accessor:

```cpp
    static Value array(ArrayValue value);
    const ArrayValue& asArray() const;
```

Add private member:

```cpp
    std::shared_ptr<ArrayValue> array_;
```

Add `ArrayValue` after the `Value` class definition and before `Cell`:

```cpp
struct ArrayValue {
    std::size_t identity = 0;
    std::shared_ptr<std::vector<Value>> elements;
};
```

- [ ] **Step 2: Implement array value behavior**

Edit `src/Value.cpp`.

Add factory after `Value::function()`:

```cpp
Value Value::array(ArrayValue value)
{
    Value result(Type::Array);
    result.array_ = std::make_shared<ArrayValue>(std::move(value));
    return result;
}
```

Add accessor after `Value::asFunction()`:

```cpp
const ArrayValue& Value::asArray() const
{
    if (type_ != Type::Array || !array_) {
        throw std::runtime_error("value is not an array");
    }
    return *array_;
}
```

In `valuesEqual()`, add:

```cpp
    case Value::Type::Array:
        return left.asArray().identity == right.asArray().identity;
```

In `valueToString()`, add:

```cpp
    case Value::Type::Array: {
        std::ostringstream out;
        out << '[';
        const auto& elements = *value.asArray().elements;
        for (std::size_t i = 0; i < elements.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << valueToString(elements[i]);
        }
        out << ']';
        return out.str();
    }
```

- [ ] **Step 3: Build value layer**

Run:

```bash
cmake --build build
```

Expected: build succeeds or fails because `typeName()` in `IRInterpreter.cpp` does not yet cover `Value::Type::Array`. If it fails with a non-exhaustive warning treated as error, add the `typeName()` case in Task 6. The current build normally succeeds because warnings are not errors.

- [ ] **Step 4: Commit array values**

Run:

```bash
git add include/Value.hpp src/Value.cpp
git commit -m "feat: add array runtime values"
```

Expected: commit succeeds.

## Task 5: Add Array IR and Compiler Lowering

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Add IR operations and emit helpers**

Edit `include/IR.hpp`.

Add ops:

```cpp
    Array,
```

after `MakeFunction`, and:

```cpp
    Index,
```

after `Call`.

Add public helpers near `emitMakeFunction()` and `emitCall()`:

```cpp
    IRRegister emitArray(std::vector<IRRegister> elements);
    IRRegister emitIndex(IRRegister collection, IRRegister index);
```

- [ ] **Step 2: Implement IR emit helpers**

Edit `src/IR.cpp`.

In `isBinary()` non-binary cases, add `IROp::Array` and `IROp::Index`.

Add after `emitMakeFunction()`:

```cpp
IRRegister IRProgram::emitArray(std::vector<IRRegister> elements)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Array, dest, std::nullopt, std::nullopt, std::move(elements), 0});
    return dest;
}
```

Add after `emitCall()`:

```cpp
IRRegister IRProgram::emitIndex(IRRegister collection, IRRegister index)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Index, dest, collection, index, {}, 0});
    return dest;
}
```

- [ ] **Step 3: Add IR printing**

In `printInstruction()`, add after `MakeFunction` branch:

```cpp
    } else if (instruction.op == IROp::Array) {
        out << " [";
        for (std::size_t arg = 0; arg < instruction.arguments.size(); ++arg) {
            if (arg != 0) {
                out << ", ";
            }
            out << instruction.arguments[arg];
        }
        out << "]";
```

Add after `Call` branch:

```cpp
    } else if (instruction.op == IROp::Index) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        if (instruction.right) {
            out << ", " << *instruction.right;
        }
```

In `irOpName()`, add:

```cpp
    case IROp::Array:
        return "array";
    case IROp::Index:
        return "index";
```

- [ ] **Step 4: Add compiler helper declarations**

Edit `include/IRCompiler.hpp`.

Add private helpers near `emitCall()`:

```cpp
    IRRegister emitArray(const ArrayExpr& expression);
    IRRegister emitIndex(const IndexExpr& expression);
```

- [ ] **Step 5: Lower array and index expressions**

Edit `src/IRCompiler.cpp`.

In `compileExpression()`, add before unsupported expression:

```cpp
    if (const auto* array = dynamic_cast<const ArrayExpr*>(&expression)) {
        return emitArray(*array);
    }

    if (const auto* index = dynamic_cast<const IndexExpr*>(&expression)) {
        return emitIndex(*index);
    }
```

Add after `emitCall()`:

```cpp
IRRegister IRCompiler::emitArray(const ArrayExpr& expression)
{
    std::vector<IRRegister> elements;
    for (const auto& element : expression.elements) {
        elements.push_back(compileExpression(*element));
    }
    return ir_.emitArray(std::move(elements));
}

IRRegister IRCompiler::emitIndex(const IndexExpr& expression)
{
    IRRegister collection = compileExpression(*expression.collection);
    IRRegister index = compileExpression(*expression.index);
    return ir_.emitIndex(collection, index);
}
```

- [ ] **Step 6: Build and run IR/compiler stage**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: build succeeds. Success fixtures may fail at runtime until Task 6 executes `Array` and `Index`. IR fixtures should print array/index if any are present.

- [ ] **Step 7: Commit IR/compiler support**

Run:

```bash
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp
git commit -m "feat: compile arrays and indexes to IR"
```

Expected: commit succeeds.

## Task 6: Execute Arrays and Indexes in IRInterpreter

**Files:**
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Possibly update: array fixture `ir.out` files if added later.

- [ ] **Step 1: Add array identity state and helper declarations**

Edit `include/IRInterpreter.hpp`.

Add private helper declarations near arithmetic helpers:

```cpp
    Value executeArray(const IRInstruction& instruction, const Frame& frame);
    Value executeIndex(const Frame& frame, IRRegister collection, IRRegister index);
```

Add member near `nextFunctionIdentity_`:

```cpp
    std::size_t nextArrayIdentity_ = 1;
```

- [ ] **Step 2: Reset array identity per execution**

Edit `src/IRInterpreter.cpp`.

In `IRInterpreter::execute()`, after resetting `nextFunctionIdentity_ = 1;`, add:

```cpp
    nextArrayIdentity_ = 1;
```

- [ ] **Step 3: Add array type name**

In the anonymous `typeName(Value::Type type)` switch, add:

```cpp
    case Value::Type::Array:
        return "array";
```

- [ ] **Step 4: Execute Array and Index ops**

In `executeInstructions()`, add after `IROp::MakeFunction` case:

```cpp
        case IROp::Array:
            writeRegister(frame, readDest(instruction), executeArray(instruction, frame));
            break;
```

Add after `IROp::Call` case:

```cpp
        case IROp::Index:
            writeRegister(frame, readDest(instruction), executeIndex(frame, readLeft(instruction), readRight(instruction)));
            break;
```

- [ ] **Step 5: Implement executeArray and executeIndex**

Add these definitions after `callFunction()`:

```cpp
Value IRInterpreter::executeArray(const IRInstruction& instruction, const Frame& frame)
{
    auto elements = std::make_shared<std::vector<Value>>();
    elements->reserve(instruction.arguments.size());
    for (IRRegister argument : instruction.arguments) {
        elements->push_back(readRegister(frame, argument));
    }
    return Value::array(ArrayValue{nextArrayIdentity_++, std::move(elements)});
}

Value IRInterpreter::executeIndex(const Frame& frame, IRRegister collection, IRRegister index)
{
    const Value& collectionValue = readRegister(frame, collection);
    if (collectionValue.type() != Value::Type::Array) {
        throw IRRuntimeError("can only index arrays");
    }

    const Value& indexValue = readRegister(frame, index);
    if (indexValue.type() != Value::Type::Number) {
        throw IRRuntimeError("array index must be number");
    }

    const double numericIndex = indexValue.asNumber();
    const double integerIndex = std::trunc(numericIndex);
    if (integerIndex != numericIndex) {
        throw IRRuntimeError("array index must be integer");
    }
    if (integerIndex < 0) {
        throw IRRuntimeError("array index out of range");
    }

    const auto& elements = *collectionValue.asArray().elements;
    const auto position = static_cast<std::size_t>(integerIndex);
    if (position >= elements.size()) {
        throw IRRuntimeError("array index out of range");
    }

    return elements[position];
}
```

`src/IRInterpreter.cpp` already includes `<cmath>`, so `std::trunc` is available.

- [ ] **Step 6: Run golden tests and verify behavior**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all array success, type-error, and runtime-error fixtures pass. If only IR expected files are missing for array fixtures, add them in Task 7.

- [ ] **Step 7: Commit interpreter support**

Run:

```bash
git add include/IRInterpreter.hpp src/IRInterpreter.cpp
git commit -m "feat: execute array indexing"
```

Expected: commit succeeds.

## Task 7: Add Focused IR Golden Coverage

**Files:**
- Create: `tests/golden/array_index_basic/ir.out`

- [ ] **Step 1: Generate array IR golden**

Run:

```bash
./build/compiler_demo --ir tests/golden/array_index_basic/input.cd > tests/golden/array_index_basic/ir.out
git diff -- tests/golden/array_index_basic/ir.out
```

Expected: `ir.out` includes `array [...]` and `index` instructions.

- [ ] **Step 2: Run golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass with zero failures.

- [ ] **Step 3: Commit array IR golden**

Run:

```bash
git add tests/golden/array_index_basic/ir.out
git commit -m "test: document array index IR"
```

Expected: commit succeeds.

## Task 8: Update Documentation

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar**

Edit `docs/language-grammar.ebnf`.

Replace the `call` production:

```ebnf
call        = primary,
              { "(", [ arguments ], ")" } ;
```

with:

```ebnf
call        = primary,
              { "(", [ arguments ], ")"
              | "[", expression, "]" } ;
```

Add `array` to `primary` before `identifier`:

```ebnf
            | array
```

Add this production after `arguments`:

```ebnf
array       = "[", [ arguments ], "]" ;
```

The grammar reuses `arguments` for comma-separated array elements.

- [ ] **Step 2: Update README language section**

Edit `README.md`.

In supported expressions, add after literals:

```markdown
- Arrays: `[element, ...]` and `[]`; elements may be mixed runtime types.
```

Add after calls:

```markdown
- Indexing: `array[index]` reads an element. Indexes must be integer numbers in range. Array mutation, `push`, and `len` are not implemented yet.
```

- [ ] **Step 3: Update roadmap Phase 7**

Edit `docs/roadmap.md`.

Replace Phase 7 opening:

```markdown
## Phase 7: Arrays and Indexing

Goal: add a first compound data type.
```

with:

```markdown
## Phase 7: Arrays and Indexing — Implemented

Status: implemented. The language supports array literals, mixed element values, read-only indexing, chained indexing/calls, and runtime errors for invalid index operations. Array mutation, `push`, and `len` remain future work.
```

- [ ] **Step 4: Update AGENTS current semantics**

Edit `AGENTS.md`.

Replace the supported expressions bullet:

```markdown
- Supported expressions include literals, variables, calls, grouping, unary operators, binary/logical operators, and assignment expressions.
```

with:

```markdown
- Supported expressions include literals, arrays, indexing, variables, calls, grouping, unary operators, binary/logical operators, and assignment expressions.
```

Add after runtime values bullet:

```markdown
- Arrays are immutable-length runtime values with mixed element types. Indexing is read-only and validates array-ness, numeric integer indexes, and bounds at runtime when static types are unknown.
```

- [ ] **Step 5: Review documentation diff**

Run:

```bash
git diff -- docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
```

Expected: docs describe arrays as implemented and do not document mutation, push, len, or array type annotations as implemented.

- [ ] **Step 6: Commit docs**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document arrays and indexing"
```

Expected: commit succeeds.

## Task 9: Full Verification and Cleanup

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
100% tests passed, 0 tests failed out of 2
golden tests: all cases passed with 0 failed
Ran 9 tests
OK
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

Include this concise feature summary:

```text
Implemented Phase 7 arrays and indexing with mixed element array literals, read-only index expressions, chained postfix indexing/calls, array IR, runtime index validation, type diagnostics, goldens, and docs.
```

After this task, use `superpowers:finishing-a-development-branch` to offer merge/PR/keep/discard options.
