# Map `merge` Design

Date: 2026-07-22

## Goal

Add a small bulk map helper that composes ordered maps without introducing a
general collection protocol or a new bytecode opcode.

## Public API

The shadowable function form and unshadowed member form are:

```cd
let left = {"a": 1, "b": 2};
let right = {"b": 20, "c": 3};
print merge(left, right);       // map{a: 1, b: 20, c: 3}
print left.merge(right);        // map{a: 1, b: 20, c: 3}
```

`merge(left, right)` and `left.merge(right)` return a fresh ordered map. The
result begins with the left map's entries in their existing order. A matching
right-hand key replaces that existing value without moving the key; a new
right-hand key is appended in right-hand insertion order. Neither input map is
mutated. Entries are shallow-copied, so nested arrays, maps, structs, and
closures remain shared.

The function-style name is shadowable. Member-call sugar always resolves to
the builtin.

## Static and runtime checks

Known operands must be maps. When both operands have known key and value
types, compatible components are merged using the existing collection-type
merge rules, including nullable combinations. Incompatible known components
produce a dynamic `map` result rather than rejecting valid runtime maps with
mixed components.

Unknown operands defer validation to runtime. Runtime errors use these stable
messages:

```text
merge expects 2 arguments
merge expects map as first argument
merge expects map as second argument
```

## Compiler and runtime architecture

`merge` reuses the native registry, generic `native_call` lowering, and member
receiver convention. The Rust VM snapshots both ordered entry vectors and
passes their concatenation to the existing map constructor, whose duplicate-key
handling already updates values while preserving the first key's position. No
new AST, IR, bytecode, or runtime value representation is needed.

## Coverage

The slice covers function and member forms, replacement order, new-key order,
input non-mutation, shallow sharing, function shadowing, static non-map
errors, dynamic operand errors, bytecode artifact output, and Rust VM parity.
