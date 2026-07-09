# Native Stdlib String Builtins Design

## Goal

Add a small string standard-library slice on top of the existing shadowable `native_call` mechanism: `str(value)`, `substr(value, start, length)`, and `charAt(value, index)`.

This phase should improve everyday language usability while continuing the native-stdlib direction proven by `push`/`pop` and `floor`/`ceil`/`sqrt`: new builtins should not require bespoke IR or bytecode opcodes.

## User-Facing Semantics

The language gains three implicit stdlib functions when no user binding with the same name is visible:

```cd
print str(123);              // 123
print str(true);             // true
print substr("hello", 1, 3); // ell
print charAt("hello", 4);    // o
```

Signatures:

```text
str(value): string
substr(string, number, number): string
charAt(string, number): string
```

Rules:

- `str` accepts exactly one argument of any type and returns the same textual form used by `print` for that value.
- `substr` accepts a string, a start offset, and a length. Offsets and lengths are numeric integer byte counts, matching the existing `len(string)` byte-length behavior.
- `charAt` accepts a string and an offset. It returns a one-byte string at that offset.
- `substr` requires `0 <= start <= len(string)`, `length >= 0`, and `start + length <= len(string)`.
- `charAt` requires `0 <= index < len(string)`.
- Numeric offsets must be finite integers. Non-integer, negative, or out-of-range offsets are runtime errors.
- If the string argument or numeric arguments are statically known to have the wrong type, type checking fails.
- If an argument type is unknown, runtime checks enforce the same rules.
- User bindings named `str`, `substr`, or `charAt` shadow the stdlib functions, matching existing native stdlib behavior.

String indexing is intentionally byte-oriented in this slice because `len(string)` already reports byte length. Unicode scalar or grapheme-aware helpers are out of scope.

## Diagnostics

Static type errors should follow existing type diagnostic location conventions. Suggested messages:

```text
Type error at 1:8: expected 3 arguments but got 2
Type error at 1:13: substr expects string as first argument, got number
Type error at 1:18: substr expects number as second argument, got string
Type error at 1:22: substr expects number as third argument, got bool
Type error at 1:12: charAt expects string as first argument, got bool
Type error at 1:19: charAt expects number as second argument, got string
```

Runtime errors should be concise and stable:

```text
Runtime error: substr expects string as first argument
Runtime error: substr expects number as second argument
Runtime error: substr expects number as third argument
Runtime error: substr expects integer start offset
Runtime error: substr expects integer length
Runtime error: substr start offset out of bounds
Runtime error: substr length out of bounds
Runtime error: charAt expects string as first argument
Runtime error: charAt expects number as second argument
Runtime error: charAt expects integer index
Runtime error: charAt index out of bounds
```

Exact column numbers in type fixtures should follow the existing call diagnostics.

## Scope

In scope:

- Add `str`, `substr`, and `charAt` to the native stdlib registry.
- Type-check arity and known argument types.
- Lower unshadowed calls through existing `IROp::NativeCall` and bytecode `native_call`.
- Implement the functions in both the C++ IR interpreter and Rust VM.
- Add success, type-error, runtime-error, bytecode artifact, and Rust VM parity coverage.
- Update README, grammar/user docs as needed, roadmap, and project memory.

Out of scope:

- Migrating `len` to `native_call`.
- String concatenation operators or interpolation.
- Unicode scalar, grapheme-aware, locale-aware, or regex helpers.
- Mutating strings.
- Method syntax such as `text.substr(1, 3)`.
- Source-level stdlib modules.

## Architecture

### Native Stdlib Registry

Extend the existing native stdlib registry with three new entries. If the registry already carries a function kind, add:

```cpp
enum class NativeFunctionKind {
    Push,
    Pop,
    Floor,
    Ceil,
    Sqrt,
    Str,
    Substr,
    CharAt,
};
```

Arity metadata should be the source of truth for all three functions:

- `str`: 1
- `substr`: 3
- `charAt`: 2

The type checker, IR compiler, and runtime should continue using the existing native-call recognition rule: a call is native only when the callee is an unshadowed variable whose name appears in the registry.

### Type Checking

`TypeChecker::checkNativeStdlibCall` should add function-specific rules:

- `str`: check the argument expression normally, accept any argument type, return `string`.
- `substr`: first argument must be `string` when known; second and third arguments must be `number` when known; return `string`.
- `charAt`: first argument must be `string` when known; second argument must be `number` when known; return `string`.

Shadowed calls should use normal call checking, so this remains valid and produces a normal non-function call error:

```cd
let substr = 123;
substr("abc", 0, 1);
```

### IR and Bytecode

No new IR or bytecode opcodes are required. Unshadowed calls lower to existing native calls:

```text
vD = native_call @Name str(vArg)
vD = native_call @Name substr(vString, vStart, vLength)
vD = native_call @Name charAt(vString, vIndex)
```

The bytecode artifact should use the current `.cdbc` native-call representation without format changes.

### C++ Runtime

`IRInterpreter::executeNativeCall` dispatches the three names.

Implementation rules:

- `str`: call the same value-formatting path used by `print`, and wrap the result as a string value.
- `substr`: validate arity defensively, require string/number/number, require integer start and length, then return `source.substr(start, length)` after bounds checks.
- `charAt`: validate arity defensively, require string/number, require integer index, then return a one-character string from the selected byte.

Use a shared helper for converting runtime numeric offsets to `std::size_t` where practical so `substr` and `charAt` agree on integer and bounds rules.

### Rust VM

`Vm::execute_native_call` mirrors C++ behavior.

Implementation rules:

- `str`: use the VM value display/formatting path used for print output.
- `substr` and `charAt`: validate runtime types and integer offsets, operate on bytes, and convert the selected bytes back to a string value.

Because language string literals are UTF-8 source text but this slice is byte-oriented, tests should use ASCII examples. If a byte range does not form valid UTF-8 in Rust, returning a runtime error is acceptable for this phase as long as C++ and Rust parity tests stay focused on documented ASCII behavior.

## Testing Strategy

### Success Golden

Add `tests/golden/native_stdlib_strings/` with at least:

```cd
print str(123);
print str(true);
print str(nil);
print str([1, "x"]);
print substr("hello", 1, 3);
print substr("hello", 0, 5);
print substr("hello", 5, 0);
print charAt("hello", 0);
print charAt("hello", 4);
```

Expected run output:

```text
123
true
nil
[1, x]
ell
hello

h
o
```

Include `ast.out`, `ir.out`, `bytecode.out`, and `run.out`. IR and bytecode outputs should prove all three builtins lower through `native_call`.

### Static Errors

Add type-error fixtures covering:

- `str` wrong arity.
- `substr` wrong arity.
- `substr` statically non-string first argument.
- `substr` statically non-number start argument.
- `substr` statically non-number length argument.
- `charAt` statically non-string first argument.
- `charAt` statically non-number index argument.
- A shadowing fixture such as `let charAt = 123; charAt("x", 0);` to prove normal call checking wins.

### Runtime Errors

Add runtime-error fixtures using unknown values from unannotated functions:

```cd
fun id(x) { return x; }
substr(id(123), 0, 1);
substr("abc", id("x"), 1);
substr("abc", 0, id(false));
charAt(id(123), 0);
charAt("abc", id("x"));
```

Also cover invalid numeric values and bounds:

```cd
substr("abc", 1.5, 1);
substr("abc", 1, 1.5);
substr("abc", -1, 1);
substr("abc", 2, 2);
charAt("abc", 1.5);
charAt("abc", 3);
```

### Bytecode / Rust VM Parity

- Add a bytecode artifact fixture for the success case and update expected `.cdbc` manually with `--emit-bytecode`.
- Add the success golden directory to Rust VM golden parity coverage.
- If direct Rust unit tests for native stdlib functions exist, add focused cases for these names.

## Documentation

Update:

- `README.md` builtin overview and examples.
- `docs/roadmap.md` Phase 13 status to mention the string builtin slice once implemented.
- `AGENTS.md` current language semantics to mention the new builtins.

`docs/language-grammar.ebnf` should not need syntax changes because this is a library-only feature.

## Implementation Notes

- Keep `len` on its existing dedicated path for this phase.
- Prefer shared helper functions over repeated string-name checks where the current native stdlib code allows it.
- Avoid introducing a separate string object model; runtime strings remain value strings.
- Preserve existing output formatting, especially the fact that printed strings do not include quotes.
