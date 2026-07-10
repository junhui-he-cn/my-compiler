# Re-export Design

## Goal

Add Phase 14E re-export syntax so a module can forward selected exports from another source file without making those names local to the forwarding module.

Example:

```cd
// shapes.cd
struct Point { x: number, y: number }
fun origin(): Point { return Point { x: 0, y: 0 }; }
export Point, origin;

// api.cd
export Point, origin from "./shapes.cd";

// main.cd
import "./api.cd";
let p: Point = origin();
print p.x + p.y;
```

## Chosen Syntax

Use the existing export-list style with a trailing source clause:

```ebnf
re_export = "export" IDENTIFIER ("," IDENTIFIER)* "from" STRING ";" ;
```

`from` should be parsed as a context-sensitive word inside `export` declarations rather than as a new globally reserved keyword. This avoids breaking existing variables named `from` in ordinary expressions and still allows `export from;` to export a local declaration named `from`.

## Semantics

`export name[, name...] from "path";` means:

1. Load the target source file relative to the file containing the re-export declaration.
2. Type-check the target module before resolving the re-export.
3. Find each requested name among the target module's exported values/functions and exported structs.
4. Copy the requested export metadata into the current module's export tables.
5. Do not create local variable bindings, local struct type declarations, or local method tables in the forwarding module.

The feature is forward-only. A forwarding module cannot use a re-exported name unless it also imports that dependency explicitly:

```cd
export value from "./lib.cd";
print value; // type error: value is not local
```

Re-exported structs include their method metadata. Direct imports and namespace imports of the forwarding module should behave the same as if they imported the original module for those forwarded names:

```cd
import "./api.cd";
let p: Point = origin();
print p.method();

import "./api.cd" as api;
let q: api.Point = api.origin();
print q.method();
```

## Non-goals

- No renaming syntax such as `export Foo as Bar from "path";`.
- No wildcard re-export such as `export * from "path";`.
- No namespace forwarding such as `export "path" as alias;`.
- No package search paths or import maps.
- No separate compilation, linker, or runtime module opcode.
- No local scope pollution from re-export declarations.

## Conflict Rules

Exported names are treated as a shared public namespace for this slice.

A module must report a type error if it attempts to export the same public name more than once, regardless of whether the duplicate comes from a local export, a re-export, or a second re-export:

```cd
let value = 1;
export value;
export value from "./lib.cd"; // duplicate export `value`
```

```cd
export value from "./a.cd";
export value from "./b.cd"; // duplicate export `value`
```

If a requested re-exported name is not exported by the target module as either a value/function or a struct, report a type error at that name:

```text
Type error at <line>:<column>: module `./lib.cd` has no exported name `missing`
```

The diagnostic text should use the spelling from the re-export path token for readability. File-backed diagnostics will still be wrapped by existing module source-context logic.

## Import Loading and Cycles

The source loader must scan parsed statements for re-export source clauses in addition to ordinary `import` declarations. Re-export paths use the same resolution rules as `import "path";`:

- relative to the containing source file;
- canonical duplicate suppression;
- import-cycle detection;
- missing-file diagnostics;
- no source loading from stdin.

A re-export from stdin should fail with the same import-loading category as ordinary source imports from stdin, because there is no file-relative base path.

Cycles through re-export declarations should use the existing import-cycle diagnostic machinery. For example, `a.cd` re-exporting from `b.cd` while `b.cd` re-exports from `a.cd` is an import cycle.

## Parser and AST

Extend `ExportStmt` rather than adding a separate statement kind.

Add optional source metadata:

```cpp
struct ExportStmt final : Stmt {
    ExportStmt(Token keyword, std::vector<Token> names, std::optional<Token> sourcePath = std::nullopt);

    Token keyword;
    std::vector<Token> names;
    std::optional<Token> sourcePath;
    std::size_t resolvedModuleId = static_cast<std::size_t>(-1);
};
```

Parsing remains top-level only through the existing export-declaration path.

The parser should accept both:

```cd
export value, Type;
export value, Type from "./lib.cd";
```

It should reject malformed source clauses with parse errors, such as:

```cd
export value from;
export value from 123;
export value from "./lib.cd", other;
```

AST printing should distinguish re-export from local export, for example:

```text
Export value Type from "./lib.cd"
```

Exact tree formatting can follow the existing compact `Export ...` style as long as golden output is stable.

## TypeChecker Architecture

Keep existing local export behavior for `export name[, name...];`.

For `export name[, name...] from "path";`:

1. Ensure the checker is currently inside a module context.
2. Resolve `statement.resolvedModuleId` to a `ModuleStmt` using `findModule`.
3. Run `checkModule` on the target module so its export tables are populated.
4. For each requested name:
   - find a value export in `moduleSymbols_.valueExports(targetId)`;
   - find a struct export in `moduleSymbols_.structExports(targetId)`;
   - if the struct is found, also forward any method signatures from `moduleSymbols_.methodExports(targetId)` for that struct;
   - report missing export if neither value nor struct exists;
   - report duplicate export if the current module already has that public name exported.
5. Record forwarded exports into the current module's export tables without calling `declareImportedVariable`, without inserting into `structTypes_`, and without inserting into `methods_`.

To keep conflict checks centralized, add ModuleSymbols helper APIs rather than open-coding map probes throughout `TypeChecker`, for example:

```cpp
bool hasAnyExport(std::size_t moduleId, const std::string& name) const;
void recordForwardedStructMethodExports(
    std::size_t moduleId,
    std::string structName,
    const StructMethodTable& methods);
```

The exact helper names can differ during implementation if the responsibilities remain clear.

## IR, Bytecode, and Rust VM

No runtime representation changes are needed, but IR compilation must follow re-export source edges.

`IRCompiler` should continue to ignore local export declarations. For a re-export declaration with a resolved module id, it should compile the target module the same way it compiles ordinary imported modules. This ensures forwarded functions and struct method bodies are present in the IR when an entry module imports a barrel module but not the original defining module directly.

Bytecode text artifacts and Rust VM execution formats should remain unchanged except for new fixture coverage that exercises existing runtime behavior through a forwarding module.

## Documentation

Update `README.md` to document:

- `export name[, name...] from "path";` syntax;
- forward-only behavior;
- direct and namespace imports of a barrel module;
- no renaming, wildcard exports, package search paths, or separate compilation.

Update `docs/language-grammar.ebnf` to add the re-export form.

Update `docs/roadmap.md` after implementation by removing Phase 14E from active future work and moving the immediate order to search paths.

## Tests

Add successful golden fixtures for:

1. Direct import through a barrel module that re-exports a value and function.
2. Direct import through a barrel module that re-exports a struct and its method metadata.
3. Namespace import through a barrel module, including `alias.Type` construction and method call.
4. Multi-hop re-export, such as `api.cd -> domain.cd -> shapes.cd`.

Add type-error fixtures for:

1. Re-exported name is not local inside the forwarding module.
2. Re-exported name is missing from the target module.
3. Duplicate local export and re-export of the same name.
4. Duplicate re-export from two target modules.

Add import-error or parse-error fixtures for:

1. Re-export missing source file.
2. Re-export cycle.
3. Malformed re-export source clause.

Run golden tests, bytecode artifact tests, Rust VM parity tests, and focused parser/import-error coverage.

## Open Questions Resolved

- Syntax: `export name[, name...] from "path";`.
- Scope behavior: forward-only, no local binding.
- Conflict behavior: duplicate public export is an error.
- Renaming: not supported in this slice.
