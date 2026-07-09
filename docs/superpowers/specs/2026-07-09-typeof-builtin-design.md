# `typeOf(value)` Native Builtin Design

## Goal

Add a small debug/introspection builtin named `typeOf(value)` that returns the runtime type of a value as a string.

This is Phase 13C from the language roadmap. It should be a focused standard-library slice, not a broader runtime type-system feature.

## User-Facing Semantics

`typeOf(value)` returns one of these strings based on the value's current runtime representation:

| Value kind | Result |
| --- | --- |
| `nil` | `"nil"` |
| number | `"number"` |
| boolean | `"bool"` |
| string | `"string"` |
| function | `"function"` |
| array | `"array"` |
| struct | `"struct"` |

Examples:

```cd
print typeOf(nil);       // nil
print typeOf(1);         // number
print typeOf(true);      // bool
print typeOf("x");       // string
print typeOf(fun () {}); // function
print typeOf([]);        // array
print typeOf({ x: 1 });  // struct
```

Named struct constructor values still return `"struct"`. The runtime value does not currently retain named struct type metadata, so this slice does not return names such as `Person`. Arrays return `"array"` regardless of static or inferred element type.

## Scope

Included:

- Register `typeOf` as a shadowable native stdlib function with arity 1.
- Static type checking returns `string` for unshadowed `typeOf(value)` calls.
- The argument accepts any static type, including unknown dynamic values.
- C++ IR interpreter and Rust VM native dispatch return the same type strings.
- Golden tests cover all runtime value categories, shadowing, arity diagnostics, bytecode artifact output, and Rust VM parity.
- Documentation and roadmap updates mark Phase 13C as implemented.

Excluded:

- Named struct runtime type names.
- Array element type strings such as `[number]`.
- A general reflection API.
- A dedicated IR or bytecode opcode.
- Migrating legacy `len` to the native-call path.

## Architecture

Use the existing shadowable native stdlib path used by `push`, `pop`, `floor`, `ceil`, `sqrt`, `str`, `substr`, and `charAt`.

1. Add `NativeFunctionKind::TypeOf` and register `{ "typeOf", 1, NativeFunctionKind::TypeOf }` in `NativeStdlib`.
2. Extend `TypeChecker::checkNativeStdlibCall` so unshadowed `typeOf(value)` checks its single argument expression and returns static type `string`.
3. Keep IR and bytecode lowering unchanged. The existing `IRCompiler` detects unshadowed native stdlib calls and emits `native_call`.
4. Add C++ `IRInterpreter::executeNativeTypeOf` and route `native_call "typeOf"` to it.
5. Add Rust VM `execute_native_type_of` and route `native_call "typeOf"` to it.
6. Reuse existing bytecode text format for `native_call`; no format change is needed.

## Shadowing

`typeOf` follows the same shadowing rule as the other native stdlib functions. If a user declares a variable or function named `typeOf`, normal lexical lookup wins:

```cd
let typeOf = fun (value) { return "custom"; };
print typeOf(123); // custom
```

If a user shadows `typeOf` with a non-function value and calls it, the existing call type error should apply.

## Static Type Checking

Unshadowed `typeOf(value)` behavior:

- Exactly one argument is required through the existing native arity check.
- The argument expression is still checked, so undefined variables and nested type errors surface normally.
- No argument type is rejected.
- The result type is `string`, so assignments and function returns annotated as `string` should accept it, while incompatible annotations should reject it through existing assignment/return checks.

## Runtime Behavior

C++ and Rust should both map runtime value variants to the same strings:

- nil -> `nil`
- number -> `number`
- bool -> `bool`
- string -> `string`
- function -> `function`
- array -> `array`
- struct -> `struct`

This mapping already exists in Rust as `Value::type_name()`. C++ has local type-name helpers for diagnostics; this slice should either reuse or add a small helper that maps `Value::Type` to the public strings above.

## Tests

Add a success fixture such as `tests/golden/native_stdlib_typeof/`:

```cd
print typeOf(nil);
print typeOf(1);
print typeOf(true);
print typeOf("x");
print typeOf(fun () { return nil; });
print typeOf([1]);
print typeOf({ x: 1 });
struct Box { value: number }
print typeOf(Box { value: 1 });
let typeOf = fun (value) { return "shadowed"; };
print typeOf(123);
```

Expected output:

```text
nil
number
bool
string
function
array
struct
struct
shadowed
```

Add type-error coverage for wrong arity:

```cd
typeOf();
```

Expected first diagnostic line:

```text
Type error at 1:7: expected 1 arguments but got 0
```

Add a bytecode artifact fixture containing at least one `typeOf` call and include the success fixture in the Rust VM golden allowlist.

## Documentation

Update:

- `README.md`: mention `typeOf` in the builtin list and describe returned strings.
- `docs/roadmap.md`: mark Phase 13C implemented and move the near-term recommendation to the next slice.
- `AGENTS.md`: update current semantics for native stdlib builtins.

No grammar update is needed because this uses existing call syntax.
