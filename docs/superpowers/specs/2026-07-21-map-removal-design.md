# Map Removal Design

Date: 2026-07-21

## Goal

Add one focused map mutation operation without introducing a map-specific IR or
bytecode opcode. Removal follows the existing native standard-library and
member-call conventions and remains available in both emitted `.cdbc` programs
and the Rust VM.

## Public API

Function-style calls use a shadowable native function:

```cd
let values = {"a": 1, "b": 2};
print remove(values, "a");
print values;
print values.remove("b");
```

`remove(map, key)` and `map.remove(key)` both mutate the shared map in place and
return the value associated with the removed key. Removing an absent key is a
runtime error with the existing message `map key not found`; this avoids
ambiguity when a present value is `nil` and matches direct map lookup behavior.

The key must be `nil`, a number, a boolean, or a string. Invalid keys reuse the
existing `map key must be nil, number, bool, or string` diagnostic. A statically
known non-map receiver is a type error; an unknown receiver is checked by the
VM and reports `remove expects map as first argument`. The function form is
shadowable by a lexical binding, while member-call sugar is always builtin and
cannot be shadowed.

## Static typing

For a known `map<K, V>`, the key argument must be a supported key type compatible
with `K`, and the result type is `V`. For a map with an unknown value component,
or an unknown receiver, the result remains dynamic. Known non-map receivers and
incompatible known keys are rejected before IR compilation. The operation does
not change the map's static type after deletion.

## Compiler and runtime architecture

`remove` is registered in `NativeStdlib`, checked in the existing native-call
switch, recognized as a builtin member name, and lowered through the existing
`native_call` IR/bytecode operation. The Rust VM removes the matching ordered
entry from the shared `Vec<(Value, Value)>`, preserving alias visibility and the
relative order of all remaining entries. No `.cdbc` syntax change is needed;
the existing name-table native-call form is sufficient.

## Coverage

The slice includes a success golden for return values, aliasing, and member
sugar; type-error fixtures for known invalid receivers and keys; runtime-error
fixtures for dynamic invalid receivers, invalid keys, and missing keys; a Rust
unit test for shared removal; and a bytecode artifact/parity fixture.

Map iteration, map deletion predicates, bulk removal, and custom iterators remain
out of scope.
