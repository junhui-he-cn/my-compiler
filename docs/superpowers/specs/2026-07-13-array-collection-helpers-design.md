# Array Collection Helpers Design

## Goal

Add a small, coherent set of non-higher-order array helpers for querying,
copying, slicing, and concatenation while preserving the language's existing
array typing, shadowing, bytecode, and Rust VM conventions.

The slice adds `contains`, `slice`, `copy`, and `concat` in both function-call
and member-call forms. It does not add syntax, higher-order callbacks, generic
types, or new IR and bytecode opcodes.

## Scope

This phase includes:

- `contains(array, value)` and `array.contains(value)`;
- `slice(array, start, length)` and `array.slice(start, length)`;
- `copy(array)` and `array.copy()`;
- `concat(left, right)` and `left.concat(right)`;
- static checks and return types for known argument types;
- runtime validation for dynamically typed arguments and malformed `.cdbc`;
- shallow-copy semantics for returned arrays;
- function shadowing and unshadowed member-call sugar consistent with existing
  builtins;
- C++ compiler lowering, `.cdbc` emission, Rust VM execution, docs, and golden
  parity coverage.

This phase excludes:

- higher-order helpers such as `map`, `filter`, and `reduce`;
- additional helpers such as `indexOf`, `reverse`, `sort`, and `join`;
- generic type parameters or custom iterator protocols;
- mutating variants of slice, copy, or concat;
- deep cloning of nested reference values;
- migrating the legacy `len` opcode;
- changes to source grammar or bytecode text format.

## Public API and Value Semantics

### `contains(array, value)`

`contains` returns `true` when any array element compares equal to `value` using
the language's existing `==` semantics and returns `false` otherwise. Primitive
values retain value equality. Arrays, structs, functions, and other reference
values retain their existing equality behavior; this feature does not introduce
structural or deep equality.

The helper does not modify the input array.

### `slice(array, start, length)`

`slice` returns a new array containing `length` elements beginning at `start`.
Both offsets are zero-based. `start` and `length` must be finite, non-negative
integer numbers, and `start + length` must not exceed the source length.

An empty slice is valid, including `slice(xs, len(xs), 0)`. The returned top-level
array container is independent from the input container.

### `copy(array)`

`copy` returns a new array containing all input elements in their original order.
It is the explicit whole-array counterpart to `slice` and avoids requiring users
to spell a full-range slice.

### `concat(left, right)`

`concat` returns a new array containing every element of `left`, followed by
every element of `right`. Neither input array is modified.

### Shallow copies

`slice`, `copy`, and `concat` allocate a new top-level array container and copy
element values into it. Nested arrays, structs, closures, and other reference
values remain shared. Mutating the returned array's top-level elements or length
does not mutate either input container, while mutation through a copied nested
reference remains observable through aliases.

## Call Forms, Shadowing, and Member Names

Function-style helpers are implicit native stdlib functions and remain
shadowable:

```cd
let copy = fun (value) { return value; };
print copy(3);
```

A call is native only when the callee is an unshadowed variable registered in
`NativeStdlib`. Shadowed calls use normal function-value checking and lowering.

Member-call sugar is not shadowed by lexical bindings, matching the existing
`push`, `pop`, `len`, `substr`, and `charAt` behavior:

```cd
let slice = 1;
print [1, 2, 3].slice(1, 2);
```

The explicit member arities are:

- `array.contains(value)` — one argument;
- `array.slice(start, length)` — two arguments;
- `array.copy()` — zero arguments;
- `array.concat(right)` — one argument.

These names join the existing builtin-member name set. Struct methods with the
same names remain rejected by the current builtin member-call conflict rule.

## Static Type Checking

Known non-array inputs are type errors. Unknown inputs are allowed through the
compiler and validated by the Rust VM.

### `contains`

- The first argument or receiver must be an array when statically known.
- For a statically known `[T]`, the queried value must be assignable to `T`.
- A dynamic array with unknown element type accepts any queried value.
- The return type is always `bool`.

### `slice` and `copy`

- The source must be an array when statically known.
- `slice` start and length expressions must be `number` when statically known.
- `[T]` inputs return `[T]`.
- Dynamic array inputs return dynamic arrays with unknown element type.

Integer, finiteness, sign, and bounds checks remain runtime checks because the
type system does not track numeric refinements or constant ranges.

### `concat`

- Both inputs must be arrays when statically known.
- If both inputs have known element types, use the existing array-element merge
  rules used for unannotated array literals.
- Equal element types remain that type.
- `T` merged with `T?` produces `T?`.
- Incompatible known types produce a dynamic array result instead of a type
  error, preserving the language's mixed-array escape hatch.
- If either input has unknown element type, the result is a dynamic array.

`concat` does not refine, widen, or otherwise mutate either input binding's
static type.

## Compiler Architecture and Data Flow

No parser or AST changes are required because all new forms use existing call
and member-call expressions.

Add four `NativeFunctionKind` values and registry entries in `NativeStdlib`.
Function-style calls then use the existing native stdlib resolution, arity
checking, and lowering path.

Extend `TypeChecker` native-call dispatch with the static rules above. Extend
builtin member-call recognition and checking with the corresponding receiver and
explicit-argument rules. Reuse the existing type merge utility for `concat`
rather than introducing a second collection type-merging algorithm.

Extend `IRCompiler::emitMemberCall` so the new member forms prepend the receiver
and emit the same native function names used by function-style calls. No new
`IROp`, `BytecodeOp`, `.cdbc` instruction, or bytecode text syntax is needed.

The resulting data flow is:

```text
source call
-> TypeChecker resolves native function or builtin member sugar
-> IRCompiler emits native_call
-> BytecodeCompiler preserves native_call
-> BytecodeTextEmitter writes the existing .cdbc native_call form
-> Rust VM dispatches by native function name
```

## Rust VM Runtime Design

Add native dispatch cases for the four function names. Each case validates
arity independently so malformed or hand-written `.cdbc` cannot bypass source
checks.

`slice`, `copy`, and `concat` create a new shared array container populated with
cloned `Value` handles. Cloning a reference-valued element preserves its existing
shared identity and therefore implements the required shallow-copy semantics.

`contains` iterates without mutating the array and delegates comparisons to the
same equality implementation used by the language's `==` operator. The native
implementation must not define a separate equality relation.

`slice` converts numeric offsets with checked integer conversion and checks the
end index without arithmetic overflow. The VM must reject invalid offsets before
allocating or modifying any result.

## Runtime Diagnostics

Runtime diagnostics follow the existing native stdlib wording:

```text
Runtime error: contains expects 2 arguments
Runtime error: contains expects array as first argument
Runtime error: slice expects 3 arguments
Runtime error: slice expects array as first argument
Runtime error: slice expects number as second argument
Runtime error: slice expects number as third argument
Runtime error: slice expects integer start offset
Runtime error: slice start offset out of bounds
Runtime error: slice expects integer length
Runtime error: slice length out of bounds
Runtime error: copy expects 1 argument
Runtime error: copy expects array as first argument
Runtime error: concat expects 2 arguments
Runtime error: concat expects array as first argument
Runtime error: concat expects array as second argument
```

Negative or non-representable start values use `slice start offset out of
bounds`. Negative or non-representable lengths and `start + length` beyond the
array use `slice length out of bounds`. Fractional or non-finite values use the
corresponding `expects integer` diagnostic.

All failures occur before a result is returned, produce no stdout, and leave
input arrays unchanged.

## Tests

### Success goldens

Add focused function-style and member-style fixtures covering:

- `contains` true and false results for numbers, strings, and booleans;
- reference identity behavior for nested arrays and named structs;
- empty arrays and an empty slice at the end boundary;
- partial, full, and zero-length slices;
- whole-array copy;
- concat ordering and empty-array inputs;
- function-name shadowing with member calls remaining available;
- typed return propagation through `contains`, `slice`, `copy`, and `concat`;
- nullable element merging and incompatible concat falling back to a dynamic
  array;
- shallow-copy behavior for top-level mutation and nested reference mutation.

At least one fixture should include `ir.out`, `bytecode.out`, and `run.out` so
the new helpers visibly share the existing `native_call` pipeline.

### Type-error goldens

Cover:

- known non-array source arguments and receivers;
- known non-array right-hand `concat` arguments;
- non-number `slice` offsets;
- incompatible `contains` query values for known `[T]` arrays;
- wrong arity for function and member forms;
- conflicts between the new builtin member names and struct method declarations.

### Runtime-error goldens

Use unknown function results to defer validation and cover:

- non-array arguments for every helper;
- invalid dynamic `concat` right arguments;
- fractional, non-finite, negative, and out-of-range slice offsets;
- slice end overflow or range overflow;
- no stdout and unchanged input behavior on failure where observable.

### Artifact and Rust VM coverage

- Add `.cdbc` artifact expectations showing the existing `native_call` form.
- Add Rust VM unit tests for each native function, malformed argument counts,
  shallow-copy identity, and slice boundary arithmetic.
- Run focused goldens during development and the repository's full verification
  suite before completion.

## Documentation Updates

Update:

- `README.md` with the four APIs, member forms, shallow-copy semantics,
  shadowing, and runtime validation rules;
- `docs/roadmap.md` to mark the first small collection-helper slice complete and
  move the near-term recommendation to runtime diagnostics;
- `AGENTS.md` current-language semantics and builtin inventory.

Do not update `docs/language-grammar.ebnf` because no grammar changes. Do not
update `docs/bytecode-text-format.md` because the existing `native_call` format
does not change.

## Success Criteria

- All four helpers work in function and member forms through `.cdbc` and the
  Rust VM.
- Function names remain shadowable and member sugar remains unshadowed.
- Returned arrays have the approved shallow-copy and non-mutation semantics.
- Static return types preserve or merge array element information as specified.
- Invalid known inputs are type errors; unknown invalid inputs are runtime
  errors with stable diagnostics.
- No new source syntax, IR opcode, bytecode opcode, or artifact format is added.
- Existing tests continue to pass alongside focused success, type-error,
  runtime-error, artifact, and Rust VM coverage.

## Decisions Locked for This Phase

- The API consists exactly of `contains`, `slice`, `copy`, and `concat`.
- All four APIs have function and member forms.
- `slice` uses `(start, length)`, matching `substr`, rather than an exclusive end
  index.
- All returned arrays are shallow copies and none of the helpers mutate inputs.
- `contains` uses existing language equality.
- `concat` preserves mixed arrays by falling back to a dynamic element type.
- Higher-order collection helpers wait for generic type design.
