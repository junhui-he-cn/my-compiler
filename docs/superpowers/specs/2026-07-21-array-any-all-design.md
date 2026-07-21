# Array `any` / `all` Design

Date: 2026-07-21

## Goal

Add the next small higher-order collection slice after array `map`, `filter`,
and `reduce`, using the existing callback-native execution path.

## Public API

The shadowable function forms and unshadowed member forms are:

```cd
let values = [1, 2, 3];
print any(values, fun (value) { return value == 2; }); // true
print values.all(fun (value) { return value > 0; });   // true
```

`any(array, predicate)` returns `true` when the predicate returns `true` for
at least one element. `all(array, predicate)` returns `false` when the
predicate returns `false` for at least one element. Both invoke a one-argument
boolean predicate from left to right over a snapshot of the input elements and
short-circuit at the first decisive result. Therefore `any([])` is `false`
and `all([])` is `true` without invoking the callback.

Known arrays and callbacks are checked statically: the first argument must be
an array, the callback must be non-generic with arity one, its parameter must
accept the known element type, and its known return type must be `bool`.
Unknown values defer validation to runtime. Callback errors propagate through
the existing Rust VM call stack.

## Compiler and runtime architecture

Both helpers use the native registry, `native_call` IR/bytecode lowering, and
member-call sugar. The Rust VM clones the array elements before calling the
predicate, then returns the boolean result as soon as the operation can be
decided. No new IR, bytecode, artifact, or VM value representation is needed.

## Coverage

The slice includes typed and empty-array success cases, snapshot and
short-circuit side effects, shadowed function names with builtin member forms,
static and dynamic operand errors, Rust unit coverage, and a bytecode artifact
with Rust VM parity.
