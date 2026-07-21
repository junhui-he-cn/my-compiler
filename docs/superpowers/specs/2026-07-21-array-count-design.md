# Array `count` Design

Date: 2026-07-21

## Goal

Add a focused counting helper after the boolean array predicate helpers, while
reusing their callback typing and snapshot semantics.

## Public API

The shadowable function form and unshadowed member form are:

```cd
let values = [1, 2, 3, 4];
print count(values, fun (value) { return value > 2; }); // 2
print values.count(fun (value) { return value == 2; }); // 1
```

`count(array, predicate)` and `array.count(predicate)` invoke a one-argument
boolean predicate from left to right over a snapshot of the input and return
the number of `true` results. The callback is invoked for every snapshot
element, including after earlier matches. Empty arrays return `0` without
invoking the callback. Function-style `count` is shadowable; member-call sugar
is always builtin.

Known arrays and callbacks are checked statically: the first argument must be
an array, the callback must be non-generic with arity one, its parameter must
accept the known element type, and its known return type must be `bool`.
Unknown values defer validation to runtime, and callback errors propagate
through the existing Rust VM call stack.

## Compiler and runtime architecture

`count` uses the native registry, `native_call` IR/bytecode lowering, and
member-call lowering. The Rust VM clones the source elements before invoking
the callback and returns a numeric count. No new IR, bytecode, artifact, or
runtime value representation is needed.

## Coverage

The slice includes typed, empty, snapshot, shadowing, static-error,
runtime-error, Rust unit, and bytecode artifact/parity coverage.
