# Module Interface Metadata Design

## Goal

Add Phase 14G module-interface metadata as a stable, debug-friendly frontend
output. The feature should let users and tests inspect the public API that the
compiler has inferred for each loaded module.

This phase intentionally stops short of separate compilation. It does not add a
linker, persistent interface artifacts, module versioning policy, source hashes,
IR summaries, bytecode summaries, or runtime module objects.

## User-Facing CLI

Add a new frontend mode:

```sh
compiler_design --module-interface main.cd
```

The mode parses imports, performs normal type checking, and prints a stable text
representation of each loaded module's exported interface. It does not execute
the program and does not emit IR or bytecode.

`--module-interface` should work with existing source-loading behavior,
including:

- direct file inputs;
- normal imports and re-exports;
- namespace imports;
- `-I DIR` and `--import-path DIR`;
- file-aware import, parse, and type diagnostics.

If import loading, parsing, or type checking fails, the CLI should report the
existing diagnostic and exit with the same kind of failure behavior as other
frontend-only modes.

## Interface Scope

The interface is the module's public static API after type checking.

Include:

- module id;
- module display path;
- entry-module marker;
- directly exported values and their static types;
- exported named structs and their fields;
- exported struct methods and their signatures;
- exports made available through re-export forwarding, represented as ordinary
  final exports of the re-exporting module.

Exclude:

- private top-level declarations;
- namespace aliases as exported names;
- import/re-export dependency edges;
- function bodies;
- closure captures;
- local variables;
- resolved lowering names in the default text output;
- IR, bytecode, and runtime implementation details;
- source hashes, format versions, compatibility ranges, and `.cdmi` artifacts.

## Architecture

Add a small module-interface layer:

- `include/ModuleInterface.hpp`
- `include/ModuleInterfaceEmitter.hpp`
- `src/ModuleInterfaceEmitter.cpp`

The data model should be independent from mutable type-checker state. A likely
shape is:

```cpp
struct ModuleInterfaceValue {
    std::string name;
    TypeInfo type;
};

struct ModuleInterfaceField {
    std::string name;
    TypeInfo type;
};

struct ModuleInterfaceMethod {
    std::string name;
    std::vector<TypeInfo> parameterTypes;
    TypeInfo returnType;
};

struct ModuleInterfaceStruct {
    std::string name;
    std::vector<ModuleInterfaceField> fields;
    std::vector<ModuleInterfaceMethod> methods;
};

struct ModuleInterface {
    std::size_t moduleId = 0;
    std::string path;
    bool isEntry = false;
    std::vector<ModuleInterfaceValue> values;
    std::vector<ModuleInterfaceStruct> structs;
};
```

Exact names may change during implementation, but the boundary should stay the
same: a read-only interface snapshot feeds the emitter; the emitter does not
walk `TypeChecker` internals directly.

### Data Flow

`TypeChecker` already records exported values, structs, and methods in
`ModuleSymbols`. After a successful type-check pass, expose a snapshot operation
that builds `std::vector<ModuleInterface>` for all loaded modules.

The snapshot should:

1. iterate the program's `ModuleStmt` nodes to get module id, path, and entry
   marker;
2. look up value, struct, and method exports in `ModuleSymbols`;
3. copy the public export data into module-interface structs;
4. preserve struct field declaration order;
5. sort values, structs, and methods for stable output.

The snapshot should copy data instead of returning references to mutable
`ModuleSymbols` storage. This keeps `ModuleSymbols` as an internal bookkeeping
component and leaves future separate-compilation work free to change internal
storage without changing the interface emitter.

### Type Formatting

Use the existing `typeInfoName(TypeInfo)` helper for all type text. This keeps
module-interface output aligned with existing diagnostics and annotations,
including:

- primitive types such as `number`, `bool`, `string`, and `nil`;
- unknown dynamic types as `unknown`;
- named structs;
- arrays such as `[number?]`;
- nullable types such as `Point?`;
- function types such as `fun(number): string`.

Method output should use the user-visible parameter list and return type. The
implicit receiver should not be printed as an explicit first parameter.

## Text Format

Use a simple stable text format rather than JSON. This keeps it consistent with
the existing AST, IR, and bytecode debug outputs and easy to cover with golden
tests.

Example:

```text
module 0 entry "main.cd"
  export value answer: number
  export struct Point
    field x: number
    field y: number
    method length(): number

module 1 "lib/math.cd"
  export value add: fun(number, number): number
```

Sorting and layout rules:

- modules print in ascending module id order;
- values print by name in lexicographic order;
- structs print by name in lexicographic order;
- fields print in declaration order;
- methods print by name in lexicographic order;
- modules with no exports still print their module header;
- blank lines separate modules;
- paths use the same stable display path stored on `ModuleStmt`;
- entry modules include the `entry` marker after the module id.

The default text output should not print internal resolved names. If future
debugging needs them, add an explicit later flag rather than expanding this
stable format now.

## Error Handling

`--module-interface` should not introduce a new diagnostic category.

The mode can fail with existing diagnostics from:

- import loading;
- lexing;
- parsing;
- type checking.

The module-interface builder and emitter should assume they receive a
successfully checked program. They should not create user-facing language
diagnostics. Internal consistency problems can use normal C++ assertions or
logic errors if needed, but the expected implementation path should not require
them.

## Tests

Add focused C++ coverage for the emitter or snapshot/emitter boundary:

- stable sorting of modules, values, structs, and methods;
- declaration-order preservation for fields;
- formatting of primitive, nullable, array, function, and named struct types;
- modules without exports.

Add CLI/golden coverage for:

- a single-file module exporting values, a struct, and a method;
- a multi-file direct import where only exported API appears;
- namespace imports not exporting the namespace alias itself;
- re-export forwarding appearing as final exports of the re-exporting module;
- import search paths working under `--module-interface`.

No IR, bytecode, or Rust VM behavior should change. Existing run, bytecode
artifact, and Rust VM parity tests should continue to pass without golden
updates, except for any new `--module-interface` fixtures added for this mode.

## Documentation

Update `README.md` to document `--module-interface` as a debug/introspection
mode.

Do not update `docs/language-grammar.ebnf` for this phase because no syntax or
grammar changes are introduced.

## Success Criteria

- `compiler_design --module-interface ...` prints stable module-interface text
  after successful frontend checking.
- The output includes only public exported API metadata for each loaded module.
- Re-exported names appear as ordinary final exports of the re-exporting module.
- Namespace aliases and private declarations do not appear as exports.
- Import search paths are honored.
- No linker, separate compilation, `.cdmi` artifact, source hash, format version,
  IR summary, or bytecode summary is introduced.
- New focused tests and CLI fixtures cover the mode.
- The existing full verification suite continues to pass.
