# Array Index Assignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `array[index] = value` as a mutable array element assignment expression with matching IR interpreter and bytecode VM behavior.

**Architecture:** Reuse the existing assignment parser shape by parsing the left expression first, then validating whether it is a variable target or index target. Add a dedicated `IndexAssignExpr` AST node, lower it to `IROp::AssignIndex`, lower that to `BytecodeOp::AssignIndex`, and execute both by validating the array/index and mutating the shared array element vector.

**Tech Stack:** C++17 compiler/interpreter, recursive-descent parser, AST printer, static type checker, register IR, bytecode lowering/VM, Python golden tests.

---

## File Structure

- `include/Ast.hpp`: declare `IndexAssignExpr` next to `AssignExpr` and `IndexExpr`.
- `src/Ast.cpp`: construct and print `IndexAssignExpr` as `(= (index <collection> <index>) <value>)`.
- `src/Parser.cpp`: update `assignment()` so an `IndexExpr` left side becomes `IndexAssignExpr`.
- `include/TypeChecker.hpp`: declare `checkIndexAssignment(const IndexAssignExpr&)`.
- `src/TypeChecker.cpp`: type-check collection, index, and assigned value; return assigned value expression info.
- `include/IR.hpp`: add `IROp::AssignIndex` and `IRProgram::emitAssignIndex(...)`.
- `src/IR.cpp`: print/name/emit the new IR operation.
- `include/IRCompiler.hpp`: declare `emitIndexAssign(const IndexAssignExpr&)`.
- `src/IRCompiler.cpp`: lower `IndexAssignExpr` in source evaluation order.
- `include/IRInterpreter.hpp`: declare `executeAssignIndex(...)`.
- `src/IRInterpreter.cpp`: execute `AssignIndex` by mutating shared array elements.
- `include/Bytecode.hpp`: add `BytecodeOp::AssignIndex`.
- `src/Bytecode.cpp`: print/name the bytecode operation.
- `src/BytecodeCompiler.cpp`: lower `IROp::AssignIndex` to `BytecodeOp::AssignIndex`.
- `include/BytecodeVM.hpp`: declare `executeAssignIndex(...)`.
- `src/BytecodeVM.cpp`: execute bytecode assignment with the same validations and mutation.
- `tests/golden/array_index_assignment/`: success fixture for simple mutation, aliasing, assignment result, right associativity, and mixed value assignment.
- `tests/golden/array_nested_assignment/`: success fixture for nested shared arrays.
- `tests/golden/type_errors/array_assign_non_array.*`: known non-array target type error.
- `tests/golden/type_errors/array_assign_non_number_index.*`: known non-number index type error.
- `tests/golden/runtime_errors/array_assign_dynamic_non_array.*`: runtime non-array target error.
- `tests/golden/runtime_errors/array_assign_dynamic_non_number_index.*`: runtime non-number index error.
- `tests/golden/runtime_errors/array_assign_non_integer_index.*`: runtime non-integer index error.
- `tests/golden/runtime_errors/array_assign_out_of_range.*`: runtime out-of-range index error.
- `tests/golden/parse_errors/call_assignment_target.*`: parse error coverage for `foo() = 4;`.
- `docs/language-grammar.ebnf`: document assignment target semantics for index assignment.
- `README.md`: document user-visible array mutation behavior.
- `docs/roadmap.md`: mark Phase 10B implemented.
- `AGENTS.md`: update current language semantics to mention mutable array element assignment.

---

### Task 1: Add failing golden fixtures

**Files:**
- Create: `tests/golden/array_index_assignment/input.cd`
- Create: `tests/golden/array_index_assignment/ast.out`
- Create: `tests/golden/array_index_assignment/run.out`
- Create: `tests/golden/array_index_assignment/run_bytecode.out`
- Create: `tests/golden/array_nested_assignment/input.cd`
- Create: `tests/golden/array_nested_assignment/ast.out`
- Create: `tests/golden/array_nested_assignment/run.out`
- Create: `tests/golden/array_nested_assignment/run_bytecode.out`
- Create: `tests/golden/type_errors/array_assign_non_array.cd`
- Create: `tests/golden/type_errors/array_assign_non_array.err`
- Create: `tests/golden/type_errors/array_assign_non_array.exit`
- Create: `tests/golden/type_errors/array_assign_non_number_index.cd`
- Create: `tests/golden/type_errors/array_assign_non_number_index.err`
- Create: `tests/golden/type_errors/array_assign_non_number_index.exit`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_array.exit`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_array.run_bytecode.err`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_array.run_bytecode.exit`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_number_index.cd`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_number_index.run.err`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_number_index.exit`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_number_index.run_bytecode.err`
- Create: `tests/golden/runtime_errors/array_assign_dynamic_non_number_index.run_bytecode.exit`
- Create: `tests/golden/runtime_errors/array_assign_non_integer_index.cd`
- Create: `tests/golden/runtime_errors/array_assign_non_integer_index.run.err`
- Create: `tests/golden/runtime_errors/array_assign_non_integer_index.exit`
- Create: `tests/golden/runtime_errors/array_assign_non_integer_index.run_bytecode.err`
- Create: `tests/golden/runtime_errors/array_assign_non_integer_index.run_bytecode.exit`
- Create: `tests/golden/runtime_errors/array_assign_out_of_range.cd`
- Create: `tests/golden/runtime_errors/array_assign_out_of_range.run.err`
- Create: `tests/golden/runtime_errors/array_assign_out_of_range.exit`
- Create: `tests/golden/runtime_errors/array_assign_out_of_range.run_bytecode.err`
- Create: `tests/golden/runtime_errors/array_assign_out_of_range.run_bytecode.exit`
- Create: `tests/golden/parse_errors/call_assignment_target.cd`
- Create: `tests/golden/parse_errors/call_assignment_target.err`
- Create: `tests/golden/parse_errors/call_assignment_target.exit`

- [ ] **Step 1: Create success fixtures**

Run:

```bash
mkdir -p tests/golden/array_index_assignment tests/golden/array_nested_assignment
cat > tests/golden/array_index_assignment/input.cd <<'CASE'
let xs = [1, 2, 3];
xs[1] = 42;
print xs[1];

let ys = xs;
ys[0] = 9;
print xs[0];

print xs[2] = 7;
xs[0] = xs[1] = 5;
print xs[0];
print xs[1];
xs[1] = "changed";
print xs[1];
CASE
cat > tests/golden/array_index_assignment/ast.out <<'CASE'
Let xs = (array 1 2 3)
Expr (= (index xs 1) 42)
Print (index xs 1)
Let ys = xs
Expr (= (index ys 0) 9)
Print (index xs 0)
Print (= (index xs 2) 7)
Expr (= (index xs 0) (= (index xs 1) 5))
Print (index xs 0)
Print (index xs 1)
Expr (= (index xs 1) "changed")
Print (index xs 1)
CASE
cat > tests/golden/array_index_assignment/run.out <<'CASE'
42
9
7
5
5
changed
CASE
cp tests/golden/array_index_assignment/run.out tests/golden/array_index_assignment/run_bytecode.out
cat > tests/golden/array_nested_assignment/input.cd <<'CASE'
let inner = [1];
let outer = [inner];
outer[0][0] = 7;
print inner[0];
CASE
cat > tests/golden/array_nested_assignment/ast.out <<'CASE'
Let inner = (array 1)
Let outer = (array inner)
Expr (= (index (index outer 0) 0) 7)
Print (index inner 0)
CASE
cat > tests/golden/array_nested_assignment/run.out <<'CASE'
7
CASE
cp tests/golden/array_nested_assignment/run.out tests/golden/array_nested_assignment/run_bytecode.out
```

- [ ] **Step 2: Create type-error fixtures**

Run:

```bash
cat > tests/golden/type_errors/array_assign_non_array.cd <<'CASE'
let x = 1;
x[0] = 2;
CASE
cat > tests/golden/type_errors/array_assign_non_array.err <<'CASE'
Type error at 2:2: can only assign array elements
CASE
cat > tests/golden/type_errors/array_assign_non_array.exit <<'CASE'
1
CASE
cat > tests/golden/type_errors/array_assign_non_number_index.cd <<'CASE'
let xs = [1];
xs["0"] = 2;
CASE
cat > tests/golden/type_errors/array_assign_non_number_index.err <<'CASE'
Type error at 2:3: array index must be number
CASE
cat > tests/golden/type_errors/array_assign_non_number_index.exit <<'CASE'
1
CASE
```

- [ ] **Step 3: Create runtime-error fixtures**

Run:

```bash
cat > tests/golden/runtime_errors/array_assign_dynamic_non_array.cd <<'CASE'
fun id(x) {
  return x;
}
id(1)[0] = 2;
CASE
cat > tests/golden/runtime_errors/array_assign_dynamic_non_array.run.err <<'CASE'
Runtime error: can only assign array elements
CASE
cat > tests/golden/runtime_errors/array_assign_dynamic_non_array.exit <<'CASE'
1
CASE
cp tests/golden/runtime_errors/array_assign_dynamic_non_array.run.err tests/golden/runtime_errors/array_assign_dynamic_non_array.run_bytecode.err
cp tests/golden/runtime_errors/array_assign_dynamic_non_array.exit tests/golden/runtime_errors/array_assign_dynamic_non_array.run_bytecode.exit

cat > tests/golden/runtime_errors/array_assign_dynamic_non_number_index.cd <<'CASE'
fun id(x) {
  return x;
}
let xs = [1];
xs[id("0")] = 2;
CASE
cat > tests/golden/runtime_errors/array_assign_dynamic_non_number_index.run.err <<'CASE'
Runtime error: array index must be number
CASE
cat > tests/golden/runtime_errors/array_assign_dynamic_non_number_index.exit <<'CASE'
1
CASE
cp tests/golden/runtime_errors/array_assign_dynamic_non_number_index.run.err tests/golden/runtime_errors/array_assign_dynamic_non_number_index.run_bytecode.err
cp tests/golden/runtime_errors/array_assign_dynamic_non_number_index.exit tests/golden/runtime_errors/array_assign_dynamic_non_number_index.run_bytecode.exit

cat > tests/golden/runtime_errors/array_assign_non_integer_index.cd <<'CASE'
let xs = [1];
xs[0.5] = 2;
CASE
cat > tests/golden/runtime_errors/array_assign_non_integer_index.run.err <<'CASE'
Runtime error: array index must be integer
CASE
cat > tests/golden/runtime_errors/array_assign_non_integer_index.exit <<'CASE'
1
CASE
cp tests/golden/runtime_errors/array_assign_non_integer_index.run.err tests/golden/runtime_errors/array_assign_non_integer_index.run_bytecode.err
cp tests/golden/runtime_errors/array_assign_non_integer_index.exit tests/golden/runtime_errors/array_assign_non_integer_index.run_bytecode.exit

cat > tests/golden/runtime_errors/array_assign_out_of_range.cd <<'CASE'
let xs = [1];
xs[9] = 2;
CASE
cat > tests/golden/runtime_errors/array_assign_out_of_range.run.err <<'CASE'
Runtime error: array index out of range
CASE
cat > tests/golden/runtime_errors/array_assign_out_of_range.exit <<'CASE'
1
CASE
cp tests/golden/runtime_errors/array_assign_out_of_range.run.err tests/golden/runtime_errors/array_assign_out_of_range.run_bytecode.err
cp tests/golden/runtime_errors/array_assign_out_of_range.exit tests/golden/runtime_errors/array_assign_out_of_range.run_bytecode.exit
```

- [ ] **Step 4: Create parse-error fixture for call assignment target**

Run:

```bash
cat > tests/golden/parse_errors/call_assignment_target.cd <<'CASE'
foo() = 4;
CASE
cat > tests/golden/parse_errors/call_assignment_target.err <<'CASE'
Parse error at 1:7: invalid assignment target
CASE
cat > tests/golden/parse_errors/call_assignment_target.exit <<'CASE'
1
CASE
```

- [ ] **Step 5: Run golden tests to verify the new fixtures fail before implementation**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: FAIL for the new array assignment success/type/runtime cases because `xs[0] = value` still reports `Parse error ... invalid assignment target`. Existing unrelated tests should keep their previous status.

- [ ] **Step 6: Commit failing fixtures**

Run:

```bash
git add tests/golden/array_index_assignment tests/golden/array_nested_assignment \
  tests/golden/type_errors/array_assign_non_array.* \
  tests/golden/type_errors/array_assign_non_number_index.* \
  tests/golden/runtime_errors/array_assign_dynamic_non_array.* \
  tests/golden/runtime_errors/array_assign_dynamic_non_number_index.* \
  tests/golden/runtime_errors/array_assign_non_integer_index.* \
  tests/golden/runtime_errors/array_assign_out_of_range.* \
  tests/golden/parse_errors/call_assignment_target.*
git commit -m "test: cover array index assignment"
```

---

### Task 2: Parse and print index assignment expressions

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `src/Parser.cpp`
- Test: `tests/golden/array_index_assignment/ast.out`
- Test: `tests/golden/array_nested_assignment/ast.out`
- Test: `tests/golden/parse_errors/call_assignment_target.*`

- [ ] **Step 1: Add the AST node declaration**

In `include/Ast.hpp`, insert this declaration after `AssignExpr` and before `UnaryExpr`:

```cpp
struct IndexAssignExpr final : Expr {
    IndexAssignExpr(ExprPtr collection, Token bracket, ExprPtr index, ExprPtr value);
    void print(std::ostream& out) const override;

    ExprPtr collection;
    Token bracket;
    ExprPtr index;
    ExprPtr value;
};
```

- [ ] **Step 2: Add the AST node implementation**

In `src/Ast.cpp`, insert this implementation after `AssignExpr::print` and before `UnaryExpr::UnaryExpr`:

```cpp
IndexAssignExpr::IndexAssignExpr(ExprPtr collection, Token bracket, ExprPtr index, ExprPtr value)
    : collection(std::move(collection))
    , bracket(std::move(bracket))
    , index(std::move(index))
    , value(std::move(value))
{
}

void IndexAssignExpr::print(std::ostream& out) const
{
    out << "(= (index ";
    writeExpr(out, collection);
    out << ' ';
    writeExpr(out, index);
    out << ") ";
    writeExpr(out, value);
    out << ')';
}
```

- [ ] **Step 3: Update assignment parsing**

Replace `Parser::assignment()` in `src/Parser.cpp` with:

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

        if (auto* index = dynamic_cast<IndexExpr*>(expr.get())) {
            ExprPtr collection = std::move(index->collection);
            Token bracket = std::move(index->bracket);
            ExprPtr indexExpression = std::move(index->index);
            return std::make_unique<IndexAssignExpr>(
                std::move(collection), std::move(bracket), std::move(indexExpression), std::move(value));
        }

        throw ParseError(equals, "invalid assignment target");
    }

    return expr;
}
```

- [ ] **Step 4: Build and run parser-focused golden checks**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: the new `array_index_assignment default(ast)`, `array_nested_assignment default(ast)`, and `parse_errors/call_assignment_target default(ast)` checks pass. Runtime, type, IR, and bytecode checks for index assignment may still fail because later layers do not support `IndexAssignExpr` yet.

- [ ] **Step 5: Commit parser and AST support**

Run:

```bash
git add include/Ast.hpp src/Ast.cpp src/Parser.cpp
git commit -m "feat: parse array index assignment"
```

---

### Task 3: Type-check index assignment expressions

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: `tests/golden/type_errors/array_assign_non_array.*`
- Test: `tests/golden/type_errors/array_assign_non_number_index.*`

- [ ] **Step 1: Declare the type-checking helper**

In `include/TypeChecker.hpp`, add this declaration after `StaticType checkIndex(const IndexExpr& expression);`:

```cpp
CheckedExpression checkIndexAssignment(const IndexAssignExpr& expression);
```

- [ ] **Step 2: Dispatch to the helper**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkExpressionInfo`, insert this branch after the `IndexExpr` branch or immediately before it:

```cpp
    if (const auto* indexAssign = dynamic_cast<const IndexAssignExpr*>(&expression)) {
        return checkIndexAssignment(*indexAssign);
    }
```

Use the position before the generic `throw TypeError("unsupported expression node");` and keep the existing `IndexExpr` branch intact.

- [ ] **Step 3: Implement the helper**

In `src/TypeChecker.cpp`, insert this function after `TypeChecker::checkIndex`:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkIndexAssignment(const IndexAssignExpr& expression)
{
    const StaticType collection = checkExpression(*expression.collection);
    const StaticType index = checkExpression(*expression.index);
    const CheckedExpression value = checkExpressionInfo(*expression.value);

    if (collection != StaticType::Unknown && collection != StaticType::Array) {
        throw TypeError(expression.bracket, "can only assign array elements");
    }

    if (index != StaticType::Unknown && index != StaticType::Number) {
        throw TypeError(expression.bracket, "array index must be number");
    }

    return value;
}
```

- [ ] **Step 4: Build and run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `type_errors/array_assign_non_array default(ast)` and `type_errors/array_assign_non_number_index default(ast)` pass. Runtime success and runtime-error cases may still fail with compile errors until IR support is added.

- [ ] **Step 5: Commit type-checking support**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: type-check array index assignment"
```

---

### Task 4: Add IR lowering and IR interpreter execution

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Test: `tests/golden/array_index_assignment/run.out`
- Test: `tests/golden/array_nested_assignment/run.out`
- Test: `tests/golden/runtime_errors/array_assign_*.run.err`

- [ ] **Step 1: Add the IR operation and emitter declaration**

In `include/IR.hpp`, add `AssignIndex` immediately after `Index` in `enum class IROp`:

```cpp
    Index,
    AssignIndex,
    Len,
```

In `include/IR.hpp`, add this method declaration after `IRRegister emitIndex(IRRegister collection, IRRegister index);`:

```cpp
    IRRegister emitAssignIndex(IRRegister collection, IRRegister index, IRRegister value);
```

- [ ] **Step 2: Update IR printing and naming**

In `src/IR.cpp`, update `isBinary` so `IROp::AssignIndex` returns `false` in the non-binary list:

```cpp
    case IROp::Index:
    case IROp::AssignIndex:
    case IROp::Len:
```

In `printInstruction`, replace the `IROp::Index` printing branch with this combined branch:

```cpp
    } else if (instruction.op == IROp::Index || instruction.op == IROp::AssignIndex) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        if (instruction.right) {
            out << ", " << *instruction.right;
        }
        if (instruction.op == IROp::AssignIndex && !instruction.arguments.empty()) {
            out << ", " << instruction.arguments.front();
        }
```

Add this emitter after `IRProgram::emitIndex`:

```cpp
IRRegister IRProgram::emitAssignIndex(IRRegister collection, IRRegister index, IRRegister value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::AssignIndex, dest, collection, index, {value}, 0});
    return dest;
}
```

In `irOpName`, add:

```cpp
    case IROp::AssignIndex:
        return "assign_index";
```

- [ ] **Step 3: Declare IR compiler helper**

In `include/IRCompiler.hpp`, add this private helper declaration after `IRRegister emitIndex(const IndexExpr& expression);`:

```cpp
    IRRegister emitIndexAssign(const IndexAssignExpr& expression);
```

- [ ] **Step 4: Lower `IndexAssignExpr`**

In `src/IRCompiler.cpp`, insert this branch in `IRCompiler::compileExpression` after `AssignExpr` and before `GroupingExpr`:

```cpp
    if (const auto* indexAssign = dynamic_cast<const IndexAssignExpr*>(&expression)) {
        return emitIndexAssign(*indexAssign);
    }
```

Insert this helper after `IRCompiler::emitIndex`:

```cpp
IRRegister IRCompiler::emitIndexAssign(const IndexAssignExpr& expression)
{
    IRRegister collection = compileExpression(*expression.collection);
    IRRegister index = compileExpression(*expression.index);
    IRRegister value = compileExpression(*expression.value);
    return ir_.emitAssignIndex(collection, index, value);
}
```

- [ ] **Step 5: Declare IR interpreter helper**

In `include/IRInterpreter.hpp`, add this method declaration after `Value executeIndex(const Frame& frame, IRRegister collection, IRRegister index);`:

```cpp
    Value executeAssignIndex(const Frame& frame, IRRegister collection, IRRegister index, IRRegister value);
```

- [ ] **Step 6: Execute `IROp::AssignIndex`**

In `src/IRInterpreter.cpp`, add this switch branch immediately after `case IROp::Index:`:

```cpp
        case IROp::AssignIndex:
            writeRegister(frame,
                readDest(instruction),
                executeAssignIndex(frame, readLeft(instruction), readRight(instruction), instruction.arguments.at(0)));
            break;
```

Insert this helper after `IRInterpreter::executeIndex`:

```cpp
Value IRInterpreter::executeAssignIndex(const Frame& frame, IRRegister collection, IRRegister index, IRRegister value)
{
    const Value& collectionValue = readRegister(frame, collection);
    if (collectionValue.type() != Value::Type::Array) {
        throw IRRuntimeError("can only assign array elements");
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

    auto& elements = *collectionValue.asArray().elements;
    const auto position = static_cast<std::size_t>(integerIndex);
    if (position >= elements.size()) {
        throw IRRuntimeError("array index out of range");
    }

    Value assignedValue = readRegister(frame, value);
    elements[position] = assignedValue;
    return assignedValue;
}
```

- [ ] **Step 7: Build and run IR runtime-focused tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `array_index_assignment --run`, `array_nested_assignment --run`, and the new runtime-error `--run` checks pass. Bytecode checks for `AssignIndex` may still fail until Task 5 is complete.

- [ ] **Step 8: Commit IR support**

Run:

```bash
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp
git commit -m "feat: execute array index assignment in IR"
```

---

### Task 5: Add bytecode lowering and VM execution

**Files:**
- Modify: `include/Bytecode.hpp`
- Modify: `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `include/BytecodeVM.hpp`
- Modify: `src/BytecodeVM.cpp`
- Test: `tests/golden/array_index_assignment/run_bytecode.out`
- Test: `tests/golden/array_nested_assignment/run_bytecode.out`
- Test: `tests/golden/runtime_errors/array_assign_*.run_bytecode.err`

- [ ] **Step 1: Add the bytecode operation**

In `include/Bytecode.hpp`, add `AssignIndex` immediately after `Index` in `enum class BytecodeOp`:

```cpp
    Index,
    AssignIndex,
    Len,
```

- [ ] **Step 2: Update bytecode printing and naming**

In `src/Bytecode.cpp`, update `isBinary` so `BytecodeOp::AssignIndex` returns `false` in the non-binary list:

```cpp
    case BytecodeOp::Index:
    case BytecodeOp::AssignIndex:
    case BytecodeOp::Len:
```

In `printInstruction`, replace the `BytecodeOp::Index` printing branch with this combined branch:

```cpp
    } else if (instruction.op == BytecodeOp::Index || instruction.op == BytecodeOp::AssignIndex) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        if (instruction.right) {
            out << ", " << *instruction.right;
        }
        if (instruction.op == BytecodeOp::AssignIndex && !instruction.arguments.empty()) {
            out << ", " << instruction.arguments.front();
        }
```

In `bytecodeOpName`, add:

```cpp
    case BytecodeOp::AssignIndex:
        return "assign_index";
```

- [ ] **Step 3: Lower IR to bytecode**

In `src/BytecodeCompiler.cpp`, add this case in `lowerOp` after `IROp::Index`:

```cpp
    case IROp::AssignIndex:
        return BytecodeOp::AssignIndex;
```

- [ ] **Step 4: Declare the VM helper**

In `include/BytecodeVM.hpp`, add this method declaration after `Value executeIndex(const VMFrame& frame, BytecodeRegister collection, BytecodeRegister index);`:

```cpp
    Value executeAssignIndex(const VMFrame& frame, BytecodeRegister collection, BytecodeRegister index, BytecodeRegister value);
```

- [ ] **Step 5: Execute `BytecodeOp::AssignIndex`**

In `src/BytecodeVM.cpp`, add this switch branch immediately after `case BytecodeOp::Index:`:

```cpp
        case BytecodeOp::AssignIndex:
            writeRegister(frame,
                readDest(instruction),
                executeAssignIndex(frame, readLeft(instruction), readRight(instruction), instruction.arguments.at(0)));
            break;
```

Insert this helper after `BytecodeVM::executeIndex`:

```cpp
Value BytecodeVM::executeAssignIndex(const VMFrame& frame, BytecodeRegister collection, BytecodeRegister index, BytecodeRegister value)
{
    const Value& collectionValue = readRegister(frame, collection);
    if (collectionValue.type() != Value::Type::Array) {
        throw BytecodeRuntimeError("can only assign array elements");
    }

    const Value& indexValue = readRegister(frame, index);
    if (indexValue.type() != Value::Type::Number) {
        throw BytecodeRuntimeError("array index must be number");
    }

    const double numericIndex = indexValue.asNumber();
    const double integerIndex = std::trunc(numericIndex);
    if (integerIndex != numericIndex) {
        throw BytecodeRuntimeError("array index must be integer");
    }
    if (integerIndex < 0) {
        throw BytecodeRuntimeError("array index out of range");
    }

    auto& elements = *collectionValue.asArray().elements;
    const auto position = static_cast<std::size_t>(integerIndex);
    if (position >= elements.size()) {
        throw BytecodeRuntimeError("array index out of range");
    }

    Value assignedValue = readRegister(frame, value);
    elements[position] = assignedValue;
    return assignedValue;
}
```

- [ ] **Step 6: Build and run all golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all new success, type-error, runtime-error, and parse-error fixtures pass. If existing `ir.out` or `bytecode.out` goldens are affected only by the new operation name in new fixtures, add those expected files in Task 6 after reviewing generated output.

- [ ] **Step 7: Commit bytecode support**

Run:

```bash
git add include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp include/BytecodeVM.hpp src/BytecodeVM.cpp
git commit -m "feat: execute array index assignment in bytecode"
```

---

### Task 6: Add IR and bytecode golden output for the new success cases

**Files:**
- Create: `tests/golden/array_index_assignment/ir.out`
- Create: `tests/golden/array_index_assignment/bytecode.out`
- Create: `tests/golden/array_nested_assignment/ir.out`
- Create: `tests/golden/array_nested_assignment/bytecode.out`

- [ ] **Step 1: Generate expected IR and bytecode output for only the new success fixtures**

Run:

```bash
./build/compiler_demo --ir tests/golden/array_index_assignment/input.cd > tests/golden/array_index_assignment/ir.out
./build/compiler_demo --bytecode tests/golden/array_index_assignment/input.cd > tests/golden/array_index_assignment/bytecode.out
./build/compiler_demo --ir tests/golden/array_nested_assignment/input.cd > tests/golden/array_nested_assignment/ir.out
./build/compiler_demo --bytecode tests/golden/array_nested_assignment/input.cd > tests/golden/array_nested_assignment/bytecode.out
```

- [ ] **Step 2: Review generated output for the expected operation shape**

Run:

```bash
grep -R "assign_index" tests/golden/array_index_assignment tests/golden/array_nested_assignment
```

Expected: output contains lines shaped like:

```text
vN = assign_index vA, vB, vC
```

and bytecode contains lines shaped like:

```text
rN = assign_index rA, rB, rC
```

- [ ] **Step 3: Run golden tests with the new generated output**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: the new `--ir` and `--bytecode` checks pass along with `default(ast)`, `--run`, and `--run-bytecode`.

- [ ] **Step 4: Commit generated IR and bytecode goldens**

Run:

```bash
git add tests/golden/array_index_assignment/ir.out tests/golden/array_index_assignment/bytecode.out \
  tests/golden/array_nested_assignment/ir.out tests/golden/array_nested_assignment/bytecode.out
git commit -m "test: add array index assignment IR bytecode goldens"
```

---

### Task 7: Update user and project documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README language feature docs**

In `README.md`, find the arrays/indexing section or nearest language-feature list and add this concise user-facing text:

````markdown
Arrays support mutable element assignment:

```cd
let xs = [1, 2, 3];
xs[1] = 42;
print xs[1]; // 42
```

`array[index] = value` is an expression whose result is `value`. Arrays are reference values, so aliases observe element mutation:

```cd
let xs = [1];
let ys = xs;
ys[0] = 9;
print xs[0]; // 9
```
````

If the existing README uses a bullet list rather than feature sections, add these bullets instead:

```markdown
- Arrays support read indexing with `array[index]` and mutable element assignment with `array[index] = value`.
- Index assignment evaluates to the assigned value. Array aliases share the same elements, so mutations through one alias are visible through another.
```

- [ ] **Step 2: Update grammar documentation**

In `docs/language-grammar.ebnf`, keep the implemented expression precedence intact and document the assignment target rule. Use this shape for the assignment part:

```ebnf
assignment     = logical_or, [ "=", assignment ] ;
assignment_target = identifier | postfix, "[", expression, "]" ; (* enforced semantically by parser *)
```

Add a nearby note if the grammar file already has comments for semantic restrictions:

```ebnf
(* Assignment is right-associative. The parser accepts only variable targets and index targets (`name = value` and `array[index] = value`); all other left sides are parse errors. *)
```

- [ ] **Step 3: Update roadmap status**

In `docs/roadmap.md`, change the Phase 10 status paragraph from:

```markdown
Status: in progress. Phase 10A is implemented: `len(value)` returns array element counts or string byte lengths with IR and bytecode parity. Index assignment and array mutation helpers remain future work.
```

to:

```markdown
Status: in progress. Phase 10A is implemented: `len(value)` returns array element counts or string byte lengths with IR and bytecode parity. Phase 10B is implemented: `array[index] = value` mutates shared array elements and works in both runtime paths. Array mutation helpers such as `push` and `pop` remain future work.
```

Also change the recommended split lines to:

```markdown
- Phase 10A: `len` builtin as a small usability slice. Implemented.
- Phase 10B: index assignment. Implemented.
- Phase 10C: `push` / `pop` mutation helpers.
```

- [ ] **Step 4: Update AGENTS project memory**

In `AGENTS.md`, replace this line:

```markdown
- Arrays are immutable-length runtime values with mixed element types. Indexing is read-only and validates array-ness, numeric integer indexes, and bounds at runtime when static types are unknown.
```

with:

```markdown
- Arrays are mutable, immutable-length runtime values with mixed element types. Indexing validates array-ness, numeric integer indexes, and bounds at runtime when static types are unknown; `array[index] = value` mutates an existing element and evaluates to the assigned value.
```

- [ ] **Step 5: Run documentation-adjacent checks**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: docs edits do not change compiler output; golden tests still pass.

- [ ] **Step 6: Commit docs**

Run:

```bash
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document array index assignment"
```

---

### Task 8: Full verification and cleanup

**Files:**
- Verify: all source, docs, and tests changed in Tasks 1-7.

- [ ] **Step 1: Configure CMake**

Run:

```bash
cmake -S . -B build
```

Expected: configure completes with exit code 0.

- [ ] **Step 2: Build**

Run:

```bash
cmake --build build
```

Expected: build completes with exit code 0.

- [ ] **Step 3: Run CTest**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: all CTest tests pass.

- [ ] **Step 4: Run golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden checks pass.

- [ ] **Step 5: Run golden runner selftests**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: all selftests pass.

- [ ] **Step 6: Remove Python cache artifacts**

Run:

```bash
rm -rf tests/__pycache__
```

- [ ] **Step 7: Review branch diff and status**

Run:

```bash
git diff --stat f575efd..HEAD
git diff --name-status f575efd..HEAD
git status --short --branch
```

Expected: diff contains only the array index assignment spec/plan, tests, implementation, docs, and goldens. `git status --short --branch` shows a clean `array-index-assignment` branch.

- [ ] **Step 8: Prepare completion summary**

Report the exact verification commands and results, mention that both `--run` and `--run-bytecode` support `array[index] = value`, and then use `superpowers:finishing-a-development-branch` to present integration options.
