# Variable Compound Assignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add variable-only numeric compound assignment operators `+=`, `-=`, `*=`, and `/=` that update existing variables and evaluate to the assigned value.

**Architecture:** Add compound assignment tokens and a `CompoundAssignExpr` AST node parsed at the same precedence as plain assignment. Type checking resolves variable targets and statically rejects known non-number variables or values. IR lowering loads the old value, evaluates the RHS once, runtime-checks both operands as numbers, applies the existing binary op, assigns the variable, and returns the result; a small `assert_number` IR/bytecode operation is added because existing `+` also accepts strings and `+=` is intentionally numeric-only.

**Tech Stack:** C++17 lexer/parser/AST/type checker/IR/bytecode compiler/IR interpreter, Python golden tests, `.cdbc` bytecode artifacts, Rust VM parser/formatter/executor.

---

## File Structure

- Modify `include/Token.hpp`, `src/Lexer.cpp`: add `PlusEqual`, `MinusEqual`, `StarEqual`, and `SlashEqual` tokens.
- Modify `include/Ast.hpp`, `src/Ast.cpp`: add `CompoundAssignExpr` and printer output.
- Modify `include/Parser.hpp`, `src/Parser.cpp`: parse compound assignment and reject non-variable targets.
- Modify `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: record resolved compound assignment targets and type-check numeric-only rules.
- Modify `include/IR.hpp`, `src/IR.cpp`: add `IROp::AssertNumber` and `IRProgram::emitAssertNumber` for runtime numeric enforcement.
- Modify `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: execute `assert_number` with operator-specific messages.
- Modify `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: lower `CompoundAssignExpr` to load/assert/assert/binary/assign.
- Modify `include/Bytecode.hpp`, `src/Bytecode.cpp`, `src/BytecodeCompiler.cpp`, `src/BytecodeTextEmitter.cpp`: lower and print `assert_number` bytecode.
- Modify `vm-rs/src/bytecode.rs`, `vm-rs/src/format.rs`, `vm-rs/src/vm.rs`: parse, format, and execute `assert_number` in Rust.
- Modify `tests/run_rust_vm_tests.py`: include representative compound assignment success fixtures in Rust VM parity.
- Create `tests/golden/compound_assignment_*`: success fixtures.
- Create `tests/golden/parse_errors/compound_assignment_*`: unsupported target fixtures.
- Create `tests/golden/type_errors/compound_assignment_*`: static diagnostics.
- Create `tests/golden/runtime_errors/compound_assignment_*`: dynamic diagnostics.
- Create `tests/bytecode_artifacts/compound_assignment/`: `.cdbc` artifact fixture.
- Modify `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, `AGENTS.md`: document implemented compound assignment.

---

### Task 1: RED success fixtures for compound assignment

**Files:**
- Create: `tests/golden/compound_assignment_basic/input.cd`
- Create: `tests/golden/compound_assignment_basic/ast.out`
- Create: `tests/golden/compound_assignment_basic/run.out`
- Create: `tests/golden/compound_assignment_expression_result/input.cd`
- Create: `tests/golden/compound_assignment_expression_result/run.out`

- [ ] **Step 1: Add basic success fixture**

Create `tests/golden/compound_assignment_basic/input.cd`:

```cd
let x = 1;
print x += 2;
print x -= 1;
print x *= 5;
print x /= 2;
print x;
```

Create `tests/golden/compound_assignment_basic/ast.out`:

```text
Program
  Let x = 1
  Print (+= x 2)
  Print (-= x 1)
  Print (*= x 5)
  Print (/= x 2)
  Print x
```

Create `tests/golden/compound_assignment_basic/run.out`:

```text
3
2
10
5
5
```

- [ ] **Step 2: Add expression-result fixture**

Create `tests/golden/compound_assignment_expression_result/input.cd`:

```cd
let x = 1;
let y = (x += 4) * 2;
print x;
print y;
```

Create `tests/golden/compound_assignment_expression_result/run.out`:

```text
5
10
```

- [ ] **Step 3: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: the new fixtures fail with parse errors because `+=`, `-=`, `*=`, and `/=` are not lexed or parsed yet.

- [ ] **Step 4: Commit RED fixtures**

```bash
git add tests/golden/compound_assignment_basic tests/golden/compound_assignment_expression_result
git commit -m "test: add compound assignment fixtures"
```

---

### Task 2: Lexer, AST, parser, and parse errors

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Create: `tests/golden/parse_errors/compound_assignment_index_target.cd`
- Create: `tests/golden/parse_errors/compound_assignment_index_target.err`
- Create: `tests/golden/parse_errors/compound_assignment_index_target.exit`
- Create: `tests/golden/parse_errors/compound_assignment_field_target.cd`
- Create: `tests/golden/parse_errors/compound_assignment_field_target.err`
- Create: `tests/golden/parse_errors/compound_assignment_field_target.exit`

- [ ] **Step 1: Add parse-error fixtures**

Create `tests/golden/parse_errors/compound_assignment_index_target.cd`:

```cd
let xs = [1];
xs[0] += 1;
```

Create `tests/golden/parse_errors/compound_assignment_index_target.err`:

```text
Parse error at 2:7: invalid compound assignment target
```

Create `tests/golden/parse_errors/compound_assignment_index_target.exit`:

```text
1
```

Create `tests/golden/parse_errors/compound_assignment_field_target.cd`:

```cd
let s = { value: 1 };
s.value += 1;
```

Create `tests/golden/parse_errors/compound_assignment_field_target.err`:

```text
Parse error at 2:9: invalid compound assignment target
```

Create `tests/golden/parse_errors/compound_assignment_field_target.exit`:

```text
1
```

- [ ] **Step 2: Add compound assignment tokens**

In `include/Token.hpp`, add the compound tokens after the single-character arithmetic tokens:

```cpp
    Plus,
    Minus,
    Star,
    Slash,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
```

In `src/Lexer.cpp`, update arithmetic scanning:

```cpp
    case '+':
        addToken(match('=') ? TokenType::PlusEqual : TokenType::Plus);
        break;
    case '-':
        addToken(match('=') ? TokenType::MinusEqual : TokenType::Minus);
        break;
    case '*':
        addToken(match('=') ? TokenType::StarEqual : TokenType::Star);
        break;
    case '/':
        if (match('/')) {
            while (peek() != '\n' && !isAtEnd()) {
                advance();
            }
        } else {
            addToken(match('=') ? TokenType::SlashEqual : TokenType::Slash);
        }
        break;
```

In `tokenTypeName`, add:

```cpp
    case TokenType::PlusEqual:
        return "PlusEqual";
    case TokenType::MinusEqual:
        return "MinusEqual";
    case TokenType::StarEqual:
        return "StarEqual";
    case TokenType::SlashEqual:
        return "SlashEqual";
```

- [ ] **Step 3: Add `CompoundAssignExpr` AST node**

In `include/Ast.hpp`, after `AssignExpr`, add:

```cpp
struct CompoundAssignExpr final : Expr {
    CompoundAssignExpr(Token name, Token op, ExprPtr value);
    void print(std::ostream& out) const override;

    Token name;
    Token op;
    ExprPtr value;
};
```

In `src/Ast.cpp`, add constructor and printer after `AssignExpr::print`:

```cpp
CompoundAssignExpr::CompoundAssignExpr(Token name, Token op, ExprPtr value)
    : name(std::move(name))
    , op(std::move(op))
    , value(std::move(value))
{
}

void CompoundAssignExpr::print(std::ostream& out) const
{
    out << '(' << op.lexeme << ' ' << name.lexeme << ' ';
    writeExpr(out, value);
    out << ')';
}
```

- [ ] **Step 4: Add parser helper declaration**

In `include/Parser.hpp`, add this private helper near `assignment()`:

```cpp
    bool matchCompoundAssignment();
```

- [ ] **Step 5: Parse variable compound assignment**

In `src/Parser.cpp`, add helper near other parser helpers:

```cpp
bool Parser::matchCompoundAssignment()
{
    return match(TokenType::PlusEqual)
        || match(TokenType::MinusEqual)
        || match(TokenType::StarEqual)
        || match(TokenType::SlashEqual);
}
```

In `Parser::assignment()`, after the existing plain `=` block and before `return expr;`, add:

```cpp
    if (matchCompoundAssignment()) {
        Token op = previous();
        ExprPtr value = assignment();

        if (const auto* variable = dynamic_cast<const VariableExpr*>(expr.get())) {
            return std::make_unique<CompoundAssignExpr>(variable->name, std::move(op), std::move(value));
        }

        throw ParseError(op, "invalid compound assignment target");
    }
```

- [ ] **Step 6: Verify parser and AST**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: parse-error fixtures pass and `compound_assignment_basic` AST passes. Run modes still fail because type checking does not handle `CompoundAssignExpr` yet.

- [ ] **Step 7: Commit parser slice**

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/parse_errors/compound_assignment_* tests/golden/compound_assignment_basic/ast.out
git commit -m "feat: parse compound assignment"
```

---

### Task 3: Type checking and resolved target names

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/compound_assignment_undefined.cd`
- Create: `tests/golden/type_errors/compound_assignment_undefined.err`
- Create: `tests/golden/type_errors/compound_assignment_undefined.exit`
- Create: `tests/golden/type_errors/compound_assignment_non_number_variable.cd`
- Create: `tests/golden/type_errors/compound_assignment_non_number_variable.err`
- Create: `tests/golden/type_errors/compound_assignment_non_number_variable.exit`
- Create: `tests/golden/type_errors/compound_assignment_non_number_value.cd`
- Create: `tests/golden/type_errors/compound_assignment_non_number_value.err`
- Create: `tests/golden/type_errors/compound_assignment_non_number_value.exit`
- Create: `tests/golden/type_errors/compound_assignment_typed_non_number_value.cd`
- Create: `tests/golden/type_errors/compound_assignment_typed_non_number_value.err`
- Create: `tests/golden/type_errors/compound_assignment_typed_non_number_value.exit`

- [ ] **Step 1: Add type-error fixtures**

Create `tests/golden/type_errors/compound_assignment_undefined.cd`:

```cd
x += 1;
```

Create `tests/golden/type_errors/compound_assignment_undefined.err`:

```text
Type error at 1:1: undefined variable `x`
```

Create `tests/golden/type_errors/compound_assignment_undefined.exit`:

```text
1
```

Create `tests/golden/type_errors/compound_assignment_non_number_variable.cd`:

```cd
let s = "x";
s += 1;
```

Create `tests/golden/type_errors/compound_assignment_non_number_variable.err`:

```text
Type error at 2:3: `+=` expects number variable, got string
```

Create `tests/golden/type_errors/compound_assignment_non_number_variable.exit`:

```text
1
```

Create `tests/golden/type_errors/compound_assignment_non_number_value.cd`:

```cd
let n = 1;
n += true;
```

Create `tests/golden/type_errors/compound_assignment_non_number_value.err`:

```text
Type error at 2:3: `+=` expects number value, got bool
```

Create `tests/golden/type_errors/compound_assignment_non_number_value.exit`:

```text
1
```

Create `tests/golden/type_errors/compound_assignment_typed_non_number_value.cd`:

```cd
let n: number = 1;
n += "x";
```

Create `tests/golden/type_errors/compound_assignment_typed_non_number_value.err`:

```text
Type error at 2:3: `+=` expects number value, got string
```

Create `tests/golden/type_errors/compound_assignment_typed_non_number_value.exit`:

```text
1
```

- [ ] **Step 2: Extend `ResolvedNames` for compound assignment**

In `include/TypeChecker.hpp`, add public accessor after `assignmentName`:

```cpp
    const std::string& compoundAssignmentName(const CompoundAssignExpr& expression) const;
```

Add private recorder after `recordAssignment`:

```cpp
    void recordCompoundAssignment(const CompoundAssignExpr& expression, std::string name);
```

Add storage after `assignmentNames_`:

```cpp
    std::unordered_map<const CompoundAssignExpr*, std::string> compoundAssignmentNames_;
```

In `src/TypeChecker.cpp`, add implementation after `ResolvedNames::assignmentName`:

```cpp
const std::string& ResolvedNames::compoundAssignmentName(const CompoundAssignExpr& expression) const
{
    const auto found = compoundAssignmentNames_.find(&expression);
    if (found == compoundAssignmentNames_.end()) {
        throw std::logic_error("missing resolved compound assignment name");
    }
    return found->second;
}
```

Add to `ResolvedNames::clear()`:

```cpp
    compoundAssignmentNames_.clear();
```

Add recorder after `recordAssignment`:

```cpp
void ResolvedNames::recordCompoundAssignment(const CompoundAssignExpr& expression, std::string name)
{
    compoundAssignmentNames_.emplace(&expression, std::move(name));
}
```

- [ ] **Step 3: Type-check compound assignment**

In `src/TypeChecker.cpp`, in `TypeChecker::checkExpressionInfo`, add this branch immediately after the plain `AssignExpr` branch:

```cpp
    if (const auto* compound = dynamic_cast<const CompoundAssignExpr*>(&expression)) {
        Binding* target = findVariable(compound->name.lexeme);
        if (!target) {
            if (findNamespace(compound->name.lexeme)) {
                throw TypeError(compound->name, "cannot assign to namespace alias `" + compound->name.lexeme + "`");
            }
            throw TypeError(compound->name, "undefined variable `" + compound->name.lexeme + "`");
        }

        const CheckedExpression value = checkExpressionInfo(*compound->value);
        if (target->type.kind != StaticType::Unknown && target->type.kind != StaticType::Number) {
            throw TypeError(compound->op,
                "`" + compound->op.lexeme + "` expects number variable, got " + typeInfoName(target->type));
        }
        if (value.type.kind != StaticType::Unknown && value.type.kind != StaticType::Number) {
            throw TypeError(compound->op,
                "`" + compound->op.lexeme + "` expects number value, got " + typeInfoName(value.type));
        }

        if (!isKnown(target->type)) {
            target->type = simpleType(StaticType::Number);
        }
        resolvedNames_.recordCompoundAssignment(*compound, target->resolvedName);
        return CheckedExpression{simpleType(StaticType::Number)};
    }
```

This intentionally narrows unknown variable bindings to `number` after a valid compound assignment, matching numeric-only semantics.

- [ ] **Step 4: Verify type checker**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: new type-error fixtures pass. Success fixtures fail in compile/run modes because `IRCompiler` does not lower `CompoundAssignExpr` yet.

- [ ] **Step 5: Commit type checker slice**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/compound_assignment_*
git commit -m "feat: type check compound assignment"
```

---

### Task 4: Runtime numeric checks, IR lowering, bytecode, and Rust VM parity

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/Bytecode.hpp`
- Modify: `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `src/BytecodeTextEmitter.cpp`
- Modify: `vm-rs/src/bytecode.rs`
- Modify: `vm-rs/src/format.rs`
- Modify: `vm-rs/src/vm.rs`
- Modify: `tests/run_rust_vm_tests.py`
- Create: `tests/golden/runtime_errors/compound_assignment_dynamic_non_number_value.cd`
- Create: `tests/golden/runtime_errors/compound_assignment_dynamic_non_number_value.run.err`
- Create: `tests/golden/runtime_errors/compound_assignment_dynamic_non_number_value.exit`
- Create: `tests/golden/runtime_errors/compound_assignment_dynamic_string_plus.cd`
- Create: `tests/golden/runtime_errors/compound_assignment_dynamic_string_plus.run.err`
- Create: `tests/golden/runtime_errors/compound_assignment_dynamic_string_plus.exit`
- Create: `tests/golden/runtime_errors/compound_assignment_division_by_zero.cd`
- Create: `tests/golden/runtime_errors/compound_assignment_division_by_zero.run.err`
- Create: `tests/golden/runtime_errors/compound_assignment_division_by_zero.exit`
- Create: `tests/bytecode_artifacts/compound_assignment/input.cd`
- Create: `tests/bytecode_artifacts/compound_assignment/run.out`
- Create: `tests/bytecode_artifacts/compound_assignment/expected.cdbc`

- [ ] **Step 1: Add runtime-error fixtures**

Create `tests/golden/runtime_errors/compound_assignment_dynamic_non_number_value.cd`:

```cd
fun id(x) { return x; }
let n = id(1);
n += id("x");
```

Create `tests/golden/runtime_errors/compound_assignment_dynamic_non_number_value.run.err`:

```text
Runtime error: `+=` expects number value
```

Create `tests/golden/runtime_errors/compound_assignment_dynamic_non_number_value.exit`:

```text
1
```

Create `tests/golden/runtime_errors/compound_assignment_dynamic_string_plus.cd`:

```cd
fun id(x) { return x; }
let n = id("a");
n += id("b");
```

Create `tests/golden/runtime_errors/compound_assignment_dynamic_string_plus.run.err`:

```text
Runtime error: `+=` expects number variable
```

Create `tests/golden/runtime_errors/compound_assignment_dynamic_string_plus.exit`:

```text
1
```

Create `tests/golden/runtime_errors/compound_assignment_division_by_zero.cd`:

```cd
let n = 1;
n /= 0;
```

Create `tests/golden/runtime_errors/compound_assignment_division_by_zero.run.err`:

```text
Runtime error: division by zero
```

Create `tests/golden/runtime_errors/compound_assignment_division_by_zero.exit`:

```text
1
```

- [ ] **Step 2: Add `assert_number` IR operation**

In `include/IR.hpp`, add `AssertNumber` after `AssertArray` in `enum class IROp` and add this method declaration after `emitAssertArray`:

```cpp
    IRRegister emitAssertNumber(IRRegister value, std::string message);
```

In `src/IR.cpp`, update unary/binary classification and `irOpName` to include:

```cpp
    case IROp::AssertNumber:
        return "assert_number";
```

Add the emitter after `emitAssertArray`:

```cpp
IRRegister IRProgram::emitAssertNumber(IRRegister value, std::string message)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::AssertNumber, dest, value, std::nullopt, {}, addName(std::move(message))});
    return dest;
}
```

In the IR printer's instruction switch, print message names:

```cpp
    } else if (instruction.op == IROp::AssertNumber) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        printNameOperand(out, program, instruction.operand);
```

- [ ] **Step 3: Execute `assert_number` in C++ runtime**

In `include/IRInterpreter.hpp`, add:

```cpp
    Value executeAssertNumber(const IRProgram& program, const Frame& frame, IRRegister value, std::size_t messageIndex);
```

In `src/IRInterpreter.cpp`, dispatch in `executeInstructions`:

```cpp
        case IROp::AssertNumber:
            writeRegister(frame, readDest(instruction), executeAssertNumber(program, frame, readLeft(instruction), instruction.operand));
            break;
```

Add implementation near `executeAssertArray`:

```cpp
Value IRInterpreter::executeAssertNumber(const IRProgram& program, const Frame& frame, IRRegister value, std::size_t messageIndex)
{
    const Value& input = readRegister(frame, value);
    if (input.type() != Value::Type::Number) {
        throw IRRuntimeError(readName(program, messageIndex));
    }
    return input;
}
```

- [ ] **Step 4: Add bytecode `assert_number` support in C++**

In `include/Bytecode.hpp`, add `AssertNumber` after `AssertArray`.

In `src/Bytecode.cpp`, add `BytecodeOp::AssertNumber` to `bytecodeOpName` as `"assert_number"` and to the bytecode printer with message name output:

```cpp
    } else if (instruction.op == BytecodeOp::AssertNumber) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        printNameOperand(out, program, instruction.operand);
```

In `src/BytecodeCompiler.cpp`, map `IROp::AssertNumber` to `BytecodeOp::AssertNumber`.

In `src/BytecodeTextEmitter.cpp`, add text emission:

```cpp
    case BytecodeOp::AssertNumber:
        out << reg(requireDest(instruction)) << " = assert_number " << reg(requireLeft(instruction)) << ", " << nameRef(instruction.operand);
        break;
```

- [ ] **Step 5: Add Rust bytecode `assert_number` support**

In `vm-rs/src/bytecode.rs`, add near `AssertArray`:

```rust
    AssertNumber {
        dest: usize,
        value: usize,
        message: usize,
    },
```

In `vm-rs/src/format.rs`, parse instruction opcode `"assert_number"` with a register and name operand:

```rust
            "assert_number" => {
                let (value, message) = split_once(line, operands, ", ")?;
                Ok(Instruction::AssertNumber {
                    dest,
                    value: parse_register(line, value)?,
                    message: parse_name_ref(line, message)?,
                })
            }
```

Format it as:

```rust
        Instruction::AssertNumber { dest, value, message } => {
            format!("r{} = assert_number r{}, n{}", dest, value, message)
        }
```

Add one `assert_number` line to the existing `parses_all_opcode_shapes` Rust unit test source and increase the main register count if needed.

In `vm-rs/src/vm.rs`, execute:

```rust
                Instruction::AssertNumber { dest, value, message } => {
                    let input = self.read_register(frame, *value)?;
                    if !matches!(input, Value::Number(_)) {
                        let message = self.read_name(*message)?;
                        return Err(RuntimeError::new(message));
                    }
                    self.write_register(frame, *dest, input)?;
                }
```

- [ ] **Step 6: Add IRCompiler declarations and operator helper**

In `include/IRCompiler.hpp`, add:

```cpp
    IRRegister emitCompoundAssign(const CompoundAssignExpr& expression);
    IROp compoundAssignmentOp(TokenType op) const;
```

In `src/IRCompiler.cpp`, add dispatch in `compileExpression` after `AssignExpr`:

```cpp
    if (const auto* compound = dynamic_cast<const CompoundAssignExpr*>(&expression)) {
        return emitCompoundAssign(*compound);
    }
```

Add helper:

```cpp
IROp IRCompiler::compoundAssignmentOp(TokenType op) const
{
    switch (op) {
    case TokenType::PlusEqual:
        return IROp::Add;
    case TokenType::MinusEqual:
        return IROp::Subtract;
    case TokenType::StarEqual:
        return IROp::Multiply;
    case TokenType::SlashEqual:
        return IROp::Divide;
    default:
        throw IRCompileError("unsupported compound assignment operator: " + tokenTypeName(op));
    }
}
```

- [ ] **Step 7: Lower compound assignment**

In `src/IRCompiler.cpp`, add implementation near `emitIndexAssign`:

```cpp
IRRegister IRCompiler::emitCompoundAssign(const CompoundAssignExpr& expression)
{
    const std::string& name = resolvedNames_->compoundAssignmentName(expression);
    const IRRegister oldValue = ir_.emitLoadVar(name);
    const IRRegister checkedOldValue = ir_.emitAssertNumber(
        oldValue, "`" + expression.op.lexeme + "` expects number variable");
    const IRRegister value = compileExpression(*expression.value);
    const IRRegister checkedValue = ir_.emitAssertNumber(
        value, "`" + expression.op.lexeme + "` expects number value");
    const IRRegister result = ir_.emitBinary(compoundAssignmentOp(expression.op.type), checkedOldValue, checkedValue);
    ir_.emitAssignVar(name, result);
    return result;
}
```

- [ ] **Step 8: Add Rust VM golden allowlist**

In `tests/run_rust_vm_tests.py`, add:

```python
            "compound_assignment_basic",
            "compound_assignment_expression_result",
```

near the other golden allowlist entries.

- [ ] **Step 9: Generate success IR/bytecode goldens**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --update
git diff -- tests/golden/compound_assignment_basic tests/golden/compound_assignment_expression_result
```

Expected: `ir.out`, `bytecode.out`, and missing `ast.out` files are created/updated for compound assignment success fixtures. Review that IR includes `assert_number` before the binary operation.

If `--update` modifies unrelated existing goldens, run `git restore` on unrelated paths before continuing.

- [ ] **Step 10: Add bytecode artifact fixture**

Create `tests/bytecode_artifacts/compound_assignment/input.cd`:

```cd
let x = 1;
x += 2;
print x;
```

Create `tests/bytecode_artifacts/compound_assignment/run.out`:

```text
3
```

Generate `expected.cdbc`:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/compound_assignment/expected.cdbc tests/bytecode_artifacts/compound_assignment/input.cd
sed -n '1,200p' tests/bytecode_artifacts/compound_assignment/expected.cdbc
```

Expected: `expected.cdbc` includes `assert_number` instructions before `add`.

- [ ] **Step 11: Verify runtime, bytecode, and Rust VM**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: all commands exit 0. Golden tests include runtime errors for dynamic non-number value, dynamic string `+=`, and division by zero.

- [ ] **Step 12: Commit lowering and runtime slice**

```bash
git add include/IR.hpp src/IR.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp include/IRCompiler.hpp src/IRCompiler.cpp include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp src/BytecodeTextEmitter.cpp vm-rs/src/bytecode.rs vm-rs/src/format.rs vm-rs/src/vm.rs tests/run_rust_vm_tests.py tests/golden/compound_assignment_* tests/golden/runtime_errors/compound_assignment_* tests/bytecode_artifacts/compound_assignment
git commit -m "feat: lower compound assignment"
```

---

### Task 5: Documentation and final verification

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update EBNF grammar**

In `docs/language-grammar.ebnf`, update assignment grammar to include compound operators:

```ebnf
assignment  = ( identifier, ( "=" | "+=" | "-=" | "*=" | "/=" ), assignment )
            | logicOr ;
```

If the file currently expresses assignment differently, preserve existing precedence wording and add compound assignment at the same level as plain assignment.

- [ ] **Step 2: Update README expression section**

In `README.md`, add to supported expressions near assignment:

```markdown
- Compound assignment: `name += expression`, `name -= expression`, `name *= expression`, and `name /= expression` update an existing numeric variable and evaluate to the assigned value. This first slice supports variable targets only; array index and struct field compound assignment are not implemented yet.
```

- [ ] **Step 3: Update roadmap Phase 15**

In `docs/roadmap.md`, update Phase 15 status and near-term queue:

```markdown
Phase 15D is implemented for numeric variable compound assignment (`+=`, `-=`, `*=`, `/=`). Index and field compound assignment remain future work.
```

Remove Phase 15D from the top near-term recommendation queue and make `typeOf(value)` the next suggested small slice.

- [ ] **Step 4: Update AGENTS project memory**

In `AGENTS.md`, update current semantics with:

```markdown
- Numeric variable compound assignment supports `name += expression`, `name -= expression`, `name *= expression`, and `name /= expression`. It updates an existing variable, evaluates to the assigned value, and is numeric-only; array index and struct field compound assignment are not implemented yet.
```

- [ ] **Step 5: Run full verification**

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
git status --short
```

Expected:

- CMake configure and build exit 0.
- CTest reports all tests passed.
- Golden tests report 0 failed.
- Golden selftests report OK.
- Bytecode artifact tests report 0 failed.
- Rust VM golden tests report 0 failed.
- Cargo tests report all tests passed.
- `git status --short` shows only intentional documentation changes before the final commit, then clean after committing.

- [ ] **Step 6: Commit docs**

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document compound assignment"
```

- [ ] **Step 7: Report results**

After final verification, final commit, and clean status, report the commits made and the exact verification command results.
