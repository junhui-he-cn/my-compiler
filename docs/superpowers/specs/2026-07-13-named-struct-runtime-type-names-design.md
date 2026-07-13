# Named Struct Runtime Type Names Design

## Goal

Make named struct runtime values expose their static struct name through the
existing `typeOf(value)` builtin, while anonymous struct values continue to report
`"struct"`.

This is a small user-visible struct polish slice. Runtime values gain optional
metadata, but struct printing, equality, field access, field assignment, method
calls, construction rules, and static type checking remain unchanged.

## User-Facing Behavior

Named struct constructors attach a runtime type name to the created struct value:

```cd
struct Person { name: string }
let p = Person { name: "Ada" };

print typeOf(p);                    // Person
print typeOf(Person { name: "Ada" }); // Person
print typeOf({ name: "Ada" });        // struct
```

The runtime type name is preserved through aliases and field mutation:

```cd
let q = p;
q.name = "Grace";
print typeOf(p); // Person
print p.name;    // Grace
```

Anonymous struct literals remain anonymous and `typeOf({ x: 1 })` remains
`"struct"`.

For namespaced constructors, use the same static type name the type checker uses
for the constructed value. For example, if `import "./geometry.cd" as geo;` makes
`geo.Point { ... }` produce static type `geo.Point`, then `typeOf(...)` should
return `"geo.Point"`.

## Scope

Include:

- optional runtime type-name metadata on struct values;
- C++ value representation support for optional struct type names;
- IR and bytecode representation of optional struct type names;
- `.cdbc` text artifact support for anonymous and named struct construction;
- Rust VM parser/formatter/runtime support for optional struct type names;
- `typeOf` returning a named struct runtime name when present;
- golden and artifact coverage;
- README, bytecode text format docs, and roadmap updates.

Exclude:

- changing printed struct value format;
- changing struct equality semantics;
- runtime nominal type checks;
- constructor function syntax such as `Person(...)`;
- recursive structs;
- field creation by assignment;
- dynamic dispatch, inheritance, overloading, protocols, or optional chaining;
- changing anonymous struct behavior.

## Runtime Value Model

Add optional type-name metadata to struct values:

- C++ `StructValue`: add `std::optional<std::string> typeName`.
- Rust `StructValue`: add `Option<String> type_name`.

The metadata is part of the shared struct reference value. Aliases see the same
fields and the same type-name metadata. Field assignment mutates fields only and
must not change the type name.

`valueToString` / Rust `Display` should keep the current field-only format:

```text
{name: Ada}
```

Do not print `Person { ... }` or any wrapper.

Equality remains identity-based for all structs, regardless of type name.

## IR and Bytecode Model

Current struct construction stores only field name indexes and value registers.
Extend the existing struct instruction shape with an optional type-name index
rather than adding a new opcode.

Suggested IR/bytecode instruction data:

- existing field-name indexes: unchanged;
- existing field value registers: unchanged;
- new optional type-name index: `std::optional<std::size_t>` in C++ IR and
  bytecode structures.

Anonymous struct literals set the optional type-name index to empty. Named
constructors set it to a name-table entry for the static type name:

- `Person { ... }` -> `Person`
- `geo.Point { ... }` -> `geo.Point`

This keeps field instruction behavior unchanged and avoids new opcodes.

## `.cdbc` Text Format

Keep the anonymous form unchanged:

```text
r3 = struct {n1: r2}
```

Add a named form with an optional name reference immediately after `struct`:

```text
r3 = struct n0 {n1: r2}
```

In the named form, `n0` references the name table entry containing the runtime
struct type name. The field list format remains unchanged.

The Rust formatter should round-trip both forms canonically:

- anonymous: `rD = struct {nField: rValue, ...}`
- named: `rD = struct nType {nField: rValue, ...}`

## Compiler Lowering

`IRCompiler::emitStruct` for anonymous `StructExpr` should continue to emit a
struct instruction without a type-name index.

`IRCompiler::emitStructConstructor` for `StructConstructExpr` should emit a
struct instruction with a type-name index matching the static constructed type
name. Use the same naming convention as type checking:

- no qualifier: `expression.name.lexeme`
- qualifier: `qualifier.lexeme + "." + expression.name.lexeme`

The type checker already validates constructor names and namespace aliases before
IR lowering, so IR lowering can treat the syntax as valid.

## Rust VM Execution

When executing a struct instruction, the Rust VM should construct `StructValue`
with the parsed optional type name. `Value::type_name()` should return:

- existing primitive names for non-struct values;
- `type_name.as_deref().unwrap_or("struct")` for structs.

The native `typeOf` implementation already delegates to `Value::type_name()`, so
that path should need only the value-model update.

## Tests

Update existing fixture:

- `tests/golden/native_stdlib_typeof/run.out`: the named `Box { value: 1 }` case
  should change from `struct` to `Box`.

Add success fixture under `tests/golden/struct_runtime_type_name/`:

```cd
struct Person { name: string }
let p = Person { name: "Ada" };
let q = p;
q.name = "Grace";
print typeOf(p);
print p.name;
print typeOf({ name: "Ada" });
```

Expected run output:

```text
Person
Grace
struct
```

Add namespaced success fixture because namespace struct constructors are already
supported:

```cd
import "./geometry.cd" as geo;
print typeOf(geo.Point { x: 1, y: 2 });
```

Expected output:

```text
geo.Point
```

Add bytecode artifact coverage for a named struct constructor so the emitted
`.cdbc` includes the named form. Existing anonymous struct artifact or golden
coverage should continue to cover the anonymous form.

Add Rust format unit coverage for parsing/formatting both forms:

```text
r0 = struct {n0: r1}
r2 = struct n1 {n0: r1}
```

## Documentation

Update:

- `README.md`: named struct runtime values expose their static name through
  `typeOf`; anonymous structs still return `"struct"`; printing remains field
  based.
- `docs/bytecode-text-format.md`: document optional struct type-name prefix.
- `docs/roadmap.md`: replace the open question about named runtime values with
  the implemented decision.

Do not update `docs/language-grammar.ebnf`, because syntax is unchanged.

## Success Criteria

- `typeOf(Person { ... })` returns `"Person"`.
- `typeOf(p)` returns `"Person"` when `p` was initialized from a named struct
  constructor.
- `typeOf({ ... })` still returns `"struct"`.
- Named type metadata survives aliasing and field assignment.
- Namespaced constructors report the qualified static type name.
- Struct printing and equality remain unchanged.
- `.cdbc` parser/formatter round-trips anonymous and named struct forms.
- Existing language, bytecode artifact, and Rust VM parity tests continue to pass.
