# Member Call Sugar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add builtin-only member-call sugar such as `xs.push(1)`, `xs.len()`, and `text.substr(1, 3)` while preserving existing builtin runtime behavior and bytecode/Rust VM parity.

**Architecture:** Add a distinct `MemberCallExpr` AST node when the parser sees a field access followed by call parentheses. Type checking validates only the supported builtin member names and receiver/argument types. IR lowering compiles the receiver once, prepends it to existing native calls, and uses the existing `Len` IR for `.len()`.

**Tech Stack:** C++17 recursive-descent parser, AST/type checker/IR compiler, Python golden tests, C++ bytecode artifacts, Rust VM parity tests.

---

## File Structure

- `include/Ast.hpp`: declare `MemberCallExpr` next to `CallExpr` and `FieldAccessExpr`.
- `src/Ast.cpp`: implement `MemberCallExpr` constructor and stable AST printer output.
- `src/Parser.cpp`: convert `FieldAccessExpr` callees in `finishCall` to `MemberCallExpr`.
- `include/TypeChecker.hpp`: declare `checkMemberCall`.
- `src/TypeChecker.cpp`: dispatch `MemberCallExpr` and validate builtin member names, arities, receivers, arguments, and return types.
- `include/IRCompiler.hpp`: declare `emitMemberCall`.
- `src/IRCompiler.cpp`: dispatch `MemberCallExpr` and lower to existing `Len`/`NativeCall` IR.
- `tests/golden/member_calls_arrays/`: success fixture for `push`, `pop`, `len`, and expression results.
- `tests/golden/member_calls_strings/`: success fixture for `len`, `substr`, `charAt`, and expression results.
- `tests/golden/member_calls_shadowing/`: success fixture proving lexical `push`, `pop`, `len`, `substr`, and `charAt` do not shadow member sugar.
- `tests/golden/type_errors/member_call_*.cd|.err|.exit`: static error fixtures for unsupported members, wrong arity, wrong receivers, and wrong arguments.
- `tests/golden/runtime_errors/member_call_*.cd|.run.err|.exit`: dynamic error fixtures for unknown receiver values.
- `tests/bytecode_artifacts/member_calls_arrays/` and `tests/bytecode_artifacts/member_calls_strings/`: bytecode text artifacts for C++/Rust boundary.
- `README.md`: document supported member-call sugar and exclusions.
- `docs/language-grammar.ebnf`: document member-call suffix syntax.
- `docs/roadmap.md`: mark Phase 12E builtin member-call sugar done and leave full methods as future work.
- `AGENTS.md`: update current language semantics and limitations.

---

### Task 1: RED success golden fixtures

**Files:**
- Create: `tests/golden/member_calls_arrays/input.cd`
- Create: `tests/golden/member_calls_arrays/run.out`
- Create: `tests/golden/member_calls_arrays/ast.out`
- Create: `tests/golden/member_calls_arrays/ir.out`
- Create: `tests/golden/member_calls_strings/input.cd`
- Create: `tests/golden/member_calls_strings/run.out`
- Create: `tests/golden/member_calls_strings/ast.out`
- Create: `tests/golden/member_calls_strings/ir.out`
- Create: `tests/golden/member_calls_shadowing/input.cd`
- Create: `tests/golden/member_calls_shadowing/run.out`
- Create: `tests/golden/member_calls_shadowing/ast.out`
- Create: `tests/golden/member_calls_shadowing/ir.out`

- [ ] **Step 1: Create array member-call input and expected run output**

Create `tests/golden/member_calls_arrays/input.cd`:

```cd
let xs: number[] = [1];
print xs.len();
xs.push(2);
print xs.len();
print xs.pop();
print xs.len();
print xs.pop();
```

Create `tests/golden/member_calls_arrays/run.out`:

```text
1
2
2
1
1
```

Create initial empty expected files so the golden runner reports AST/IR mismatches during RED:

```bash
: > tests/golden/member_calls_arrays/ast.out
: > tests/golden/member_calls_arrays/ir.out
```

- [ ] **Step 2: Create string member-call input and expected run output**

Create `tests/golden/member_calls_strings/input.cd`:

```cd
let text = "hello";
print text.len();
print text.substr(1, 3);
print text.charAt(4);
print ("abc" + "def").substr(2, 3);
print ("xy" + "z").charAt(1);
```

Create `tests/golden/member_calls_strings/run.out`:

```text
5
ell
o
cde
y
```

Create initial empty expected files:

```bash
: > tests/golden/member_calls_strings/ast.out
: > tests/golden/member_calls_strings/ir.out
```

- [ ] **Step 3: Create shadowing input and expected run output**

Create `tests/golden/member_calls_shadowing/input.cd`:

```cd
let push = fun (array, value) { print "custom push"; };
let pop = fun (array) { print "custom pop"; return 99; };
let len = fun (value) { return 123; };
let substr = fun (value, start, length) { return "custom substr"; };
let charAt = fun (value, index) { return "custom charAt"; };

let xs = [];
xs.push(10);
print xs.len();
print xs.pop();

let text = "hello";
print text.len();
print text.substr(1, 2);
print text.charAt(1);
print len(text);
print substr(text, 1, 2);
print charAt(text, 1);
```

Create `tests/golden/member_calls_shadowing/run.out`:

```text
1
10
5
el
e
123
custom substr
custom charAt
```

Create initial empty expected files:

```bash
: > tests/golden/member_calls_shadowing/ast.out
: > tests/golden/member_calls_shadowing/ir.out
```

- [ ] **Step 4: Run success goldens and verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. At least the new cases fail because `xs.len()` is still parsed as a normal call of field access and type checking reports field access/call errors, or because empty `ast.out`/`ir.out` mismatch after parser work.

- [ ] **Step 5: Commit RED success fixtures**

```bash
git add tests/golden/member_calls_arrays tests/golden/member_calls_strings tests/golden/member_calls_shadowing
git commit -m "test: add member call success fixtures"
```

---

### Task 2: RED error golden fixtures

**Files:**
- Create: `tests/golden/type_errors/member_call_push_wrong_receiver.cd`
- Create: `tests/golden/type_errors/member_call_push_wrong_receiver.err`
- Create: `tests/golden/type_errors/member_call_push_wrong_receiver.exit`
- Create: `tests/golden/type_errors/member_call_substr_wrong_receiver.cd`
- Create: `tests/golden/type_errors/member_call_substr_wrong_receiver.err`
- Create: `tests/golden/type_errors/member_call_substr_wrong_receiver.exit`
- Create: `tests/golden/type_errors/member_call_char_at_wrong_argument.cd`
- Create: `tests/golden/type_errors/member_call_char_at_wrong_argument.err`
- Create: `tests/golden/type_errors/member_call_char_at_wrong_argument.exit`
- Create: `tests/golden/type_errors/member_call_len_wrong_arity.cd`
- Create: `tests/golden/type_errors/member_call_len_wrong_arity.err`
- Create: `tests/golden/type_errors/member_call_len_wrong_arity.exit`
- Create: `tests/golden/type_errors/member_call_unknown_name.cd`
- Create: `tests/golden/type_errors/member_call_unknown_name.err`
- Create: `tests/golden/type_errors/member_call_unknown_name.exit`
- Create: `tests/golden/runtime_errors/member_call_push_dynamic_bad_receiver.cd`
- Create: `tests/golden/runtime_errors/member_call_push_dynamic_bad_receiver.run.err`
- Create: `tests/golden/runtime_errors/member_call_push_dynamic_bad_receiver.exit`
- Create: `tests/golden/runtime_errors/member_call_len_dynamic_bad_receiver.cd`
- Create: `tests/golden/runtime_errors/member_call_len_dynamic_bad_receiver.run.err`
- Create: `tests/golden/runtime_errors/member_call_len_dynamic_bad_receiver.exit`
- Create: `tests/golden/runtime_errors/member_call_substr_dynamic_bad_receiver.cd`
- Create: `tests/golden/runtime_errors/member_call_substr_dynamic_bad_receiver.run.err`
- Create: `tests/golden/runtime_errors/member_call_substr_dynamic_bad_receiver.exit`

- [ ] **Step 1: Create static type-error fixtures**

Create `tests/golden/type_errors/member_call_push_wrong_receiver.cd`:

```cd
1.push(2);
```

Create `tests/golden/type_errors/member_call_push_wrong_receiver.err`:

```text
Type error at 1:8: push expects array receiver, got number
```

Create `tests/golden/type_errors/member_call_push_wrong_receiver.exit`:

```text
1
```

Create `tests/golden/type_errors/member_call_substr_wrong_receiver.cd`:

```cd
[1].substr(0, 1);
```

Create `tests/golden/type_errors/member_call_substr_wrong_receiver.err`:

```text
Type error at 1:13: substr expects string receiver, got array
```

Create `tests/golden/type_errors/member_call_substr_wrong_receiver.exit`:

```text
1
```

Create `tests/golden/type_errors/member_call_char_at_wrong_argument.cd`:

```cd
"abc".charAt("x");
```

Create `tests/golden/type_errors/member_call_char_at_wrong_argument.err`:

```text
Type error at 1:16: charAt expects number as first argument, got string
```

Create `tests/golden/type_errors/member_call_char_at_wrong_argument.exit`:

```text
1
```

Create `tests/golden/type_errors/member_call_len_wrong_arity.cd`:

```cd
"abc".len(1);
```

Create `tests/golden/type_errors/member_call_len_wrong_arity.err`:

```text
Type error at 1:10: expected 0 arguments but got 1
```

Create `tests/golden/type_errors/member_call_len_wrong_arity.exit`:

```text
1
```

Create `tests/golden/type_errors/member_call_unknown_name.cd`:

```cd
[1].map(2);
```

Create `tests/golden/type_errors/member_call_unknown_name.err`:

```text
Type error at 1:8: unknown member call `map`
```

Create `tests/golden/type_errors/member_call_unknown_name.exit`:

```text
1
```

- [ ] **Step 2: Create runtime-error fixtures for dynamic unknown receivers**

Create `tests/golden/runtime_errors/member_call_push_dynamic_bad_receiver.cd`:

```cd
fun id(value) { return value; }
id(123).push(1);
```

Create `tests/golden/runtime_errors/member_call_push_dynamic_bad_receiver.run.err`:

```text
Runtime error: push expects array as first argument
```

Create `tests/golden/runtime_errors/member_call_push_dynamic_bad_receiver.exit`:

```text
1
```

Create `tests/golden/runtime_errors/member_call_len_dynamic_bad_receiver.cd`:

```cd
fun id(value) { return value; }
id(123).len();
```

Create `tests/golden/runtime_errors/member_call_len_dynamic_bad_receiver.run.err`:

```text
Runtime error: len expects array or string
```

Create `tests/golden/runtime_errors/member_call_len_dynamic_bad_receiver.exit`:

```text
1
```

Create `tests/golden/runtime_errors/member_call_substr_dynamic_bad_receiver.cd`:

```cd
fun id(value) { return value; }
id(123).substr(0, 1);
```

Create `tests/golden/runtime_errors/member_call_substr_dynamic_bad_receiver.run.err`:

```text
Runtime error: substr expects string as first argument
```

Create `tests/golden/runtime_errors/member_call_substr_dynamic_bad_receiver.exit`:

```text
1
```

- [ ] **Step 3: Run goldens and verify RED**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. The new type-error cases should fail until parser/type checker member-call support exists. The runtime cases may fail earlier with type errors until unknown receiver member calls are accepted and lowered.

- [ ] **Step 4: Commit RED error fixtures**

```bash
git add tests/golden/type_errors/member_call_* tests/golden/runtime_errors/member_call_*
git commit -m "test: add member call error fixtures"
```

---

### Task 3: Parser and AST support

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add `MemberCallExpr` declaration**

In `include/Ast.hpp`, insert this declaration immediately after `CallExpr`:

```cpp
struct MemberCallExpr final : Expr {
    MemberCallExpr(ExprPtr receiver, Token name, Token paren, std::vector<ExprPtr> arguments);
    void print(std::ostream& out) const override;

    ExprPtr receiver;
    Token name;
    Token paren;
    std::vector<ExprPtr> arguments;
};
```

- [ ] **Step 2: Implement `MemberCallExpr` printing**

In `src/Ast.cpp`, insert this implementation immediately after `CallExpr::print`:

```cpp
MemberCallExpr::MemberCallExpr(ExprPtr receiver, Token name, Token paren, std::vector<ExprPtr> arguments)
    : receiver(std::move(receiver))
    , name(std::move(name))
    , paren(std::move(paren))
    , arguments(std::move(arguments))
{
}

void MemberCallExpr::print(std::ostream& out) const
{
    out << "(member-call ";
    writeExpr(out, receiver);
    out << ' ' << name.lexeme;
    for (const auto& argument : arguments) {
        out << ' ';
        writeExpr(out, argument);
    }
    out << ')';
}
```

If `CallExpr::print` is not adjacent to field access printing, keep this implementation near the other expression printer implementations and use the existing private `writeExpr` helper.

- [ ] **Step 3: Convert field-call syntax in `Parser::finishCall`**

Replace `src/Parser.cpp` `Parser::finishCall` return logic with:

```cpp
ExprPtr Parser::finishCall(ExprPtr callee)
{
    std::vector<ExprPtr> arguments;
    if (!check(TokenType::RightParen)) {
        do {
            arguments.push_back(expression());
        } while (match(TokenType::Comma));
    }
    Token paren = consume(TokenType::RightParen, "expected `)` after arguments");

    if (auto* field = dynamic_cast<FieldAccessExpr*>(callee.get())) {
        ExprPtr receiver = std::move(field->object);
        Token name = std::move(field->name);
        return std::make_unique<MemberCallExpr>(std::move(receiver), std::move(name), std::move(paren), std::move(arguments));
    }

    return std::make_unique<CallExpr>(std::move(callee), std::move(paren), std::move(arguments));
}
```

This keeps `receiver.name` as `FieldAccessExpr` when no call follows, while parsing `receiver.name(...)` as `MemberCallExpr`.

- [ ] **Step 4: Build and run focused goldens to observe next RED stage**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Parser/AST compilation should succeed, but type checking still rejects `MemberCallExpr` with `unsupported expression node` until Task 4.

- [ ] **Step 5: Commit parser/AST slice**

```bash
git add include/Ast.hpp src/Ast.cpp src/Parser.cpp
git commit -m "feat: parse member call expressions"
```

---

### Task 4: Static type checking for member calls

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Declare `checkMemberCall`**

In `include/TypeChecker.hpp`, add this private declaration next to `checkCall`:

```cpp
CheckedExpression checkMemberCall(const MemberCallExpr& expression);
```

- [ ] **Step 2: Dispatch `MemberCallExpr` before ordinary `CallExpr` or field access**

In `src/TypeChecker.cpp::checkExpressionInfo`, add this branch near the existing call branch and before `FieldAccessExpr` handling:

```cpp
if (const auto* memberCall = dynamic_cast<const MemberCallExpr*>(&expression)) {
    return checkMemberCall(*memberCall);
}
```

- [ ] **Step 3: Add exact member-call validation implementation**

Add this implementation before `TypeChecker::checkCall`:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkMemberCall(const MemberCallExpr& expression)
{
    const std::string& name = expression.name.lexeme;
    const std::size_t arity = expression.arguments.size();

    auto expectArity = [&](std::size_t expected) {
        if (arity != expected) {
            throw TypeError(expression.paren,
                "expected " + std::to_string(expected) + " arguments but got " + std::to_string(arity));
        }
    };

    auto checkReceiver = [&]() {
        return checkExpressionInfo(*expression.receiver);
    };

    if (name == "push") {
        expectArity(1);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::Array) {
            throw TypeError(expression.paren, "push expects array receiver, got " + typeInfoName(receiver.type));
        }
        const TypeInfo* expectedElement = receiver.type.elementType.get();
        const CheckedExpression value = checkExpressionInfo(*expression.arguments[0], expectedElement);
        if (expectedElement && !compatible(*expectedElement, value.type)) {
            throw TypeError(expression.paren,
                "push value expects " + typeInfoName(*expectedElement) + ", got " + typeInfoName(value.type));
        }
        return CheckedExpression{simpleType(StaticType::Nil)};
    }

    if (name == "pop") {
        expectArity(0);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::Array) {
            throw TypeError(expression.paren, "pop expects array receiver, got " + typeInfoName(receiver.type));
        }
        if (receiver.type.kind == StaticType::Array && receiver.type.elementType) {
            return CheckedExpression{*receiver.type.elementType};
        }
        return CheckedExpression{unknownType()};
    }

    if (name == "len") {
        expectArity(0);
        const CheckedExpression receiver = checkReceiver();
        if (isKnown(receiver.type) && receiver.type.kind != StaticType::Array && receiver.type.kind != StaticType::String) {
            throw TypeError(expression.paren, "len expects array or string receiver, got " + typeInfoName(receiver.type));
        }
        return CheckedExpression{simpleType(StaticType::Number)};
    }

    if (name == "substr") {
        expectArity(2);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::String) {
            throw TypeError(expression.paren, "substr expects string receiver, got " + typeInfoName(receiver.type));
        }
        const CheckedExpression start = checkExpressionInfo(*expression.arguments[0]);
        if (start.type.kind != StaticType::Unknown && start.type.kind != StaticType::Number) {
            throw TypeError(expression.paren, "substr expects number as first argument, got " + typeInfoName(start.type));
        }
        const CheckedExpression length = checkExpressionInfo(*expression.arguments[1]);
        if (length.type.kind != StaticType::Unknown && length.type.kind != StaticType::Number) {
            throw TypeError(expression.paren, "substr expects number as second argument, got " + typeInfoName(length.type));
        }
        return CheckedExpression{simpleType(StaticType::String)};
    }

    if (name == "charAt") {
        expectArity(1);
        const CheckedExpression receiver = checkReceiver();
        if (receiver.type.kind != StaticType::Unknown && receiver.type.kind != StaticType::String) {
            throw TypeError(expression.paren, "charAt expects string receiver, got " + typeInfoName(receiver.type));
        }
        const CheckedExpression index = checkExpressionInfo(*expression.arguments[0]);
        if (index.type.kind != StaticType::Unknown && index.type.kind != StaticType::Number) {
            throw TypeError(expression.paren, "charAt expects number as first argument, got " + typeInfoName(index.type));
        }
        return CheckedExpression{simpleType(StaticType::String)};
    }

    throw TypeError(expression.paren, "unknown member call `" + name + "`");
}
```

Receiver evaluation in type checking intentionally happens before explicit argument checks for known member names, matching lowering order and preserving undefined-variable diagnostics on receivers first.

- [ ] **Step 4: Build and run focused goldens to observe next RED stage**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Static type-error fixtures should now pass or show only caret/column adjustments. Success/runtime fixtures still fail until IR lowering handles `MemberCallExpr`.

- [ ] **Step 5: If diagnostic columns differ, refresh only the new error fixtures intentionally**

Run each failing case directly to inspect exact first line:

```bash
./build/compiler_design tests/golden/type_errors/member_call_push_wrong_receiver.cd >/tmp/member.out 2>/tmp/member.err; cat /tmp/member.err
./build/compiler_design tests/golden/type_errors/member_call_substr_wrong_receiver.cd >/tmp/member.out 2>/tmp/member.err; cat /tmp/member.err
./build/compiler_design tests/golden/type_errors/member_call_char_at_wrong_argument.cd >/tmp/member.out 2>/tmp/member.err; cat /tmp/member.err
./build/compiler_design tests/golden/type_errors/member_call_len_wrong_arity.cd >/tmp/member.out 2>/tmp/member.err; cat /tmp/member.err
./build/compiler_design tests/golden/type_errors/member_call_unknown_name.cd >/tmp/member.out 2>/tmp/member.err; cat /tmp/member.err
```

Update the corresponding `.err` files only when the implementation's diagnostic token is correct and stable.

- [ ] **Step 6: Commit type checker slice**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/member_call_*
git commit -m "feat: type check member calls"
```

---

### Task 5: IR lowering and golden refresh

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `tests/golden/member_calls_arrays/ast.out`
- Modify: `tests/golden/member_calls_arrays/ir.out`
- Modify: `tests/golden/member_calls_strings/ast.out`
- Modify: `tests/golden/member_calls_strings/ir.out`
- Modify: `tests/golden/member_calls_shadowing/ast.out`
- Modify: `tests/golden/member_calls_shadowing/ir.out`
- Modify: `tests/golden/runtime_errors/member_call_*.run.err` only if exact runtime messages differ from existing helpers.

- [ ] **Step 1: Declare `emitMemberCall`**

In `include/IRCompiler.hpp`, add this private declaration next to `emitCall`:

```cpp
IRRegister emitMemberCall(const MemberCallExpr& expression);
```

- [ ] **Step 2: Dispatch `MemberCallExpr` in `compileExpression`**

In `src/IRCompiler.cpp::compileExpression`, add this branch before ordinary `CallExpr` or field access handling:

```cpp
if (const auto* memberCall = dynamic_cast<const MemberCallExpr*>(&expression)) {
    return emitMemberCall(*memberCall);
}
```

- [ ] **Step 3: Implement member-call lowering**

Add this implementation before `IRCompiler::emitCall`:

```cpp
IRRegister IRCompiler::emitMemberCall(const MemberCallExpr& expression)
{
    const IRRegister receiver = compileExpression(*expression.receiver);

    if (expression.name.lexeme == "len") {
        if (!expression.arguments.empty()) {
            throw IRCompileError("len member call expects no arguments");
        }
        return ir_.emitLen(receiver);
    }

    if (expression.name.lexeme == "push"
        || expression.name.lexeme == "pop"
        || expression.name.lexeme == "substr"
        || expression.name.lexeme == "charAt") {
        std::vector<IRRegister> arguments;
        arguments.push_back(receiver);
        for (const auto& argument : expression.arguments) {
            arguments.push_back(compileExpression(*argument));
        }
        return ir_.emitNativeCall(expression.name.lexeme, std::move(arguments));
    }

    throw IRCompileError("unknown member call `" + expression.name.lexeme + "`");
}
```

This compiles the receiver exactly once and before explicit arguments.

- [ ] **Step 4: Build and run goldens with update for intentional AST/IR additions**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --update
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS for `tests/run_golden_tests.py` after reviewing updated new `ast.out` and `ir.out` files. If unrelated golden files change, stop and inspect before committing.

- [ ] **Step 5: Review generated member-call AST and IR**

Run:

```bash
cat tests/golden/member_calls_arrays/ast.out
cat tests/golden/member_calls_arrays/ir.out
cat tests/golden/member_calls_strings/ast.out
cat tests/golden/member_calls_strings/ir.out
cat tests/golden/member_calls_shadowing/ast.out
cat tests/golden/member_calls_shadowing/ir.out
git diff -- tests/golden/member_calls_arrays tests/golden/member_calls_strings tests/golden/member_calls_shadowing
```

Expected AST includes explicit member-call forms such as:

```text
(member-call xs len)
(member-call xs push 2)
(member-call text substr 1 3)
```

Expected IR includes existing operations such as `len` and `native_call push`, `native_call pop`, `native_call substr`, and `native_call charAt`; it must not introduce a new member-call IR opcode.

- [ ] **Step 6: Commit lowering and refreshed goldens**

```bash
git add include/IRCompiler.hpp src/IRCompiler.cpp tests/golden/member_calls_arrays tests/golden/member_calls_strings tests/golden/member_calls_shadowing tests/golden/runtime_errors/member_call_*
git commit -m "feat: lower member calls to builtins"
```

---

### Task 6: Bytecode artifacts and Rust VM parity coverage

**Files:**
- Create: `tests/bytecode_artifacts/member_calls_arrays/input.cd`
- Create: `tests/bytecode_artifacts/member_calls_arrays/run.out`
- Create: `tests/bytecode_artifacts/member_calls_arrays/expected.cdbc`
- Create: `tests/bytecode_artifacts/member_calls_strings/input.cd`
- Create: `tests/bytecode_artifacts/member_calls_strings/run.out`
- Create: `tests/bytecode_artifacts/member_calls_strings/expected.cdbc`

- [ ] **Step 1: Copy focused bytecode artifact inputs**

Create `tests/bytecode_artifacts/member_calls_arrays/input.cd`:

```cd
let xs: number[] = [1];
xs.push(2);
print xs.len();
print xs.pop();
```

Create `tests/bytecode_artifacts/member_calls_arrays/run.out`:

```text
2
2
```

Create `tests/bytecode_artifacts/member_calls_strings/input.cd`:

```cd
let text = "hello";
print text.len();
print text.substr(1, 3);
print text.charAt(4);
```

Create `tests/bytecode_artifacts/member_calls_strings/run.out`:

```text
5
ell
o
```

- [ ] **Step 2: Generate initial expected bytecode artifacts**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --update
```

Expected: PASS and create `expected.cdbc` files for the two new artifact cases.

- [ ] **Step 3: Review artifact text for existing opcodes only**

Run:

```bash
cat tests/bytecode_artifacts/member_calls_arrays/expected.cdbc
cat tests/bytecode_artifacts/member_calls_strings/expected.cdbc
git diff -- tests/bytecode_artifacts/member_calls_arrays tests/bytecode_artifacts/member_calls_strings
```

Expected: artifacts contain existing `Len` and `NativeCall` bytecode text. They must not require changes to `include/Bytecode.hpp`, `src/Bytecode.cpp`, `src/BytecodeTextEmitter.cpp`, or `vm-rs/src/format.rs`.

- [ ] **Step 4: Run bytecode and Rust VM focused verification**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: PASS. `run_rust_vm_tests.py --goldens` discovers `tests/golden/*/run.out`; no allowlist change is required unless the runner later introduces filtering.

- [ ] **Step 5: Commit bytecode artifact coverage**

```bash
git add tests/bytecode_artifacts/member_calls_arrays tests/bytecode_artifacts/member_calls_strings
git commit -m "test: cover member calls in bytecode artifacts"
```

---

### Task 7: Documentation updates

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README member-call documentation**

In `README.md`, add a concise language feature paragraph near existing arrays/strings/builtins documentation:

```markdown
Builtin member-call sugar is available for selected array and string helpers:
`array.push(value)`, `array.pop()`, `array.len()`, `string.len()`,
`string.substr(start, length)`, and `string.charAt(index)`. These forms lower to
the existing builtins with the receiver as the first argument; lexical bindings
named `push`, `pop`, `len`, `substr`, or `charAt` do not shadow member-call sugar.
User-defined methods, `this`, and struct methods are not implemented yet.
```

- [ ] **Step 2: Update EBNF call suffix grammar**

In `docs/language-grammar.ebnf`, update the postfix/call expression section to include member call suffixes. Use the exact nonterminals already present in that file. The desired shape is:

```ebnf
call        = primary, { callSuffix } ;
callSuffix  = arguments
            | index
            | field
            | memberCall ;
arguments   = "(", [ argumentsList ], ")" ;
memberCall  = ".", identifier, arguments ;
```

If the existing grammar models suffixes differently, preserve its naming and add the `".", identifier, arguments` alternative. Keep ordinary field access `".", identifier` documented separately.

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, mark Phase 12E builtin member-call sugar as implemented with a dated note:

```markdown
- [x] Phase 12E: builtin member-call sugar for selected array/string helpers (`push`, `pop`, `len`, `substr`, `charAt`).
  Full user-defined methods, `this`, struct methods, and optional chaining remain future work.
```

Use the roadmap's existing checkbox/list style instead of duplicating a new section if Phase 12E already exists.

- [ ] **Step 4: Update AGENTS current semantics**

In `AGENTS.md`, update the arrays/strings/current semantics bullets to include:

```markdown
- Builtin member-call sugar supports `array.push(value)`, `array.pop()`, `array.len()`, `string.len()`, `string.substr(start, length)`, and `string.charAt(index)`. These forms lower to existing builtins with the receiver as the first argument; user-defined methods, `this`, and struct methods are not implemented yet. Member-call names are not shadowed by lexical bindings with the same name.
```

- [ ] **Step 5: Commit documentation**

```bash
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document member call sugar"
```

---

### Task 8: Full verification and final cleanup

**Files:**
- No source files should be edited in this task unless verification exposes a bug.

- [ ] **Step 1: Run the full required verification suite**

Run from repository root:

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

Expected: every command before `rm -rf` exits 0. `rm -rf tests/__pycache__` exits 0 and leaves no Python cache files staged.

- [ ] **Step 2: Inspect final workspace state**

Run:

```bash
git status --short
```

Expected: clean working tree. If documentation or generated goldens remain modified, review and commit them with a focused message before final reporting.

- [ ] **Step 3: Report exact verification results**

Final response must include:

```text
Implemented member-call sugar.
Verification run:
- cmake -S . -B build: PASS
- cmake --build build: PASS
- ctest --test-dir build --output-on-failure: PASS
- python3 tests/run_golden_tests.py ./build/compiler_design: PASS
- python3 tests/run_golden_tests_selftest.py: PASS
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: PASS
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: PASS
- cargo test --manifest-path vm-rs/Cargo.toml: PASS
- git status --short: clean
```

If any command fails, do not claim completion. Fix the failure or report the exact failing command and error.

---

## Self-Review

- Spec coverage: The plan covers parser/AST distinction, builtin-only names, shadowing behavior, static checking, existing IR lowering, runtime reuse, success/type/runtime tests, bytecode artifact coverage, Rust VM parity, README/EBNF/roadmap/AGENTS updates, and full verification.
- Placeholder scan: No forbidden placeholder terms or unspecified test instructions remain. Each code-edit step includes concrete code or exact text.
- Type consistency: The AST type is consistently named `MemberCallExpr`; type checker uses `checkMemberCall`; IR compiler uses `emitMemberCall`; supported member names are consistently `push`, `pop`, `len`, `substr`, and `charAt`.
