# Register IR Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the stack-based IR with a linear three-address IR that uses explicit virtual registers while preserving current language behavior.

**Architecture:** `IRProgram` will allocate virtual registers and emit instructions with explicit `dest`, `left`, and `right` operands. `IRCompiler` will return an `IRRegister` from each expression instead of relying on stack effects. `IRInterpreter` will execute the same linear instruction stream using a `std::vector<Value>` register file instead of an operand stack.

**Tech Stack:** C++17, CMake, existing CLI smoke tests through `compiler_demo`.

---

## File Structure

- Modify `include/IR.hpp`: define `IRRegister`, expand `IRInstruction`, remove `Pop`, add register-emitting `IRProgram` helpers and `registerCount()`.
- Modify `src/IR.cpp`: implement virtual register allocation, register-style emit helpers, and register-style printing.
- Modify `include/IRCompiler.hpp`: make `compileExpression`, `emitUnary`, and `emitBinary` return `IRRegister`.
- Modify `src/IRCompiler.cpp`: generate register IR from AST expressions and statements.
- Modify `include/IRInterpreter.hpp`: replace stack helpers and `stack_` with register helpers and `registers_`.
- Modify `src/IRInterpreter.cpp`: execute instructions from explicit register operands.
- Modify `README.md`: update the IR description from stack-based to register-based.
- Modify `CMakeLists.txt`: enable a smoke test target through CTest.
- Create `tests/smoke_register_ir.sh`: end-to-end CLI smoke test for IR printing and runtime semantics.

## Task 1: Add CLI Smoke Test for Register IR

**Files:**
- Create: `tests/smoke_register_ir.sh`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the smoke test script**

Create `tests/smoke_register_ir.sh` with this exact content:

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 /path/to/compiler_demo" >&2
    exit 64
fi

compiler="$1"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

hello_run="$(${compiler} --run "${repo_root}/examples/hello.cd")"
expected_hello_run=$'answer:\n42\ntrue'
if [[ "${hello_run}" != "${expected_hello_run}" ]]; then
    echo "unexpected examples/hello.cd runtime output" >&2
    echo "expected:" >&2
    printf '%s\n' "${expected_hello_run}" >&2
    echo "actual:" >&2
    printf '%s\n' "${hello_run}" >&2
    exit 1
fi

hello_ir="$(${compiler} --ir "${repo_root}/examples/hello.cd")"

grep -Eq '^0000  v[0-9]+ = constant #[0-9]+ 40$' <<<"${hello_ir}"
grep -Eq '^0001  v[0-9]+ = constant #[0-9]+ 2$' <<<"${hello_ir}"
grep -Eq '^0002  v[0-9]+ = add v[0-9]+, v[0-9]+$' <<<"${hello_ir}"
grep -Eq '^0003  store_var @[0-9]+ answer, v[0-9]+$' <<<"${hello_ir}"
grep -Eq '^0[0-9]+  print v[0-9]+$' <<<"${hello_ir}"
if grep -Eq '(^|  )pop($| )' <<<"${hello_ir}"; then
    echo "register IR must not contain pop instructions" >&2
    printf '%s\n' "${hello_ir}" >&2
    exit 1
fi

stdin_source=$'print 1 + 2 * 3;\nprint "a" + "b";\nprint !nil;\nlet x = 10;\nx + 1;\nprint x;\n'
stdin_run="$(printf '%s' "${stdin_source}" | "${compiler}" --run)"
expected_stdin_run=$'7\nab\ntrue\n10'
if [[ "${stdin_run}" != "${expected_stdin_run}" ]]; then
    echo "unexpected stdin runtime output" >&2
    echo "expected:" >&2
    printf '%s\n' "${expected_stdin_run}" >&2
    echo "actual:" >&2
    printf '%s\n' "${stdin_run}" >&2
    exit 1
fi

stdin_ir="$(printf '%s' "${stdin_source}" | "${compiler}" --ir)"
grep -Eq 'v[0-9]+ = multiply v[0-9]+, v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'v[0-9]+ = add v[0-9]+, v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'v[0-9]+ = not v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'store_var @[0-9]+ x, v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'v[0-9]+ = load_var @[0-9]+ x' <<<"${stdin_ir}"
```

- [ ] **Step 2: Make the script executable**

Run:

```bash
chmod +x tests/smoke_register_ir.sh
```

Expected: command exits with status 0.

- [ ] **Step 3: Register the smoke test in CMake**

Edit `CMakeLists.txt` so the full file is:

```cmake
cmake_minimum_required(VERSION 3.16)

project(compiler_demo LANGUAGES CXX)

# Keep the demo portable by requiring only standard C++17 facilities.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so editors and language servers can index the project.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(compiler_demo
    src/Ast.cpp
    src/IR.cpp
    src/IRCompiler.cpp
    src/IRInterpreter.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Value.cpp
    src/main.cpp
)

target_include_directories(compiler_demo PRIVATE include)

enable_testing()
add_test(
    NAME smoke_register_ir
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/smoke_register_ir.sh $<TARGET_FILE:compiler_demo>
)
```

- [ ] **Step 4: Configure and run the smoke test to verify it fails before implementation**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `smoke_register_ir` fails because the current IR output is stack-based and does not contain lines such as `v0 = constant`.

- [ ] **Step 5: Commit the test harness**

Run:

```bash
git add CMakeLists.txt tests/smoke_register_ir.sh
git commit -m "test: add register IR smoke test"
```

Expected: commit succeeds.

## Task 2: Replace IR Data Model and Printer

**Files:**
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`

- [ ] **Step 1: Replace the IR public interface**

Replace `include/IR.hpp` with this exact content:

```cpp
#pragma once

#include "Value.hpp"

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct IRRegister {
    std::size_t index = 0;
};

enum class IROp {
    Constant,
    LoadVar,
    StoreVar,
    Print,
    Negate,
    Not,
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
};

struct IRInstruction {
    IROp op;
    std::optional<IRRegister> dest;
    std::optional<IRRegister> left;
    std::optional<IRRegister> right;
    std::size_t operand = 0;
};

class IRProgram {
public:
    std::size_t addConstant(Value value);
    std::size_t addName(std::string name);
    IRRegister makeRegister();

    IRRegister emitConstant(Value value);
    IRRegister emitLoadVar(std::string name);
    void emitStoreVar(std::string name, IRRegister value);
    void emitPrint(IRRegister value);
    IRRegister emitUnary(IROp op, IRRegister value);
    IRRegister emitBinary(IROp op, IRRegister left, IRRegister right);

    const std::vector<Value>& constants() const;
    const std::vector<std::string>& names() const;
    const std::vector<IRInstruction>& instructions() const;
    std::size_t registerCount() const;

    // Print a compact, assembly-like view of the generated register IR.
    void print(std::ostream& out) const;

private:
    void emit(IRInstruction instruction);

    std::vector<Value> constants_;
    std::vector<std::string> names_;
    std::vector<IRInstruction> instructions_;
    std::size_t registerCount_ = 0;
};

std::string irOpName(IROp op);
std::ostream& operator<<(std::ostream& out, IRRegister reg);
```

- [ ] **Step 2: Implement register IR emission and printing**

Replace `src/IR.cpp` with this exact content:

```cpp
#include "IR.hpp"

#include <iomanip>
#include <utility>

namespace {

bool isUnary(IROp op)
{
    return op == IROp::Negate || op == IROp::Not;
}

bool isBinary(IROp op)
{
    switch (op) {
    case IROp::Add:
    case IROp::Subtract:
    case IROp::Multiply:
    case IROp::Divide:
    case IROp::Equal:
    case IROp::NotEqual:
    case IROp::Greater:
    case IROp::GreaterEqual:
    case IROp::Less:
    case IROp::LessEqual:
        return true;
    case IROp::Constant:
    case IROp::LoadVar:
    case IROp::StoreVar:
    case IROp::Print:
    case IROp::Negate:
    case IROp::Not:
        return false;
    }

    return false;
}

void printConstantOperand(std::ostream& out, const IRProgram& program, std::size_t operand)
{
    out << " #" << operand;
    if (operand < program.constants().size()) {
        out << " " << program.constants()[operand];
    }
}

void printNameOperand(std::ostream& out, const IRProgram& program, std::size_t operand)
{
    out << " @" << operand;
    if (operand < program.names().size()) {
        out << " " << program.names()[operand];
    }
}

} // namespace

std::size_t IRProgram::addConstant(Value value)
{
    constants_.push_back(std::move(value));
    return constants_.size() - 1;
}

std::size_t IRProgram::addName(std::string name)
{
    names_.push_back(std::move(name));
    return names_.size() - 1;
}

IRRegister IRProgram::makeRegister()
{
    return IRRegister{registerCount_++};
}

IRRegister IRProgram::emitConstant(Value value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Constant, dest, std::nullopt, std::nullopt, addConstant(std::move(value))});
    return dest;
}

IRRegister IRProgram::emitLoadVar(std::string name)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::LoadVar, dest, std::nullopt, std::nullopt, addName(std::move(name))});
    return dest;
}

void IRProgram::emitStoreVar(std::string name, IRRegister value)
{
    emit(IRInstruction{IROp::StoreVar, std::nullopt, value, std::nullopt, addName(std::move(name))});
}

void IRProgram::emitPrint(IRRegister value)
{
    emit(IRInstruction{IROp::Print, std::nullopt, value, std::nullopt, 0});
}

IRRegister IRProgram::emitUnary(IROp op, IRRegister value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{op, dest, value, std::nullopt, 0});
    return dest;
}

IRRegister IRProgram::emitBinary(IROp op, IRRegister left, IRRegister right)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{op, dest, left, right, 0});
    return dest;
}

const std::vector<Value>& IRProgram::constants() const
{
    return constants_;
}

const std::vector<std::string>& IRProgram::names() const
{
    return names_;
}

const std::vector<IRInstruction>& IRProgram::instructions() const
{
    return instructions_;
}

std::size_t IRProgram::registerCount() const
{
    return registerCount_;
}

void IRProgram::print(std::ostream& out) const
{
    out << "IR\n";
    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        const IRInstruction& instruction = instructions_[i];
        out << std::setw(4) << std::setfill('0') << i << std::setfill(' ') << "  ";

        if (instruction.dest) {
            out << *instruction.dest << " = ";
        }

        out << irOpName(instruction.op);

        if (instruction.op == IROp::Constant) {
            printConstantOperand(out, *this, instruction.operand);
        } else if (instruction.op == IROp::LoadVar) {
            printNameOperand(out, *this, instruction.operand);
        } else if (instruction.op == IROp::StoreVar) {
            printNameOperand(out, *this, instruction.operand);
            if (instruction.left) {
                out << ", " << *instruction.left;
            }
        } else if (instruction.op == IROp::Print) {
            if (instruction.left) {
                out << " " << *instruction.left;
            }
        } else if (isUnary(instruction.op)) {
            if (instruction.left) {
                out << " " << *instruction.left;
            }
        } else if (isBinary(instruction.op)) {
            if (instruction.left) {
                out << " " << *instruction.left;
            }
            if (instruction.right) {
                out << ", " << *instruction.right;
            }
        }

        out << '\n';
    }
}

void IRProgram::emit(IRInstruction instruction)
{
    instructions_.push_back(std::move(instruction));
}

std::string irOpName(IROp op)
{
    switch (op) {
    case IROp::Constant:
        return "constant";
    case IROp::LoadVar:
        return "load_var";
    case IROp::StoreVar:
        return "store_var";
    case IROp::Print:
        return "print";
    case IROp::Negate:
        return "negate";
    case IROp::Not:
        return "not";
    case IROp::Add:
        return "add";
    case IROp::Subtract:
        return "subtract";
    case IROp::Multiply:
        return "multiply";
    case IROp::Divide:
        return "divide";
    case IROp::Equal:
        return "equal";
    case IROp::NotEqual:
        return "not_equal";
    case IROp::Greater:
        return "greater";
    case IROp::GreaterEqual:
        return "greater_equal";
    case IROp::Less:
        return "less";
    case IROp::LessEqual:
        return "less_equal";
    }

    return "unknown";
}

std::ostream& operator<<(std::ostream& out, IRRegister reg)
{
    out << 'v' << reg.index;
    return out;
}
```

- [ ] **Step 3: Build to expose compiler call-site errors**

Run:

```bash
cmake --build build
```

Expected: build fails because `IRCompiler` and `IRInterpreter` still reference the removed stack-style `emit` API and `IROp::Pop`.

- [ ] **Step 4: Confirm this is an intermediate non-buildable checkpoint**

Run:

```bash
git diff -- include/IR.hpp src/IR.cpp
```

Expected: the diff shows only the IR data model and printer changes. Do not commit yet because the project is intentionally non-buildable until the compiler and interpreter are migrated.

## Task 3: Update IRCompiler to Emit Virtual Registers

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Update the compiler interface**

Replace `include/IRCompiler.hpp` with this exact content:

```cpp
#pragma once

#include "Ast.hpp"
#include "IR.hpp"

#include <stdexcept>
#include <string>

class IRCompileError final : public std::runtime_error {
public:
    explicit IRCompileError(const std::string& message);
};

class IRCompiler {
public:
    IRProgram compile(const Program& program);

private:
    void compileStatement(const Stmt& statement);
    IRRegister compileExpression(const Expr& expression);
    IRRegister emitUnary(TokenType op, IRRegister value);
    IRRegister emitBinary(TokenType op, IRRegister left, IRRegister right);

    IRProgram ir_;
};
```

- [ ] **Step 2: Update the compiler implementation**

Replace `src/IRCompiler.cpp` with this exact content:

```cpp
#include "IRCompiler.hpp"

#include <cstdlib>
#include <utility>

namespace {

Value literalValue(const std::string& text)
{
    if (text == "nil") {
        return Value::nil();
    }
    if (text == "true") {
        return Value::boolean(true);
    }
    if (text == "false") {
        return Value::boolean(false);
    }
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return Value::string(text.substr(1, text.size() - 2));
    }

    std::size_t parsed = 0;
    const double number = std::stod(text, &parsed);
    if (parsed != text.size()) {
        throw IRCompileError("invalid literal: " + text);
    }
    return Value::number(number);
}

} // namespace

IRCompileError::IRCompileError(const std::string& message)
    : std::runtime_error("IR compile error: " + message)
{
}

IRProgram IRCompiler::compile(const Program& program)
{
    ir_ = IRProgram();
    for (const auto& statement : program.statements) {
        compileStatement(*statement);
    }
    return std::move(ir_);
}

void IRCompiler::compileStatement(const Stmt& statement)
{
    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const IRRegister value = let->initializer
            ? compileExpression(*let->initializer)
            : ir_.emitConstant(Value::nil());
        ir_.emitStoreVar(let->name.lexeme, value);
        return;
    }

    if (const auto* print = dynamic_cast<const PrintStmt*>(&statement)) {
        const IRRegister value = compileExpression(*print->expression);
        ir_.emitPrint(value);
        return;
    }

    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        compileExpression(*expression->expression);
        return;
    }

    throw IRCompileError("unsupported statement node");
}

IRRegister IRCompiler::compileExpression(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        return ir_.emitConstant(literalValue(literal->value));
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        return ir_.emitLoadVar(variable->name.lexeme);
    }

    if (const auto* grouping = dynamic_cast<const GroupingExpr*>(&expression)) {
        return compileExpression(*grouping->expression);
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        const IRRegister value = compileExpression(*unary->right);
        return emitUnary(unary->op.type, value);
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        const IRRegister left = compileExpression(*binary->left);
        const IRRegister right = compileExpression(*binary->right);
        return emitBinary(binary->op.type, left, right);
    }

    throw IRCompileError("unsupported expression node");
}

IRRegister IRCompiler::emitUnary(TokenType op, IRRegister value)
{
    switch (op) {
    case TokenType::Bang:
        return ir_.emitUnary(IROp::Not, value);
    case TokenType::Minus:
        return ir_.emitUnary(IROp::Negate, value);
    default:
        throw IRCompileError("unsupported unary operator: " + tokenTypeName(op));
    }
}

IRRegister IRCompiler::emitBinary(TokenType op, IRRegister left, IRRegister right)
{
    switch (op) {
    case TokenType::Plus:
        return ir_.emitBinary(IROp::Add, left, right);
    case TokenType::Minus:
        return ir_.emitBinary(IROp::Subtract, left, right);
    case TokenType::Star:
        return ir_.emitBinary(IROp::Multiply, left, right);
    case TokenType::Slash:
        return ir_.emitBinary(IROp::Divide, left, right);
    case TokenType::EqualEqual:
        return ir_.emitBinary(IROp::Equal, left, right);
    case TokenType::BangEqual:
        return ir_.emitBinary(IROp::NotEqual, left, right);
    case TokenType::Greater:
        return ir_.emitBinary(IROp::Greater, left, right);
    case TokenType::GreaterEqual:
        return ir_.emitBinary(IROp::GreaterEqual, left, right);
    case TokenType::Less:
        return ir_.emitBinary(IROp::Less, left, right);
    case TokenType::LessEqual:
        return ir_.emitBinary(IROp::LessEqual, left, right);
    default:
        throw IRCompileError("unsupported binary operator: " + tokenTypeName(op));
    }
}
```

- [ ] **Step 3: Build to verify remaining failures are interpreter-only**

Run:

```bash
cmake --build build
```

Expected: build still fails because `IRInterpreter` still references `IROp::Pop`, stack helpers, and old instruction shape. There should be no remaining compile errors from `IRCompiler`.

- [ ] **Step 4: Confirm the compiler migration diff**

Run:

```bash
git diff -- include/IRCompiler.hpp src/IRCompiler.cpp
```

Expected: the diff shows `compileExpression`, `emitUnary`, and `emitBinary` returning `IRRegister`. Do not commit yet because the interpreter has not been migrated and the project is still intentionally non-buildable.

## Task 4: Update IRInterpreter to Execute Register IR

**Files:**
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`

- [ ] **Step 1: Update the interpreter interface**

Replace `include/IRInterpreter.hpp` with this exact content:

```cpp
#pragma once

#include "IR.hpp"

#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class IRRuntimeError final : public std::runtime_error {
public:
    explicit IRRuntimeError(const std::string& message);
};

class IRInterpreter {
public:
    explicit IRInterpreter(std::ostream& output);

    void execute(const IRProgram& program);
    const std::unordered_map<std::string, Value>& globals() const;

private:
    Value readConstant(const IRProgram& program, std::size_t index) const;
    std::string readName(const IRProgram& program, std::size_t index) const;
    IRRegister readDest(const IRInstruction& instruction) const;
    IRRegister readLeft(const IRInstruction& instruction) const;
    IRRegister readRight(const IRInstruction& instruction) const;
    const Value& readRegister(IRRegister reg) const;
    void writeRegister(IRRegister reg, Value value);

    Value executeUnaryNumber(const std::string& opName, IRRegister value, Value (*operation)(double));
    Value executeBinaryNumber(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeBinaryComparison(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeAdd(IRRegister left, IRRegister right);

    std::ostream& output_;
    std::vector<Value> registers_;
    std::unordered_map<std::string, Value> globals_;
};
```

- [ ] **Step 2: Update the interpreter implementation**

Replace `src/IRInterpreter.cpp` with this exact content:

```cpp
#include "IRInterpreter.hpp"

#include <cmath>
#include <utility>

namespace {

Value makeNumber(double value)
{
    return Value::number(value);
}

Value addNumber(double left, double right)
{
    return Value::number(left + right);
}

Value subtractNumber(double left, double right)
{
    return Value::number(left - right);
}

Value multiplyNumber(double left, double right)
{
    return Value::number(left * right);
}

Value divideNumber(double left, double right)
{
    if (right == 0.0) {
        throw IRRuntimeError("division by zero");
    }
    return Value::number(left / right);
}

Value greaterNumber(double left, double right)
{
    return Value::boolean(left > right);
}

Value greaterEqualNumber(double left, double right)
{
    return Value::boolean(left >= right);
}

Value lessNumber(double left, double right)
{
    return Value::boolean(left < right);
}

Value lessEqualNumber(double left, double right)
{
    return Value::boolean(left <= right);
}

std::string typeName(Value::Type type)
{
    switch (type) {
    case Value::Type::Nil:
        return "nil";
    case Value::Type::Number:
        return "number";
    case Value::Type::Bool:
        return "bool";
    case Value::Type::String:
        return "string";
    }

    return "unknown";
}

} // namespace

IRRuntimeError::IRRuntimeError(const std::string& message)
    : std::runtime_error("IR runtime error: " + message)
{
}

IRInterpreter::IRInterpreter(std::ostream& output)
    : output_(output)
{
}

void IRInterpreter::execute(const IRProgram& program)
{
    registers_.assign(program.registerCount(), Value::nil());
    globals_.clear();

    const auto& instructions = program.instructions();
    for (std::size_t ip = 0; ip < instructions.size(); ++ip) {
        const IRInstruction& instruction = instructions[ip];
        switch (instruction.op) {
        case IROp::Constant:
            writeRegister(readDest(instruction), readConstant(program, instruction.operand));
            break;
        case IROp::LoadVar: {
            const std::string name = readName(program, instruction.operand);
            const auto found = globals_.find(name);
            if (found == globals_.end()) {
                throw IRRuntimeError("undefined variable `" + name + "`");
            }
            writeRegister(readDest(instruction), found->second);
            break;
        }
        case IROp::StoreVar: {
            const std::string name = readName(program, instruction.operand);
            globals_.insert_or_assign(name, readRegister(readLeft(instruction)));
            break;
        }
        case IROp::Print:
            output_ << valueToString(readRegister(readLeft(instruction))) << '\n';
            break;
        case IROp::Negate:
            writeRegister(readDest(instruction), executeUnaryNumber("negate", readLeft(instruction), [](double value) {
                return makeNumber(-value);
            }));
            break;
        case IROp::Not:
            writeRegister(readDest(instruction), Value::boolean(!isTruthy(readRegister(readLeft(instruction)))));
            break;
        case IROp::Add:
            writeRegister(readDest(instruction), executeAdd(readLeft(instruction), readRight(instruction)));
            break;
        case IROp::Subtract:
            writeRegister(readDest(instruction), executeBinaryNumber("subtract", readLeft(instruction), readRight(instruction), subtractNumber));
            break;
        case IROp::Multiply:
            writeRegister(readDest(instruction), executeBinaryNumber("multiply", readLeft(instruction), readRight(instruction), multiplyNumber));
            break;
        case IROp::Divide:
            writeRegister(readDest(instruction), executeBinaryNumber("divide", readLeft(instruction), readRight(instruction), divideNumber));
            break;
        case IROp::Equal:
            writeRegister(readDest(instruction), Value::boolean(valuesEqual(readRegister(readLeft(instruction)), readRegister(readRight(instruction)))));
            break;
        case IROp::NotEqual:
            writeRegister(readDest(instruction), Value::boolean(!valuesEqual(readRegister(readLeft(instruction)), readRegister(readRight(instruction)))));
            break;
        case IROp::Greater:
            writeRegister(readDest(instruction), executeBinaryComparison("greater", readLeft(instruction), readRight(instruction), greaterNumber));
            break;
        case IROp::GreaterEqual:
            writeRegister(readDest(instruction), executeBinaryComparison("greater_equal", readLeft(instruction), readRight(instruction), greaterEqualNumber));
            break;
        case IROp::Less:
            writeRegister(readDest(instruction), executeBinaryComparison("less", readLeft(instruction), readRight(instruction), lessNumber));
            break;
        case IROp::LessEqual:
            writeRegister(readDest(instruction), executeBinaryComparison("less_equal", readLeft(instruction), readRight(instruction), lessEqualNumber));
            break;
        }
    }
}

const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    return globals_;
}

Value IRInterpreter::readConstant(const IRProgram& program, std::size_t index) const
{
    if (index >= program.constants().size()) {
        throw IRRuntimeError("constant index out of range");
    }
    return program.constants()[index];
}

std::string IRInterpreter::readName(const IRProgram& program, std::size_t index) const
{
    if (index >= program.names().size()) {
        throw IRRuntimeError("name index out of range");
    }
    return program.names()[index];
}

IRRegister IRInterpreter::readDest(const IRInstruction& instruction) const
{
    if (!instruction.dest) {
        throw IRRuntimeError(irOpName(instruction.op) + " missing destination register");
    }
    return *instruction.dest;
}

IRRegister IRInterpreter::readLeft(const IRInstruction& instruction) const
{
    if (!instruction.left) {
        throw IRRuntimeError(irOpName(instruction.op) + " missing left register");
    }
    return *instruction.left;
}

IRRegister IRInterpreter::readRight(const IRInstruction& instruction) const
{
    if (!instruction.right) {
        throw IRRuntimeError(irOpName(instruction.op) + " missing right register");
    }
    return *instruction.right;
}

const Value& IRInterpreter::readRegister(IRRegister reg) const
{
    if (reg.index >= registers_.size()) {
        throw IRRuntimeError("register index out of range");
    }
    return registers_[reg.index];
}

void IRInterpreter::writeRegister(IRRegister reg, Value value)
{
    if (reg.index >= registers_.size()) {
        throw IRRuntimeError("register index out of range");
    }
    registers_[reg.index] = std::move(value);
}

Value IRInterpreter::executeUnaryNumber(const std::string& opName, IRRegister value, Value (*operation)(double))
{
    const Value& input = readRegister(value);
    if (input.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects number, got " + typeName(input.type()));
    }
    return operation(input.asNumber());
}

Value IRInterpreter::executeBinaryNumber(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double))
{
    const Value& leftValue = readRegister(left);
    const Value& rightValue = readRegister(right);
    if (leftValue.type() != Value::Type::Number || rightValue.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects numbers");
    }
    return operation(leftValue.asNumber(), rightValue.asNumber());
}

Value IRInterpreter::executeBinaryComparison(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double))
{
    return executeBinaryNumber(opName, left, right, operation);
}

Value IRInterpreter::executeAdd(IRRegister left, IRRegister right)
{
    const Value& leftValue = readRegister(left);
    const Value& rightValue = readRegister(right);

    if (leftValue.type() == Value::Type::Number && rightValue.type() == Value::Type::Number) {
        return addNumber(leftValue.asNumber(), rightValue.asNumber());
    }

    if (leftValue.type() == Value::Type::String && rightValue.type() == Value::Type::String) {
        return Value::string(leftValue.asString() + rightValue.asString());
    }

    throw IRRuntimeError("add expects two numbers or two strings");
}
```

- [ ] **Step 3: Build successfully**

Run:

```bash
cmake --build build
```

Expected: build succeeds.

- [ ] **Step 4: Run the register IR smoke test**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: `100% tests passed` and `smoke_register_ir` passes.

- [ ] **Step 5: Commit the interpreter change**

Run:

```bash
git add include/IRInterpreter.hpp src/IRInterpreter.cpp
git add include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp
git commit -m "refactor: execute register IR"
```

Expected: commit succeeds and contains the full buildable register IR migration.

## Task 5: Update README and Run Final Verification

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update README IR wording**

Replace this line in `README.md`:

```markdown
- IR compiler: lowers the AST to a small stack-based intermediate representation.
```

with:

```markdown
- IR compiler: lowers the AST to a small three-address intermediate representation with virtual registers.
```

Replace this line:

```markdown
- IR interpreter: executes that IR directly.
```

with:

```markdown
- IR interpreter: executes that virtual-register IR directly.
```

- [ ] **Step 2: Configure, build, and run all registered tests**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: configure succeeds, build succeeds, and CTest reports `100% tests passed`.

- [ ] **Step 3: Manually inspect hello IR output**

Run:

```bash
./build/compiler_demo --ir examples/hello.cd
```

Expected output starts with register-style instructions like:

```text
IR
0000  v0 = constant #0 40
0001  v1 = constant #1 2
0002  v2 = add v0, v1
0003  store_var @0 answer, v2
```

The exact later name-pool indexes may differ because `addName` appends each name occurrence.

- [ ] **Step 4: Manually verify hello runtime output**

Run:

```bash
./build/compiler_demo --run examples/hello.cd
```

Expected exact output:

```text
answer:
42
true
```

- [ ] **Step 5: Manually verify stdin runtime output**

Run:

```bash
printf 'print 1 + 2 * 3;\nprint "a" + "b";\nprint !nil;\nlet x = 10;\nx + 1;\nprint x;\n' | ./build/compiler_demo --run
```

Expected exact output:

```text
7
ab
true
10
```

- [ ] **Step 6: Commit documentation and final verification updates**

Run:

```bash
git add README.md
git commit -m "docs: describe register IR"
```

Expected: commit succeeds.

## Self-Review Notes

- Spec coverage: Tasks 2 through 4 implement explicit virtual registers, register-returning expression compilation, register-file interpretation, and register-style printing. Task 1 and Task 5 cover the verification plan and documentation update.
- Scope: The plan does not add SSA, CFG, phi nodes, optimization passes, liveness analysis, register allocation, or machine-code generation.
- Type consistency: The plan consistently uses `IRRegister`, `IRInstruction::dest`, `IRInstruction::left`, `IRInstruction::right`, `IRProgram::registerCount()`, and register-returning compiler helpers.
