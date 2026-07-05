# Named Struct Types Design

## Goal

Implement Phase 12C named struct types as a static type-checking feature:

```cd
struct Person {
  name: string,
  age: number
}

let p: Person = { name: "Ada", age: 36 };
print p.name;
p.age = 37;
```

A named struct declaration defines a compile-time field shape. Runtime values remain the existing anonymous struct values introduced in Phase 12A, and field assignment keeps the Phase 12B runtime behavior.

## Scope

This phase includes:

- A `struct` declaration syntax for named field shapes.
- Named struct type annotations such as `let p: Person = ...`.
- Exact static checking for struct literal initialization against named struct annotations.
- Static field access types for known named struct values.
- Static field assignment checks for known named struct values.
- Parse-error and type-error golden coverage.
- Success golden coverage for AST, IR, bytecode, run, bytecode artifact, and Rust VM parity where applicable.
- Documentation updates.

This phase does not include:

- Runtime values carrying the struct type name.
- Constructor syntax such as `Person { name: "Ada" }`.
- Named struct declarations inside blocks, functions, or modules.
- Methods, inheritance, protocols, or member-call dispatch.
- Recursive struct type checking.
- Generic struct types.
- Optional fields, nullable field syntax, or default field values.
- Creating new fields by assignment.

## User-Visible Syntax

Named struct declarations are top-level declarations:

```cd
struct Person {
  name: string,
  age: number
}
```

Fields use existing type annotation syntax. Field separators use commas. A trailing comma is not part of this phase unless the parser already accepts one incidentally; tests should document the actual parser behavior.

Struct declarations do not produce runtime output and are not executable statements.

Named structs can be used as type annotations:

```cd
let person: Person = { name: "Ada", age: 36 };
```

Function parameters and returns may also use named struct types because they use the same `TypeAnnotation` path:

```cd
fun birthday(person: Person): Person {
  person.age = person.age + 1;
  return person;
}
```

## Type Model

Extend `TypeInfo` so `StaticType::Struct` can optionally carry a named struct identity, for example `structName = "Person"`.

Two struct type categories exist:

- Anonymous struct: `StaticType::Struct` with no name. This is the current conservative type for unannotated struct literals and unknown dynamic struct shapes.
- Named struct: `StaticType::Struct` with a known declared name. This has a known field shape.

Compatibility rules:

- Unknown remains compatible with all types.
- Anonymous struct is compatible with any named struct only when the assignment context performs explicit struct literal shape checking. This prevents arbitrary anonymous struct variables from being silently treated as a named struct without checking their shape.
- Same named struct types are compatible.
- Different named struct types are not compatible, even if their fields are structurally identical. This phase uses nominal identity once a value is known as a named struct.
- Anonymous struct remains compatible with anonymous struct for existing dynamic behavior.

`typeInfoName` should print named structs as their declared name, e.g. `Person`, and anonymous structs as `struct`.

## Struct Declaration Table

The type checker owns a `structTypes_` table keyed by struct name:

```cpp
struct StructFieldType {
    Token name;
    TypeInfo type;
};

struct StructTypeDecl {
    Token name;
    std::vector<StructFieldType> fields;
};
```

Rules:

- Duplicate struct declarations in the same program are type errors.
- Duplicate field names within a struct declaration are type errors.
- Field type annotations resolve through the same annotation resolver used for variables, parameters, and returns.
- A field type may reference a previously declared named struct.
- A field type referencing a later struct declaration is a type error in this phase. This keeps declaration resolution single-pass and avoids recursive type design.

Suggested diagnostics:

```text
Type error at 1:8: duplicate struct `Person`
Type error at 2:3: duplicate field `name` in struct `Person`
Type error at 2:9: unknown type `Address`
```

## Struct Literal Initialization

For `let p: Person = { ... };`, the initializer must be a direct `StructExpr` literal and must exactly match the declared fields:

- All declared fields must be present.
- No extra fields are allowed.
- Field order does not matter.
- Each field value must be compatible with the declared field type.
- Duplicate literal field names are type errors when checking against a named struct.

Examples:

```cd
struct Person { name: string, age: number }
let ok: Person = { age: 36, name: "Ada" };
```

Errors:

```cd
let missing: Person = { name: "Ada" };       // missing `age`
let extra: Person = { name: "Ada", age: 36, title: "Dr" }; // extra `title`
let wrong: Person = { name: "Ada", age: "old" }; // wrong field type
```

Suggested diagnostics:

```text
Type error at 2:19: missing field `age` for struct `Person`
Type error at 2:42: extra field `title` for struct `Person`
Type error at 2:35: field `age` expects number, got string
```

If the initializer is not a direct struct literal, use normal assignment compatibility. Unknown remains flexible, same named struct is compatible, and anonymous struct variables are not promoted to named structs without a literal shape check.

## Field Access

For known named struct values, field access is statically checked:

```cd
let p: Person = { name: "Ada", age: 36 };
let name: string = p.name;
```

Rules:

- If object type is a named struct, the field must exist.
- The expression result type is the declared field type.
- If object type is anonymous struct, preserve existing behavior: field access result is unknown.
- If object type is unknown, preserve existing dynamic behavior.
- If object type is a known non-struct, keep the existing type error.

Suggested diagnostic:

```text
Type error at 3:9: struct `Person` has no field `missing`
```

## Field Assignment

For known named struct values, field assignment is statically checked:

```cd
p.age = 37;
```

Rules:

- If object type is a named struct, the field must exist.
- The assigned value must be compatible with the declared field type.
- The expression result type is the assigned field type.
- If object type is anonymous struct or unknown, preserve existing dynamic behavior and result type as the value type.
- If object type is a known non-struct, keep the existing type error.

Suggested diagnostics:

```text
Type error at 3:3: struct `Person` has no field `missing`
Type error at 3:9: field `age` expects number, got string
```

Runtime semantics remain the existing Phase 12B behavior: mutate an existing field and return the assigned value. Missing fields can still be runtime errors for dynamically typed cases.

## Parser and AST

Add `TokenType::Struct` and lexer keyword `struct`.

Add a declaration node:

```cpp
struct StructFieldDecl {
    Token name;
    TypeAnnotation typeName;
};

struct StructDeclStmt final : Stmt {
    StructDeclStmt(Token name, std::vector<StructFieldDecl> fields);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::vector<StructFieldDecl> fields;
};
```

The parser recognizes `struct` in `declaration()` before generic statements:

```ebnf
structDecl = "struct", identifier, "{", [ structFields ], "}" ;
structFields = structField, { ",", structField } ;
structField = identifier, ":", typeExpr ;
```

AST printer output should be stable, for example:

```text
Struct Person {name: string, age: number}
```

Struct declarations should appear in the program AST output at their declaration position.

## IR, Bytecode, and Runtime

Struct declarations are compile-time only.

- IR compiler ignores `StructDeclStmt`.
- IR interpreter has no new runtime behavior.
- Bytecode compiler and `.cdbc` emitter have no new opcode.
- Rust VM has no new runtime instruction.

Existing struct literals still lower as anonymous struct runtime values. Existing field access and assignment IR/bytecode operations are reused.

## Tests

### Success fixture

Add a success fixture covering:

```cd
struct Person { name: string, age: number }
let p: Person = { age: 36, name: "Ada" };
print p.name;
p.age = 37;
print p.age;

fun birthday(person: Person): Person {
  person.age = person.age + 1;
  return person;
}
let older: Person = birthday(p);
print older.age;
```

Expected run output:

```text
Ada
37
38
```

The fixture should include `ast.out`, `ir.out`, `bytecode.out`, and `run.out`. Rust VM parity should include the case automatically if it has `run.out`.

### Type-error fixtures

Add fixtures for:

- Unknown named struct type.
- Duplicate struct declaration.
- Duplicate field declaration.
- Missing field in named struct literal initialization.
- Extra field in named struct literal initialization.
- Wrong field type in named struct literal initialization.
- Unknown field access on known named struct.
- Wrong field assignment type on known named struct.
- Assigning a different named struct type.

### Parse-error fixtures

Add fixtures for:

- Missing struct name.
- Missing `{` after struct name.
- Missing field type after `:`.
- Missing `}` after fields.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf` with `structDecl`.
- `README.md` with named struct syntax and limitations.
- `docs/roadmap.md` marking Phase 12C implemented after code lands.
- `AGENTS.md` current language semantics.

## Open Decisions Locked for This Phase

- Named structs are static-only and do not affect runtime representation.
- Literal initialization uses exact field matching.
- Named struct compatibility is nominal after a value is known as named.
- No constructor syntax in this phase.
- No recursive or forward-referenced struct types in this phase.
