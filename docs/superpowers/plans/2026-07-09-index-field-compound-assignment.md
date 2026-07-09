# Index and Field Compound Assignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend numeric compound assignment so `array[index] += value` and `object.field += value` work like variable compound assignment, including mutation and expression-result behavior.

**Architecture:** Add explicit AST nodes for index and field compound assignment, parse them from existing `IndexExpr` and `FieldAccessExpr` assignment targets, type-check numeric-only rules using existing array/struct type information, and lower to existing IR operations: read target, assert numbers, binary op, assign target. No new bytecode/Rust VM opcode is required.

**Tech Stack:** C++17 recursive-descent parser, AST/type checker/IR compiler, Python golden tests, bytecode artifact tests, Rust VM parity tests.

---

## File Structure

- `include/Ast.hpp`: add `IndexCompoundAssignExpr` and `FieldCompoundAssignExpr`.
- `src/Ast.cpp`: add constructors and stable printer output for the new AST nodes.
- `src/Parser.cpp`: allow compound assignment targets for `IndexExpr` and `FieldAccessExpr` while preserving parse errors for non-target expressions.
- `include/TypeChecker.hpp`: declare `checkIndexCompoundAssignment` and `checkFieldCompoundAssignment`.
- `src/TypeChecker.cpp`: dispatch/check new nodes with numeric-only target and value rules.
- `include/IRCompiler.hpp`: declare `emitIndexCompoundAssign` and `emitFieldCompoundAssign`.
- `src/IRCompiler.cpp`: lower new nodes using existing index/field read, assert-number, binary, and assignment IR.
- `tests/golden/index_compound_assignment/`: success fixture for array index compound assignment.
- `tests/golden/field_compound_assignment/`: success fixture for struct field compound assignment.
- `tests/golden/struct_method_compound_assignment/`: success fixture for `this.field += value` inside methods.
- `tests/golden/type_errors/*compound*`: type-error fixtures for static invalid targets/values.
- `tests/golden/runtime_errors/*compound*`: runtime-error fixtures for dynamic invalid target old values.
- `tests/bytecode_artifacts/*compound*`: bytecode text artifact and Rust VM parity coverage.
- `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, `AGENTS.md`: document the expanded compound assignment target set.

---

### Task 1: RED success fixtures

**Files:**
- Create: `tests/golden/index_compound_assignment/input.cd`
- Create: `tests/golden/index_compound_assignment/run.out`
- Create: `tests/golden/index_compound_assignment/ast.out`
- Create: `tests/golden/index_compound_assignment/ir.out`
- Create: `tests/golden/field_compound_assignment/input.cd`
- Create: `tests/golden/field_compound_assignment/run.out`
- Create: `tests/golden/field_compound_assignment/ast.out`
- Create: `tests/golden/field_compound_assignment/ir.out`
- Create: `tests/golden/struct_method_compound_assignment/input.cd`
- Create: `tests/golden/struct_method_compound_assignment/run.out`
- Create: `tests/golden/struct_method_compound_assignment/ast.out`
- Create: `tests/golden/struct_method_compound_assignment/ir.out`

- [ ] **Step 1: Create array index compound fixture**

Create `tests/golden/index_compound_assignment/input.cd`:

```cd
let xs: [number] = [10, 4];
xs[0] += 5;
print xs[0];
print xs[0] -= 3;
print xs[0] *= 2;
print xs[0] /= 4;
print xs[1];
```

Create `tests/golden/index_compound_assignment/run.out`:

```text
15
12
24
6
4
```

Create empty expected files:

```bash
: > tests/golden/index_compound_assignment/ast.out
: > tests/golden/index_compound_assignment/ir.out
```

- [ ] **Step 2: Create struct field compound fixture**

Create `tests/golden/field_compound_assignment/input.cd`:

```cd
struct Counter { value: number, other: number }
let c: Counter = Counter { value: 10, other: 2 };
c.value += 5;
print c.value;
print c.value -= 3;
print c.value *= 2;
print c.value /= 4;
print c.other;
```

Create `tests/golden/field_compound_assignment/run.out`:

```text
15
12
24
6
2
```

Create empty expected files:

```bash
: > tests/golden/field_compound_assignment/ast.out
: > tests/golden/field_compound_assignment/ir.out
```

- [ ] **Step 3: Create struct method `this.field +=` fixture**

Create `tests/golden/struct_method_compound_assignment/input.cd`:

```cd
struct Counter { value: number }

impl Counter {
  fun inc(delta: number): number {
    this.value += delta;
    return this.value;
  }
}

let c: Counter = Counter { value: 1 };
print c.inc(4);
print c.value;
```

Create `tests/golden/struct_method_compound_assignment/run.out`:

```text
5
5
```

Create empty expected files:

```bash
: > tests/golden/struct_method_compound_assignment/ast.out
: > tests/golden/struct_method_compound_assignment/ir.out
```

- [ ] **Step 4: Verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. The new cases fail with parse errors `invalid compound assignment target` for index and field targets because only variable compound assignment is currently accepted.

- [ ] **Step 5: Commit RED success fixtures**

```bash
git add tests/golden/index_compound_assignment tests/golden/field_compound_assignment tests/golden/struct_method_compound_assignment
git commit -m "test: add index and field compound assignment fixtures"
```

---

### Task 2: RED error fixtures and obsolete parse-error cleanup

**Files:**
- Delete: `tests/golden/parse_errors/compound_assignment_index_target.cd`
- Delete: `tests/golden/parse_errors/compound_assignment_index_target.err`
- Delete: `tests/golden/parse_errors/compound_assignment_index_target.exit`
- Delete: `tests/golden/parse_errors/compound_assignment_field_target.cd`
- Delete: `tests/golden/parse_errors/compound_assignment_field_target.err`
- Delete: `tests/golden/parse_errors/compound_assignment_field_target.exit`
- Create: `tests/golden/type_errors/index_compound_non_array.cd`
- Create: `tests/golden/type_errors/index_compound_non_array.err`
- Create: `tests/golden/type_errors/index_compound_non_array.exit`
- Create: `tests/golden/type_errors/index_compound_non_number_index.cd`
- Create: `tests/golden/type_errors/index_compound_non_number_index.err`
- Create: `tests/golden/type_errors/index_compound_non_number_index.exit`
- Create: `tests/golden/type_errors/index_compound_non_number_element.cd`
- Create: `tests/golden/type_errors/index_compound_non_number_element.err`
- Create: `tests/golden/type_errors/index_compound_non_number_element.exit`
- Create: `tests/golden/type_errors/index_compound_non_number_value.cd`
- Create: `tests/golden/type_errors/index_compound_non_number_value.err`
- Create: `tests/golden/type_errors/index_compound_non_number_value.exit`
- Create: `tests/golden/type_errors/field_compound_non_struct.cd`
- Create: `tests/golden/type_errors/field_compound_non_struct.err`
- Create: `tests/golden/type_errors/field_compound_non_struct.exit`
- Create: `tests/golden/type_errors/field_compound_non_number_field.cd`
- Create: `tests/golden/type_errors/field_compound_non_number_field.err`
- Create: `tests/golden/type_errors/field_compound_non_number_field.exit`
- Create: `tests/golden/type_errors/field_compound_unknown_field.cd`
- Create: `tests/golden/type_errors/field_compound_unknown_field.err`
- Create: `tests/golden/type_errors/field_compound_unknown_field.exit`
- Create: `tests/golden/type_errors/field_compound_non_number_value.cd`
- Create: `tests/golden/type_errors/field_compound_non_number_value.err`
- Create: `tests/golden/type_errors/field_compound_non_number_value.exit`
- Create: `tests/golden/parse_errors/compound_assignment_invalid_expression_target.cd`
- Create: `tests/golden/parse_errors/compound_assignment_invalid_expression_target.err`
- Create: `tests/golden/parse_errors/compound_assignment_invalid_expression_target.exit`
- Create: `tests/golden/runtime_errors/index_compound_dynamic_non_number_old.cd`
- Create: `tests/golden/runtime_errors/index_compound_dynamic_non_number_old.run.err`
- Create: `tests/golden/runtime_errors/index_compound_dynamic_non_number_old.exit`
- Create: `tests/golden/runtime_errors/field_compound_dynamic_non_number_old.cd`
- Create: `tests/golden/runtime_errors/field_compound_dynamic_non_number_old.run.err`
- Create: `tests/golden/runtime_errors/field_compound_dynamic_non_number_old.exit`

- [ ] **Step 1: Remove obsolete parse-error fixtures**

Run:

```bash
git rm tests/golden/parse_errors/compound_assignment_index_target.cd \
       tests/golden/parse_errors/compound_assignment_index_target.err \
       tests/golden/parse_errors/compound_assignment_index_target.exit \
       tests/golden/parse_errors/compound_assignment_field_target.cd \
       tests/golden/parse_errors/compound_assignment_field_target.err \
       tests/golden/parse_errors/compound_assignment_field_target.exit
```

- [ ] **Step 2: Create index type-error fixtures**

Create `tests/golden/type_errors/index_compound_non_array.cd`:

```cd
let x = 1;
x[0] += 1;
```

Create `tests/golden/type_errors/index_compound_non_array.err`:

```text
Type error at 2:2: can only assign array elements
```

Create `tests/golden/type_errors/index_compound_non_array.exit`:

```text
1
```

Create `tests/golden/type_errors/index_compound_non_number_index.cd`:

```cd
let xs = [1];
xs["bad"] += 1;
```

Create `tests/golden/type_errors/index_compound_non_number_index.err`:

```text
Type error at 2:3: array index must be number
```

Create `tests/golden/type_errors/index_compound_non_number_index.exit`:

```text
1
```

Create `tests/golden/type_errors/index_compound_non_number_element.cd`:

```cd
let xs: [string] = ["a"];
xs[0] += 1;
```

Create `tests/golden/type_errors/index_compound_non_number_element.err`:

```text
Type error at 2:7: compound assignment target must be number, got string
```

Create `tests/golden/type_errors/index_compound_non_number_element.exit`:

```text
1
```

Create `tests/golden/type_errors/index_compound_non_number_value.cd`:

```cd
let xs: [number] = [1];
xs[0] += "bad";
```

Create `tests/golden/type_errors/index_compound_non_number_value.err`:

```text
Type error at 2:7: compound assignment value must be number, got string
```

Create `tests/golden/type_errors/index_compound_non_number_value.exit`:

```text
1
```

- [ ] **Step 3: Create field type-error fixtures**

Create `tests/golden/type_errors/field_compound_non_struct.cd`:

```cd
let x = 1;
x.value += 1;
```

Create `tests/golden/type_errors/field_compound_non_struct.err`:

```text
Type error at 2:3: can only assign fields on structs
```

Create `tests/golden/type_errors/field_compound_non_struct.exit`:

```text
1
```

Create `tests/golden/type_errors/field_compound_non_number_field.cd`:

```cd
struct Person { name: string }
let p: Person = Person { name: "Ada" };
p.name += 1;
```

Create `tests/golden/type_errors/field_compound_non_number_field.err`:

```text
Type error at 3:8: compound assignment target must be number, got string
```

Create `tests/golden/type_errors/field_compound_non_number_field.exit`:

```text
1
```

Create `tests/golden/type_errors/field_compound_unknown_field.cd`:

```cd
struct Person { name: string }
let p: Person = Person { name: "Ada" };
p.age += 1;
```

Create `tests/golden/type_errors/field_compound_unknown_field.err`:

```text
Type error at 3:3: struct `Person` has no field `age`
```

Create `tests/golden/type_errors/field_compound_unknown_field.exit`:

```text
1
```

Create `tests/golden/type_errors/field_compound_non_number_value.cd`:

```cd
struct Counter { value: number }
let c: Counter = Counter { value: 1 };
c.value += "bad";
```

Create `tests/golden/type_errors/field_compound_non_number_value.err`:

```text
Type error at 3:9: compound assignment value must be number, got string
```

Create `tests/golden/type_errors/field_compound_non_number_value.exit`:

```text
1
```

- [ ] **Step 4: Create invalid target parse-error fixture**

Create `tests/golden/parse_errors/compound_assignment_invalid_expression_target.cd`:

```cd
let a = 1;
let b = 2;
(a + b) += 1;
```

Create `tests/golden/parse_errors/compound_assignment_invalid_expression_target.err`:

```text
Parse error at 3:9: invalid compound assignment target
```

Create `tests/golden/parse_errors/compound_assignment_invalid_expression_target.exit`:

```text
1
```

- [ ] **Step 5: Create runtime-error fixtures for dynamic old values**

Create `tests/golden/runtime_errors/index_compound_dynamic_non_number_old.cd`:

```cd
let xs = ["bad"];
xs[0] += 1;
```

Create `tests/golden/runtime_errors/index_compound_dynamic_non_number_old.run.err`:

```text
Runtime error: `+=` expects number target
```

Create `tests/golden/runtime_errors/index_compound_dynamic_non_number_old.exit`:

```text
1
```

Create `tests/golden/runtime_errors/field_compound_dynamic_non_number_old.cd`:

```cd
let value = { count: "bad" };
value.count += 1;
```

Create `tests/golden/runtime_errors/field_compound_dynamic_non_number_old.run.err`:

```text
Runtime error: `+=` expects number target
```

Create `tests/golden/runtime_errors/field_compound_dynamic_non_number_old.exit`:

```text
1
```

- [ ] **Step 6: Verify RED for error fixtures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Newly valid index/field compound syntax still fails at parse time until Task 3; invalid expression target parse fixture should continue to fail with `invalid compound assignment target`.

- [ ] **Step 7: Commit RED error fixtures**

```bash
git add tests/golden/type_errors/index_compound_* tests/golden/type_errors/field_compound_* \
        tests/golden/runtime_errors/index_compound_* tests/golden/runtime_errors/field_compound_* \
        tests/golden/parse_errors/compound_assignment_invalid_expression_target.*
git commit -m "test: add index and field compound assignment errors"
```

---

### Task 3: AST and parser support

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add AST declarations**

In `include/Ast.hpp`, insert after `IndexAssignExpr`:

```cpp
struct IndexCompoundAssignExpr final : Expr {
    IndexCompoundAssignExpr(ExprPtr collection, Token bracket, ExprPtr index, Token op, ExprPtr value);
    void print(std::ostream& out) const override;

    ExprPtr collection;
    Token bracket;
    ExprPtr index;
    Token op;
    ExprPtr value;
};
```

Insert after `FieldAssignExpr`:

```cpp
struct FieldCompoundAssignExpr final : Expr {
    FieldCompoundAssignExpr(ExprPtr object, Token name, Token op, ExprPtr value);
    void print(std::ostream& out) const override;

    ExprPtr object;
    Token name;
    Token op;
    ExprPtr value;
};
```

- [ ] **Step 2: Implement AST constructors and printers**

In `src/Ast.cpp`, add after `IndexAssignExpr::print`:

```cpp
IndexCompoundAssignExpr::IndexCompoundAssignExpr(ExprPtr collection, Token bracket, ExprPtr index, Token op, ExprPtr value)
    : collection(std::move(collection))
    , bracket(std::move(bracket))
    , index(std::move(index))
    , op(std::move(op))
    , value(std::move(value))
{
}

void IndexCompoundAssignExpr::print(std::ostream& out) const
{
    out << '(' << op.lexeme << " (index ";
    writeExpr(out, collection);
    out << ' ';
    writeExpr(out, index);
    out << ") ";
    writeExpr(out, value);
    out << ')';
}
```

Add after `FieldAssignExpr::print`:

```cpp
FieldCompoundAssignExpr::FieldCompoundAssignExpr(ExprPtr object, Token name, Token op, ExprPtr value)
    : object(std::move(object))
    , name(std::move(name))
    , op(std::move(op))
    , value(std::move(value))
{
}

void FieldCompoundAssignExpr::print(std::ostream& out) const
{
    out << '(' << op.lexeme << " (field ";
    writeExpr(out, object);
    out << ' ' << name.lexeme << ") ";
    writeExpr(out, value);
    out << ')';
}
```

- [ ] **Step 3: Parse index and field compound targets**

In `Parser::assignment`, replace the compound-assignment target branch with:

```cpp
    if (matchCompoundAssignment()) {
        Token op = previous();
        ExprPtr value = assignment();

        if (const auto* variable = dynamic_cast<const VariableExpr*>(expr.get())) {
            return std::make_unique<CompoundAssignExpr>(variable->name, std::move(op), std::move(value));
        }

        if (auto* index = dynamic_cast<IndexExpr*>(expr.get())) {
            ExprPtr collection = std::move(index->collection);
            Token bracket = std::move(index->bracket);
            ExprPtr indexExpression = std::move(index->index);
            return std::make_unique<IndexCompoundAssignExpr>(
                std::move(collection), std::move(bracket), std::move(indexExpression), std::move(op), std::move(value));
        }

        if (auto* field = dynamic_cast<FieldAccessExpr*>(expr.get())) {
            ExprPtr object = std::move(field->object);
            Token name = std::move(field->name);
            return std::make_unique<FieldCompoundAssignExpr>(std::move(object), std::move(name), std::move(op), std::move(value));
        }

        throw ParseError(op, "invalid compound assignment target");
    }
```

- [ ] **Step 4: Build and run goldens to reach type-check RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. New syntax parses and appears in AST, but type checker fails with `unsupported expression node` for new AST nodes.

- [ ] **Step 5: Commit parser/AST slice**

```bash
git add include/Ast.hpp src/Ast.cpp src/Parser.cpp
git commit -m "feat: parse index and field compound assignment"
```

---

### Task 4: Type checking

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Declare checker methods**

In `include/TypeChecker.hpp`, add near `checkIndexAssignment` and `checkFieldAssignment`:

```cpp
CheckedExpression checkIndexCompoundAssignment(const IndexCompoundAssignExpr& expression);
CheckedExpression checkFieldCompoundAssignment(const FieldCompoundAssignExpr& expression);
```

- [ ] **Step 2: Dispatch new nodes**

In `TypeChecker::checkExpressionInfo`, add before `IndexExpr`/`FieldAccessExpr` read branches:

```cpp
    if (const auto* indexCompound = dynamic_cast<const IndexCompoundAssignExpr*>(&expression)) {
        return checkIndexCompoundAssignment(*indexCompound);
    }

    if (const auto* fieldCompound = dynamic_cast<const FieldCompoundAssignExpr*>(&expression)) {
        return checkFieldCompoundAssignment(*fieldCompound);
    }
```

- [ ] **Step 3: Implement index compound checker**

Add near `checkIndexAssignment`:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkIndexCompoundAssignment(const IndexCompoundAssignExpr& expression)
{
    const TypeInfo collection = checkExpression(*expression.collection);
    const TypeInfo index = checkExpression(*expression.index);

    if (collection.kind != StaticType::Unknown && collection.kind != StaticType::Array) {
        throw TypeError(expression.bracket, "can only assign array elements");
    }
    if (index.kind != StaticType::Unknown && index.kind != StaticType::Number) {
        throw TypeError(expression.bracket, "array index must be number");
    }

    if (collection.kind == StaticType::Array && collection.elementType
        && collection.elementType->kind != StaticType::Unknown
        && collection.elementType->kind != StaticType::Number) {
        throw TypeError(expression.op,
            "compound assignment target must be number, got " + typeInfoName(*collection.elementType));
    }

    const CheckedExpression value = checkExpressionInfo(*expression.value);
    if (value.type.kind != StaticType::Unknown && value.type.kind != StaticType::Number) {
        throw TypeError(expression.op,
            "compound assignment value must be number, got " + typeInfoName(value.type));
    }

    return CheckedExpression{simpleType(StaticType::Number)};
}
```

- [ ] **Step 4: Implement field compound checker**

Add near `checkFieldAssignment`:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkFieldCompoundAssignment(const FieldCompoundAssignExpr& expression)
{
    const TypeInfo object = checkExpression(*expression.object);
    if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
        throw TypeError(expression.name, "can only assign fields on structs");
    }

    if (object.kind == StaticType::Struct && object.structName) {
        const StructTypeDecl* structType = findStructType(*object.structName);
        const StructFieldType* structField = structType ? findStructField(*structType, expression.name.lexeme) : nullptr;
        if (!structField) {
            throw TypeError(expression.name,
                "struct `" + *object.structName + "` has no field `" + expression.name.lexeme + "`");
        }
        if (structField->type.kind != StaticType::Unknown && structField->type.kind != StaticType::Number) {
            throw TypeError(expression.op,
                "compound assignment target must be number, got " + typeInfoName(structField->type));
        }
    }

    const CheckedExpression value = checkExpressionInfo(*expression.value);
    if (value.type.kind != StaticType::Unknown && value.type.kind != StaticType::Number) {
        throw TypeError(expression.op,
            "compound assignment value must be number, got " + typeInfoName(value.type));
    }

    return CheckedExpression{simpleType(StaticType::Number)};
}
```

- [ ] **Step 5: Build and run goldens to reach lowering RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Type-error fixtures mostly pass or show column adjustments; success fixtures fail in IR compilation with unsupported expression node until Task 5.

- [ ] **Step 6: Commit type checker slice**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/index_compound_* tests/golden/type_errors/field_compound_*
git commit -m "feat: type check index and field compound assignment"
```

---

### Task 5: IR lowering and golden refresh

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: success fixture `ast.out` and `ir.out` files.
- Modify: new error `.err` files only for exact diagnostic locations.

- [ ] **Step 1: Declare lowering methods**

In `include/IRCompiler.hpp`, add near `emitCompoundAssign`:

```cpp
IRRegister emitIndexCompoundAssign(const IndexCompoundAssignExpr& expression);
IRRegister emitFieldCompoundAssign(const FieldCompoundAssignExpr& expression);
```

- [ ] **Step 2: Dispatch new nodes**

In `IRCompiler::compileExpression`, add after `CompoundAssignExpr` branch and before plain index/field assignment branches:

```cpp
    if (const auto* indexCompound = dynamic_cast<const IndexCompoundAssignExpr*>(&expression)) {
        return emitIndexCompoundAssign(*indexCompound);
    }

    if (const auto* fieldCompound = dynamic_cast<const FieldCompoundAssignExpr*>(&expression)) {
        return emitFieldCompoundAssign(*fieldCompound);
    }
```

- [ ] **Step 3: Implement index compound lowering**

Add after `emitCompoundAssign`:

```cpp
IRRegister IRCompiler::emitIndexCompoundAssign(const IndexCompoundAssignExpr& expression)
{
    IRRegister collection = compileExpression(*expression.collection);
    IRRegister index = compileExpression(*expression.index);
    IRRegister oldValue = ir_.emitIndex(collection, index);
    IRRegister checkedOldValue = ir_.emitAssertNumber(
        oldValue, "`" + expression.op.lexeme + "` expects number target");
    IRRegister value = compileExpression(*expression.value);
    IRRegister checkedValue = ir_.emitAssertNumber(
        value, "`" + expression.op.lexeme + "` expects number value");
    IRRegister result = ir_.emitBinary(compoundAssignmentOp(expression.op.type), checkedOldValue, checkedValue);
    ir_.emitAssignIndex(collection, index, result);
    return result;
}
```

- [ ] **Step 4: Implement field compound lowering**

Add after `emitIndexCompoundAssign`:

```cpp
IRRegister IRCompiler::emitFieldCompoundAssign(const FieldCompoundAssignExpr& expression)
{
    IRRegister object = compileExpression(*expression.object);
    IRRegister oldValue = ir_.emitField(object, expression.name.lexeme);
    IRRegister checkedOldValue = ir_.emitAssertNumber(
        oldValue, "`" + expression.op.lexeme + "` expects number target");
    IRRegister value = compileExpression(*expression.value);
    IRRegister checkedValue = ir_.emitAssertNumber(
        value, "`" + expression.op.lexeme + "` expects number value");
    IRRegister result = ir_.emitBinary(compoundAssignmentOp(expression.op.type), checkedOldValue, checkedValue);
    ir_.emitAssignField(object, expression.name.lexeme, result);
    return result;
}
```

- [ ] **Step 5: Build, update, verify goldens**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --update
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS after reviewing intentional output changes. If `--update` creates unrelated generated files or broad parse/type error rewrites, run:

```bash
git checkout -- tests/golden/parse_errors tests/golden/type_errors
git clean -fd tests/golden
```

Then manually update only the new fixtures based on direct command output.

- [ ] **Step 6: Review generated outputs**

Run:

```bash
git diff -- tests/golden/index_compound_assignment tests/golden/field_compound_assignment tests/golden/struct_method_compound_assignment tests/golden/type_errors/index_compound_* tests/golden/type_errors/field_compound_* tests/golden/runtime_errors/index_compound_* tests/golden/runtime_errors/field_compound_*
```

Expected: AST includes `(<op> (index ...))` and `(<op> (field ...))`; IR includes `assert_number`, read old target, binary op, and assign index/field.

- [ ] **Step 7: Commit lowering and refreshed goldens**

```bash
git add include/IRCompiler.hpp src/IRCompiler.cpp \
        tests/golden/index_compound_assignment tests/golden/field_compound_assignment tests/golden/struct_method_compound_assignment \
        tests/golden/type_errors/index_compound_* tests/golden/type_errors/field_compound_* \
        tests/golden/runtime_errors/index_compound_* tests/golden/runtime_errors/field_compound_* \
        tests/golden/parse_errors/compound_assignment_invalid_expression_target.*
git commit -m "feat: lower index and field compound assignment"
```

---

### Task 6: Bytecode artifacts and Rust VM parity

**Files:**
- Create: `tests/bytecode_artifacts/index_compound_assignment/input.cd`
- Create: `tests/bytecode_artifacts/index_compound_assignment/run.out`
- Create: `tests/bytecode_artifacts/index_compound_assignment/expected.cdbc`
- Create: `tests/bytecode_artifacts/field_compound_assignment/input.cd`
- Create: `tests/bytecode_artifacts/field_compound_assignment/run.out`
- Create: `tests/bytecode_artifacts/field_compound_assignment/expected.cdbc`
- Create: `tests/bytecode_artifacts/struct_method_compound_assignment/input.cd`
- Create: `tests/bytecode_artifacts/struct_method_compound_assignment/run.out`
- Create: `tests/bytecode_artifacts/struct_method_compound_assignment/expected.cdbc`

- [ ] **Step 1: Create bytecode artifact inputs**

Create `tests/bytecode_artifacts/index_compound_assignment/input.cd`:

```cd
let xs: [number] = [1];
xs[0] += 2;
print xs[0];
```

Create `tests/bytecode_artifacts/index_compound_assignment/run.out`:

```text
3
```

Create `tests/bytecode_artifacts/field_compound_assignment/input.cd`:

```cd
struct Counter { value: number }
let c: Counter = Counter { value: 1 };
c.value += 2;
print c.value;
```

Create `tests/bytecode_artifacts/field_compound_assignment/run.out`:

```text
3
```

Create `tests/bytecode_artifacts/struct_method_compound_assignment/input.cd`:

```cd
struct Counter { value: number }
impl Counter { fun inc(delta: number): number { this.value += delta; return this.value; } }
let c: Counter = Counter { value: 1 };
print c.inc(2);
```

Create `tests/bytecode_artifacts/struct_method_compound_assignment/run.out`:

```text
3
```

- [ ] **Step 2: Generate `.cdbc` expected artifacts**

Run:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/index_compound_assignment/expected.cdbc tests/bytecode_artifacts/index_compound_assignment/input.cd
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/field_compound_assignment/expected.cdbc tests/bytecode_artifacts/field_compound_assignment/input.cd
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/struct_method_compound_assignment/expected.cdbc tests/bytecode_artifacts/struct_method_compound_assignment/input.cd
```

Expected: all commands exit 0 and create expected artifacts.

- [ ] **Step 3: Verify artifacts and Rust VM parity**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: PASS. Artifact text uses existing opcodes only.

- [ ] **Step 4: Review artifacts**

Run:

```bash
cat tests/bytecode_artifacts/index_compound_assignment/expected.cdbc
cat tests/bytecode_artifacts/field_compound_assignment/expected.cdbc
cat tests/bytecode_artifacts/struct_method_compound_assignment/expected.cdbc
```

Expected: artifacts include `assert_number`, binary op, and existing assign-index/assign-field text; no new opcode names appear.

- [ ] **Step 5: Commit artifact coverage**

```bash
git add tests/bytecode_artifacts/index_compound_assignment tests/bytecode_artifacts/field_compound_assignment tests/bytecode_artifacts/struct_method_compound_assignment
git commit -m "test: cover index and field compound assignment artifacts"
```

---

### Task 7: Documentation updates

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README**

Replace the current compound assignment bullet with:

```markdown
- Compound assignment: `name += expression`, `array[index] += expression`, and `object.field += expression` forms, plus `-=`, `*=`, and `/=`, update the target and evaluate to the assigned value. Compound assignment is numeric-only for both the old target value and the right-hand value.
```

Also update the top feature summary from numeric variable compound assignment to numeric compound assignment for variables, array elements, and struct fields.

- [ ] **Step 2: Update EBNF notes**

In `docs/language-grammar.ebnf`, change the compound assignment note to:

```ebnf
(* Assignment is right-associative. Plain `=` and compound assignment operators
   accept variable, index, and field targets (`name = value`, `array[index] = value`,
   `object.field = value`, `name += value`, `array[index] += value`, and
   `object.field += value`). Compound assignment is numeric-only. All other left
   sides are parse errors. *)
```

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, update Phase 15 status:

```markdown
Phase 15E is implemented for numeric compound assignment on array index and struct field targets.
```

Update near-term recommendations if they still list Phase 15E.

- [ ] **Step 4: Update AGENTS.md**

Replace the current compound assignment semantics bullet with:

```markdown
- Numeric compound assignment supports `name += expression`, `array[index] += expression`, and `object.field += expression`, plus `-=`, `*=`, and `/=`. It updates an existing target, evaluates to the assigned value, and is numeric-only for both the old target value and right-hand value.
```

- [ ] **Step 5: Commit documentation**

```bash
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document index and field compound assignment"
```

---

### Task 8: Full verification and cleanup

**Files:**
- No planned edits.

- [ ] **Step 1: Run full verification suite**

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

Expected: every command exits 0. Record the actual test counts.

- [ ] **Step 2: Check git status**

Run:

```bash
git status --short
```

Expected: clean working tree. If files remain modified, review and commit focused changes before final reporting.

- [ ] **Step 3: Final report**

Report exact verification evidence:

```text
Implemented index and field compound assignment.
Verification run:
- cmake -S . -B build: PASS
- cmake --build build: PASS
- ctest --test-dir build --output-on-failure: PASS
- python3 tests/run_golden_tests.py ./build/compiler_design: PASS (<actual count>)
- python3 tests/run_golden_tests_selftest.py: PASS
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: PASS (<actual count>)
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: PASS (<actual count>)
- cargo test --manifest-path vm-rs/Cargo.toml: PASS
- git status --short: clean
```

If any command fails, do not claim completion. Fix the failure or report the exact failing command and output.

---

## Self-Review

- Spec coverage: The plan covers parser/AST, type checker, IR lowering, old parse-error fixture conversion, success/type/runtime tests, bytecode artifact/Rust VM parity, and docs.
- Placeholder scan: No forbidden placeholder text remains. Each task includes exact files, commands, and expected outcomes.
- Type consistency: New nodes are consistently named `IndexCompoundAssignExpr` and `FieldCompoundAssignExpr`; checker/lowering methods use the same names.
- Scope check: The plan excludes arbitrary expression targets, string `+=`, operator overloading, `++`/`--`, and new bytecode opcodes, matching the approved spec.
