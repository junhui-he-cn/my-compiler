# Struct Literals and Field Access Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 12A anonymous struct literals (`{ name: value }`) and dot field reads (`value.name`) across parser, type checker, IR, bytecode, C++ runtime, `.cdbc`, and Rust VM.

**Architecture:** Add a `Struct` runtime value with stable insertion-order printing and identity equality. Add AST nodes for struct literals and field access, lower them to new `struct` and `field` IR/bytecode operations, and use the bytecode names table for field names. Keep this slice read-only: no field assignment, named structs, methods, or static field-shape tracking.

**Tech Stack:** C++17 recursive-descent parser, AST printer, TypeChecker, register IR, bytecode emitter, `.cdbc` text format, Rust VM parser/formatter/executor, Python golden tests, Cargo tests.

---

## File Structure

- Modify `include/Token.hpp`, `src/Token.cpp` if present, and `src/Lexer.cpp`: add `Dot` token for `.`.
- Modify `include/Ast.hpp`, `src/Ast.cpp`: add `StructField`, `StructExpr`, and `FieldAccessExpr`; print `(struct ...)` and `(field object name)`.
- Modify `include/Parser.hpp`, `src/Parser.cpp`: parse struct literals in `primary()` and dot access in postfix `call()`.
- Modify `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: add `StaticType::Struct`; type-check struct fields and field access.
- Modify `include/Value.hpp`, `src/Value.cpp`: add `StructValue` representation, `Value::Type::Struct`, formatting, and identity equality.
- Modify `include/IR.hpp`, `src/IR.cpp`: add `IROp::Struct` and `IROp::Field`; add field-name operand support for struct construction.
- Modify `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: lower `StructExpr` and `FieldAccessExpr`.
- Modify `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: execute struct construction and field reads.
- Modify `include/Bytecode.hpp`, `src/Bytecode.cpp`, `src/BytecodeCompiler.cpp`, `src/BytecodeTextEmitter.cpp`: lower and print new bytecode operations.
- Modify `docs/bytecode-text-format.md`: document `struct` and `field` opcodes.
- Modify `vm-rs/src/bytecode.rs`, `vm-rs/src/format.rs`, `vm-rs/src/runtime.rs`, `vm-rs/src/value.rs`, `vm-rs/src/vm.rs`: parse/format/execute structs.
- Modify `tests/run_rust_vm_tests.py`: add struct success fixtures to the `--goldens` allowlist.
- Add success fixtures under `tests/golden/struct_*`.
- Add parse-error fixtures under `tests/golden/parse_errors/struct_*`.
- Add type-error fixtures under `tests/golden/type_errors/struct_*`.
- Add runtime-error fixtures under `tests/golden/runtime_errors/struct_*`.
- Add bytecode artifact fixture under `tests/bytecode_artifacts/structs/`.
- Modify `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, and `AGENTS.md` after behavior lands.

---

### Task 1: RED fixture for struct literals and field access AST

**Files:**
- Create: `tests/golden/struct_literals_field_access/input.cd`
- Create: `tests/golden/struct_literals_field_access/ast.out`

- [ ] **Step 1: Add the failing fixture**

Create `tests/golden/struct_literals_field_access/input.cd`:

```cd
let person = { name: "Ada", age: 36 };
print person.name;
print person.age;

let nested = { point: { x: 1, y: 2 }, label: "p" };
print nested.point.x;
print {};
```

Create `tests/golden/struct_literals_field_access/ast.out`:

```text
Program
  Let person = (struct name: "Ada" age: 36)
  Print (field person name)
  Print (field person age)
  Let nested = (struct point: (struct x: 1 y: 2) label: "p")
  Print (field (field nested point) x)
  Print (struct)
```

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. The new fixture should fail in parsing because `{` is currently only a statement/block token and `.` is not tokenized.

- [ ] **Step 3: Commit the RED fixture**

```bash
git add tests/golden/struct_literals_field_access/input.cd tests/golden/struct_literals_field_access/ast.out
git commit -m "test: cover struct literal AST"
```

---

### Task 2: Lexer, AST, and parser support

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Token.cpp` or the file containing `tokenTypeName()`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Test: `tests/golden/struct_literals_field_access/ast.out`

- [ ] **Step 1: Add the `Dot` token**

In `include/Token.hpp`, add `Dot` next to punctuation tokens:

```cpp
    Colon,
    Semicolon,
    Comma,
    Dot,
```

In the `tokenTypeName()` switch, add:

```cpp
case TokenType::Dot:
    return "Dot";
```

In `src/Lexer.cpp`, add a scanner case:

```cpp
case '.':
    addToken(TokenType::Dot);
    break;
```

Use the existing token emission helper name (`addToken` or current equivalent) exactly as defined in `src/Lexer.cpp`.

- [ ] **Step 2: Add AST node declarations**

In `include/Ast.hpp`, after `ArrayExpr` or near other expression nodes, add:

```cpp
struct StructField {
    Token name;
    ExprPtr value;
};

struct StructExpr final : Expr {
    explicit StructExpr(std::vector<StructField> fields);
    void print(std::ostream& out) const override;

    std::vector<StructField> fields;
};

struct FieldAccessExpr final : Expr {
    FieldAccessExpr(ExprPtr object, Token name);
    void print(std::ostream& out) const override;

    ExprPtr object;
    Token name;
};
```

- [ ] **Step 3: Implement AST constructors and printing**

In `src/Ast.cpp`, add:

```cpp
StructExpr::StructExpr(std::vector<StructField> fields)
    : fields(std::move(fields))
{
}

void StructExpr::print(std::ostream& out) const
{
    out << "(struct";
    for (const StructField& field : fields) {
        out << ' ' << field.name.lexeme << ": ";
        writeExpr(out, field.value);
    }
    out << ')';
}

FieldAccessExpr::FieldAccessExpr(ExprPtr object, Token name)
    : object(std::move(object))
    , name(std::move(name))
{
}

void FieldAccessExpr::print(std::ostream& out) const
{
    out << "(field ";
    writeExpr(out, object);
    out << ' ' << name.lexeme << ')';
}
```

- [ ] **Step 4: Add parser declarations**

In `include/Parser.hpp`, add:

```cpp
ExprPtr structLiteral();
ExprPtr finishFieldAccess(ExprPtr object);
```

near the existing `arrayLiteral()` / `finishIndex()` declarations.

- [ ] **Step 5: Parse dot field access in postfix position**

In `Parser::call()`, add a branch after index parsing:

```cpp
} else if (match(TokenType::Dot)) {
    expr = finishFieldAccess(std::move(expr));
```

Implement:

```cpp
ExprPtr Parser::finishFieldAccess(ExprPtr object)
{
    Token name = consume(TokenType::Identifier, "expected field name after `.`");
    return std::make_unique<FieldAccessExpr>(std::move(object), std::move(name));
}
```

- [ ] **Step 6: Parse struct literals as primary expressions**

In `Parser::primary()`, before grouping or after array literal handling, add:

```cpp
if (match(TokenType::LeftBrace)) {
    return structLiteral();
}
```

Implement:

```cpp
ExprPtr Parser::structLiteral()
{
    std::vector<StructField> fields;
    if (!check(TokenType::RightBrace)) {
        do {
            Token name = consume(TokenType::Identifier, "expected struct field name");
            consume(TokenType::Colon, "expected `:` after struct field name");
            ExprPtr value = expression();
            fields.push_back(StructField{std::move(name), std::move(value)});
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBrace, "expected `}` after struct fields");
    return std::make_unique<StructExpr>(std::move(fields));
}
```

This intentionally rejects trailing commas because after a comma it expects another field name.

- [ ] **Step 7: Verify AST fixture passes**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: existing tests pass and `struct_literals_field_access default(ast)` passes. The fixture may still fail for `--run` later because no `run.out` exists yet.

- [ ] **Step 8: Commit parser and AST support**

```bash
git add include/Token.hpp src/Lexer.cpp src/Token.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse struct literals and field access"
```

If token names live in a different file than `src/Token.cpp`, stage that file instead.

---

### Task 3: Static type checking for structs

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Add: `tests/golden/type_errors/struct_field_non_struct.cd`
- Add: `tests/golden/type_errors/struct_field_non_struct.err`
- Add: `tests/golden/type_errors/struct_field_non_struct.exit`

- [ ] **Step 1: Add a RED type-error fixture**

Create `tests/golden/type_errors/struct_field_non_struct.cd`:

```cd
print 123.name;
```

Create `tests/golden/type_errors/struct_field_non_struct.err`:

```text
Type error at 1:10: can only access fields on structs
```

Create `tests/golden/type_errors/struct_field_non_struct.exit`:

```text
1
```

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. The new type-error fixture should not yet produce the expected type error.

- [ ] **Step 3: Add `StaticType::Struct`**

In `include/TypeChecker.hpp`, add:

```cpp
    Struct,
```

near `Array` in `enum class StaticType`.

In `src/TypeChecker.cpp`, update `staticTypeName()`:

```cpp
case StaticType::Struct:
    return "struct";
```

- [ ] **Step 4: Type-check struct expressions**

In `checkExpressionInfo`, before the index or call branches, add:

```cpp
if (const auto* structExpr = dynamic_cast<const StructExpr*>(&expression)) {
    for (const StructField& field : structExpr->fields) {
        checkExpression(*field.value);
    }
    return CheckedExpression{simpleType(StaticType::Struct)};
}
```

- [ ] **Step 5: Type-check field access**

In `checkExpressionInfo`, add:

```cpp
if (const auto* field = dynamic_cast<const FieldAccessExpr*>(&expression)) {
    const TypeInfo object = checkExpression(*field->object);
    if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
        throw TypeError(field->name, "can only access fields on structs");
    }
    return CheckedExpression{unknownType()};
}
```

- [ ] **Step 6: Verify GREEN for type checking**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass. If the diagnostic column differs, inspect actual stderr and update only `struct_field_non_struct.err` to match the field token position.

- [ ] **Step 7: Commit type-checking support**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/struct_field_non_struct.*
git commit -m "feat: type check struct field access"
```

---

### Task 4: Runtime `Value::Struct` support in C++

**Files:**
- Modify: `include/Value.hpp`
- Modify: `src/Value.cpp`
- Test later through runtime fixtures

- [ ] **Step 1: Add struct value declarations**

In `include/Value.hpp`, forward-declare:

```cpp
struct StructValue;
```

Add `Struct` to `Value::Type`:

```cpp
Struct,
```

Add API methods:

```cpp
static Value structure(StructValue value);
const StructValue& asStruct() const;
```

Add storage:

```cpp
std::shared_ptr<StructValue> struct_;
```

Add after `ArrayValue`:

```cpp
struct StructValue {
    std::size_t identity = 0;
    std::shared_ptr<std::vector<std::pair<std::string, Value>>> fields;
};
```

- [ ] **Step 2: Implement struct value construction/access**

In `src/Value.cpp`, implement:

```cpp
Value Value::structure(StructValue value)
{
    Value result(Type::Struct);
    result.struct_ = std::make_shared<StructValue>(std::move(value));
    return result;
}

const StructValue& Value::asStruct() const
{
    if (type_ != Type::Struct || !struct_) {
        throw std::runtime_error("value is not a struct");
    }
    return *struct_;
}
```

- [ ] **Step 3: Update equality and formatting**

In `valuesEqual`, add:

```cpp
case Value::Type::Struct:
    return left.asStruct().identity == right.asStruct().identity;
```

In `valueToString`, add:

```cpp
case Value::Type::Struct: {
    std::ostringstream out;
    out << '{';
    const auto& fields = *value.asStruct().fields;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << fields[i].first << ": " << valueToString(fields[i].second);
    }
    out << '}';
    return out.str();
}
```

- [ ] **Step 4: Build to verify no compile errors**

Run:

```bash
cmake --build build
```

Expected: build succeeds.

- [ ] **Step 5: Commit C++ value support**

```bash
git add include/Value.hpp src/Value.cpp
git commit -m "feat: add struct runtime value"
```

---

### Task 5: IR compiler, printer, and interpreter support

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Add/modify: `tests/golden/struct_literals_field_access/run.out`
- Add/modify: `tests/golden/struct_literals_field_access/ir.out`
- Add: `tests/golden/runtime_errors/struct_missing_field.cd`
- Add: `tests/golden/runtime_errors/struct_missing_field.run.err`
- Add: `tests/golden/runtime_errors/struct_missing_field.exit`
- Add: `tests/golden/runtime_errors/struct_dynamic_non_struct.cd`
- Add: `tests/golden/runtime_errors/struct_dynamic_non_struct.run.err`
- Add: `tests/golden/runtime_errors/struct_dynamic_non_struct.exit`

- [ ] **Step 1: Add runtime success and error fixtures**

Create `tests/golden/struct_literals_field_access/run.out`:

```text
Ada
36
1
{}
```

Create `tests/golden/runtime_errors/struct_missing_field.cd`:

```cd
let person = { name: "Ada" };
print person.age;
```

Create `tests/golden/runtime_errors/struct_missing_field.run.err`:

```text
Runtime error: undefined field `age`
```

Create `tests/golden/runtime_errors/struct_missing_field.exit`:

```text
1
```

Create `tests/golden/runtime_errors/struct_dynamic_non_struct.cd`:

```cd
fun id(x) {
  return x;
}
print id(123).name;
```

Create `tests/golden/runtime_errors/struct_dynamic_non_struct.run.err`:

```text
Runtime error: can only access fields on structs
```

Create `tests/golden/runtime_errors/struct_dynamic_non_struct.exit`:

```text
1
```

- [ ] **Step 2: Verify RED for runtime**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Runtime fixtures should fail because IR lowering/execution is not implemented.

- [ ] **Step 3: Extend IR instruction storage for struct field names**

In `include/IR.hpp`, add operations:

```cpp
Struct,
Field,
```

Add a name-index vector to `IRInstruction`:

```cpp
std::vector<std::size_t> operands;
```

Place it after `std::size_t operand = 0;` with a default member initializer so existing aggregate initializers remain valid.

Add methods:

```cpp
IRRegister emitStruct(std::vector<std::size_t> fieldNames, std::vector<IRRegister> fieldValues);
IRRegister emitField(IRRegister object, std::string fieldName);
```

- [ ] **Step 4: Implement IR emission and printing**

In `src/IR.cpp`, add `IROp::Struct` / `IROp::Field` to `irOpName()`.

Implement:

```cpp
IRRegister IRProgram::emitStruct(std::vector<std::size_t> fieldNames, std::vector<IRRegister> fieldValues)
{
    IRRegister dest = makeRegister();
    IRInstruction instruction{IROp::Struct, dest, std::nullopt, std::nullopt, std::move(fieldValues), 0};
    instruction.operands = std::move(fieldNames);
    emit(std::move(instruction));
    return dest;
}

IRRegister IRProgram::emitField(IRRegister object, std::string fieldName)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Field, dest, object, std::nullopt, {}, addName(std::move(fieldName))});
    return dest;
}
```

In the IR printer, print:

```cpp
} else if (instruction.op == IROp::Struct) {
    out << " {";
    for (std::size_t i = 0; i < instruction.arguments.size(); ++i) {
        if (i != 0) out << ", ";
        printNameOperand(out, program, instruction.operands[i]);
        out << ": " << instruction.arguments[i];
    }
    out << "}";
} else if (instruction.op == IROp::Field) {
    out << " " << *instruction.left << ".";
    if (instruction.operand < program.names().size()) {
        out << program.names()[instruction.operand];
    } else {
        out << "@" << instruction.operand;
    }
```

- [ ] **Step 5: Lower AST nodes to IR**

In `include/IRCompiler.hpp`, declare:

```cpp
IRRegister emitStruct(const StructExpr& expression);
IRRegister emitFieldAccess(const FieldAccessExpr& expression);
```

In `IRCompiler::compileExpression`, add branches for `StructExpr` and `FieldAccessExpr`.

Implement:

```cpp
IRRegister IRCompiler::emitStruct(const StructExpr& expression)
{
    std::vector<std::size_t> names;
    std::vector<IRRegister> values;
    names.reserve(expression.fields.size());
    values.reserve(expression.fields.size());
    for (const StructField& field : expression.fields) {
        names.push_back(ir_.addName(field.name.lexeme));
        values.push_back(compileExpression(*field.value));
    }
    return ir_.emitStruct(std::move(names), std::move(values));
}

IRRegister IRCompiler::emitFieldAccess(const FieldAccessExpr& expression)
{
    IRRegister object = compileExpression(*expression.object);
    return ir_.emitField(object, expression.name.lexeme);
}
```

- [ ] **Step 6: Execute struct IR in C++ interpreter**

In `include/IRInterpreter.hpp`, add a counter:

```cpp
std::size_t nextStructIdentity_ = 1;
```

and helper declarations:

```cpp
Value executeStruct(const IRProgram& program, const IRInstruction& instruction, const Frame& frame);
Value executeField(const IRProgram& program, const Frame& frame, IRRegister object, std::size_t fieldNameIndex);
```

In `IRInterpreter::executeInstruction` switch, add:

```cpp
case IROp::Struct:
    writeRegister(frame, readDest(instruction), executeStruct(program, instruction, frame));
    break;
case IROp::Field:
    writeRegister(frame, readDest(instruction), executeField(program, frame, readLeft(instruction), instruction.operand));
    break;
```

Implement:

```cpp
Value IRInterpreter::executeStruct(const IRProgram& program, const IRInstruction& instruction, const Frame& frame)
{
    if (instruction.operands.size() != instruction.arguments.size()) {
        throw IRRuntimeError("struct field metadata mismatch");
    }
    auto fields = std::make_shared<std::vector<std::pair<std::string, Value>>>();
    fields->reserve(instruction.arguments.size());
    for (std::size_t i = 0; i < instruction.arguments.size(); ++i) {
        fields->push_back({readName(program, instruction.operands[i]), readRegister(frame, instruction.arguments[i])});
    }
    return Value::structure(StructValue{nextStructIdentity_++, std::move(fields)});
}

Value IRInterpreter::executeField(const IRProgram& program, const Frame& frame, IRRegister object, std::size_t fieldNameIndex)
{
    const Value& input = readRegister(frame, object);
    if (input.type() != Value::Type::Struct) {
        throw IRRuntimeError("can only access fields on structs");
    }
    const std::string fieldName = readName(program, fieldNameIndex);
    const auto& fields = *input.asStruct().fields;
    for (const auto& field : fields) {
        if (field.first == fieldName) {
            return field.second;
        }
    }
    throw IRRuntimeError("undefined field `" + fieldName + "`");
}
```

- [ ] **Step 7: Generate and review IR golden output**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

Review only the new or intentionally changed files under `tests/golden/struct_literals_field_access/`. The expected `ir.out` should include `struct` and `field` instructions. Do not accept unrelated golden changes.

- [ ] **Step 8: Verify C++ runtime GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass.

- [ ] **Step 9: Commit IR and interpreter support**

```bash
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/struct_literals_field_access tests/golden/runtime_errors/struct_missing_field.* tests/golden/runtime_errors/struct_dynamic_non_struct.*
git commit -m "feat: execute struct literals and field access"
```

---

### Task 6: Bytecode debug output, `.cdbc`, and C++ bytecode lowering

**Files:**
- Modify: `include/Bytecode.hpp`
- Modify: `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `src/BytecodeTextEmitter.cpp`
- Modify: `docs/bytecode-text-format.md`
- Add/modify: `tests/golden/struct_literals_field_access/bytecode.out`
- Create: `tests/bytecode_artifacts/structs/input.cd`
- Create: `tests/bytecode_artifacts/structs/expected.cdbc`

- [ ] **Step 1: Add bytecode fixture inputs**

Create `tests/bytecode_artifacts/structs/input.cd`:

```cd
let person = { name: "Ada", age: 36 };
print person.name;
let nested = { point: { x: 1 }, label: "p" };
print nested.point.x;
```

Run the current artifact test later with `--update`-style manual generation in Step 6 after implementation.

- [ ] **Step 2: Add bytecode operation declarations**

In `include/Bytecode.hpp`, add:

```cpp
Struct,
Field,
```

near `Array` / `Index`, and add to `BytecodeInstruction`:

```cpp
std::vector<std::uint32_t> operands;
```

with a default member initializer after `operand` so existing aggregate initializers remain valid.

- [ ] **Step 3: Lower IR struct instructions to bytecode**

In `src/BytecodeCompiler.cpp`, map:

```cpp
case IROp::Struct:
    return BytecodeOp::Struct;
case IROp::Field:
    return BytecodeOp::Field;
```

Add helper:

```cpp
std::vector<std::uint32_t> lowerOperands(const std::vector<std::size_t>& operands)
{
    std::vector<std::uint32_t> lowered;
    lowered.reserve(operands.size());
    for (std::size_t operand : operands) {
        lowered.push_back(checkedU32(operand, "operand out of range"));
    }
    return lowered;
}
```

Update `lowerInstruction` to set the new `operands` field from `instruction.operands`.

- [ ] **Step 4: Print debug bytecode**

In `src/Bytecode.cpp`, add names to `bytecodeOpName()` and printer support:

```cpp
} else if (instruction.op == BytecodeOp::Struct) {
    out << " {";
    for (std::size_t i = 0; i < instruction.arguments.size(); ++i) {
        if (i != 0) out << ", ";
        printNameOperand(out, program, instruction.operands[i]);
        out << ": " << instruction.arguments[i];
    }
    out << "}";
} else if (instruction.op == BytecodeOp::Field) {
    out << " " << *instruction.left;
    out << ", ";
    printNameOperand(out, program, instruction.operand);
```

- [ ] **Step 5: Emit `.cdbc` text**

Use stable syntax:

```text
rD = struct {n0: rA, n1: rB}
rD = field rObj, nField
```

In `src/BytecodeTextEmitter.cpp`, add support:

```cpp
case BytecodeOp::Struct:
    out << reg(requireDest(instruction)) << " = struct {";
    for (std::size_t i = 0; i < instruction.arguments.size(); ++i) {
        if (i != 0) out << ", ";
        out << nameRef(instruction.operands[i]) << ": " << reg(instruction.arguments[i]);
    }
    out << "}";
    break;
case BytecodeOp::Field:
    out << reg(requireDest(instruction)) << " = field " << reg(requireLeft(instruction)) << ", " << nameRef(instruction.operand);
    break;
```

Validate `Struct` has matching `arguments.size()` and `operands.size()` before emitting.

- [ ] **Step 6: Update bytecode docs and goldens**

In `docs/bytecode-text-format.md`, add `struct` and `field` to the opcode list and document their shapes:

```text
rD = struct {nName: rValue, ...}
rD = field rObject, nName
```

Generate expected outputs:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --update
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/structs/expected.cdbc tests/bytecode_artifacts/structs/input.cd
```

Review:

```bash
git diff -- tests/golden/struct_literals_field_access/bytecode.out tests/bytecode_artifacts/structs/expected.cdbc docs/bytecode-text-format.md
```

Expected: only struct-related bytecode output changes.

- [ ] **Step 7: Verify bytecode artifact test reaches Rust parse failure RED**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: FAIL for the new `structs` artifact because Rust `.cdbc` parser does not yet know `struct` / `field`.

- [ ] **Step 8: Commit C++ bytecode and artifact changes**

```bash
git add include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp src/BytecodeTextEmitter.cpp docs/bytecode-text-format.md tests/golden/struct_literals_field_access/bytecode.out tests/bytecode_artifacts/structs/input.cd tests/bytecode_artifacts/structs/expected.cdbc
git commit -m "feat: emit struct bytecode artifacts"
```

---

### Task 7: Rust `.cdbc` parser/formatter support

**Files:**
- Modify: `vm-rs/src/bytecode.rs`
- Modify: `vm-rs/src/format.rs`

- [ ] **Step 1: Add Rust bytecode instruction variants**

In `vm-rs/src/bytecode.rs`, add:

```rust
Struct {
    dest: usize,
    fields: Vec<(usize, usize)>,
},
Field {
    dest: usize,
    object: usize,
    name: usize,
},
```

where each struct field pair is `(name_index, value_register)`.

- [ ] **Step 2: Parse `struct` and `field` instructions**

In `vm-rs/src/format.rs`, add parse branches:

```rust
"struct" => Ok(Instruction::Struct {
    dest,
    fields: parse_struct_fields(line, operands)?,
}),
"field" => {
    let (object, name) = split_once(line, operands, ", ")?;
    Ok(Instruction::Field {
        dest,
        object: parse_register(line, object)?,
        name: parse_name_ref(line, name)?,
    })
}
```

Add helper:

```rust
fn parse_struct_fields(line: usize, text: &str) -> Result<Vec<(usize, usize)>, ParseError> {
    let inner = text.strip_prefix('{').and_then(|value| value.strip_suffix('}')).ok_or_else(|| ParseError {
        line,
        message: "struct fields must be wrapped in braces".to_string(),
    })?;
    if inner.is_empty() {
        return Ok(Vec::new());
    }
    let mut fields = Vec::new();
    for part in split_comma_parts(inner) {
        let (name, value) = split_once(line, part, ": ")?;
        fields.push((parse_name_ref(line, name)?, parse_register(line, value)?));
    }
    Ok(fields)
}
```

- [ ] **Step 3: Format `struct` and `field` instructions**

In `format_instruction`, add:

```rust
Instruction::Struct { dest, fields } => {
    let parts = fields.iter()
        .map(|(name, value)| format!("n{}: r{}", name, value))
        .collect::<Vec<_>>()
        .join(", ");
    format!("r{} = struct {{{}}}", dest, parts)
}
Instruction::Field { dest, object, name } => {
    format!("r{} = field r{}, n{}", dest, object, name)
}
```

- [ ] **Step 4: Update Rust format unit test**

In `vm-rs/src/format.rs` test `parses_all_opcode_shapes`, add struct/field examples to the sample `.cdbc` program:

```text
  r30 = struct {n0: r0, n1: r1}
  r31 = field r30, n0
```

Increase the main register count in the sample if needed.

- [ ] **Step 5: Verify parser/formatter GREEN**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml format::tests::parses_all_opcode_shapes
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: cargo format test passes and bytecode artifact tests pass, including the new `structs` fixture.

- [ ] **Step 6: Commit Rust parser/formatter support**

```bash
git add vm-rs/src/bytecode.rs vm-rs/src/format.rs
git commit -m "feat: parse struct bytecode artifacts"
```

---

### Task 8: Rust VM runtime and execution support

**Files:**
- Modify: `vm-rs/src/runtime.rs`
- Modify: `vm-rs/src/value.rs`
- Modify: `vm-rs/src/vm.rs`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Add Rust runtime struct storage**

In `vm-rs/src/runtime.rs`, add:

```rust
#[derive(Clone, Debug)]
pub struct StructValue {
    pub identity: usize,
    pub fields: Rc<RefCell<Vec<(String, Value)>>>,
}
```

Import `Value` if needed and reuse existing `Rc` / `RefCell` imports.

- [ ] **Step 2: Add Rust `Value::Struct`**

In `vm-rs/src/value.rs`, import `StructValue` and add:

```rust
Struct(StructValue),
```

Add constructor:

```rust
pub fn structure(value: StructValue) -> Self {
    Self::Struct(value)
}
```

Update `type_name()`:

```rust
Self::Struct(_) => "struct",
```

Update `runtime_equals()`:

```rust
(Self::Struct(left), Self::Struct(right)) => left.identity == right.identity,
```

Update `fmt::Display`:

```rust
Self::Struct(value) => {
    write!(f, "{{")?;
    let fields = value.fields.borrow();
    for (index, (name, field_value)) in fields.iter().enumerate() {
        if index != 0 {
            write!(f, ", ")?;
        }
        write!(f, "{}: {}", name, field_value)?;
    }
    write!(f, "}}")
}
```

Add unit tests for formatting and identity equality.

- [ ] **Step 3: Execute struct bytecode in Rust VM**

In `vm-rs/src/vm.rs`, add `next_struct_identity: usize` to `VM` and initialize it to `1` in `VM::new()`.

Add `Instruction::Struct` and `Instruction::Field` match arms:

```rust
Instruction::Struct { dest, fields } => {
    let value = self.make_struct(frame, fields)?;
    self.write_register(frame, *dest, value)?;
}
Instruction::Field { dest, object, name } => {
    let object = self.read_register(frame, *object)?;
    let name = self.read_name(*name)?;
    let value = self.execute_field(object, &name)?;
    self.write_register(frame, *dest, value)?;
}
```

Add helpers:

```rust
fn make_struct(&mut self, frame: &Frame, fields: &[(usize, usize)]) -> Result<Value, RuntimeError> {
    let identity = self.next_struct_identity;
    self.next_struct_identity += 1;
    let mut values = Vec::with_capacity(fields.len());
    for (name_index, register) in fields {
        values.push((self.read_name(*name_index)?, self.read_register(frame, *register)?));
    }
    Ok(Value::structure(StructValue {
        identity,
        fields: Rc::new(RefCell::new(values)),
    }))
}

fn execute_field(&self, object: Value, name: &str) -> Result<Value, RuntimeError> {
    let Value::Struct(value) = object else {
        return Err(RuntimeError::new("can only access fields on structs"));
    };
    for (field_name, field_value) in value.fields.borrow().iter() {
        if field_name == name {
            return Ok(field_value.clone());
        }
    }
    Err(RuntimeError::new(format!("undefined field `{}`", name)))
}
```

- [ ] **Step 4: Add struct fixture to Rust VM golden allowlist**

In `tests/run_rust_vm_tests.py`, add:

```python
"struct_literals_field_access",
"struct_identity_equality",
```

only for fixtures that have `run.out` and are expected to execute in Rust VM.

- [ ] **Step 5: Verify Rust VM parity GREEN**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case struct_literals_field_access
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: all commands pass.

- [ ] **Step 6: Commit Rust VM execution support**

```bash
git add vm-rs/src/runtime.rs vm-rs/src/value.rs vm-rs/src/vm.rs tests/run_rust_vm_tests.py
git commit -m "feat: execute structs in rust vm"
```

---

### Task 9: Additional success, parse-error, and type/runtime coverage

**Files:**
- Create: `tests/golden/struct_identity_equality/input.cd`
- Create: `tests/golden/struct_identity_equality/run.out`
- Create: `tests/golden/parse_errors/struct_missing_field_name.cd`
- Create: `tests/golden/parse_errors/struct_missing_field_name.err`
- Create: `tests/golden/parse_errors/struct_missing_field_name.exit`
- Create: `tests/golden/parse_errors/struct_missing_colon.cd`
- Create: `tests/golden/parse_errors/struct_missing_colon.err`
- Create: `tests/golden/parse_errors/struct_missing_colon.exit`
- Create: `tests/golden/parse_errors/struct_missing_value.cd`
- Create: `tests/golden/parse_errors/struct_missing_value.err`
- Create: `tests/golden/parse_errors/struct_missing_value.exit`
- Create: `tests/golden/parse_errors/struct_missing_brace.cd`
- Create: `tests/golden/parse_errors/struct_missing_brace.err`
- Create: `tests/golden/parse_errors/struct_missing_brace.exit`
- Create: `tests/golden/parse_errors/struct_trailing_comma.cd`
- Create: `tests/golden/parse_errors/struct_trailing_comma.err`
- Create: `tests/golden/parse_errors/struct_trailing_comma.exit`
- Create: `tests/golden/parse_errors/field_missing_name.cd`
- Create: `tests/golden/parse_errors/field_missing_name.err`
- Create: `tests/golden/parse_errors/field_missing_name.exit`

- [ ] **Step 1: Add identity-equality success fixture**

Create `tests/golden/struct_identity_equality/input.cd`:

```cd
let a = { x: 1 };
let b = a;
let c = { x: 1 };
print a == b;
print a == c;
fun id(value) {
  return value;
}
print id(a).x;
```

Create `tests/golden/struct_identity_equality/run.out`:

```text
true
false
1
```

- [ ] **Step 2: Add parse-error fixtures**

Create `tests/golden/parse_errors/struct_missing_field_name.cd`:

```cd
print { : 1 };
```

Create `tests/golden/parse_errors/struct_missing_field_name.err` after observing parser output. Expected shape:

```text
Parse error at 1:9: expected struct field name, found Colon `:`
```

Create `tests/golden/parse_errors/struct_missing_field_name.exit`:

```text
1
```

Create `tests/golden/parse_errors/struct_missing_colon.cd`:

```cd
print { name 1 };
```

Create `tests/golden/parse_errors/struct_missing_colon.err` with expected shape:

```text
Parse error at 1:14: expected `:` after struct field name, found Number `1`
```

Create `tests/golden/parse_errors/struct_missing_colon.exit` containing `1`.

Create `tests/golden/parse_errors/struct_missing_value.cd`:

```cd
print { name: };
```

Create `tests/golden/parse_errors/struct_missing_value.err` with expected shape:

```text
Parse error at 1:15: expected expression
```

Create `tests/golden/parse_errors/struct_missing_value.exit` containing `1`.

Create `tests/golden/parse_errors/struct_missing_brace.cd`:

```cd
print { name: 1;
```

Create `tests/golden/parse_errors/struct_missing_brace.err` with expected shape:

```text
Parse error at 1:16: expected `}` after struct fields, found Semicolon `;`
```

Create `tests/golden/parse_errors/struct_missing_brace.exit` containing `1`.

Create `tests/golden/parse_errors/struct_trailing_comma.cd`:

```cd
print { name: 1, };
```

Create `tests/golden/parse_errors/struct_trailing_comma.err` with expected shape:

```text
Parse error at 1:18: expected struct field name, found RightBrace `}`
```

Create `tests/golden/parse_errors/struct_trailing_comma.exit` containing `1`.

Create `tests/golden/parse_errors/field_missing_name.cd`:

```cd
let value = { name: "Ada" };
print value.;
```

Create `tests/golden/parse_errors/field_missing_name.err` with expected shape:

```text
Parse error at 2:13: expected field name after `.`, found Semicolon `;`
```

Create `tests/golden/parse_errors/field_missing_name.exit` containing `1`.

- [ ] **Step 3: Verify and adjust parse-error columns**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: parse-error fixtures may fail by exact column/token wording. Update only the new `.err` files to match actual parser output, then rerun until all pass.

- [ ] **Step 4: Verify Rust VM parity for added success fixture**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case struct_identity_equality
```

Expected: pass. If it is not in the allowlist yet, add it as described in Task 8.

- [ ] **Step 5: Commit additional coverage**

```bash
git add tests/golden/struct_identity_equality tests/golden/parse_errors/struct_* tests/golden/parse_errors/field_missing_name.* tests/run_rust_vm_tests.py
git commit -m "test: cover struct edge cases"
```

---

### Task 10: Documentation updates

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar docs**

In `docs/language-grammar.ebnf`, update `call`:

```ebnf
call        = primary,
              { "(", [ arguments ], ")"
              | "[", expression, "]"
              | ".", identifier } ;
```

Add:

```ebnf
struct      = "{", [ fields ], "}" ;

fields      = field,
              { ",", field } ;

field       = identifier, ":", expression ;
```

Add `struct` to `primary` before `identifier`.

- [ ] **Step 2: Update README**

Add struct examples to the language section:

```markdown
Struct literals use `{ field: expression, ... }`, preserve field order when printed, and support field reads with `value.field`. Structs are reference values with identity equality. Field assignment, named struct declarations, methods, and struct type annotations are not implemented yet.
```

Add to supported expressions:

```markdown
- Structs: `{ name: value, ... }` and field reads `value.name`.
```

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, update Phase 12 status:

```markdown
Status: in progress. Phase 12A is implemented: anonymous struct literals and dot field access work across C++ `--run`, bytecode artifacts, and the Rust VM. Field assignment, named structs, methods, and struct type annotations remain future work.
```

Keep later bullets for field assignment and named structs.

- [ ] **Step 4: Update AGENTS language notes**

In `AGENTS.md`, add current semantics:

```markdown
Struct literals create anonymous reference values with ordered fields. Field access `value.name` reads an existing field; statically known non-struct field access is a type error, dynamic non-struct or missing-field access is a runtime error. Field assignment and named struct declarations are not implemented yet.
```

- [ ] **Step 5: Verify docs do not affect tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: pass.

- [ ] **Step 6: Commit docs**

```bash
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document struct literals"
```

---

### Task 11: Full verification and cleanup

**Files:**
- No source edits expected unless verification reveals defects.

- [ ] **Step 1: Run full verification**

Run:

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

Expected: every command exits 0. Report exact pass counts.

- [ ] **Step 2: Check workspace status**

Run:

```bash
git status --short
```

Expected: no build outputs or `tests/__pycache__/`; only intentional files if verification fixes were made.

- [ ] **Step 3: Commit verification fixes if needed**

If verification required changes, commit them:

```bash
git add <changed-files>
git commit -m "fix: stabilize struct literals"
```

If no fixes were needed, do not create an empty commit.

- [ ] **Step 4: Completion handoff**

Use `superpowers:verification-before-completion` before claiming completion. Then use `superpowers:finishing-a-development-branch` to present merge/push/keep/discard options, unless the user has explicitly instructed direct push or another workflow.

---

## Self-Review Notes

- Spec coverage: parser, AST, type checker, C++ runtime, IR, bytecode, `.cdbc`, Rust parser/formatter, Rust VM, docs, and all requested test classes are covered.
- Scope control: field assignment, named structs, methods, static field-shape typing, and struct type annotations are explicitly excluded.
- TDD order: each behavior task starts with failing golden/unit coverage before implementation.
