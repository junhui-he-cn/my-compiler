# Struct Field Assignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 12B `object.field = value` so it mutates existing anonymous struct fields and returns the assigned value across C++ run, bytecode artifacts, and the Rust VM.

**Architecture:** Extend the existing assignment pipeline with a new `FieldAssignExpr` AST node, lower it to a new `assign_field` IR/bytecode operation, and execute that operation by mutating the shared struct field vector in place. Keep static checking conservative: known non-struct receivers are type errors; unknown receivers defer validation to runtime.

**Tech Stack:** C++17 recursive-descent parser, AST/IR/bytecode layers, Python golden tests, Rust bytecode VM and text artifact parser.

---

## File Structure

- `include/Ast.hpp`, `src/Ast.cpp`: add and print `FieldAssignExpr`.
- `src/Parser.cpp`: accept `FieldAccessExpr` as an assignment target and move out its object/name.
- `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: type-check field assignment receiver/value and return the assigned value type.
- `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: lower `FieldAssignExpr` to `IRProgram::emitAssignField`.
- `include/IR.hpp`, `src/IR.cpp`: add `IROp::AssignField`, emitter, printer, and opcode name.
- `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: execute `AssignField` by mutating existing struct fields.
- `include/Bytecode.hpp`, `src/Bytecode.cpp`: add `BytecodeOp::AssignField` and debug printer support.
- `src/BytecodeCompiler.cpp`: map `IROp::AssignField` to bytecode.
- `src/BytecodeTextEmitter.cpp`: emit `.cdbc` syntax `rD = assign_field rObject, nField, rValue`.
- `vm-rs/src/bytecode.rs`, `vm-rs/src/format.rs`, `vm-rs/src/vm.rs`: parse/format/execute Rust VM `AssignField`.
- `tests/golden/struct_field_assignment/`: success fixture for AST, IR, bytecode, and run output.
- `tests/golden/parse_errors/`: parser coverage for missing RHS and invalid field assignment target.
- `tests/golden/type_errors/`: static non-struct receiver coverage.
- `tests/golden/runtime_errors/`: dynamic non-struct and missing-field coverage.
- `tests/bytecode_artifacts/struct_field_assignment/`: `.cdbc` artifact fixture.
- `README.md`, `docs/language-grammar.ebnf`, `docs/bytecode-text-format.md`, `docs/roadmap.md`, `AGENTS.md`: user-visible docs and project memory.

---

### Task 1: RED golden fixtures for successful field assignment

**Files:**
- Create: `tests/golden/struct_field_assignment/input.cd`
- Create: `tests/golden/struct_field_assignment/ast.out`
- Create: `tests/golden/struct_field_assignment/run.out`

- [ ] **Step 1: Add the success fixture input**

Create `tests/golden/struct_field_assignment/input.cd`:

```cd
let person = { name: "Ada", age: 36 };
person.age = 37;
print person.age;
print person.age = 38;

let outer = { inner: { x: 1 } };
outer.inner.x = 2;
print outer.inner.x;

let a = { x: 1 };
let b = a;
b.x = 3;
print a.x;
```

- [ ] **Step 2: Add the expected AST shape**

Create `tests/golden/struct_field_assignment/ast.out`:

```text
Program
  Let person = {name: "Ada", age: 36}
  Expr (= (field person age) 37)
  Print (field person age)
  Print (= (field person age) 38)
  Let outer = {inner: {x: 1}}
  Expr (= (field (field outer inner) x) 2)
  Print (field (field outer inner) x)
  Let a = {x: 1}
  Let b = a
  Expr (= (field b x) 3)
  Print (field a x)
```

- [ ] **Step 3: Add the expected run output**

Create `tests/golden/struct_field_assignment/run.out`:

```text
37
38
2
3
```

- [ ] **Step 4: Verify the fixture is RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/struct_field_assignment
```

Expected: FAIL during parse with `invalid assignment target` or missing expected IR/bytecode outputs until implementation is complete.

- [ ] **Step 5: Commit the RED fixture**

```bash
git add tests/golden/struct_field_assignment/input.cd tests/golden/struct_field_assignment/ast.out tests/golden/struct_field_assignment/run.out
git commit -m "test: add struct field assignment fixture"
```

---

### Task 2: Parse and print `FieldAssignExpr`

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add the AST node declaration**

In `include/Ast.hpp`, add this node after `FieldAccessExpr`:

```cpp
struct FieldAssignExpr final : Expr {
    FieldAssignExpr(ExprPtr object, Token name, ExprPtr value);
    void print(std::ostream& out) const override;

    ExprPtr object;
    Token name;
    ExprPtr value;
};
```

- [ ] **Step 2: Add AST construction and printing**

In `src/Ast.cpp`, add after `FieldAccessExpr::print`:

```cpp
FieldAssignExpr::FieldAssignExpr(ExprPtr object, Token name, ExprPtr value)
    : object(std::move(object))
    , name(std::move(name))
    , value(std::move(value))
{
}

void FieldAssignExpr::print(std::ostream& out) const
{
    out << "(= (field ";
    writeExpr(out, object);
    out << ' ' << name.lexeme << ") ";
    writeExpr(out, value);
    out << ')';
}
```

- [ ] **Step 3: Accept field access as an assignment target**

In `src/Parser.cpp`, inside `Parser::assignment()` after the `IndexExpr` target branch and before `throw ParseError(...)`, add:

```cpp
        if (auto* field = dynamic_cast<FieldAccessExpr*>(expr.get())) {
            ExprPtr object = std::move(field->object);
            Token name = std::move(field->name);
            return std::make_unique<FieldAssignExpr>(std::move(object), std::move(name), std::move(value));
        }
```

- [ ] **Step 4: Run parser-focused verification**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/struct_field_assignment
```

Expected: AST output now matches; run/IR/bytecode still fail because `FieldAssignExpr` is not type-checked or compiled.

- [ ] **Step 5: Commit parser/AST support**

```bash
git add include/Ast.hpp src/Ast.cpp src/Parser.cpp tests/golden/struct_field_assignment
git commit -m "feat: parse struct field assignment"
```

---

### Task 3: Type-check field assignment

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/struct_field_assignment_non_struct.cd`
- Create: `tests/golden/type_errors/struct_field_assignment_non_struct.err`
- Create: `tests/golden/type_errors/struct_field_assignment_non_struct.exit`

- [ ] **Step 1: Add the type-error fixture**

Create `tests/golden/type_errors/struct_field_assignment_non_struct.cd`:

```cd
123.name = "x";
```

Create `tests/golden/type_errors/struct_field_assignment_non_struct.err`:

```text
Type error at 1:5: can only assign fields on structs
```

Create `tests/golden/type_errors/struct_field_assignment_non_struct.exit`:

```text
1
```

- [ ] **Step 2: Declare the checker helper**

In `include/TypeChecker.hpp`, add this private helper near `checkIndexAssignment`:

```cpp
    CheckedExpression checkFieldAssignment(const FieldAssignExpr& expression);
```

- [ ] **Step 3: Dispatch to the helper**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkExpressionInfo`, add after the `FieldAccessExpr` branch and before `IndexExpr`/`IndexAssignExpr` branches:

```cpp
    if (const auto* fieldAssign = dynamic_cast<const FieldAssignExpr*>(&expression)) {
        return checkFieldAssignment(*fieldAssign);
    }
```

- [ ] **Step 4: Implement the helper**

In `src/TypeChecker.cpp`, add after `checkIndexAssignment`:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkFieldAssignment(const FieldAssignExpr& expression)
{
    const TypeInfo object = checkExpression(*expression.object);
    const CheckedExpression value = checkExpressionInfo(*expression.value);

    if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
        throw TypeError(expression.name, "can only assign fields on structs");
    }

    return value;
}
```

- [ ] **Step 5: Run type-check verification**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/type_errors/struct_field_assignment_non_struct.cd
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/struct_field_assignment
```

Expected: type-error fixture PASS; success fixture advances to IR compile error until lowering is added.

- [ ] **Step 6: Commit type-checking support**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/struct_field_assignment_non_struct.*
git commit -m "feat: type check struct field assignment"
```

---

### Task 4: Add IR lowering and C++ runtime execution

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Create/refresh: `tests/golden/struct_field_assignment/ir.out`
- Create: `tests/golden/runtime_errors/struct_field_assignment_non_struct.cd`
- Create: `tests/golden/runtime_errors/struct_field_assignment_non_struct.run.err`
- Create: `tests/golden/runtime_errors/struct_field_assignment_non_struct.exit`
- Create: `tests/golden/runtime_errors/struct_field_assignment_missing_field.cd`
- Create: `tests/golden/runtime_errors/struct_field_assignment_missing_field.run.err`
- Create: `tests/golden/runtime_errors/struct_field_assignment_missing_field.exit`

- [ ] **Step 1: Add runtime-error fixtures**

Create `tests/golden/runtime_errors/struct_field_assignment_non_struct.cd`:

```cd
fun id(x) { return x; }
id(123).name = "x";
```

Create `tests/golden/runtime_errors/struct_field_assignment_non_struct.run.err`:

```text
Runtime error: can only assign fields on structs
```

Create `tests/golden/runtime_errors/struct_field_assignment_non_struct.exit`:

```text
1
```

Create `tests/golden/runtime_errors/struct_field_assignment_missing_field.cd`:

```cd
let a = {};
a.x = 1;
```

Create `tests/golden/runtime_errors/struct_field_assignment_missing_field.run.err`:

```text
Runtime error: undefined field `x`
```

Create `tests/golden/runtime_errors/struct_field_assignment_missing_field.exit`:

```text
1
```

- [ ] **Step 2: Add IR opcode and emitter declaration**

In `include/IR.hpp`, add `AssignField` immediately after `Field` in `IROp`, and add this method after `emitField`:

```cpp
    IRRegister emitAssignField(IRRegister object, std::string fieldName, IRRegister value);
```

- [ ] **Step 3: Update IR printing helpers**

In `src/IR.cpp`, update `isBinary` to treat `IROp::AssignField` as non-binary, and extend `printInstruction` after the `IROp::Field` block:

```cpp
    } else if (instruction.op == IROp::AssignField) {
        if (instruction.left) {
            out << " " << *instruction.left << ".";
            if (instruction.operand < program.names().size()) {
                out << program.names()[instruction.operand];
            } else {
                out << "@" << instruction.operand;
            }
        }
        if (!instruction.arguments.empty()) {
            out << ", " << instruction.arguments.front();
        }
```

Then add `case IROp::AssignField: return "assign_field";` in `irOpName`.

- [ ] **Step 4: Implement IR emitter**

In `src/IR.cpp`, add after `IRProgram::emitField`:

```cpp
IRRegister IRProgram::emitAssignField(IRRegister object, std::string fieldName, IRRegister value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::AssignField, dest, object, std::nullopt, {value}, addName(std::move(fieldName))});
    return dest;
}
```

- [ ] **Step 5: Lower `FieldAssignExpr`**

In `include/IRCompiler.hpp`, add:

```cpp
    IRRegister emitFieldAssign(const FieldAssignExpr& expression);
```

In `src/IRCompiler.cpp`, dispatch after `IndexAssignExpr`:

```cpp
    if (const auto* fieldAssign = dynamic_cast<const FieldAssignExpr*>(&expression)) {
        return emitFieldAssign(*fieldAssign);
    }
```

Add after `emitFieldAccess`:

```cpp
IRRegister IRCompiler::emitFieldAssign(const FieldAssignExpr& expression)
{
    IRRegister object = compileExpression(*expression.object);
    IRRegister value = compileExpression(*expression.value);
    return ir_.emitAssignField(object, expression.name.lexeme, value);
}
```

- [ ] **Step 6: Add interpreter declaration and dispatch**

In `include/IRInterpreter.hpp`, add after `executeField`:

```cpp
    Value executeAssignField(const IRProgram& program, const Frame& frame, IRRegister object, std::size_t fieldNameIndex, IRRegister value);
```

In `src/IRInterpreter.cpp`, add a switch case after `IROp::Field`:

```cpp
        case IROp::AssignField:
            writeRegister(frame,
                readDest(instruction),
                executeAssignField(program, frame, readLeft(instruction), instruction.operand, instruction.arguments.at(0)));
            break;
```

- [ ] **Step 7: Implement runtime mutation**

In `src/IRInterpreter.cpp`, add after `executeField`:

```cpp
Value IRInterpreter::executeAssignField(
    const IRProgram& program,
    const Frame& frame,
    IRRegister object,
    std::size_t fieldNameIndex,
    IRRegister value)
{
    const Value& input = readRegister(frame, object);
    if (input.type() != Value::Type::Struct) {
        throw IRRuntimeError("can only assign fields on structs");
    }

    const std::string fieldName = readName(program, fieldNameIndex);
    auto& fields = *input.asStruct().fields;
    for (auto& field : fields) {
        if (field.first == fieldName) {
            Value assignedValue = readRegister(frame, value);
            field.second = assignedValue;
            return assignedValue;
        }
    }

    throw IRRuntimeError("undefined field `" + fieldName + "`");
}
```

- [ ] **Step 8: Refresh success IR golden and verify runtime errors**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/struct_field_assignment --update
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/runtime_errors/struct_field_assignment_non_struct.cd
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/runtime_errors/struct_field_assignment_missing_field.cd
```

Expected: success fixture and new runtime-error fixtures PASS for C++ `--run`/IR paths.

- [ ] **Step 9: Review and commit IR/runtime support**

```bash
git diff -- tests/golden/struct_field_assignment/ir.out
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/struct_field_assignment/ir.out tests/golden/runtime_errors/struct_field_assignment_*
git commit -m "feat: execute struct field assignment in IR"
```

---

### Task 5: Add C++ bytecode and `.cdbc` artifact support

**Files:**
- Modify: `include/Bytecode.hpp`
- Modify: `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `src/BytecodeTextEmitter.cpp`
- Create/refresh: `tests/golden/struct_field_assignment/bytecode.out`
- Create: `tests/bytecode_artifacts/struct_field_assignment/input.cd`
- Create/refresh: `tests/bytecode_artifacts/struct_field_assignment/program.cdbc`

- [ ] **Step 1: Add bytecode opcode**

In `include/Bytecode.hpp`, add `AssignField` immediately after `Field` in `BytecodeOp`.

- [ ] **Step 2: Update debug bytecode printer**

In `src/Bytecode.cpp`, update `isBinary` to treat `BytecodeOp::AssignField` as non-binary, add after the `Field` printer block:

```cpp
    } else if (instruction.op == BytecodeOp::AssignField) {
        if (instruction.arguments.size() != 1) {
            out << " <invalid assign_field>";
        } else {
            out << " " << *instruction.left << ", ";
            printNameOperand(out, program, instruction.operand);
            out << ", " << instruction.arguments.front();
        }
```

Then add `case BytecodeOp::AssignField: return "assign_field";` in `bytecodeOpName`.

- [ ] **Step 3: Map IR to bytecode**

In `src/BytecodeCompiler.cpp`, add `case IROp::AssignField: return BytecodeOp::AssignField;` near `Field`. The existing instruction copy path should preserve `dest`, `left`, `operand`, and `arguments`.

- [ ] **Step 4: Emit text artifact instruction**

In `src/BytecodeTextEmitter.cpp`, add after `BytecodeOp::Field`:

```cpp
    case BytecodeOp::AssignField:
        if (instruction.arguments.size() != 1) {
            throw std::runtime_error("assign_field expects one value operand");
        }
        out << reg(requireDest(instruction)) << " = assign_field " << reg(requireLeft(instruction)) << ", "
            << nameRef(instruction.operand) << ", " << reg(instruction.arguments.front());
        break;
```

- [ ] **Step 5: Add bytecode artifact fixture input**

Create `tests/bytecode_artifacts/struct_field_assignment/input.cd`:

```cd
let person = { name: "Ada", age: 36 };
person.age = 37;
print person.age;
```

- [ ] **Step 6: Refresh bytecode outputs**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/struct_field_assignment --update
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --update
```

Expected: `tests/golden/struct_field_assignment/bytecode.out` and `tests/bytecode_artifacts/struct_field_assignment/program.cdbc` are created/updated and contain `assign_field`.

- [ ] **Step 7: Review and commit bytecode support**

```bash
git diff -- tests/golden/struct_field_assignment/bytecode.out tests/bytecode_artifacts/struct_field_assignment/program.cdbc
git add include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp src/BytecodeTextEmitter.cpp tests/golden/struct_field_assignment/bytecode.out tests/bytecode_artifacts/struct_field_assignment
git commit -m "feat: lower struct field assignment to bytecode"
```

---

### Task 6: Add Rust VM parser, formatter, and execution parity

**Files:**
- Modify: `vm-rs/src/bytecode.rs`
- Modify: `vm-rs/src/format.rs`
- Modify: `vm-rs/src/vm.rs`

- [ ] **Step 1: Add Rust bytecode variant**

In `vm-rs/src/bytecode.rs`, add this variant after `Field`:

```rust
    AssignField {
        dest: Register,
        object: Register,
        name: NameId,
        value: Register,
    },
```

- [ ] **Step 2: Parse `assign_field`**

In `vm-rs/src/format.rs`, in the instruction parser `match` for assignment-form opcodes, add:

```rust
            "assign_field" => {
                let object = parse_register(parts.next().ok_or_else(|| ParseError::new("missing assign_field object"))?)?;
                let name = parse_name(parts.next().ok_or_else(|| ParseError::new("missing assign_field name"))?)?;
                let value = parse_register(parts.next().ok_or_else(|| ParseError::new("missing assign_field value"))?)?;
                ensure_no_extra(parts)?;
                Ok(Instruction::AssignField { dest, object, name, value })
            }
```

Use the existing local helper names in `format.rs`; if the parser uses different variable names for comma-separated operands, adapt only the variable names and keep the exact error messages above.

- [ ] **Step 3: Format `assign_field`**

In `vm-rs/src/format.rs`, in the formatter `match`, add:

```rust
        Instruction::AssignField { dest, object, name, value } => {
            writeln!(out, "  {} = assign_field {}, {}, {}", reg(*dest), reg(*object), name_ref(*name), reg(*value))?;
        }
```

Use the existing formatter helper names (`reg`, `name_ref`, and output variable) from nearby `Instruction::Field` code.

- [ ] **Step 4: Execute mutation in Rust VM**

In `vm-rs/src/vm.rs`, add an instruction arm after `Instruction::Field`:

```rust
                Instruction::AssignField { dest, object, name, value } => {
                    let object_value = self.read_register(frame, object)?.clone();
                    let assigned_value = self.read_register(frame, value)?.clone();
                    let field_name = program.name(name)?.to_owned();
                    let result = self.assign_field(object_value, &field_name, assigned_value)?;
                    self.write_register(frame, dest, result)?;
                }
```

Then add a helper near existing `field` access helper:

```rust
    fn assign_field(&self, object: Value, name: &str, value: Value) -> Result<Value, RuntimeError> {
        let Value::Struct(struct_value) = object else {
            return Err(RuntimeError::new("can only assign fields on structs"));
        };

        let mut fields = struct_value.fields.borrow_mut();
        for (field_name, field_value) in fields.iter_mut() {
            if field_name == name {
                *field_value = value.clone();
                return Ok(value);
            }
        }

        Err(RuntimeError::new(format!("undefined field `{}`", name)))
    }
```

Adapt `borrow_mut()` to the actual storage type if existing struct fields are not a `RefCell`; keep the same runtime error strings.

- [ ] **Step 5: Run Rust-focused verification**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: Rust unit tests PASS, bytecode artifact parser PASS, and Rust VM golden parity includes `struct_field_assignment`.

- [ ] **Step 6: Commit Rust VM parity**

```bash
git add vm-rs/src/bytecode.rs vm-rs/src/format.rs vm-rs/src/vm.rs tests/bytecode_artifacts/struct_field_assignment/program.cdbc
git commit -m "feat: execute struct field assignment in Rust VM"
```

---

### Task 7: Add parse-error regression fixtures

**Files:**
- Create: `tests/golden/parse_errors/field_assignment_missing_value.cd`
- Create: `tests/golden/parse_errors/field_assignment_missing_value.err`
- Create: `tests/golden/parse_errors/field_assignment_missing_value.exit`
- Create: `tests/golden/parse_errors/field_assignment_invalid_target.cd`
- Create: `tests/golden/parse_errors/field_assignment_invalid_target.err`
- Create: `tests/golden/parse_errors/field_assignment_invalid_target.exit`

- [ ] **Step 1: Add missing RHS parse-error fixture**

Create `tests/golden/parse_errors/field_assignment_missing_value.cd`:

```cd
let person = { age: 36 };
person.age = ;
```

Create `tests/golden/parse_errors/field_assignment_missing_value.err` after checking actual parser location with `./build/compiler_design --ast tests/golden/parse_errors/field_assignment_missing_value.cd`. Expected shape:

```text
Parse error at 2:14: expected expression
```

Create `tests/golden/parse_errors/field_assignment_missing_value.exit`:

```text
1
```

- [ ] **Step 2: Add invalid target regression fixture**

Create `tests/golden/parse_errors/field_assignment_invalid_target.cd`:

```cd
(person.age + 1) = 2;
```

Create `tests/golden/parse_errors/field_assignment_invalid_target.err`:

```text
Parse error at 1:18: invalid assignment target
```

Create `tests/golden/parse_errors/field_assignment_invalid_target.exit`:

```text
1
```

- [ ] **Step 3: Run parse-error verification and update exact locations if needed**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/parse_errors/field_assignment_missing_value.cd
python3 tests/run_golden_tests.py ./build/compiler_design tests/golden/parse_errors/field_assignment_invalid_target.cd
```

Expected: PASS. If a column differs, update only the `.err` column to the actual parser token location and rerun.

- [ ] **Step 4: Commit parse-error coverage**

```bash
git add tests/golden/parse_errors/field_assignment_missing_value.* tests/golden/parse_errors/field_assignment_invalid_target.*
git commit -m "test: cover struct field assignment parse errors"
```

---

### Task 8: Update documentation and roadmap

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/bytecode-text-format.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar docs**

In `docs/language-grammar.ebnf`, update the assignment/comment section so field assignment is documented as an assignment target:

```ebnf
assignment     = logical_or, [ "=", assignment ] ;
assignment_target = IDENTIFIER | postfix, "[", expression, "]" | postfix, ".", IDENTIFIER ;
```

Keep the surrounding grammar style consistent with the existing file.

- [ ] **Step 2: Update README struct docs**

In `README.md`, extend the struct section with:

```markdown
Struct fields can be reassigned when the field already exists. Field assignment evaluates to the assigned value, and aliases observe the mutation because structs are reference values.

```cd
let person = { name: "Ada", age: 36 };
person.age = 37;
print person.age;      // 37
print person.age = 38; // 38
```
```

- [ ] **Step 3: Update bytecode text format docs**

In `docs/bytecode-text-format.md`, add the instruction form near `field`:

```text
rD = assign_field rObject, nField, rValue
```

Document that it mutates an existing struct field and returns the assigned value.

- [ ] **Step 4: Mark roadmap Phase 12B complete**

In `docs/roadmap.md`, update Phase 12B / struct field assignment from planned/in-progress to implemented. Mention that field creation, named structs, and field typing remain future work.

- [ ] **Step 5: Update project memory**

In `AGENTS.md`, adjust current language semantics to say structs support field access and existing-field assignment, while assignment to missing fields is a runtime error.

- [ ] **Step 6: Commit docs**

```bash
git add README.md docs/language-grammar.ebnf docs/bytecode-text-format.md docs/roadmap.md AGENTS.md
git commit -m "docs: document struct field assignment"
```

---

### Task 9: Full verification and cleanup

**Files:**
- No source edits expected.
- Remove: `tests/__pycache__/` if created.

- [ ] **Step 1: Run full verification**

Run from repo root:

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

Expected: every command exits 0.

- [ ] **Step 2: Inspect workspace state**

Run:

```bash
git status --short
git log --oneline -5
```

Expected: only intentional committed changes are present; no build or cache artifacts are staged/untracked.

- [ ] **Step 3: Final handoff**

Report:

```text
Implemented Phase 12B struct field assignment.
Verification:
- cmake -S . -B build: PASS
- cmake --build build: PASS
- ctest --test-dir build --output-on-failure: PASS
- python3 tests/run_golden_tests.py ./build/compiler_design: PASS
- python3 tests/run_golden_tests_selftest.py: PASS
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: PASS
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: PASS
- cargo test --manifest-path vm-rs/Cargo.toml: PASS
```

---

## Self-Review

- Spec coverage: syntax, AST, type checks, runtime mutation, IR, bytecode, `.cdbc`, Rust VM, parse/type/runtime/success tests, and docs are covered by Tasks 1-8.
- Placeholder scan: no `TBD`, `TODO`, or unspecified test-writing steps remain; exact fixture contents and commands are included.
- Type consistency: the same names are used throughout: `FieldAssignExpr`, `emitAssignField`, `IROp::AssignField`, `BytecodeOp::AssignField`, and Rust `Instruction::AssignField`.
