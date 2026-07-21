# Map `for-in` Design

Date: 2026-07-21

## Goal

Allow the existing single-binding `for item in expression` form to iterate map
keys while preserving the current array/range lowering and loop-control
semantics.

## Public semantics

```cd
let values = {"first": 1, "second": 2};
for key in values {
  print key;
}
```

Map iteration yields keys, not values or key/value records. Keys are produced in
map insertion order. The key list is snapshotted when the loop starts, so keys
inserted or removed by the loop body do not change the iteration boundary. The
loop variable's static type is the map key type when `map<K, V>` is known and is
dynamic when the map's key component is unknown.

The existing array and range behavior remains unchanged. Array iteration keeps
its length snapshot; ranges remain immutable finite integer sequences. A
statically known non-array/range/map value is a type error, while an unknown
value is validated at runtime with `for-in expects array, range, or map`.

## Compiler and VM architecture

No new IR or bytecode opcode is needed. `IRCompiler::compileForIn` already
lowers every iterable through `assert_array`, stores the result, snapshots its
length, and indexes it by a numeric loop counter. The existing `AssertArray`
instruction will additionally accept a map and produce a fresh array containing
the map's keys. Arrays and ranges continue to pass through unchanged. This
turns map mutation during the loop into ordinary mutation of the original map
while the loop iterates over its independent key snapshot.

The Rust VM owns the key snapshot allocation, preserving ordered shared map
storage and the existing `Index`/`Len` execution paths. The `.cdbc` text format
continues to use `assert_array`; its documented runtime domain expands to
arrays, ranges, and maps.

## Coverage

The slice includes static and dynamic map iteration success fixtures, key-order
and mutation-during-loop coverage, a Rust VM instruction test for key snapshot
creation, updated non-iterable diagnostics, and a bytecode artifact/Rust parity
fixture. Map values, custom iterator protocols, and multi-binding entry syntax
remain out of scope.
