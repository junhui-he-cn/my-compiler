# Register-Based IR Design

Date: 2026-06-19

## Goal

Replace the current stack-based IR with a linear three-address IR that uses explicit virtual registers. The new IR should make value definitions and uses visible, so later compiler phases such as liveness analysis and register allocation can operate on the IR directly.

The change should preserve the existing source language and command-line workflow:

```sh
./build/compiler_demo --ir examples/hello.cd
./build/compiler_demo --run examples/hello.cd
```

The `--ir` output will change from stack-style instructions to register-style instructions. The `--run` behavior should remain semantically unchanged.

## Current State

The compiler currently follows this pipeline:

```text
Source -> Lexer -> Parser -> AST -> Stack IR -> IRInterpreter
```

The current IR is stack-oriented. For example:

```text
constant #0 40
constant #1 2
add
store_var @0 answer
```

This format is simple to interpret, but it hides operand relationships in stack effects. A later register allocator would first need to reconstruct which instruction defines each value and which instructions use it.

## Target Architecture

The new pipeline remains structurally the same, but the IR representation changes:

```text
Source -> Lexer -> Parser -> AST -> Register IR -> IRInterpreter
```

The main affected files are:

```text
include/IR.hpp
src/IR.cpp
include/IRCompiler.hpp
src/IRCompiler.cpp
include/IRInterpreter.hpp
src/IRInterpreter.cpp
```

Lexer, parser, AST nodes, language syntax, and language runtime semantics stay unchanged.

## IR Representation

Introduce explicit virtual registers:

```cpp
struct IRRegister {
    std::size_t index;
};
```

Keep a linear instruction stream and replace stack effects with explicit operands:

```cpp
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
```

Field meanings:

- `dest`: virtual register defined by the instruction.
- `left`: first input virtual register.
- `right`: second input virtual register.
- `operand`: constant-pool index or variable-name-pool index, depending on opcode.

Instruction conventions:

```text
Constant:   dest + operand(constant)
LoadVar:    dest + operand(name)
StoreVar:   left + operand(name)
Print:      left
Unary op:   dest + left
Binary op:  dest + left + right
```

The old `Pop` opcode is removed. Expression statements still compile their expression, but the result register is unused. Later dead-code elimination or register allocation work can ignore values that have no uses.

## IRProgram API

`IRProgram` should own constants, names, instructions, and virtual register allocation. It should expose helper methods so callers do not construct partially valid instructions by hand:

```cpp
IRRegister makeRegister();
IRRegister emitConstant(Value value);
IRRegister emitLoadVar(std::string name);
void emitStoreVar(std::string name, IRRegister value);
void emitPrint(IRRegister value);
IRRegister emitUnary(IROp op, IRRegister value);
IRRegister emitBinary(IROp op, IRRegister left, IRRegister right);
```

It should continue to expose read-only accessors for constants, names, and instructions. It should also expose the number of allocated virtual registers so the interpreter can size its register file.

## IRCompiler Behavior

Change expression compilation from stack-producing to register-returning:

```cpp
IRRegister compileExpression(const Expr& expression);
```

Statement rules:

```text
let name = expr;   -> value = compile(expr); store_var name, value
print expr;        -> value = compile(expr); print value
expr;              -> compile(expr), ignore result
```

Expression rules:

```text
literal            -> emitConstant(...)
variable           -> emitLoadVar(...)
grouping           -> compile(inner)
unary              -> input = compile(right); emitUnary(op, input)
binary             -> left = compile(left); right = compile(right); emitBinary(op, left, right)
```

Example for:

```text
let answer = 40 + 2;
```

Target IR:

```text
v0 = constant #0 40
v1 = constant #1 2
v2 = add v0, v1
store_var @0 answer, v2
```

## IRInterpreter Behavior

Replace the operand stack with a virtual register file:

```cpp
std::vector<Value> registers_;
```

At the start of execution, size `registers_` from the IR program's register count and clear globals as before.

Execution rules:

```text
Constant:   registers[dest] = constant
LoadVar:    registers[dest] = globals[name]
StoreVar:   globals[name] = registers[left]
Print:      output registers[left]
Unary:      registers[dest] = op(registers[left])
Binary:     registers[dest] = op(registers[left], registers[right])
```

Runtime semantics remain unchanged:

- Undefined variables still produce runtime errors.
- Division by zero still produces a runtime error.
- `+` supports number + number and string + string only.
- Numeric arithmetic and ordering comparisons still require numbers.
- `!` still uses the existing truthiness rule.
- `==` and `!=` still use the existing equality rule.

Stack-specific errors such as `stack underflow` are replaced by IR validation/runtime errors such as missing register operands or register index out of range.

## IR Printing

`IRProgram::print` should show def/use relationships clearly:

```text
IR
0000  v0 = constant #0 40
0001  v1 = constant #1 2
0002  v2 = add v0, v1
0003  store_var @0 answer, v2
0004  v3 = load_var @1 answer
0005  print v3
```

Formatting conventions:

- `vN` identifies a virtual register.
- `#N` identifies a constant-pool entry.
- `@N` identifies a variable-name-pool entry.
- Instructions that define a value start with `vN =`.
- Side-effecting instructions such as `store_var` and `print` do not define a destination register.

## Verification Plan

The project currently has no dedicated test framework, so initial verification should use existing CLI behavior.

Build:

```sh
cmake --build build
```

Check IR generation:

```sh
./build/compiler_demo --ir examples/hello.cd
```

Check execution:

```sh
./build/compiler_demo --run examples/hello.cd
```

Expected output remains:

```text
answer:
42
true
```

Additional stdin smoke cases:

```text
print 1 + 2 * 3;
print "a" + "b";
print !nil;
let x = 10;
x + 1;
print x;
```

These cases verify:

- Arithmetic precedence is unchanged.
- String concatenation behavior is unchanged.
- Truthiness behavior is unchanged.
- Expression statements can produce unused registers without affecting execution.
- Variable load/store behavior is unchanged.

## Explicit Non-Goals

This change does not add:

- SSA form.
- Basic blocks or a control-flow graph.
- `phi` nodes.
- Optimization passes.
- Liveness analysis.
- Register allocation.
- Machine-code generation.

The design only creates a better IR foundation for those future phases by making instruction definitions and uses explicit.

## Future Extension Points

After this migration, later work can add utilities such as:

```cpp
std::optional<IRRegister> definedRegister(const IRInstruction& instruction);
std::vector<IRRegister> usedRegisters(const IRInstruction& instruction);
```

Those helpers can feed liveness analysis, interference graph construction, linear-scan allocation, or other register-allocation experiments without first reverse-engineering stack behavior.
