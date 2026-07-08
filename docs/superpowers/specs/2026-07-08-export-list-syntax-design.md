# Export List Syntax Design

## Goal

Change module exports from declaration-wrapping syntax to standalone export lists. A module should define names normally, then export one or more already-defined top-level names from a separate `export` declaration.

## Non-goals

- No namespace import syntax.
- No re-export syntax.
- No export aliases or renames.
- No wildcard exports.
- No separate compilation or runtime module opcode changes.
- No trailing commas in export lists.

## Syntax

The new syntax is:

```cd
let value = 1;
fun add(a, b) { return a + b; }
struct Point { x: number, y: number }

export value, add, Point;
```

Grammar:

```ebnf
exportDecl = "export", identifier, { ",", identifier }, ";" ;
```

A single-name export remains valid:

```cd
export value;
```

The old declaration-wrapping forms become invalid:

```cd
export let value = 1;
export fun add(a, b) { return a + b; }
export struct Point { x: number, y: number }
```

Trailing commas are invalid:

```cd
export value,;
export value, add,;
```

## Semantics

`export` is still allowed only at module/file top level. Each exported identifier must resolve to a name already defined in the current module top-level scope. Valid export targets are:

- `let` bindings
- named `fun` declarations
- named `struct` types

An export declaration does not define a new value and does not emit IR or bytecode. It only marks existing module-local declarations as visible to importing modules.

If an export references an undefined name, type checking reports a type error at that identifier. If a list contains multiple invalid names, this slice may continue using the compiler's current first-error behavior.

## AST and Parser

`ExportStmt` should store a list of exported name tokens instead of wrapping a declaration statement. The parser should parse `export` followed by one or more identifiers separated by commas and terminated by `;`.

AST output should represent the list directly, for example compact output like:

```text
(export value add Point)
```

and tree output should show one `Export` node with child/name entries or a concise `Export value, add, Point` line consistent with existing AST style.

## Type Checking

The type checker should process normal declarations as before. When it sees an `ExportStmt`, it should look up each exported name in the current module's top-level value bindings and struct type table:

- value/function bindings are added to the module export binding table;
- struct types are added to the module struct export table.

The export should not introduce a binding into the current scope. Duplicate exported names in one or more export declarations can be idempotent because exporting the same existing declaration twice does not change module behavior.

## IR, Bytecode, and VM

The IR compiler should skip standalone export declarations. Since the exported declarations are now compiled from their original `let`, `fun`, or `struct` declarations, no unwrap behavior is needed. Bytecode lowering and Rust VM execution should not need semantic changes, but fixtures and artifacts that use module exports must be refreshed to the new source syntax.

## Tests

Update or add golden coverage for:

- successful single-name export;
- successful multi-name export containing a variable, function, and struct;
- imported private helper remains hidden;
- duplicate imported export-name conflict still reports a type error;
- `export let ...`, `export fun ...`, and `export struct ...` are parse errors;
- nested `export name;` remains a parse error;
- `export missing;` is a type error;
- trailing comma forms are parse errors;
- bytecode artifact and Rust VM parity fixtures using exports.

## Documentation Updates

Update these documents to match the new syntax and semantics:

- `docs/language-grammar.ebnf`
- `README.md`
- `docs/roadmap.md`
- repository agent notes if project memory mentions `export let`, `export fun`, or `export struct`

## Migration

Existing examples and fixtures using declaration-wrapping export syntax should be rewritten by moving the declaration out of the export line and adding an export list after the declarations, for example:

```cd
let value = 7;
fun answer() { return value; }
export value, answer;
```
