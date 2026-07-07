# Struct Constructor Syntax Design

## Overview

Add named struct constructor expressions using `Name { ... }` syntax. This is Phase 12D of the records / structs roadmap. The feature makes named structs easier to use by allowing expressions to carry a named struct static type without requiring a separate `let name: Type = { ... }` annotation.

Example:

```cd
struct Person { name: string, age: number }

let p = Person { name: "Ada", age: 36 };
print p.name;
p.age = 37;
```

Runtime values remain the existing anonymous struct reference values. The constructor name is static type information only in this slice.

## Goals

- Add `StructName { field: value, ... }` as an expression form.
- Require the constructor name to refer to an existing named struct declaration.
- Reuse named struct exact-field checking for constructor fields:
  - duplicate literal field is a type error,
  - missing declared field is a type error,
  - extra field is a type error,
  - wrong field value type is a type error,
  - field order is irrelevant.
- Infer the expression type as the named struct type, enabling static field access and assignment without an explicit `let` annotation.
- Reuse existing struct runtime representation, IR operations, bytecode lowering, and Rust VM execution behavior as much as possible.

## Non-Goals

- Do not add constructor functions, `Person(...)`, or user-defined initialization methods.
- Do not add method syntax or method lookup.
- Do not add default field values.
- Do not create runtime type tags or expose runtime struct names.
- Do not add recursive struct type support or nullable field syntax.
- Do not allow assignment to create new fields.

## Syntax

Extend primary expressions with a named constructor form:

```ebnf
structConstructor = identifier, "{", [ fields ], "}" ;

primary = functionExpr
        | false
        | true
        | nil
        | number
        | string
        | array
        | structConstructor
        | struct
        | identifier
        | "(", expression, ")" ;
```

The parser should recognize `Identifier LeftBrace` as a constructor expression before treating the identifier as a variable expression. Anonymous struct literals continue to use `{ ... }`.

Examples:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada", age: 36 };

struct Pair { left: number, right: number }
let pair = Pair { right: 2, left: 1 };
```

## AST and Printing

Add a new expression node, for example:

```cpp
struct StructConstructExpr final : Expr {
    StructConstructExpr(Token name, std::vector<StructField> fields);
    void print(std::ostream& out) const override;

    Token name;
    std::vector<StructField> fields;
};
```

AST printing should make constructor expressions distinct from anonymous literals:

```text
(construct Person name: "Ada" age: 36)
```

This keeps existing anonymous struct output unchanged:

```text
(struct name: "Ada" age: 36)
```

## Static Checking

Constructor checking should produce a named struct type:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada", age: 36 };
print p.age; // statically known number
```

Rules:

1. The constructor name must resolve to a declared struct type. If not, report a type error at the constructor name.
2. Fields are checked against the named struct declaration using the same exact-match rules as annotated named struct literal initialization.
3. Duplicate fields in the constructor literal are type errors.
4. Missing fields are type errors.
5. Extra fields are type errors.
6. Field value expressions are checked against declared field types, including nested arrays, functions, and named structs.
7. The resulting expression type is the named struct type.

Suggested diagnostics:

```text
Type error at <line>:<column>: unknown struct type `Person`
Type error at <line>:<column>: missing field `age` for struct `Person`
Type error at <line>:<column>: extra field `height` for struct `Person`
Type error at <line>:<column>: field `age` expects number, got string
Type error at <line>:<column>: duplicate field `name` in struct literal
```

Existing diagnostics may be reused when their wording already matches these cases.

## IR, Bytecode, and Runtime

The feature should not introduce a new runtime value kind. A named constructor expression lowers to the same IR shape as an anonymous `StructExpr` with the same fields.

Expected behavior:

- `--run` creates the same mutable struct value as `{ ... }`.
- `--bytecode` and `.cdbc` artifacts use existing struct construction bytecode.
- Rust VM execution matches C++ `--run`.
- Runtime formatting remains anonymous, matching current struct value formatting.

If the IR compiler currently dispatches by concrete AST node type, add a `StructConstructExpr` branch that emits the same instructions as `StructExpr`.

## Parser Ambiguity

`Identifier "{" ... "}"` currently cannot be a valid expression followed by a block in expression position, so treating it as a constructor is unambiguous in primary expressions.

This syntax is distinct from:

```cd
let p = { name: "Ada" }; // anonymous struct literal
Person;                  // variable expression
Person();                // function call expression if Person is a function value
```

No namespace distinction between struct names and variable names is added in this slice. The constructor syntax specifically consults the struct type table during type checking, while ordinary `Person` expression lookup remains variable lookup.

## Testing Plan

Add success golden coverage for:

- Basic constructor creation, field access, and field assignment.
- Constructor field order independence.
- Passing a constructed value to a function parameter annotated with the named struct type.
- Returning a constructed value from a function annotated with the named struct type.
- Bytecode/Rust VM parity for constructor runtime behavior.

Add type-error fixtures for:

- Unknown constructor struct name.
- Missing field.
- Extra field.
- Wrong field type.
- Duplicate constructor field.

Add parse-error fixtures for malformed constructor fields where useful, such as missing field value or missing closing brace. Existing anonymous struct parse errors may already cover most field-list syntax errors; add constructor-specific parse fixtures only where the diagnostic differs.

## Documentation Updates

- Update `docs/language-grammar.ebnf` to include `structConstructor`.
- Update `README.md` named struct section to document `Name { ... }` construction.
- Update `docs/roadmap.md` Phase 12 status to mark constructor syntax implemented after the implementation is complete.

## Open Decisions Resolved

- Constructor syntax is `Name { ... }`, not `Name(...)`.
- Runtime values remain anonymous struct references.
- Field matching remains exact; no defaults or optional fields.
- Constructors use the struct type namespace, not variable lookup.
