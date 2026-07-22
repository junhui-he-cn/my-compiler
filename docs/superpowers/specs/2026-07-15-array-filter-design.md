# Array Filter Design

## Goal

Add a callback-based array `filter` helper after `map`, using the existing
shadowable native-function and unshadowed member-call conventions.

The supported forms are:

```cd
let evens = filter([1, 2, 3, 4], fun (value: number): bool {
  return value % 2 == 0;
});

let positives = [1, -2, 3].filter(fun (value: number): bool {
  return value > 0;
});
```

## Scope and semantics

- `filter(array, predicate)` is a shadowable native function.
- `array.filter(predicate)` is unshadowed member-call sugar.
- The predicate receives exactly one element and is invoked from left to right.
- `filter` snapshots the input elements before invoking predicates. Predicate
  side effects remain observable through aliases, but later predicates never
  see elements appended after the operation starts.
- A predicate returning `true` keeps the original element value. The result is
  a fresh top-level array with shallow element identity preserved.
- A predicate runtime error stops the operation and preserves the existing
  callback/native call-stack diagnostics.
- `map`, `reduce`, map collections, and custom iterator protocols remain
  separate slices.

## Static checking

- A statically known first argument must be an array. Unknown values pass to
  runtime validation.
- A statically known predicate must be a function with a complete signature and
  exactly one parameter. Unknown function values pass to runtime validation.
- A known array element type supplies the predicate parameter context, so inline
  lambdas can omit the parameter annotation.
- A known predicate return type must be `bool`; an unknown return type remains a
  runtime check. A known array element type produces the same `[T]` result type;
  unknown array element information produces a dynamic array type.
- Generic predicate values are specialized from known input element types;
  every predicate type parameter must be inferable at the call site.
- The member form is always builtin sugar and cannot be shadowed by a lexical
  binding named `filter`.

## Runtime and backend

Register `filter` in the existing native stdlib table and lower both call forms
to the existing `native_call` instruction. The Rust VM validates arity, array
and function operands, callback arity, and boolean predicate results; it then
iterates over a source snapshot and allocates the result array. No new IR
opcode, bytecode opcode, `.cdbc` syntax, or runtime type tag is needed.

Runtime diagnostics use these stable messages:

```text
Runtime error: filter expects 2 arguments
Runtime error: filter expects array as first argument
Runtime error: filter expects function as second argument
Runtime error: filter expects callback with 1 argument
Runtime error: filter expects callback to return bool
```

## Verification

Cover direct and member calls, closure capture, source snapshots, empty and
mixed arrays, static predicate mismatches, generic predicate specialization and
unresolved inference, runtime
operand/arity/return failures, callback exceptions, `.cdbc` artifact output, and
Rust VM parity.
