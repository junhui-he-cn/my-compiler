# Remove Anonymous Structs Design

## Goal

Remove source-language anonymous struct literals so struct values can only be created through declared named struct constructors such as `Person { name: "Ada" }` or `geo.Point { x: 1, y: 2 }`.

## User-facing semantics

### Removed syntax

The expression form:

```cd
{ field: value, ... }
```

is no longer valid source syntax. This includes previously supported annotated initialization:

```cd
let p: Person = { name: "Ada" };
```

Users must write the named constructor explicitly:

```cd
let p: Person = Person { name: "Ada" };
let q = Person { name: "Ada" };
let r = geo.Point { x: 1, y: 2 };
```

### Preserved syntax and behavior

The following remain supported:

- named struct declarations: `struct Name { field: type, ... }`;
- local named constructors: `Name { field: value, ... }`;
- namespaced named constructors: `alias.Name { field: value, ... }`;
- exact field validation for missing, duplicate, extra, and wrong-typed fields;
- field access and existing-field assignment on struct values;
- struct methods, including imported and re-exported method metadata;
- named runtime type names through `typeOf`.

Field creation by assignment remains unsupported. Since source-level anonymous struct literals are removed, there is no near-term dynamic object form that can grow fields.

## Parser behavior

A `{` token in expression position becomes invalid unless it belongs to existing statement/block syntax handled by statement parsing. Examples that should fail parsing include:

```cd
let p = { name: "Ada" };
print { x: 1 };
let nested = Person { child: { name: "Ada" } };
```

The preferred diagnostic can use the existing generic primary-expression wording if that is the smallest stable parser change. A dedicated message such as `anonymous struct literals are not supported; use StructName { ... }` is acceptable if it fits existing diagnostic style.

The parser should keep the shared field-list parser used by named constructors, but should stop producing an anonymous `StructExpr` AST node.

## Type checking behavior

Type checking should only accept `StructConstructExpr` for struct construction. The previous special case that accepted `let p: Person = { ... };` as a named struct initializer is removed.

Named constructor type checking remains the single source of field validation:

- unknown constructor type: type error;
- missing field: type error;
- extra field: type error;
- duplicate field: type error;
- field value not assignable to declared field type: type error.

Unannotated `let` bindings no longer infer an anonymous `struct` type from source syntax. Static type `Struct` may remain internally as the dynamic/runtime struct category used when type information is unknown, for imported/runtime paths, or for bytecode compatibility, but source-level creation should require a named declaration.

## Runtime and bytecode behavior

The language compiler should no longer emit anonymous struct construction from source programs. Named constructors continue to lower to the existing `Struct` IR/bytecode operation with attached type-name metadata.

The `.cdbc` anonymous struct form may remain supported by the bytecode text format and Rust VM as a low-level/backward-compatible artifact form. This design does not require removing VM support for anonymous `struct { ... }` instructions, because the requested change is source-language removal and preserving parser/VM compatibility reduces churn. Source documentation should no longer describe anonymous struct literals as a language feature.

## Test strategy

Update successful golden fixtures that currently rely on anonymous source struct literals by introducing small named struct declarations and named constructors. Preserve the behavior each fixture was originally testing, such as field access, field mutation, identity equality, `typeOf`, and runtime errors.

Add parse-error coverage for bare anonymous struct literals in expression position, including the previously supported annotated initializer form.

Update type-error fixtures that used annotated anonymous literals to use named constructors instead when the fixture is intended to test named struct field validation rather than anonymous syntax.

Refresh IR, bytecode, `.cdbc`, Rust VM, and run goldens where named constructors change output metadata or name-table numbering.

## Documentation updates

Update:

- `README.md` to remove anonymous struct literal syntax and examples;
- `docs/language-grammar.ebnf` to remove `struct = "{" ... "}"` from expressions;
- `docs/roadmap.md` to record that source-level anonymous structs were removed;
- `AGENTS.md` project memory to tell future agents only declared named structs can be constructed from source.

`docs/bytecode-text-format.md` may continue documenting anonymous `.cdbc` struct instructions if the VM still supports them.

## Non-goals

- Do not add field creation by assignment.
- Do not add constructor function syntax such as `Person(...)`.
- Do not add dynamic object/map literals.
- Do not remove low-level `.cdbc`/Rust VM support for anonymous struct instructions unless it becomes necessary during implementation.
- Do not change struct equality, printing for named structs, field access, methods, imports, or re-exports.
