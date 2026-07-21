# Array `findIndex` Design

Date: 2026-07-21

## Goal

Add the index-returning companion to array `find`, using the existing
predicate-native execution path and preserving the same snapshot and
short-circuit behavior.

## Public API

The shadowable function form and unshadowed member form are:

```cd
let values = [4, 7, 9];
print findIndex(values, fun (value) { return value > 6; }); // 1
print values.findIndex(fun (value) { return value == 2; }); // -1
```

`findIndex(array, predicate)` and `array.findIndex(predicate)` invoke a
one-argument boolean predicate from left to right over a snapshot of the input
array. They return the zero-based index of the first element whose predicate is
`true`, short-circuit after that match, and return `-1` for an empty array or
when no element matches. The function-style name is shadowable; member-call
sugar is always builtin.

Known arrays and callbacks are checked statically: the first argument must be
an array, the callback must be non-generic with arity one, its parameter must
accept the known element type, and its known return type must be `bool`. The
result is always a known `number` for statically valid calls. Unknown values
defer validation to runtime, and callback errors propagate through the existing
Rust VM call stack.

## Compiler and runtime architecture

`findIndex` reuses the native registry, shared predicate checking,
`native_call` IR/bytecode lowering, and member-call lowering. The Rust VM
clones source elements before invoking the callback, tracks the snapshot index,
and returns the first matching index or `-1`. No new IR, bytecode, or runtime
value representation is needed.

## Coverage

The slice includes typed match/no-match and empty-array success cases,
snapshot and short-circuit side effects, function shadowing with builtin
member sugar, static and dynamic errors, Rust unit coverage, and bytecode
artifact/Rust VM parity.
