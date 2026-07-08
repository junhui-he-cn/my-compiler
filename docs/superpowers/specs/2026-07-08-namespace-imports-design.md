# Namespace Imports Design

## Goal

Add explicit namespace imports so a module can import another module under an alias and access exported declarations through that alias instead of injecting all exported names into the importing module's top-level scope.

## Non-goals

- No package search paths.
- No re-export syntax.
- No wildcard imports.
- No nested namespace aliases.
- No runtime namespace values or new VM opcode.
- No separate compilation.

## Syntax

Existing import syntax remains valid and keeps its current behavior:

```cd
import "./math.cd";
print add(1, 2);
```

New alias import syntax:

```cd
import "./math.cd" as math;

print math.add(1, 2);
let p: math.Point = math.Point { x: 1, y: 2 };
```

Grammar additions:

```ebnf
importDecl        = "import", string, [ "as", identifier ], ";" ;
qualifiedName     = identifier, ".", identifier ;
qualifiedType     = qualifiedName ;
structConstructor = [ identifier, "." ], identifier, "{", [ fields ], "}" ;
```

`as` should be added as a keyword token so diagnostics are stable and the parser can produce clear messages for malformed imports.

## Semantics

`import "path";` remains a direct import. It loads the module and inserts exported value/function bindings and exported struct types into the importing module's top-level scope.

`import "path" as alias;` is a namespace import. It loads the module and binds only the namespace alias in the importing module. Exported declarations are available through qualified access:

- `alias.value` resolves to an exported value binding.
- `alias.function(...)` resolves to an exported function binding.
- `alias.Struct` resolves to an exported struct type in type annotations.
- `alias.Struct { ... }` constructs a value checked against the exported struct shape.

A namespace import must not insert exported names directly into the current top-level scope. This keeps the alias import from polluting the importer and allows modules with conflicting export names to be imported safely under different aliases.

Namespace aliases are compile-time-only names. They are not runtime values. They cannot be printed, assigned, passed as arguments, or used as ordinary variables.

## Name Resolution

The type checker should maintain a module-local namespace table mapping alias names to the imported module's export tables. Alias declarations are top-level only because imports are already top-level only.

Alias name conflicts should be type errors when the alias name already exists in the current top-level scope as a value/function binding, struct type, or namespace alias. This avoids ambiguous references such as `math` being both a variable and a namespace.

Qualified expression access should use existing field-access syntax in the AST where possible. During type checking, a field access whose object is a namespace alias should resolve to the exported value/function binding instead of a runtime struct field. The resolved-name table should record that expression as the imported binding's resolved name so IR generation can load the existing binding without creating a namespace runtime value.

Qualified type names should resolve only through namespace aliases. Unknown aliases and unknown exported type names are type errors.

Qualified struct constructors should resolve only through namespace aliases for the qualifier. Unknown aliases, private structs, and missing exported struct names are type errors.

## Error Handling

Malformed import syntax remains a parse error. Examples:

```cd
import "./math.cd" as;
import "./math.cd" as 123;
```

Invalid namespace usage is a type error. Examples:

```cd
import "./math.cd" as math;
print math;          // namespace alias is not a value
math = 1;            // namespace alias cannot be assigned
print math.private;  // private or missing export
print missing.add;   // unknown namespace or variable, depending on expression shape
let p: math.Missing = nil;
```

The compiler may continue to report the first error only.

## AST and Parser

`ImportStmt` should store an optional alias token. AST printing should include the alias for readability, for example:

```text
Import "./math.cd" as math
```

Type annotations should support qualified type names without disrupting existing simple, array, and function types. Struct constructor parsing should recognize `identifier "." identifier "{" ... "}"` as a namespaced constructor while leaving ordinary field access and struct literals unchanged.

Expression parsing can continue to parse `math.add` as field access. The type checker distinguishes namespace member access from runtime field access.

## IR, Bytecode, and VM

Namespace aliases are compile-time only. IR should not contain a namespace value or namespace opcode.

For `math.add`, IR lowering should load the imported function/value binding identified during type checking. For calls such as `math.add(1, 2)`, existing call lowering should work after the callee expression resolves to the imported binding. Bytecode lowering and Rust VM execution should not need semantic changes.

## Tests

Add or update golden and parity coverage for:

- direct import behavior remains unchanged;
- `import "path" as alias;` and namespaced value read;
- namespaced function call;
- namespaced struct type annotation;
- namespaced struct constructor;
- alias import does not expose exports as top-level names;
- two modules exporting the same name can be imported under different aliases;
- alias name conflicts with local value, function, struct, or alias;
- missing alias identifier parse error;
- private or missing namespaced member type errors;
- unknown namespaced type type error;
- assigning to a namespace alias type error;
- bytecode artifact and Rust VM parity for a namespaced import program.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf`
- `README.md`
- `docs/roadmap.md`
- `AGENTS.md`

Docs should explain that direct imports still insert exports into the top-level scope, while alias imports require qualified access and avoid top-level pollution.

## Migration and Compatibility

Existing programs using direct imports continue to work. Existing module export syntax from the export-list slice remains unchanged. Namespace imports are an additive feature.
