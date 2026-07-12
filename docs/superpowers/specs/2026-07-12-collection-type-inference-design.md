# Collection Type Inference Design

## Goal

Improve array type inference while preserving the language's existing dynamic
mixed-array escape hatch. This is a conservative type-checker-only slice: syntax,
AST shape, IR, bytecode, runtime values, and Rust VM behavior do not change.

The user-visible result is that unannotated arrays keep useful element types in
more common cases, including nullable literals and direct mutation through array
helpers. Later reads through indexing, `pop`, and `for-in` can then use the
refined element type.

## User-Facing Behavior

Array literals should merge compatible element types instead of requiring exact
mutual compatibility only:

```cd
let maybeNumbers = [1, nil];       // [number?]
let nested = [[1], [2]];           // [[number]]
let dynamic = [1, "x"];           // dynamic array escape hatch
```

Unannotated array variables may be refined by direct mutation:

```cd
let xs = [];
xs.push(1);
let n: number = xs.pop();

let ys = [];
push(ys, "a");
let s: string = ys[0];

let zs = [1];
zs.push(nil);
let maybe: number? = zs[0];
```

Explicit array annotations remain authoritative and strict:

```cd
let xs: [number] = [];
xs.push("x"); // type error
```

Mixed arrays remain supported. If an unannotated array variable with a known
element type is directly mutated with an incompatible value, the variable falls
back to a dynamic array type instead of becoming a hard type error:

```cd
let xs = [1];
xs.push("x");
print typeOf(xs[1]); // allowed at type-check time, runtime value is string
```

## Scope

Include:

- nullable-aware array literal element merging;
- nested array element merging;
- direct simple-variable refinement through `push(xs, value)`;
- direct simple-variable refinement through `xs.push(value)`;
- direct simple-variable refinement through `xs[index] = value`;
- existing readers (`xs[index]`, `pop(xs)`, `xs.pop()`, and `for item in xs`) using
  the refined binding type automatically;
- success and type-error golden coverage;
- README and roadmap updates.

Exclude:

- new syntax or grammar changes;
- generic types;
- global constraint solving;
- alias analysis;
- field, index, or call-result target refinement such as `obj.items.push(1)`,
  `arrays[0].push(1)`, or `getArray().push(1)`;
- cross-function mutation inference;
- runtime enforcement of inferred element types;
- IR, bytecode, `.cdbc`, or Rust VM changes.

## Type Merging Rules

Add an internal merge helper, likely in `TypeUtils`, that returns an optional
merged `TypeInfo` for two known element types. A missing merge means the array
should fall back to dynamic element typing. Literal inference should still treat
an element expression with unknown type as a dynamic escape hatch rather than
inferring from the remaining known elements. Direct mutation refinement may
refine a plain dynamic array from a known pushed or assigned value.

Rules:

- an empty inference accumulator plus `T` becomes `T`;
- an already-known `T` plus an unknown expression type does not produce a known element type;
- `T` plus compatible `T` merges to `T`;
- `T` plus `nil` merges to `T?`;
- `nil` plus `T` merges to `T?`;
- `T?` plus `nil` merges to `T?`;
- `T?` plus `T` merges to `T?` when the inner type accepts `T`;
- `[A]` plus `[B]` merges to `[merge(A, B)]` when the element merge succeeds;
- structs merge only when their existing compatibility rules accept them;
- functions merge only when their existing compatibility rules accept them;
- incompatible known types, such as `number` and `string`, do not merge.

The helper should be conservative and should not invent union types other than
existing nullable types. When no merge exists, callers that are inferring an
unannotated array should use `array` with unknown element type.

## Type Checker Architecture

### Array literals

`TypeChecker::inferArrayElementType` should use the merge helper while walking
elements. Empty array literals still infer dynamic `array`. If every element can
be merged, `checkArrayLiteral` returns `[mergedElement]`; otherwise it returns
plain dynamic `array`.

Contextual array checking stays strict. If an expected type is `[T]`, each
element is checked against `T` and incompatible elements are type errors, as they
are today.

### Binding refinement

Add a small helper on `TypeChecker` for direct variable array mutation, for
example:

```cpp
void refineArrayBindingFromMutation(Binding& target, const TypeInfo& valueType);
```

Behavior:

1. If the binding has an explicit type, do not widen it. Existing strict checks
   should reject incompatible values.
2. If the binding type is unknown or plain `array`, refine it to `[valueType]`
   when `valueType` is known. If `valueType` is unknown, leave or set the
   binding as plain dynamic `array`.
3. If the binding type is `[T]`, merge `T` with `valueType` when `valueType` is
   known.
4. If merge succeeds, update the binding to `[merged]`.
5. If merge fails, or if an unannotated `[T]` receives an unknown value type,
   update the binding to plain dynamic `array` to preserve mixed-array support.

This refinement is static metadata only. Runtime arrays remain mutable reference
values and are not tagged with element types.

### Push calls

For `push(xs, value)` and `xs.push(value)`, detect when the array argument or
receiver is a simple `VariableExpr` with a resolved binding. In that case:

- use the current binding type to perform existing array/non-array validation;
- for explicit bindings, type-check the pushed value against the annotated
  element type and keep current strict error behavior;
- for unannotated bindings, evaluate the pushed value without rejecting it solely
  because it does not match the current inferred element type, then refine,
  widen to nullable, or degrade the binding according to the merge helper;
- return `nil` as today.

For non-simple targets, keep current behavior: validate known receiver type and
argument compatibility when possible, but do not update any binding.

### Index assignment

For `xs[i] = value`, detect a simple variable collection target and apply the
same refinement rules after validating the index expression. Explicit arrays
remain strict and check the assigned value against the annotated element type.
Unannotated arrays should not reject an assignment solely because the value does
not match the current inferred element type; instead they may refine, widen to
nullable, or degrade to dynamic.

For non-simple collection expressions, keep current behavior.

### Readers

No special reader changes should be necessary. `VariableExpr` already returns the
current binding type. `checkIndex`, `pop`, member `pop`, and `for-in` should use
that updated type naturally.

## Error Handling

This phase should not introduce new diagnostic categories. Existing type-error
messages for explicit arrays should remain stable where practical:

- `push value expects [type], got [type]` for incompatible push values;
- `array index assignment expects [type], got [type]` for explicit index
  assignment mismatches.

Unannotated mixed-array mutations should not report type errors solely because
of incompatible element types. They should fall back to dynamic array typing.

## Tests

Add success fixtures under `tests/golden/`:

1. nullable literal inference:

   ```cd
   let xs = [1, nil];
   let maybe: number? = xs[0];
   print maybe;
   ```

2. empty array refined through member push:

   ```cd
   let xs = [];
   xs.push(1);
   let n: number = xs.pop();
   print n;
   ```

3. empty array refined through function-style push:

   ```cd
   let xs = [];
   push(xs, "a");
   let s: string = xs[0];
   print s;
   ```

4. existing array widened to nullable:

   ```cd
   let xs = [1];
   xs.push(nil);
   let maybe: number? = xs[1];
   print maybe;
   ```

5. direct index assignment refinement, using an existing runtime element because
   index assignment does not append:

   ```cd
   let xs = [nil];
   xs[0] = 1;
   let maybe: number? = xs[0];
   print maybe;
   ```

6. mixed dynamic fallback:

   ```cd
   let xs = [1];
   xs.push("x");
   print typeOf(xs[1]);
   ```

Add type-error fixtures under `tests/golden/type_errors/`:

1. explicit array push mismatch:

   ```cd
   let xs: [number] = [];
   xs.push("x");
   ```

2. explicit array index-assignment mismatch:

   ```cd
   let xs: [number] = [1];
   xs[0] = "x";
   ```

Run focused golden tests for the new fixtures, then full verification before
claiming completion.

## Documentation

Update `README.md` to document:

- nullable-aware and nested array literal inference;
- unannotated direct-variable array refinement through `push` and index
  assignment;
- mixed-array fallback to dynamic typing;
- explicit array annotations remaining strict.

Update `docs/roadmap.md` after implementation to mark collection inference as
complete or to narrow remaining future work. Do not update
`docs/language-grammar.ebnf`, because grammar is unchanged.

## Success Criteria

- `[1, nil]` can be used as `[number?]` without explicit annotation.
- Nested homogeneous arrays retain nested element types.
- Empty unannotated arrays refined through direct `push` provide known element
  types to `pop`, indexing, and `for-in`.
- Direct index assignment can refine unannotated array variables without changing
  runtime semantics.
- Explicit array annotations still reject incompatible pushed or assigned values.
- Mixed unannotated arrays remain accepted by degrading to dynamic array typing.
- Existing parser, type, IR, bytecode, runtime, import, and module tests continue
  to pass.
