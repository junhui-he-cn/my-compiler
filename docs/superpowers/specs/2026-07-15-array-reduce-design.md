# Array Reduce Design

## Goal

Add a callback-based array `reduce` helper after `map` and `filter`, using an
explicit initial accumulator so empty arrays have deterministic behavior.

The supported forms are:

```cd
let total = reduce([1, 2, 3], 0, fun (acc, value) {
  return acc + value;
});

let product = [1, 2, 3].reduce(1, fun (acc: number, value: number): number {
  return acc * value;
});
```

## Scope and semantics

- `reduce(array, initial, callback)` is a shadowable native function.
- `array.reduce(initial, callback)` is unshadowed member-call sugar.
- The callback receives `(accumulator, element)` and is invoked from left to
  right. Its return value becomes the accumulator for the next element.
- An explicit initial value is required. Reducing an empty array returns that
  value without invoking the callback.
- `reduce` snapshots the input array elements before invoking callbacks. The
  operation itself never mutates the input array; callback side effects remain
  observable through aliases.
- The final accumulator is returned as-is. If it is an array, map, struct, or
  function, its existing shared identity is preserved; `reduce` does not make a
  fresh result copy.
- A callback runtime error stops the operation and preserves the existing
  callback/native call-stack diagnostics.
- `map`, `filter`, collection protocols, and accumulator inference beyond the
  explicit initial value remain separate concerns.

## Static checking

- A statically known first argument must be an array. Unknown values pass to
  runtime validation.
- The initial expression supplies the accumulator type when it is known.
- A statically known callback must be a function with a complete signature and
  exactly two parameters. The first parameter must accept the initial
  accumulator type, the second must accept the array element type, and a known
  callback return type must be compatible with the accumulator type.
- Inline lambdas receive contextual parameter and return types from the known
  initial value and array element type.
- The result type is the known initial accumulator type; unknown initial values
  produce an unknown result.
- Generic callback values are rejected because this call site requires one
  monomorphic instantiation.
- The member form is always builtin sugar and cannot be shadowed by a lexical
  binding named `reduce`.

## Runtime and backend

Register `reduce` in the existing native stdlib table and lower both call forms
to the existing `native_call` instruction. The Rust VM validates arity, array
and function operands, and callback arity; it then iterates over a source
snapshot, invokes the callback with the current accumulator and element, and
returns the final accumulator. No new IR opcode, bytecode opcode, `.cdbc`
syntax, or runtime type tag is needed.

Runtime diagnostics use these stable messages:

```text
Runtime error: reduce expects 3 arguments
Runtime error: reduce expects array as first argument
Runtime error: reduce expects function as third argument
Runtime error: reduce expects callback with 2 arguments
```

## Verification

Cover direct and member calls, closure capture, empty arrays, source snapshots,
array accumulators, shadowing, static accumulator/element/result mismatches,
generic callback rejection, runtime operand/arity failures, callback
exceptions, `.cdbc` artifact output, and Rust VM parity.
