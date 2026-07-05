# Native Stdlib Array Push/Pop Design

## Goal

Introduce a low-coupling native stdlib mechanism and implement `push(xs, value)` / `pop(xs)` on mutable arrays without adding one bespoke opcode per standard-library function.

This phase is both a collection-usability slice and the first step toward Phase 13-style standard builtins. It deliberately leaves the existing `len` fast path in place to reduce migration risk.

## Motivation

The current `len` implementation is tightly coupled across the compiler stack: the type checker recognizes `len`, IR has a dedicated `Len` opcode, bytecode has a dedicated `Len` opcode, `.cdbc` has dedicated syntax, and both runtimes implement dedicated execution paths.

Repeating that pattern for `push`, `pop`, `floor`, `sqrt`, `str`, `typeOf`, and future helpers would make each library addition require changes in too many layers. Instead, this phase adds a generic native stdlib call operation and uses `push`/`pop` as its first users.

## Scope

This phase includes:

- A native stdlib registry for unshadowed implicit library functions.
- Generic IR and bytecode native-call operations.
- `.cdbc` text support for generic native calls.
- C++ IR interpreter native implementations for `push` and `pop`.
- Rust VM native implementations for `push` and `pop`.
- Static checks for `push`/`pop` arity and known non-array first arguments.
- Runtime checks for unknown/dynamic invalid array arguments and empty `pop`.
- Success, type-error, runtime-error, bytecode artifact, and Rust VM parity tests.
- Documentation updates.

This phase does not include:

- Migrating the existing `len` opcode to the new native stdlib path.
- A source-level module/import system.
- A standard-library source file format.
- Array element type inference.
- Method syntax such as `xs.push(value)` or `xs.pop()`.
- Reserving stdlib names as keywords.
- GC implementation work. The design keeps future GC compatibility in mind but does not implement a collector.

## User-Visible Semantics

Stdlib functions are implicitly available when no user binding of the same name is visible:

```cd
let xs = [1, 2];
push(xs, 3);
print xs;       // [1, 2, 3]
print pop(xs);  // 3
print xs;       // [1, 2]
```

Shadowing follows the same rule as current `len` behavior:

```cd
let push = 123;
print push;     // 123
push([], 1);    // type error: can only call functions
```

### `push(array, value)`

- Mutates the array in place by appending `value`.
- Returns `nil`.
- Aliases observe the mutation because arrays are reference values.
- `value` may be any runtime value; arrays remain mixed-element collections in this phase.

Example:

```cd
let a = [1];
let b = a;
print push(b, 2); // nil
print a;          // [1, 2]
```

### `pop(array)`

- Mutates the array in place by removing the final element.
- Returns the removed value.
- Aliases observe the mutation.
- Popping an empty array is a runtime error.

Example:

```cd
let xs = ["a", "b"];
print pop(xs); // b
print xs;      // [a]
```

## Static Type Checking

The stdlib registry exposes static metadata and check functions.

Rules for `push`:

- Arity must be exactly 2.
- If the first argument type is known and not `array`, report a type error.
- The second argument is unrestricted.
- Return type is `nil`.

Rules for `pop`:

- Arity must be exactly 1.
- If the first argument type is known and not `array`, report a type error.
- Return type is `unknown` because array element types are not inferred yet.

Suggested type diagnostics:

```text
Type error at 1:11: expected 2 arguments but got 1
Type error at 1:10: push expects array as first argument, got number
Type error at 1:9: pop expects array as first argument, got string
```

Normal call behavior still applies when a user binding shadows a stdlib function.

## Runtime Semantics

Runtime validation is required for unknown values:

```cd
fun id(x) { return x; }
push(id(123), 1); // runtime error
pop(id("x"));    // runtime error
```

Runtime diagnostics:

```text
Runtime error: push expects array as first argument
Runtime error: pop expects array as first argument
Runtime error: cannot pop from empty array
```

## Native Stdlib Registry

Add a small native stdlib registry shared conceptually between type checking and lowering.

Minimum metadata:

```cpp
struct NativeFunctionSignature {
    std::string name;
    std::size_t arity;
};
```

The type checker can use helper functions rather than a heavy abstraction in this phase, but the behavior should be centralized enough that adding `sqrt` or `floor` later does not require copying bespoke `isBuiltinXCall` patterns everywhere.

Suggested C++ API shape:

```cpp
bool isNativeStdlibName(const std::string& name);
std::optional<std::size_t> nativeStdlibArity(const std::string& name);
```

Type-checking return rules can remain in `TypeChecker` for now, with one dispatch point such as `checkNativeStdlibCall`.

## Shadowing and Name Resolution

A call is treated as a native stdlib call only when:

- the callee is a `VariableExpr`,
- the variable name is registered as a native stdlib function,
- no lexical user binding with that name is visible.

This matches current `len` shadowing behavior.

The type checker should not record a resolved variable name for unshadowed native stdlib calls. Shadowed calls should continue through normal variable resolution and normal function-call checking.

## IR Design

Add a generic IR operation:

```text
vD = native_call @name(vArg0, vArg1, ...)
```

Use the existing IR names table for native function names.

Suggested API:

```cpp
IRRegister emitNativeCall(std::string name, std::vector<IRRegister> arguments);
```

`len` remains on `IROp::Len` in this phase. Only `push` and `pop` use `NativeCall`.

## Bytecode and `.cdbc` Design

Add a generic bytecode operation:

```text
bD = native_call @name [bArg0, bArg1, ...]
```

For `.cdbc`, use name-table references and the existing bracketed register-list format:

```text
rD = native_call nName [rArg0, rArg1]
rD = native_call nName [rArg0]
```

This avoids adding new opcodes and new text syntax for each stdlib function.

## Rust VM Design

Rust bytecode gets a matching instruction:

```rust
Instruction::NativeCall {
    dest: usize,
    name: usize,
    arguments: Vec<usize>,
}
```

The Rust VM dispatches by native function name and mutates array values in place through the existing shared array storage.

Future GC compatibility:

- Arrays remain heap/reference objects.
- `push` adds a new element edge from the array object to the pushed value.
- `pop` removes the final element edge and returns the removed value.
- A future collector only needs to scan array elements as normal; no special temporary array-copy semantics are introduced.

## Tests

Success coverage:

```cd
let xs = [1, 2];
print push(xs, 3);
print xs;
print pop(xs);
print xs;

let alias = xs;
push(alias, 4);
print xs;
```

Expected output:

```text
nil
[1, 2, 3]
3
[1, 2]
[1, 2, 4]
```

Additional success coverage should include mixed values and popped value use in an expression where possible.

Type-error fixtures:

- `push([]);` wrong arity.
- `push(123, 1);` known non-array first argument.
- `pop();` wrong arity.
- `pop("x");` known non-array first argument.
- Shadowed `push` call behaves as normal non-function call.

Runtime-error fixtures:

- `push(id(123), 1);`
- `pop(id("x"));`
- `pop([]);`

Parity coverage:

- Golden `--run` output.
- `--ir` and `--bytecode` output showing `native_call`.
- `.cdbc` artifact fixture showing `native_call`.
- Rust VM `run` parity for the success fixture.

## Documentation Updates

Update:

- `README.md` with `push` and `pop` behavior and shadowing note.
- `docs/language-grammar.ebnf` only if needed; syntax remains normal calls, so no grammar change is expected.
- `docs/bytecode-text-format.md` with `native_call`.
- `docs/roadmap.md` marking Phase 10C implemented and noting stdlib-native foundation.
- `AGENTS.md` current semantics and backend workflow notes.

## Migration Notes

`len` stays implemented as a bespoke fast path in this phase. A later cleanup phase can migrate `len` to `native_call` after `push/pop` proves the generic native stdlib path works in all backends.

## Open Decisions Locked for This Phase

- `push` and `pop` are implicit stdlib functions that can be shadowed.
- `push` mutates in place and returns `nil`.
- `pop` mutates in place and returns the removed value.
- `pop([])` is a runtime error.
- No method syntax.
- No `len` migration in this phase.
