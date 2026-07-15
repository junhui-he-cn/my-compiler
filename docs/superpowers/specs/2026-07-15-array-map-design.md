# Array Map Design

## Goal

Add the first callback-based collection API now that generic function values,
generic lambdas, and array element inference are available.

The supported forms are:

```cd
let doubled = map([1, 2, 3], fun (value: number): number {
  return value * 2;
});

let shifted = [1, 2, 3].map(fun (value: number): number {
  return value + 1;
});
```

## Scope and semantics

- `map(array, callback)` is a shadowable native function.
- `array.map(callback)` is unshadowed member-call sugar, matching the existing
  collection helper convention.
- The callback receives exactly one element and is invoked from left to right.
- `map` snapshots the input array elements before invoking callbacks. The map
  operation itself never mutates the input array; callback side effects remain
  observable through ordinary aliases.
- The result is a fresh top-level array. Callback results are stored as-is, so
  nested arrays, structs, and functions retain their existing shared identity.
- A callback runtime error stops the operation and preserves the existing
  callback/native call stack diagnostics.
- `filter`, `reduce`, map collections, and custom iterator protocols remain
  separate follow-up slices.

## Static checking

- A statically known first argument must be an array. Unknown values pass to
  runtime validation.
- A statically known callback must be a function with a complete signature and
  exactly one parameter. Unknown function values pass to runtime validation.
- A known array element type supplies the callback parameter context. Inline
  lambdas therefore infer unannotated parameters from `[T]`.
- A known callback return type produces `[R]`. Unknown arrays or unknown
  callback signatures produce a dynamic array with unknown element type.
- Generic callback values are rejected when used as `map` callbacks because
  this call site requires one monomorphic instantiation; callers can wrap
  generic logic in a monomorphic lambda or named function when needed.
- The member form is always builtin sugar and cannot be shadowed by a lexical
  binding named `map`.

## Runtime and backend

Register `map` in the existing native stdlib table and lower both call forms to
the existing `native_call` instruction. The Rust VM validates arity, array and
function operands, callback arity, snapshots the source elements, invokes the
callback through the normal function-call path, and allocates the result array.
No new IR opcode, bytecode opcode, `.cdbc` syntax, or runtime type tag is
needed.

Runtime diagnostics use these stable messages:

```text
Runtime error: map expects 2 arguments
Runtime error: map expects array as first argument
Runtime error: map expects function as second argument
Runtime error: map expects callback with 1 argument
```

## Verification

Cover direct and member calls, closure capture, typed and dynamic arrays,
static callback mismatches, generic callback rejection, runtime operand and
arity failures, callback exceptions, `.cdbc` artifact output, and Rust VM
parity.
