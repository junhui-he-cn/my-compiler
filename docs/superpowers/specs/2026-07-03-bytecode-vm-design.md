# Bytecode VM Design

Date: 2026-07-03

## Goal

Implement Phase 8A as a parallel bytecode VM backend. This phase adds a bytecode instruction layer, an IR-to-bytecode compiler, a bytecode printer, and a bytecode VM that can execute the current language with behavior matching the existing IR interpreter.

The design should also establish clean boundaries for future GC, task scheduling, and JIT work without implementing those systems in this phase.

## Scope

In scope:

- Add a register-based bytecode representation independent of `IROp`.
- Lower existing `IRProgram` values into `BytecodeProgram` values.
- Add a bytecode VM capable of running the current language features:
  - literals, arithmetic, comparison, equality, truthiness, and printing
  - variables, assignment, lexical shadowing, and globals
  - `if`, `while`, `&&`, and `||`
  - named functions, returns, recursion, and closures
  - arrays and indexing
- Add CLI modes:
  - `--bytecode`
  - `--run-bytecode`
- Extend golden tests to optionally verify:
  - `bytecode.out`
  - `run_bytecode.out`
- Keep bytecode runtime diagnostics user-compatible with the current runtime diagnostics.
- Add VM-internal boundaries for future heap management, task scheduling, and JIT compilation.
- Update README, roadmap, AGENTS project memory, and test documentation where appropriate.

Out of scope:

- Replacing the current IR interpreter.
- Removing or changing `--ir` and `--run`.
- Implementing tracing GC, reference-count removal, or object moving.
- Implementing task scheduling, async operations, fibers, or `yield`.
- Implementing native code generation or JIT compilation.
- Compressing bytecode into a binary byte stream.
- Changing language syntax or semantics.

## Pipeline

The existing frontend and IR path remains the semantic source of truth:

```text
source
  -> Lexer
  -> Parser / AST
  -> TypeChecker / ResolvedNames
  -> IRCompiler / IRProgram
  -> BytecodeCompiler / BytecodeProgram
  -> BytecodeVM
```

The existing execution path remains available:

```text
source
  -> Lexer
  -> Parser / AST
  -> TypeChecker / ResolvedNames
  -> IRCompiler / IRProgram
  -> IRInterpreter
```

This keeps Phase 8A focused on backend architecture. The bytecode backend should match the current IR interpreter behavior instead of re-implementing AST lowering rules directly.

## CLI Behavior

Add:

```text
--bytecode
--run-bytecode
```

Examples:

```sh
./build/compiler_demo --bytecode examples/hello.cd
./build/compiler_demo --run-bytecode examples/hello.cd
./build/compiler_demo --bytecode --run-bytecode examples/hello.cd
```

Existing modes remain unchanged:

```text
--tokens
--ir
--run
```

If no output mode is selected, the CLI continues to print the AST, as it does today.

If multiple output modes are selected, use a stable order:

1. tokens
2. IR
3. bytecode
4. IR run output
5. bytecode run output

Separate adjacent printed sections with a blank line when needed, following the existing `--ir --run` convention.

## Bytecode Representation

Use a register-based bytecode VM. This matches the current register IR and reduces first-version lowering risk.

Add files:

```text
include/Bytecode.hpp
src/Bytecode.cpp
```

Initial opcode set:

```cpp
enum class BytecodeOp {
    Constant,
    MakeFunction,
    Array,
    Move,
    LoadVar,
    StoreVar,
    AssignVar,
    Call,
    Index,
    Print,
    Return,
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
    Jump,
    JumpIfFalse,
    JumpIfTrue,
};
```

The opcode names intentionally mirror the current IR operations but live in a separate enum. This allows future VM-only opcodes such as `Yield`, `CheckSafepoint`, `AllocObject`, profiling opcodes, or inline-cache variants without changing the IR.

First-version instructions may remain structured instead of packed:

```cpp
struct BytecodeInstruction {
    BytecodeOp op;
    std::optional<std::uint32_t> dest;
    std::optional<std::uint32_t> left;
    std::optional<std::uint32_t> right;
    std::vector<std::uint32_t> arguments;
    std::uint32_t operand = 0;
};
```

The fields mirror `IRInstruction`, but use bytecode register indexes rather than `IRRegister`. Future phases may compact this into a dense byte stream.

Recommended program structure:

```cpp
struct BytecodeFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<BytecodeInstruction> instructions;
    std::uint32_t registerCount = 0;
};

class BytecodeProgram {
public:
    const std::vector<Value>& constants() const;
    const std::vector<std::string>& names() const;
    const std::vector<BytecodeInstruction>& instructions() const;
    const std::vector<BytecodeFunction>& functions() const;
    std::uint32_t registerCount() const;
    void print(std::ostream& out) const;
};
```

The main chunk may be represented like the current `IRProgram` main instruction list, with nested functions in `functions()`. A later phase may normalize main into a synthetic function if that helps task scheduling or JIT compilation.

## Bytecode Printing

Add a compact assembly-like bytecode printer. It should be deterministic and golden-test friendly.

The exact spelling can be close to IR output but should clearly identify bytecode instructions. Example shape:

```text
constants:
  c0 = 1
  c1 = 2

main:
  b0 = constant c0
  b1 = constant c1
  b2 = add b0, b1
  print b2
```

Function printing should include function names, parameters, and register counts. The final exact format should optimize for readable goldens and stable diffs.

## IR-to-Bytecode Compiler

Add files:

```text
include/BytecodeCompiler.hpp
src/BytecodeCompiler.cpp
```

The bytecode compiler lowers `IRProgram` to `BytecodeProgram`.

Responsibilities:

- Copy the constants table.
- Copy the names table.
- Copy main register count.
- Lower main instructions.
- Lower nested `IRFunction` values to `BytecodeFunction` values.
- Preserve jump targets and function indexes.
- Translate `IROp::Copy` to `BytecodeOp::Move`.

This phase should not optimize. It should be a direct semantic translation so bytecode bugs are easy to compare with IR output.

## Bytecode VM

Add files:

```text
include/BytecodeVM.hpp
src/BytecodeVM.cpp
```

The bytecode VM executes `BytecodeProgram` independently of AST and IR.

Recommended public API:

```cpp
class BytecodeRuntimeError final : public DiagnosticError {
public:
    explicit BytecodeRuntimeError(std::string message);
};

class BytecodeVM {
public:
    explicit BytecodeVM(std::ostream& output);
    void execute(const BytecodeProgram& program);
};
```

`BytecodeRuntimeError` should use `DiagnosticKind::Runtime`, so user-visible diagnostics remain:

```text
Runtime error: ...
```

Do not introduce a user-visible `VM error` category in this phase.

### Execution State

Use explicit VM execution state:

```cpp
struct VMFrame {
    std::uint32_t functionIndex;
    std::size_t ip;
    std::vector<Value> registers;
    std::shared_ptr<Environment> locals;
    std::shared_ptr<Environment> closure;
};

struct VMThread {
    std::vector<VMFrame> frames;
    bool halted = false;
};
```

The first implementation may still use helper calls internally, but execution state should be shaped so future task scheduling can pause and resume a thread without reconstructing hidden C++ call state.

The first version may execute synchronously:

```cpp
vm.execute(program);
```

Future phases can split this into:

```cpp
VMRunResult runUntilBlockedOrBudget(VMThread& thread, InstructionBudget budget);
```

### Runtime Semantics

The bytecode VM should match `IRInterpreter` behavior for:

- truthiness
- numeric arithmetic and division-by-zero checks
- string concatenation with `+`
- equality
- comparison type checks
- function arity checks
- calling non-functions
- returns from functions and top-level return behavior
- closure capture by reference
- array allocation and index validation

Runtime error messages should match the current interpreter messages where possible, including:

```text
Runtime error: division by zero
Runtime error: can only call functions
Runtime error: expected <n> arguments but got <m>
Runtime error: can only index arrays
Runtime error: array index must be number
Runtime error: array index must be integer
Runtime error: array index out of range
```

## Future GC Boundary

Phase 8A does not implement GC. It should still introduce a small allocation boundary:

```cpp
class VMHeap {
public:
    Value makeFunction(...);
    Value makeArray(...);
};
```

For Phase 8A, `VMHeap` may wrap the current `Value::function(...)` and `Value::array(...)` factories, preserving the current `shared_ptr`-based object representation.

All bytecode VM heap-like allocations should go through `VMHeap`, especially:

- function values
- array values
- future object values

This allows a later GC phase to replace allocation internals with:

```text
Value -> tagged handle
VMHeap -> object arena / mark-sweep / optional moving collector
```

Expected future GC roots:

- registers in every active VM frame
- the VM call stack
- globals
- local and closure environments
- constants containing heap values, if future features allow them
- scheduler-owned suspended VM threads
- JIT metadata that references runtime objects

Phase 8A should document these root categories in code comments or design comments near `VMHeap` / `VMThread`.

## Future Task Scheduling Boundary

Phase 8A does not implement tasks. It should avoid embedding essential execution progress only in C++ recursion.

Design goals:

- Keep a `VMThread` or equivalent execution-state object.
- Keep frame state explicit: function, ip, registers, locals, closure.
- Make it plausible to execute by instruction budget later.

Future task scheduler model:

```text
Scheduler
  -> VMThread task 1
  -> VMThread task 2
  -> VMThread task 3
```

Future instructions or runtime features may include:

- `Yield`
- `SpawnTask`
- async blocking operations
- safepoints for GC

Phase 8A should not add user syntax for tasks.

## Future JIT Boundary

Phase 8A does not implement JIT. It should make bytecode functions stable compilation units.

Future JIT direction:

```text
BytecodeFunction -> native code
```

Useful preconditions established in Phase 8A:

- Bytecode instructions are independent from AST and IR node classes.
- Function boundaries are explicit.
- Register counts are explicit.
- Runtime helper behavior is centralized in the VM.
- Bytecode program constants and names have stable indexes.

Likely future JIT metadata:

- per-function execution count
- per-instruction counters
- inline cache slots for calls, indexing, globals, or property access
- native code pointer per `BytecodeFunction`
- deoptimization metadata if optimized JIT is added later

Phase 8A should not add counters unless they help tests or debugging immediately.

## Golden Tests

Extend `tests/run_golden_tests.py` so successful fixtures may include:

```text
bytecode.out
run_bytecode.out
```

Runtime-error fixtures may later support bytecode-specific execution checks, but Phase 8A can start with successful `run_bytecode.out` coverage plus representative runtime-error tests if the runner extension remains simple.

Recommended success coverage:

- literals, arithmetic, and `print`
- variables, assignment, and shadowing
- `if` / `else`
- `while`
- `&&` and `||` short-circuit behavior
- named functions and returns
- recursion
- closures by reference
- arrays and indexing
- calls chained with indexes where already supported by the language

Golden tests should not duplicate every existing fixture unless that is the simplest runner path. Prefer representative coverage plus a few bytecode-print goldens to keep output churn manageable.

## Documentation Updates

Update:

```text
README.md
docs/roadmap.md
AGENTS.md
```

Do not update `docs/language-grammar.ebnf` unless an implementation detail unexpectedly changes syntax. Phase 8A is a backend addition and should not change grammar.

README should describe:

- bytecode backend existence
- `--bytecode`
- `--run-bytecode`
- current relationship between IR interpreter and bytecode VM

Roadmap should mark Phase 8A as in progress or implemented after completion and mention future GC/task/JIT follow-ups.

AGENTS should mention the bytecode files and verification expectations once the implementation exists.

## Success Criteria

Phase 8A is complete when:

- Existing tests still pass.
- New bytecode golden tests pass.
- `--bytecode` prints deterministic bytecode.
- `--run-bytecode` executes representative current-language programs with output matching `--run`.
- Runtime diagnostics from the bytecode VM use the existing `Runtime error: ...` shape.
- The bytecode VM code contains explicit `VMHeap` and `VMThread` or equivalent boundaries.
- No user-visible language syntax or semantics change.
- Documentation describes the new backend without claiming future GC/task/JIT features are implemented.

