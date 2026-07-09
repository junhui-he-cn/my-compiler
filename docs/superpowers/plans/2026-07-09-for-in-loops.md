# Array For-In Loops Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `for item in array { ... }` loops for array iteration with loop-scoped item bindings, `break`/`continue`, static checks, runtime checks, bytecode artifacts, and Rust VM parity.

**Architecture:** Add `in` as a keyword and a `ForInStmt` AST node, then disambiguate `for identifier in expression { ... }` from existing C-style `for` clauses in the parser. Type checking declares the loop variable in the body scope with the array element type when known. IR lowering expands `for-in` to existing control flow using an iterable temp, index temp, length snapshot, element load, and loop variable store; a new `assert_array` IR/bytecode operation gives the required `for-in expects array` runtime diagnostic before `len` accepts strings.

**Tech Stack:** C++17 lexer/parser/AST/type checker/IR/bytecode compiler/IR interpreter, Python golden tests, `.cdbc` bytecode artifacts, Rust VM parser/formatter/executor.

---

## File Structure

- Modify `include/Token.hpp`, `src/Lexer.cpp`: add reserved keyword token `in`.
- Modify `include/Ast.hpp`, `src/Ast.cpp`: add `ForInStmt` and printer output.
- Modify `include/Parser.hpp`, `src/Parser.cpp`: parse `for name in expression { ... }` before falling back to existing C-style `for` parsing.
- Modify `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: record resolved loop variable names and type-check for-in loops.
- Modify `include/IR.hpp`, `src/IR.cpp`: add `IROp::AssertArray` plus `IRProgram::emitAssertArray`.
- Modify `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: lower `ForInStmt` using existing loop-control machinery.
- Modify `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: execute `assert_array` with `Runtime error: for-in expects array`.
- Modify `include/Bytecode.hpp`, `src/Bytecode.cpp`, `src/BytecodeCompiler.cpp`, `src/BytecodeTextEmitter.cpp`: lower and print `assert_array` bytecode.
- Modify `vm-rs/src/bytecode.rs`, `vm-rs/src/format.rs`, `vm-rs/src/vm.rs`: parse, format, and execute `assert_array` in Rust.
- Modify `tests/run_rust_vm_tests.py`: include representative `for_in_*` success fixtures in Rust VM parity.
- Create `tests/golden/for_in_*`: success fixtures.
- Create `tests/golden/parse_errors/for_in_*`: parse diagnostics.
- Create `tests/golden/type_errors/for_in_*`: static diagnostics.
- Create `tests/golden/runtime_errors/for_in_*`: runtime diagnostics.
- Create `tests/bytecode_artifacts/for_in_loops/`: `.cdbc` artifact fixture.
- Modify `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, `AGENTS.md`: document the implemented feature.

---

### Task 1: RED success fixtures for for-in syntax

**Files:**
- Create: `tests/golden/for_in_basic/input.cd`
- Create: `tests/golden/for_in_basic/ast.out`
- Create: `tests/golden/for_in_basic/run.out`
- Create: `tests/golden/for_in_control/input.cd`
- Create: `tests/golden/for_in_control/run.out`
- Create: `tests/golden/for_in_length_snapshot/input.cd`
- Create: `tests/golden/for_in_length_snapshot/run.out`

- [ ] **Step 1: Add basic success fixture**

Create `tests/golden/for_in_basic/input.cd`:

```cd
let xs = [1, 2, 3];
for x in xs {
  print x;
}
```

Create `tests/golden/for_in_basic/ast.out`:

```text
Program
  Let xs = (array 1 2 3)
  ForIn x in xs
    Block
      Print x
```

Create `tests/golden/for_in_basic/run.out`:

```text
1
2
3
```

- [ ] **Step 2: Add break/continue success fixture**

Create `tests/golden/for_in_control/input.cd`:

```cd
for x in [1, 2, 3, 4, 5] {
  if x == 2 {
    continue;
  }
  if x == 5 {
    break;
  }
  print x;
}
```

Create `tests/golden/for_in_control/run.out`:

```text
1
3
4
```

- [ ] **Step 3: Add length snapshot fixture**

Create `tests/golden/for_in_length_snapshot/input.cd`:

```cd
let xs = [1, 2];
for x in xs {
  print x;
  push(xs, x + 10);
}
print xs;
```

Create `tests/golden/for_in_length_snapshot/run.out`:

```text
1
2
[1, 2, 11, 12]
```

- [ ] **Step 4: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: the new `for_in_*` fixtures fail with parse errors because `in` is not yet a keyword and `for x in xs` is not a valid C-style for clause.

- [ ] **Step 5: Commit RED fixtures**

```bash
git add tests/golden/for_in_basic tests/golden/for_in_control tests/golden/for_in_length_snapshot
git commit -m "test: add for-in loop fixtures"
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
- Create: `tests/golden/parse_errors/for_in_missing_variable.cd`
- Create: `tests/golden/parse_errors/for_in_missing_variable.err`
- Create: `tests/golden/parse_errors/for_in_missing_variable.exit`
- Create: `tests/golden/parse_errors/for_in_missing_iterable.cd`
- Create: `tests/golden/parse_errors/for_in_missing_iterable.err`
- Create: `tests/golden/parse_errors/for_in_missing_iterable.exit`
- Create: `tests/golden/parse_errors/for_in_missing_block.cd`
- Create: `tests/golden/parse_errors/for_in_missing_block.err`
- Create: `tests/golden/parse_errors/for_in_missing_block.exit`
- Create: `tests/golden/parse_errors/for_in_missing_in_or_semicolon.cd`
- Create: `tests/golden/parse_errors/for_in_missing_in_or_semicolon.err`
- Create: `tests/golden/parse_errors/for_in_missing_in_or_semicolon.exit`

- [ ] **Step 1: Add parse-error fixtures**

Create `tests/golden/parse_errors/for_in_missing_variable.cd`:

```cd
for in xs {
}
```

Create `tests/golden/parse_errors/for_in_missing_variable.err`:

```text
Parse error at 1:5: expected for loop initializer before `in`
```

Create `tests/golden/parse_errors/for_in_missing_variable.exit`:

```text
1
```

Create `tests/golden/parse_errors/for_in_missing_iterable.cd`:

```cd
for x in {
}
```

Create `tests/golden/parse_errors/for_in_missing_iterable.err`:

```text
Parse error at 1:10: expected expression
```

Create `tests/golden/parse_errors/for_in_missing_iterable.exit`:

```text
1
```

Create `tests/golden/parse_errors/for_in_missing_block.cd`:

```cd
for x in xs print x;
```

Create `tests/golden/parse_errors/for_in_missing_block.err`:

```text
Parse error at 1:13: expected `{` after for-in iterable
```

Create `tests/golden/parse_errors/for_in_missing_block.exit`:

```text
1
```

Create `tests/golden/parse_errors/for_in_missing_in_or_semicolon.cd`:

```cd
for x xs {
}
```

Create `tests/golden/parse_errors/for_in_missing_in_or_semicolon.err`:

```text
Parse error at 1:7: expected `;` after for initializer
```

Create `tests/golden/parse_errors/for_in_missing_in_or_semicolon.exit`:

```text
1
```

- [ ] **Step 2: Add `In` token and keyword**

In `include/Token.hpp`, add `In` after `Import`:

```cpp
    Import,
    In,
    As,
```

In `src/Lexer.cpp`, add the keyword mapping:

```cpp
        {"in", TokenType::In},
```

In `tokenTypeName`, add:

```cpp
    case TokenType::In:
        return "In";
```

- [ ] **Step 3: Add `ForInStmt` AST node**

In `include/Ast.hpp`, after `ForStmt`, add:

```cpp
struct ForInStmt final : Stmt {
    ForInStmt(Token keyword, Token variable, ExprPtr iterable, StmtPtr body);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
    Token variable;
    ExprPtr iterable;
    StmtPtr body;
};
```

In `src/Ast.cpp`, add dynamic dispatch in the statement printer near `ForStmt`:

```cpp
    if (const auto* forInStmt = dynamic_cast<const ForInStmt*>(&stmt)) {
        forInStmt->print(out, indent);
        return;
    }
```

Then add constructor and printer after `ForStmt::print`:

```cpp
ForInStmt::ForInStmt(Token keyword, Token variable, ExprPtr iterable, StmtPtr body)
    : keyword(std::move(keyword))
    , variable(std::move(variable))
    , iterable(std::move(iterable))
    , body(std::move(body))
{
}

void ForInStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "ForIn " << variable.lexeme << " in ";
    writeExpr(out, iterable);
    out << "\n";
    if (body) {
        body->print(out, indent + 1);
    }
}
```

- [ ] **Step 4: Add parser declaration**

In `include/Parser.hpp`, add after `forStatement()`:

```cpp
    StmtPtr forInStatement(Token keyword, Token variable);
```

- [ ] **Step 5: Parse for-in before C-style for clauses**

In `src/Parser.cpp`, replace the start of `Parser::forStatement()` with:

```cpp
StmtPtr Parser::forStatement()
{
    Token keyword = previous();

    if (check(TokenType::In)) {
        throw ParseError(peek(), "expected for loop initializer before `in`");
    }
    if (check(TokenType::Identifier) && checkNext(TokenType::In)) {
        Token variable = advance();
        advance(); // consume `in`
        return forInStatement(std::move(keyword), std::move(variable));
    }

    StmtPtr initializer = forInitializer();
```

Add this method after `forStatement()`:

```cpp
StmtPtr Parser::forInStatement(Token keyword, Token variable)
{
    ExprPtr iterable = expression();
    consume(TokenType::LeftBrace, "expected `{` after for-in iterable");
    StmtPtr body = blockStatement();
    return std::make_unique<ForInStmt>(std::move(keyword), std::move(variable), std::move(iterable), std::move(body));
}
```

- [ ] **Step 6: Verify parser and AST**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: parse-error fixtures pass and `for_in_basic` AST passes; `--run` for for-in fixtures still fails because type checking and IR lowering do not handle `ForInStmt` yet.

- [ ] **Step 7: Commit parser slice**

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/parse_errors/for_in_* tests/golden/for_in_basic/ast.out
git commit -m "feat: parse for-in loops"
```

---

### Task 3: Type checking and resolved loop variable names

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/for_in_non_array_static.cd`
- Create: `tests/golden/type_errors/for_in_non_array_static.err`
- Create: `tests/golden/type_errors/for_in_non_array_static.exit`
- Create: `tests/golden/type_errors/for_in_item_scope_escape.cd`
- Create: `tests/golden/type_errors/for_in_item_scope_escape.err`
- Create: `tests/golden/type_errors/for_in_item_scope_escape.exit`
- Create: `tests/golden/type_errors/for_in_duplicate_item_decl.cd`
- Create: `tests/golden/type_errors/for_in_duplicate_item_decl.err`
- Create: `tests/golden/type_errors/for_in_duplicate_item_decl.exit`
- Create: `tests/golden/type_errors/for_in_typed_item_mismatch.cd`
- Create: `tests/golden/type_errors/for_in_typed_item_mismatch.err`
- Create: `tests/golden/type_errors/for_in_typed_item_mismatch.exit`
- Create: `tests/golden/type_errors/for_in_break_inside_function.cd`
- Create: `tests/golden/type_errors/for_in_break_inside_function.err`
- Create: `tests/golden/type_errors/for_in_break_inside_function.exit`
- Create: `tests/golden/type_errors/for_in_continue_inside_function.cd`
- Create: `tests/golden/type_errors/for_in_continue_inside_function.err`
- Create: `tests/golden/type_errors/for_in_continue_inside_function.exit`

- [ ] **Step 1: Add type-error fixtures**

Create `tests/golden/type_errors/for_in_non_array_static.cd`:

```cd
for x in 123 {
  print x;
}
```

Create `tests/golden/type_errors/for_in_non_array_static.err`:

```text
Type error at 1:7: for-in expects array, got number
```

Create `tests/golden/type_errors/for_in_non_array_static.exit`:

```text
1
```

Create `tests/golden/type_errors/for_in_item_scope_escape.cd`:

```cd
for x in [1] {
}
print x;
```

Create `tests/golden/type_errors/for_in_item_scope_escape.err`:

```text
Type error at 3:7: undefined variable `x`
```

Create `tests/golden/type_errors/for_in_item_scope_escape.exit`:

```text
1
```

Create `tests/golden/type_errors/for_in_duplicate_item_decl.cd`:

```cd
for x in [1] {
  let x = 2;
}
```

Create `tests/golden/type_errors/for_in_duplicate_item_decl.err`:

```text
Type error at 2:7: variable `x` already declared in this scope
```

Create `tests/golden/type_errors/for_in_duplicate_item_decl.exit`:

```text
1
```

Create `tests/golden/type_errors/for_in_typed_item_mismatch.cd`:

```cd
let xs: [number] = [1];
for x in xs {
  let y: string = x;
}
```

Create `tests/golden/type_errors/for_in_typed_item_mismatch.err`:

```text
Type error at 3:19: expected string, got number
```

Create `tests/golden/type_errors/for_in_typed_item_mismatch.exit`:

```text
1
```

Create `tests/golden/type_errors/for_in_break_inside_function.cd`:

```cd
for x in [1] {
  fun f() {
    break;
  }
}
```

Create `tests/golden/type_errors/for_in_break_inside_function.err`:

```text
Type error at 3:5: `break` can only be used inside a loop
```

Create `tests/golden/type_errors/for_in_break_inside_function.exit`:

```text
1
```

Create `tests/golden/type_errors/for_in_continue_inside_function.cd`:

```cd
for x in [1] {
  fun f() {
    continue;
  }
}
```

Create `tests/golden/type_errors/for_in_continue_inside_function.err`:

```text
Type error at 3:5: `continue` can only be used inside a loop
```

Create `tests/golden/type_errors/for_in_continue_inside_function.exit`:

```text
1
```

- [ ] **Step 2: Extend `ResolvedNames` for for-in variables**

In `include/TypeChecker.hpp`, add public accessor:

```cpp
    const std::string& forInVariableName(const ForInStmt& statement) const;
```

Add private recorder:

```cpp
    void recordForInVariable(const ForInStmt& statement, std::string name);
```

Add storage:

```cpp
    std::unordered_map<const ForInStmt*, std::string> forInVariableNames_;
```

In `src/TypeChecker.cpp`, add implementations near the other `ResolvedNames` methods:

```cpp
const std::string& ResolvedNames::forInVariableName(const ForInStmt& statement) const
{
    const auto found = forInVariableNames_.find(&statement);
    if (found == forInVariableNames_.end()) {
        throw std::logic_error("missing resolved for-in variable name");
    }
    return found->second;
}

void ResolvedNames::recordForInVariable(const ForInStmt& statement, std::string name)
{
    forInVariableNames_.emplace(&statement, std::move(name));
}
```

Also add `forInVariableNames_.clear();` to `ResolvedNames::clear()`.

- [ ] **Step 3: Type-check `ForInStmt`**

In `TypeChecker::checkStatement`, add this branch after the existing `ForStmt` branch:

```cpp
    if (const auto* forInStmt = dynamic_cast<const ForInStmt*>(&statement)) {
        const TypeInfo iterableType = checkExpression(*forInStmt->iterable);
        if (iterableType.kind != StaticType::Unknown && iterableType.kind != StaticType::Array) {
            throw TypeError(forInStmt->variable,
                "for-in expects array, got " + typeInfoName(iterableType));
        }

        TypeInfo elementType = unknownType();
        if (iterableType.kind == StaticType::Array && iterableType.elementType) {
            elementType = *iterableType.elementType;
        }

        beginScope();
        const Binding itemBinding = declareVariable(forInStmt->variable, elementType, false);
        resolvedNames_.recordForInVariable(*forInStmt, itemBinding.resolvedName);
        ++loopDepth_;
        checkStatement(*forInStmt->body);
        --loopDepth_;
        endScope();
        return;
    }
```

- [ ] **Step 4: Verify type checker**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all new type-error fixtures pass; for-in success fixtures still fail in IR compilation because `IRCompiler` does not lower `ForInStmt` yet.

- [ ] **Step 5: Commit type checker slice**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/for_in_*
git commit -m "feat: type check for-in loops"
```

---

### Task 4: IR, bytecode, Rust VM, and lowering

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
- Create: `tests/golden/runtime_errors/for_in_dynamic_non_array.cd`
- Create: `tests/golden/runtime_errors/for_in_dynamic_non_array.run.err`
- Create: `tests/golden/runtime_errors/for_in_dynamic_non_array.exit`
- Create: `tests/golden/for_in_empty/input.cd`
- Create: `tests/golden/for_in_empty/run.out`
- Create: `tests/golden/for_in_shadow_outer/input.cd`
- Create: `tests/golden/for_in_shadow_outer/run.out`
- Create: `tests/golden/for_in_typed_array/input.cd`
- Create: `tests/golden/for_in_typed_array/run.out`
- Create: `tests/bytecode_artifacts/for_in_loops/input.cd`
- Create: `tests/bytecode_artifacts/for_in_loops/run.out`
- Create: `tests/bytecode_artifacts/for_in_loops/expected.cdbc`

- [ ] **Step 1: Add runtime and additional success fixtures**

Create `tests/golden/runtime_errors/for_in_dynamic_non_array.cd`:

```cd
fun id(x) { return x; }
for x in id(123) {
  print x;
}
```

Create `tests/golden/runtime_errors/for_in_dynamic_non_array.run.err`:

```text
Runtime error: for-in expects array
```

Create `tests/golden/runtime_errors/for_in_dynamic_non_array.exit`:

```text
1
```

Create `tests/golden/for_in_empty/input.cd`:

```cd
for x in [] {
  print x;
}
print "done";
```

Create `tests/golden/for_in_empty/run.out`:

```text
done
```

Create `tests/golden/for_in_shadow_outer/input.cd`:

```cd
let x = 99;
for x in [1, 2] {
  print x;
}
print x;
```

Create `tests/golden/for_in_shadow_outer/run.out`:

```text
1
2
99
```

Create `tests/golden/for_in_typed_array/input.cd`:

```cd
let xs: [number] = [1, 2];
for x in xs {
  let y: number = x;
  print y;
}
```

Create `tests/golden/for_in_typed_array/run.out`:

```text
1
2
```

- [ ] **Step 2: Add `assert_array` IR operation**

In `include/IR.hpp`, add `AssertArray` after `Len` in `enum class IROp` and add this method declaration after `emitLen`:

```cpp
    IRRegister emitAssertArray(IRRegister value);
```

In `src/IR.cpp`, update `irOpName` / print switch mappings to include:

```cpp
    case IROp::AssertArray:
        return "assert_array";
```

Add the emitter after `IRProgram::emitLen`:

```cpp
IRRegister IRProgram::emitAssertArray(IRRegister value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::AssertArray, dest, value, std::nullopt, {}, 0});
    return dest;
}
```

In the IR printer's instruction switch, print:

```cpp
    case IROp::AssertArray:
        out << reg(requireDest(instruction)) << " = assert_array " << reg(requireLeft(instruction));
        break;
```

- [ ] **Step 3: Execute `assert_array` in C++ runtime**

In `include/IRInterpreter.hpp`, add:

```cpp
    Value executeAssertArray(const Frame& frame, IRRegister value);
```

In `src/IRInterpreter.cpp`, dispatch `IROp::AssertArray` in `executeInstructions`:

```cpp
        case IROp::AssertArray:
            writeRegister(frame, readDest(instruction), executeAssertArray(frame, readLeft(instruction)));
            break;
```

Add implementation near `executeLen`:

```cpp
Value IRInterpreter::executeAssertArray(const Frame& frame, IRRegister value)
{
    const Value& input = readRegister(frame, value);
    if (input.type() != Value::Type::Array) {
        throw IRRuntimeError("for-in expects array");
    }
    return input;
}
```

- [ ] **Step 4: Add bytecode `assert_array` support in C++**

In `include/Bytecode.hpp`, add `AssertArray` after `Len`.

In `src/Bytecode.cpp`, add `BytecodeOp::AssertArray` to `bytecodeOpName` as `"assert_array"` and to the bytecode printer with:

```cpp
    case BytecodeOp::AssertArray:
        out << reg(requireDest(instruction)) << " = assert_array " << reg(requireLeft(instruction));
        break;
```

In `src/BytecodeCompiler.cpp`, map `IROp::AssertArray` to `BytecodeOp::AssertArray`.

In `src/BytecodeTextEmitter.cpp`, add text emission:

```cpp
    case BytecodeOp::AssertArray:
        out << reg(requireDest(instruction)) << " = assert_array " << reg(requireLeft(instruction));
        break;
```

- [ ] **Step 5: Add Rust bytecode `assert_array` support**

In `vm-rs/src/bytecode.rs`, add:

```rust
    AssertArray {
        dest: usize,
        value: usize,
    },
```

near `Len`.

In `vm-rs/src/format.rs`, parse instruction opcode `"assert_array"` with one source register and format it as:

```text
rD = assert_array rV
```

Follow the existing `len` parse/format shape and add coverage to the `parses_all_opcode_shapes` Rust unit test input and expected round-trip if that test enumerates opcodes.

In `vm-rs/src/vm.rs`, execute:

```rust
                Instruction::AssertArray { dest, value } => {
                    let input = self.read_register(frame, *value)?;
                    if !matches!(input, Value::Array(_)) {
                        return Err(RuntimeError::new("for-in expects array"));
                    }
                    self.write_register(frame, *dest, input)?;
                }
```

- [ ] **Step 6: Add IRCompiler declarations and helpers**

In `include/IRCompiler.hpp`, add:

```cpp
    void compileForIn(const ForInStmt& statement);
    std::string makeSyntheticName(const std::string& prefix);
```

Add a member:

```cpp
    std::size_t nextSyntheticName_ = 0;
```

In `src/IRCompiler.cpp`, add a `ForInStmt` dispatch in `compileStatement` after `ForStmt`:

```cpp
    if (const auto* forInStmt = dynamic_cast<const ForInStmt*>(&statement)) {
        compileForIn(*forInStmt);
        return;
    }
```

Add helper:

```cpp
std::string IRCompiler::makeSyntheticName(const std::string& prefix)
{
    return "__" + prefix + "#" + std::to_string(nextSyntheticName_++);
}
```

- [ ] **Step 7: Lower `ForInStmt`**

In `src/IRCompiler.cpp`, add this implementation near `compileFor`:

```cpp
void IRCompiler::compileForIn(const ForInStmt& statement)
{
    const std::string iterableName = makeSyntheticName("for_in_iter");
    const std::string indexName = makeSyntheticName("for_in_index");
    const std::string lengthName = makeSyntheticName("for_in_len");
    const std::string itemName = resolvedNames_->forInVariableName(statement);

    const IRRegister iterableValue = compileExpression(*statement.iterable);
    const IRRegister arrayValue = ir_.emitAssertArray(iterableValue);
    ir_.emitStoreVar(iterableName, arrayValue);

    const IRRegister zero = ir_.emitConstant(Value::number(0));
    ir_.emitStoreVar(indexName, zero);

    const IRRegister loadedArrayForLen = ir_.emitLoadVar(iterableName);
    const IRRegister length = ir_.emitLen(loadedArrayForLen);
    ir_.emitStoreVar(lengthName, length);

    const std::size_t loopStart = ir_.instructionCount();
    const IRRegister currentIndex = ir_.emitLoadVar(indexName);
    const IRRegister currentLength = ir_.emitLoadVar(lengthName);
    const IRRegister condition = ir_.emitBinary(IROp::Less, currentIndex, currentLength);
    const std::size_t exitJump = ir_.emitJumpIfFalse(condition);

    const IRRegister arrayForElement = ir_.emitLoadVar(iterableName);
    const IRRegister indexForElement = ir_.emitLoadVar(indexName);
    const IRRegister item = ir_.emitIndex(arrayForElement, indexForElement);
    ir_.emitStoreVar(itemName, item);

    const std::size_t jumpOverIncrement = ir_.emitJump();
    const std::size_t incrementStart = ir_.instructionCount();
    const IRRegister indexBeforeIncrement = ir_.emitLoadVar(indexName);
    const IRRegister one = ir_.emitConstant(Value::number(1));
    const IRRegister nextIndex = ir_.emitBinary(IROp::Add, indexBeforeIncrement, one);
    ir_.emitAssignVar(indexName, nextIndex);
    ir_.emitJumpTo(loopStart);

    ir_.patchJump(jumpOverIncrement);

    loopContexts_.push_back(LoopContext{incrementStart, {}});
    compileStatement(*statement.body);
    LoopContext loop = std::move(loopContexts_.back());
    loopContexts_.pop_back();

    ir_.emitJumpTo(incrementStart);
    ir_.patchJump(exitJump);
    for (const std::size_t breakJump : loop.breakJumps) {
        ir_.patchJump(breakJump);
    }
}
```

- [ ] **Step 8: Add Rust VM parity allowlist**

In `tests/run_rust_vm_tests.py`, add these names to the golden allowlist:

```python
            "for_in_basic",
            "for_in_control",
            "for_in_empty",
            "for_in_length_snapshot",
            "for_in_shadow_outer",
            "for_in_typed_array",
```

- [ ] **Step 9: Generate success IR/bytecode goldens**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --update
git diff -- tests/golden/for_in_basic tests/golden/for_in_control tests/golden/for_in_empty tests/golden/for_in_length_snapshot tests/golden/for_in_shadow_outer tests/golden/for_in_typed_array
```

Expected: `ast.out`, `ir.out`, and `bytecode.out` are added/updated for for-in success fixtures. Review that the IR includes `assert_array`, `len`, `index`, jumps, and an increment block.

If `--update` modifies unrelated existing goldens, run `git restore` on unrelated paths before continuing.

- [ ] **Step 10: Add bytecode artifact fixture**

Create `tests/bytecode_artifacts/for_in_loops/input.cd`:

```cd
for x in [1, 2, 3] {
  print x;
}
```

Create `tests/bytecode_artifacts/for_in_loops/run.out`:

```text
1
2
3
```

Generate `expected.cdbc`:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/for_in_loops/expected.cdbc tests/bytecode_artifacts/for_in_loops/input.cd
sed -n '1,200p' tests/bytecode_artifacts/for_in_loops/expected.cdbc
```

Expected: `expected.cdbc` includes an `assert_array` instruction before `len`.

- [ ] **Step 11: Verify runtime, bytecode, and Rust VM**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: all commands exit 0. Golden tests include the runtime-error fixture `for_in_dynamic_non_array` with `Runtime error: for-in expects array`.

- [ ] **Step 12: Commit lowering and runtime slice**

```bash
git add include/IR.hpp src/IR.cpp include/IRInterpreter.hpp src/IRInterpreter.cpp include/IRCompiler.hpp src/IRCompiler.cpp include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp src/BytecodeTextEmitter.cpp vm-rs/src/bytecode.rs vm-rs/src/format.rs vm-rs/src/vm.rs tests/run_rust_vm_tests.py tests/golden/for_in_* tests/golden/runtime_errors/for_in_dynamic_non_array.* tests/bytecode_artifacts/for_in_loops
git commit -m "feat: lower for-in loops"
```

---

### Task 5: Documentation and final verification

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update EBNF grammar**

In `docs/language-grammar.ebnf`, update the for-statement grammar to include both forms:

```ebnf
forStmt     = "for", ( forInClause | cForClause ), block ;
forInClause = identifier, "in", expression ;
cForClause  = [ forInitializer ], ";", [ expression ], ";", [ expression ] ;
```

Ensure `statement` still references `forStmt` and no planned unimplemented iterator forms are documented.

- [ ] **Step 2: Update README loop section**

In `README.md`, extend the loop paragraph with:

```markdown
`for item in array { ... }` iterates arrays in index order. The item binding is scoped to the loop body and may shadow an outer variable. The loop snapshots the array length before iteration, so appending during iteration does not extend the loop; existing elements are still read at their current index when reached. `break;` exits the loop, and `continue;` advances to the next item.
```

- [ ] **Step 3: Update roadmap Phase 11**

In `docs/roadmap.md`, update Phase 11 status to mark Phase 11C implemented and change the recommended next-feature wording so `for-in` is no longer listed as future work.

- [ ] **Step 4: Update AGENTS project memory**

In `AGENTS.md`, add to current semantics:

```markdown
- `for item in array { ... }` iterates arrays with a body-scoped item binding, snapshots the array length before iteration, supports `break` and `continue`, statically rejects known non-array iterables, and runtime-checks unknown iterables. Strings, maps, ranges, and custom iterators are not implemented.
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
git commit -m "docs: document for-in loops"
```

- [ ] **Step 7: Report results**

After final verification, final commit, and clean status, report the commits made and the exact verification command results.
