# Native Stdlib Math Builtins Design

## Goal

Add the first numeric standard-library slice on top of the existing `native_call` mechanism: `floor(value)`, `ceil(value)`, and `sqrt(value)`.

This phase should prove that native stdlib growth no longer needs new bespoke IR or bytecode opcodes for every builtin. The new functions should work consistently in the C++ `--run` path, bytecode artifacts, and the Rust VM.

## User-Facing Semantics

The language gains three shadowable implicit stdlib functions:

```cd
print floor(1.9); // 1
print ceil(1.1);  // 2
print sqrt(9);    // 3
```

Signatures:

```text
floor(number): number
ceil(number): number
sqrt(number): number
```

Rules:

- Each function accepts exactly one argument.
- If the argument type is statically known and not `number`, type checking fails.
- If the argument type is unknown, runtime checks enforce that it is a number.
- `sqrt` requires a non-negative number at runtime.
- The result is always a number.
- User bindings named `floor`, `ceil`, or `sqrt` shadow the stdlib functions, matching `len`, `push`, and `pop` shadowing behavior.

Runtime errors:

```text
Runtime error: floor expects number
Runtime error: ceil expects number
Runtime error: sqrt expects number
Runtime error: sqrt expects non-negative number
```

Type errors use existing location conventions. Example:

```text
Type error at 1:10: floor expects number, got string
```

The exact column should follow the existing call-paren diagnostic behavior.

## Scope

In scope:

- Add `floor`, `ceil`, and `sqrt` to the native stdlib registry.
- Type-check arity and known argument type.
- Lower unshadowed calls through existing `IROp::NativeCall`.
- Execute the functions in both C++ IR interpreter and Rust VM.
- Add golden, type-error, runtime-error, bytecode artifact, and Rust VM parity coverage.
- Document the new functions and native stdlib extension path.

Out of scope:

- Migrating `len` to `native_call`.
- Adding string conversion, string helpers, or debug helpers.
- Adding namespacing/modules for stdlib functions.
- Adding generic numeric types or integer-specific semantics.
- Defining special NaN or infinity behavior beyond host runtime math behavior for valid numeric inputs.

## Architecture

### NativeStdlib Registry

Extend the existing registry from name/arity-only metadata to small declarative signatures.

Recommended shape:

```cpp
enum class NativeFunctionKind {
    Push,
    Pop,
    Floor,
    Ceil,
    Sqrt,
};

struct NativeFunctionSignature {
    const char* name;
    std::size_t arity;
    NativeFunctionKind kind;
};
```

Expose lookup helpers such as:

```cpp
const NativeFunctionSignature* findNativeStdlibFunction(const std::string& name);
bool isNativeStdlibName(const std::string& name);
std::optional<std::size_t> nativeStdlibArity(const std::string& name);
```

Keeping a kind in the shared registry prevents every compiler/runtime layer from rediscovering function identity through fragile string-only metadata. Runtime dispatch may still switch on the name or kind, but type checking should use the signature table as the source of truth for arity.

### Type Checking

`TypeChecker::isNativeStdlibCall` remains unchanged in spirit: a call is native only when the callee is an unshadowed variable whose name appears in the registry.

`TypeChecker::checkNativeStdlibCall` should:

1. Look up the signature.
2. Check exact arity.
3. Check arguments normally so nested expressions are resolved.
4. Apply function-specific static checks:
   - `push`: first argument must be array if known; returns nil.
   - `pop`: first argument must be array if known; returns unknown.
   - `floor`, `ceil`, `sqrt`: first argument must be number if known; returns number.

Shadowing remains automatic because a resolved user binding prevents native-call recognition.

### IR and Bytecode

No new IR or bytecode opcodes are required. Unshadowed math builtins lower to:

```text
vD = native_call @Name floor(vArg)
bD = native_call @Name floor [bArg]
rD = native_call nName [rArg]
```

The `.cdbc` contract already supports `native_call`; this phase should only add artifacts proving `floor`, `ceil`, and `sqrt` use it.

### C++ Runtime

`IRInterpreter::executeNativeCall` dispatches the three new names.

Runtime behavior:

- Read exactly one evaluated `Value` argument.
- Require `Value::Type::Number`.
- Apply `std::floor`, `std::ceil`, or `std::sqrt`.
- For `sqrt`, reject negative numbers before calling `std::sqrt`.

The runtime checks are still useful for unknown statically typed values, such as results from unannotated function calls.

### Rust VM

`Vm::execute_native_call` dispatches the same names.

Runtime behavior mirrors C++:

- Require one argument.
- Require `Value::Number`.
- Use `f64::floor`, `f64::ceil`, or `f64::sqrt`.
- Reject negative `sqrt` input with the same message as C++.

The Rust formatter/parser should not need syntax changes because `native_call` already supports arbitrary name references.

## Testing Strategy

### Success Golden

Add `tests/golden/native_stdlib_math/` with at least:

```cd
print floor(1.9);
print floor(-1.1);
print ceil(1.1);
print ceil(-1.9);
print sqrt(9);
print sqrt(2 * 8);
```

Expected run output:

```text
1
-2
2
-1
3
4
```

Include `ast.out`, `ir.out`, `bytecode.out`, and `run.out`. IR and bytecode outputs must contain `native_call` for all three functions.

### Static Errors

Add type-error fixtures:

- `floor_wrong_arity.cd`: `floor();`
- `floor_non_number_static.cd`: `floor("x");`
- `ceil_non_number_static.cd`: `ceil(true);`
- `sqrt_non_number_static.cd`: `sqrt([]);`
- `sqrt_shadowed_call_non_function.cd`:

```cd
let sqrt = 123;
sqrt(9);
```

The shadowing fixture should report `can only call functions`, proving the stdlib was not used.

### Runtime Errors

Add runtime-error fixtures using unknown values from unannotated functions:

```cd
fun id(x) { return x; }
floor(id("x"));
```

Similarly cover `ceil` and `sqrt`, plus negative sqrt:

```cd
sqrt(-1);
```

### Bytecode / Rust VM Parity

Add `tests/bytecode_artifacts/native_stdlib_math/` and include `native_stdlib_math` in the Rust golden allowlist. Verify C++ `--run` and Rust VM outputs match.

## Documentation

Update:

- `README.md`: list `floor`, `ceil`, `sqrt` with numeric behavior and shadowing.
- `docs/roadmap.md`: mark Phase 13A numeric helpers implemented after completion.
- `AGENTS.md`: mention new native stdlib math helpers and recommend `native_call` for future stdlib functions.

`docs/bytecode-text-format.md` already documents the generic `native_call` form. It only needs updates if examples or supported-native wording should mention math helpers.

## Compatibility and Future Work

This design preserves all current behavior:

- `len` remains a dedicated legacy opcode.
- Existing `push` and `pop` stay on `native_call`.
- User-defined bindings continue to shadow stdlib names.

Future standard-library slices can reuse the same registry and native dispatch pattern for string helpers, debug helpers, or migration of `len` if desired.
