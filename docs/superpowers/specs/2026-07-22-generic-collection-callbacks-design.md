# Generic Collection Callback Inference Design

Date: 2026-07-22

## Goal

Use the existing generic-function substitution machinery at callback call sites
for the already implemented array collection helpers. This closes a type
inference boundary without adding a new runtime API or changing bytecode.

## Scope

Known generic function values and generic function expressions can be passed
to existing `map`, `filter`, `flatMap`, `any`, `all`, `count`, `find`,
`findIndex`, and `reduce` callbacks.

Examples:

```cd
fun identity<T>(value: T): T { return value; }
fun keep<T>(value: T): bool { return true; }
fun singleton<T>(value: T): [T] { return [value]; }
fun keepAccumulator<T>(acc: T, value: number): T { return acc; }

let values: [number] = [1, 2];
let copied: [number] = map(values, identity);
print filter(values, keep);
print flatMap(values, singleton);
print reduce(values, 0, keepAccumulator);
```

The type checker infers generic parameters recursively from callback parameter
types and the known call-site types. Array, map, nullable, enum, and function
shapes reuse `inferTypeArguments`; generic parameters must all be inferred.
An unresolved parameter remains a type error rather than silently becoming a
dynamic callback.

Generic inline function expressions use their declared type-parameter scope
instead of the monomorphic contextual callback signature, then receive the
same call-site specialization. Generic function values remain generic when
stored or passed elsewhere, and generic functions are still not coerced to
monomorphic function annotations.

## Non-goals

- No generic constraints or new generic container syntax.
- No new array or map API.
- No runtime specialization, IR opcode, bytecode opcode, or Rust VM change.
- No inference from callback return values when a type parameter does not
  occur in a callback parameter.

## Diagnostics and coverage

Existing callback arity, parameter, and return checks run after specialization.
Unresolved parameters use `'<helper> cannot infer type parameter T'`. Coverage
includes direct and member forms, generic named functions, aliases, inline
generic lambdas, nullable/array-shaped inference, reduce accumulator inference,
and unresolved type-parameter errors.
