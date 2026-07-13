# Maps / Dictionaries Design

Date: 2026-07-14

## Goal

Add maps as a first-class collection value while keeping their public helper
operations library-shaped. The compiler and Rust VM own map representation,
indexing, mutation, and key validation; ordinary queries continue to use the
existing native-stdlib/member-call conventions. This gives maps coherent
language semantics without turning every map operation into a bespoke opcode.

The first slice deliberately does not add map iteration or a general iterator
protocol. Existing array `for-in` behavior remains unchanged.

## Scope

This phase includes:

- map literals, including the empty map literal;
- `map<K, V>` type annotations and nested/nullable map types;
- static inference for map literal key and value types;
- mutable shared map values with identity equality;
- map indexing and upsert assignment (`maps[key]` and `maps[key] = value`);
- `len(map)` and `map.len()` support;
- extending `contains` to maps (`contains(map, key)` and
  `map.contains(key)`);
- stable key validation and missing-key diagnostics;
- C++ AST/type-checker/IR/bytecode changes, `.cdbc` emission, Rust VM
  execution, documentation, and golden/artifact coverage.

This phase excludes:

- map iteration, map `for-in`, ranges, and custom iterator protocols;
- deletion/removal operations;
- user-defined hashing or equality callbacks;
- structural/deep map equality;
- map key domains beyond the supported primitive values (or a generic type
  parameter whose runtime value is validated against that domain);
- higher-order map helpers such as `map`, `filter`, or `reduce`;
- separate compilation or a module-specific map ABI.

## Public Syntax

Map literals use braces in expression position. A colon distinguishes a map
literal from a block; named struct constructors continue to use an identifier
followed by braces.

```cd
let prices: map<string, number> = {
  "tea": 3,
  "coffee": 5
};

let empty = {};
print prices["tea"];
prices["water"] = 2;
print contains(prices, "coffee");
print prices.contains("tea");
print len(prices);
print prices.len();
```

`map<K, V>` is a built-in type form. `map` remains an identifier in expression
position, so it does not reserve a new variable name. The type parser recognizes
the `map` identifier followed by `<`, a key type, a comma, a value type, and
`>`.

Map entries are written as `key: value` and are separated by commas. Trailing
commas are not accepted, matching the current array and struct entry rules.
Entries and map index expressions evaluate from left to right.

The AST printer uses a stable prefix form:

```text
(map (entry "tea" 3) (entry "coffee" 5))
```

An empty map prints as `(map)`.

## Key and Value Semantics

The initial key domain is the immutable primitive set `nil`, `number`, `bool`,
and `string`. Arrays, structs, functions, and maps are rejected as keys when
their types are known and produce a runtime error when their types are only
known dynamically. Numbers use the language's existing numeric equality; no
separate integer-only key rule is introduced.

Map key equality delegates to the existing runtime equality relation for the
supported key values. In particular, values of different runtime types never
match, and two equal strings or numbers address the same entry. The map does
not define structural equality for values.

Maps are reference values. A map literal creates one shared map container, and
assignment or closure capture copies the handle rather than the entries. Two
aliases observe subsequent insertions and updates. A map compares equal to
itself through its identity and unequal to a distinct map, even when entries
match.

Entries are stored in insertion order for deterministic printing and
cross-backend behavior. Updating an existing key changes its value without
moving its position. A literal containing duplicate keys evaluates all entries
left to right and keeps the last value for that key.

Map values print as `map{key: value, ...}` in insertion order. Keys and values
use the existing `valueToString`/Rust `Display` formatting. The `map` prefix
keeps map output distinct from named struct output.

## Lookup and Mutation

`maps[key]` performs a lookup. The collection must be a map and the key must be
one of the supported key values. A missing key is a runtime error rather than
an implicit `nil`; this keeps direct indexing consistent with array bounds and
struct field failures. `contains(map, key)` can be used to test presence before
indexing.

`maps[key] = value` performs an upsert. It inserts a new entry when the key is
absent, replaces the value in place when the key is present, evaluates to the
assigned value, and preserves map aliasing. Assignment does not create a new
map container.

Compound assignment to map elements (`maps[key] += value`, and the other
numeric compound forms) is deferred. The first slice only reuses the existing
plain index-assignment AST and lowering path.

`len(map)` returns the number of entries as a number. The existing unshadowed
member-call sugar also accepts `map.len()`. `contains` keeps its current array
behavior and additionally accepts maps, returning whether the key exists. The
function-style names remain shadowable; member-call sugar remains unshadowed,
matching other built-in member names.

## Static Type Checking and Inference

Add `StaticType::Map` and `TypeInfo::keyType`/`valueType` fields, plus a
`mapType(keyType, valueType)` helper. `typeInfoName` renders a fully known map
as `map<K, V>` and a map with unknown components as `map`. `compatible` recurses
through both map components. Generic type-parameter collection, substitution,
and inference recurse through map key and value types just as they do through
arrays and function signatures.

Known map literals infer key and value types independently. The existing
compatible/nullable merge rules are reused for each side:

- all entries with a known compatible key type and value type infer
  `map<K, V>`;
- an empty map or a literal with an unknown component produces a dynamic map
  with unknown key/value types;
- incompatible known key or value types fall back to a dynamic map, preserving
  the language's mixed-collection escape hatch;
- a known key expression must be `nil`, `number`, `bool`, or `string`;
  unsupported known key types are type errors.

For an explicit `map<K, V>` annotation:

- every statically known key must be compatible with `K` and every value with
  `V`;
- `K` itself must be a supported key type or a generic type parameter;
- map indexing checks a statically known key against `K` and returns `V`;
- map index assignment checks the key against `K` and the value against `V`;
- dynamic map components defer these checks to the VM.

Known non-map receivers in indexing and assignment are type errors. Unknown
receivers remain legal and are validated at runtime. Existing array indexing
diagnostics are generalized to `can only index arrays or maps`; existing
array-specific index-type and bounds diagnostics remain unchanged.

`contains` accepts a statically known array or map. For a known map, its key
argument is checked against the map key type and the result is `bool`. For an
unknown collection or key, runtime validation remains responsible. `len` accepts
known arrays, strings, and maps and still rejects other known types.

## Parser and AST

Add `Map` to `TypeAnnotation::Kind`, with two child annotations for key and
value types. Add `MapExpr` and `MapEntry` nodes:

```cpp
struct MapEntry {
    ExprPtr key;
    Token colon;
    ExprPtr value;
};

struct MapExpr final : Expr {
    Token brace;
    std::vector<MapEntry> entries;
};
```

The parser recognizes a left brace as a map literal only while parsing a
primary expression. Statement/block parsing keeps its existing brace handling.
The parser requires a colon after each key and a right brace after the final
value. A bare brace sequence cannot become an anonymous struct literal: only
the map expression form is added, and named struct constructors retain their
existing syntax and type checks.

The grammar adds:

```ebnf
map          = "{", [ mapEntries ], "}" ;
mapEntries   = mapEntry, { ",", mapEntry } ;
mapEntry     = expression, ":", expression ;
mapType      = "map", "<", typeExpr, ",", typeExpr, ">" ;
```

`map` is added to the primary-expression alternatives and `mapType` to
`primaryType`. Existing block, struct-constructor, array, and assignment
precedence remains unchanged.

## Compiler and Bytecode Architecture

Add a first-class `Map` operation to the register IR and bytecode. Its operand
list contains alternating key/value registers in source order. The IR printer
renders, for example:

```text
v3 = map [v0: v1, v2: v3]
```

The `.cdbc` text form uses the same explicit pairs, preserving deterministic
entry order. The Rust bytecode parser and formatter must accept and emit the
new instruction, and artifact tests must include a map construction case.

Existing `Index` and `AssignIndex` operations are generalized to dispatch on
arrays and maps at runtime. No separate map lookup or map-assignment opcode is
introduced. `Len` dispatches on arrays, strings, and maps. `NativeCall` remains
the path for `contains`.

The C++ compiler pipeline changes are:

1. Parse and type-check `MapExpr` and `map<K, V>` annotations.
2. Lower map entries left to right into the new IR `Map` operation.
3. Lower `Map` to bytecode and emit the existing source-location metadata.
4. Preserve the map instruction and pair registers in `.cdbc` text.

The map literal itself is not a native call because it has variable arity and
must preserve source evaluation order as a core expression.

## Runtime Representation and Rust VM

Add `MapValue` to the C++ `Value` representation and the Rust VM runtime. C++
map values are not serialized as constants; the C++ representation keeps
formatting, equality, and type-name behavior aligned with the existing array
and struct value cases:

```text
MapValue {
  identity: monotonically increasing id,
  entries: shared ordered vector of (Value, Value)
}
```

The Rust `Value` enum gains `Map`, `type_name()` returns `"map"`, display uses
the `map{...}` format, and runtime equality compares map identity. The VM
allocates a fresh identity for each `Map` instruction while keeping entries in
an `Rc<RefCell<...>>` so aliases mutate the same container. A vector-backed
table is intentional for this first slice: it reuses `runtime_equals`, keeps
printing deterministic across C++ tooling and Rust execution, and leaves room
for an internal hash-index optimization without changing language semantics.

Index and assignment execution:

- reject non-map/non-array receivers with the generalized indexing diagnostic;
- validate map keys before lookup or mutation;
- find entries using existing runtime equality;
- raise `map key not found` for a missing read;
- update an existing entry or append a new entry for assignment;
- return the assigned value from `AssignIndex`.

Native `contains` accepts arrays as before and maps as a second dispatch path.
Native and member `len` use the map entry count. `typeOf(map)` works through the
existing `Value::type_name` path without a new native function.

Malformed hand-written `.cdbc` files must receive the same validation and
diagnostics as source-compiled programs. Runtime failures continue to carry the
current instruction source location and call stack metadata.

## Diagnostics

The following messages are stable for this slice:

```text
Type error at <line>:<column>: can only index arrays or maps
Type error at <line>:<column>: map key must be nil, number, bool, or string
Type error at <line>:<column>: map key is incompatible with map key type
Type error at <line>:<column>: map value is incompatible with map value type
Runtime error: can only index arrays or maps
Runtime error: can only assign array elements or map entries
Runtime error: map key must be nil, number, bool, or string
Runtime error: map key not found
Runtime error: contains expects array or map as first argument
Runtime error: len expects array, string, or map
```

Array-specific index type and bounds messages remain unchanged. The existing
array type-error goldens that mention `can only index arrays` are refreshed to
the generalized wording because maps now share the index operation.

## Tests

### Success goldens

Add focused fixtures covering:

- empty and non-empty map literals;
- string, number, bool, and nil keys;
- insertion-order printing and duplicate-key last-write-wins behavior;
- typed `map<K, V>` annotations and nested/nullable value types;
- inferred map key/value types through indexing and assignment;
- aliasing through assignment and closures;
- map insertion, update, and assigned-value expressions;
- `len(map)`, `map.len()`, `contains(map, key)`, and `map.contains(key)`;
- shadowed function-style names with member forms still available;
- map identity equality and `typeOf(map)`;
- one fixture with AST, IR, bytecode, and run outputs showing map pairs.

### Parse and type errors

Cover malformed map entries, missing colons/braces, trailing commas, malformed
`map<K, V>` annotations, unsupported statically known key types, incompatible
typed keys/values, non-map indexing, and map `for-in` rejection.

### Runtime errors

Use unknown values to defer validation and cover non-map receivers, unsupported
dynamic keys, missing keys, malformed map bytecode operands, and invalid native
argument types. Runtime-error fixtures must produce no stdout and preserve source
locations/call stacks.

### Rust VM and artifact tests

Add Rust unit tests for ordered construction, identity equality, duplicate-key
updates, alias mutation, key validation, missing lookup, map length, and native
`contains`. Add `.cdbc` artifact cases for map construction, index lookup, and
assignment. Run the full CTest, golden, artifact, Rust VM, and Cargo suites.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf` with map literals and `map<K, V>` types;
- `README.md` with syntax, key restrictions, lookup/upsert behavior, helper
  forms, printing, aliasing, and the explicit lack of map iteration;
- `docs/roadmap.md` to mark the map slice complete while keeping ranges as a
  separate future direction;
- `docs/bytecode-text-format.md` with the `map` instruction;
- `AGENTS.md` with the implemented map semantics and current limitations.

## Rollout and Compatibility

This is a source- and bytecode-format extension. Existing array, struct, and
string programs retain their runtime behavior except for the generalized
non-array indexing diagnostic text. `.cdbc` artifacts containing the new `map`
instruction require the matching VM version; old artifacts remain valid because
the new instruction is additive.

No map iteration is added as a side effect. The existing array-only `for-in`
checker and lowering remain unchanged, and future ranges are designed
independently.
