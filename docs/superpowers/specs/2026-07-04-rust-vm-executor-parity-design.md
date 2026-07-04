# Rust VM Executor Parity Design

## Goal

Implement Rust VM Phase 3: `compiler-design-vm run program.cdbc` executes `.cdbc` bytecode artifacts with semantics matching the current C++ `--run-bytecode` reference backend for all currently emitted opcodes.

The C++ bytecode VM remains available and frozen as the reference backend. The Rust VM becomes the target for future backend work, but this phase focuses on correctness and parity rather than GC, task scheduling, or JIT.

## Scope

This phase includes:

- A Rust bytecode executor for all current `.cdbc` instruction variants:
  - constants, moves, variable load/store/assignment;
  - arithmetic, comparisons, equality, truthiness, and jumps;
  - print and return;
  - arrays, indexing, index assignment, and `len`;
  - function values, calls, parameter binding, closures, and shared captured cells.
- A `compiler-design-vm run <program.cdbc>` CLI command.
- Runtime values and environment/cell modeling in Rust.
- Integration tests that compile `.cd` fixtures with C++ `compiler_design --emit-bytecode`, execute them with Rust VM `run`, and compare stdout with existing expected output.
- Runtime error reporting that is stable enough for CLI use, with detailed runtime-error parity reserved for a focused follow-up if needed.

This phase does not include:

- replacing C++ `--run-bytecode`;
- changing `.cdbc` syntax or opcode names except for bug fixes required by execution;
- bytecode verification beyond executor safety checks;
- binary bytecode encoding;
- garbage collection implementation;
- task scheduling or async execution;
- JIT compilation;
- source-level diagnostics in Rust VM.

## Design Approach

Use a direct Rust implementation of the current C++ bytecode VM semantics, but split the Rust code into modules that make future backend work easier:

```text
vm-rs/src/
  bytecode.rs   # parsed artifact structures; already exists
  format.rs     # .cdbc parser/formatter; already exists
  value.rs      # runtime Value plus display, truthiness, equality
  runtime.rs    # Cell, Environment, FunctionValue, ArrayValue, heap identities
  vm.rs         # frame/thread state, instruction dispatch, opcode execution
  main.rs       # CLI dispatch for help/dump/run
```

This avoids over-design while still separating artifact data, runtime values, heap-like objects, and the interpreter loop.

## Runtime Values

Rust runtime values mirror C++ `Value`:

```text
nil
number(f64)
bool(bool)
string(String)
function(FunctionValue)
array(ArrayValue)
```

Formatting for `print` should match C++ `valueToString` for existing values:

- `nil` prints `nil`.
- Numbers use Rust formatting adjusted as needed to match existing golden output for integer-like and decimal values.
- Booleans print `true` or `false`.
- Strings print their contents without quotes.
- Functions print a stable function marker compatible with current C++ behavior where tests rely on it.
- Arrays print a bracketed list compatible with current C++ behavior.

Truthiness matches C++:

- `nil` is false.
- `false` is false.
- everything else is true.

Equality matches C++ semantics:

- same-type primitive values compare by value;
- arrays and functions compare by runtime identity;
- different types are not equal.

## Shared Cells, Closures, and Arrays

Use reference-counted interior mutability to model the current C++ shared runtime objects:

```text
Cell = Rc<RefCell<Value>>
Environment = HashMap<String, Cell>
FunctionValue = { name, function_index, arity, identity, closure }
ArrayValue = { identity, elements }
closure = Rc<RefCell<Environment>>
elements = Rc<RefCell<Vec<Value>>>
```

This preserves two important language behaviors:

1. Closures capture variables by shared cell, so assignment through one closure is visible to another closure or the outer scope when still reachable.
2. Arrays are reference values, so aliases observe `array[index] = value` mutations.

This is intentionally not a final GC design. Later GC work can replace `Rc<RefCell<_>>` with heap handles while keeping the executor-facing operations similar.

## Execution Model

A `VM` owns:

- the parsed `Program`;
- global environment cells;
- output buffer/writer;
- next function identity;
- next array identity.

A `Frame` owns:

- `ip` instruction pointer;
- register vector initialized to `nil`;
- locals environment;
- captured closure environment;
- `is_main` flag.

Main execution creates a main frame with `program.main.registers` registers and executes `program.main.instructions`.

Function calls:

1. Validate the callee is a function value.
2. Validate argument count against the bytecode function parameters.
3. Create a new frame with function register count.
4. Bind each argument into the new frame locals using the function section `param` names.
5. Use the function value closure as the new frame closure.
6. Execute function instructions until `return` or end of function.
7. Return `nil` if execution reaches the end without an explicit return.

Function creation:

- `make_function fN` creates a `FunctionValue` pointing at function `fN`.
- The closure captures the current frame closure cells and current frame local cells into a new environment map.
- Cells are shared, not copied.

Variable lookup order matches C++:

1. current frame locals;
2. current frame closure;
3. globals.

`store_var` creates or replaces a cell in globals for main frames and locals for function frames. `assign_var` finds an existing cell by lookup order and mutates it.

## Opcode Semantics

The Rust executor should implement every current `Instruction` variant from `bytecode.rs`:

- `Constant`: convert parsed constants to runtime values.
- `MakeFunction`: create a function value with captured environment.
- `Array`: create a shared array value from register values.
- `Move`: copy a runtime value handle.
- `LoadVar`, `StoreVar`, `AssignVar`: use environment/cell rules above.
- `Call`: call a function value.
- `Index`: validate array, numeric integer index, and bounds; return element value.
- `AssignIndex`: validate array/index/bounds, mutate element, and return assigned value.
- `Len`: return array length or string byte length as a number.
- `Print`: append formatted value plus newline to output.
- `Return`: stop the current frame and return the register value.
- `Negate`, `Not`: unary number/truthiness operations.
- Binary arithmetic/comparison: match C++ type checks and messages where practical.
- `Add`: support number addition and string concatenation only.
- Equality: use runtime equality rules.
- Jumps: validate targets and mutate `ip` without also incrementing it.

## Runtime Errors

Runtime errors should be represented as `RuntimeError { message: String }` in Rust.

Initial CLI shape:

```text
error: runtime error: <message>
```

This shape is distinct from `.cdbc` parse errors:

```text
error: parse error at line X: <message>
```

The first implementation should match C++ runtime messages where straightforward, including common messages such as:

- `division by zero`
- `register index out of range`
- `constant index out of range`
- `name index out of range`
- `function index out of range`
- `undefined variable `<name>``
- `can only call functions`
- `expected N arguments but got M`
- `can only index arrays`
- `can only assign array elements`
- `array index must be number`
- `array index must be integer`
- `array index out of range`
- `len expects array or string`

Full runtime-error golden parity can be handled as a follow-up once successful execution parity is in place.

## CLI

Update help to show `run` as implemented:

```sh
compiler-design-vm --help
compiler-design-vm dump <program.cdbc>
compiler-design-vm run <program.cdbc>
```

`run` behavior:

1. Read the `.cdbc` file.
2. Parse it with the existing format parser.
3. Execute it with the Rust VM.
4. Write program output to stdout.
5. Write read/parse/runtime errors to stderr and exit non-zero.

CLI argument validation should remain consistent with `dump`: missing or extra input exits `64`.

## Testing Strategy

Add a Rust VM integration runner, likely `tests/run_rust_vm_tests.py`:

1. Discover selected `.cd` fixtures.
2. Compile each fixture to a temporary `.cdbc` file:

   ```sh
   compiler_design --emit-bytecode temp.cdbc input.cd
   ```

3. Execute with Rust VM:

   ```sh
   cargo run --quiet --manifest-path vm-rs/Cargo.toml -- run temp.cdbc
   ```

4. Compare stdout to fixture expectations:
   - prefer `run_bytecode.out` when present;
   - otherwise use `run.out` for fixtures that should match bytecode semantics.

Initial fixture set should include:

- `tests/bytecode_artifacts/arithmetic`
- `tests/bytecode_artifacts/control_flow`
- `tests/bytecode_artifacts/functions_arrays`
- existing golden fixtures that already have `run_bytecode.out`, including arrays, closures, function value arity, lambdas, and `len`.

Add unit tests in Rust modules for small executor-specific behavior where useful, especially value formatting/equality and environment capture.

Once stable, add the Rust VM integration runner to CTest after existing artifact tests.

## Future Backend Compatibility

This phase should leave clear seams for future backend work:

- `runtime.rs` owns heap-like shared objects and identities, making it the natural starting point for GC handles later.
- `vm.rs` owns frame and dispatch state, making scheduler/task splitting possible later.
- The `Instruction` enum remains a stable bytecode-level representation, which can later be annotated with JIT metadata without changing the parser contract.

Do not implement GC/task/JIT in this phase. Only avoid choices that would make those future phases unnecessarily hard.

## Acceptance Criteria

- `compiler-design-vm run <program.cdbc>` executes current `.cdbc` artifacts.
- Rust VM execution matches expected stdout for the selected successful fixtures.
- Current `dump` behavior remains unchanged.
- C++ compiler, C++ VM, and existing golden tests remain unchanged except for test/docs additions.
- Rust unit tests pass.
- The new Rust VM integration runner passes and is included in CTest.
- Documentation and roadmap identify Rust VM Phase 3 as implemented only after the executor lands.
