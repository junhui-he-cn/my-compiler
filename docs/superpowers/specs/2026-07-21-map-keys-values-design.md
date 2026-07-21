# Map `keys` / `values` Design

Date: 2026-07-21

## Goal

Expose ordered map snapshots as ordinary collection helpers after map lookup,
mutation, removal, and `for-in` support are in place.

## Public API

The shadowable function forms and unshadowed member forms are:

```cd
let prices = {"tea": 3, "coffee": 5};
print keys(prices);       // [tea, coffee]
print prices.keys();      // [tea, coffee]
print values(prices);     // [3, 5]
print prices.values();    // [3, 5]
```

Both helpers accept exactly one map and return a fresh top-level array in map
insertion order. `keys` copies key values and `values` copies value handles
shallowly; later map insertion, update, or removal does not mutate an already
returned array. The function names are shadowable, while member-call sugar is
always builtin.

For a known `map<K, V>`, `keys` returns `[K]` and `values` returns `[V]`. An
unknown map or unknown receiver produces a dynamic array type and defers
receiver validation to runtime. Known non-map receivers are type errors.

## Compiler and runtime architecture

Both helpers use the existing native registry, `native_call` IR/bytecode path,
and member-call lowering. No new opcode or `.cdbc` syntax is needed. The Rust
VM walks the existing ordered shared entry vector and allocates a new array for
each result. This shares the same ordering model as map `for-in` key snapshots
without exposing map internals to user code.

## Coverage

The slice includes typed and dynamic success fixtures, shadowed function names
with builtin member forms, static and runtime non-map diagnostics, Rust unit
coverage for order/freshness/shallow values, and a bytecode artifact/parity
fixture. Map entry mutation and custom iterator protocols remain out of scope.
