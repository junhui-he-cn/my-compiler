# Array `find` Design

Date: 2026-07-21

## Goal

Add a short-circuiting element-selection helper after the array predicate
helpers, preserving static element information through the nullable result.

## Public API

The shadowable function form and unshadowed member form are:

```cd
let values = [1, 2, 3];
print find(values, fun (value) { return value > 1; }); // 2
print values.find(fun (value) { return value == 9; }); // nil
```

`find(array, predicate)` and `array.find(predicate)` invoke a one-argument
boolean predicate from left to right over a snapshot and return the first
element whose predicate is `true`. They stop invoking the callback after the
first match and return `nil` when no element matches or the snapshot is empty.
Function-style `find` is shadowable; member-call sugar is always builtin.

Known arrays and callbacks are checked statically: the first argument must be
an array, the callback must be non-generic with arity one, its parameter must
accept the known element type, and its known return type must be `bool`. A
known `[T]` array produces a nullable `T` result; when `T` is already nullable,
the existing nullable layer is preserved instead of nesting another layer.
Unknown arrays produce a dynamic result. Unknown values defer validation to
runtime, and callback errors propagate through the existing Rust VM call stack.

## Compiler and runtime architecture

`find` reuses the shared predicate checker, native registry,
`native_call` IR/bytecode lowering, and member-call lowering. The Rust VM
clones source elements before invoking the callback and returns the matching
value or `nil`. No new IR, bytecode, artifact, or runtime value
representation is needed.

## Coverage

The slice includes typed match/no-match, nullable result typing, snapshot and
short-circuit behavior, shadowing, static and dynamic errors, Rust unit
coverage, and bytecode artifact/parity coverage.
