# Map `clear` Design

Date: 2026-07-21

## Goal

Add one focused mutating map helper after map lookup, removal, iteration, and
ordered snapshot helpers are available.

## Public API

The shadowable function form and unshadowed member form are:

```cd
let values = {"a": 1, "b": 2};
let alias = values;
print clear(values); // nil
print alias;         // map{}
print alias.clear(); // nil
```

`clear(map)` and `map.clear()` accept exactly one map receiver, remove every
entry in place, and return `nil`. Empty maps are valid. Existing aliases share
the mutation, and later insertions use normal map insertion order. The
function-style `clear` name is shadowable; member-call sugar is always builtin.

Known non-map arguments and receivers are type errors. Unknown values defer
the same validation to runtime and report `clear expects map as first argument`.

## Compiler and runtime architecture

`clear` uses the existing native registry, `native_call` IR/bytecode path, and
member-call lowering. No new opcode or `.cdbc` syntax is needed. The Rust VM
clears the existing shared ordered entry vector rather than replacing the map,
so aliases continue to observe the same map object.

## Coverage

The slice includes typed success, dynamic runtime-error, function-shadowing,
static type-error, Rust unit, and bytecode artifact coverage. Missing-key
behavior is unrelated because `clear` has no key argument.
