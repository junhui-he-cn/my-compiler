# Module Symbols Extraction Design

## Goal

Extract module-level symbol table state from `TypeChecker` into a focused `ModuleSymbols` subsystem. This is an M0 front-end stabilization slice and is intentionally behavior-preserving: imports, exports, namespace aliases, diagnostics, AST/IR/bytecode output, and Rust VM execution should not change.

The immediate benefit is reducing `TypeChecker`'s module bookkeeping surface. The follow-on benefit is giving future module work, such as imported/namespaced struct methods, re-exports, and search paths, a clearer home.

## Non-Goals

This slice does not implement new module behavior. In particular, it does not add:

- methods on imported or namespaced structs;
- method metadata export/import;
- re-exports;
- package search paths;
- separate compilation;
- parser, AST, IR, bytecode, or Rust VM semantic changes;
- a visitor rewrite of `TypeChecker`.

## Current Behavior to Preserve

The current type checker maintains module-related state directly:

- exported top-level values per module;
- exported local struct declarations per module;
- local struct names per module, so `export Name;` can distinguish local structs from imported structs;
- namespace imports per importing module;
- direct-import deduplication per importing module.

Current user-visible behavior must remain unchanged:

- direct imports expose exported values and structs into the importing module's top-level scope;
- namespace imports expose exported values and structs through `alias.member` and `alias.Type`;
- namespace aliases conflict with existing values and structs in the current module scope;
- duplicate direct imports of the same module are ignored;
- exported names must refer to already-defined top-level values or local structs;
- imported/private module scoping and diagnostics remain unchanged.

## Proposed Architecture

Add a new component:

- `include/ModuleSymbols.hpp`
- `src/ModuleSymbols.cpp`

The component owns module-symbol storage and lookup. `TypeChecker` remains the orchestrator for AST traversal, local scopes, type checks, diagnostics, and deciding when a declaration should be recorded.

A likely public shape is:

```cpp
struct TypeBinding {
    TypeInfo type;
    std::string resolvedName;
    std::size_t scopeDepth = 0;
    std::size_t functionDepth = 0;
    bool explicitType = false;
    bool imported = false;
};

struct StructFieldType {
    Token name;
    TypeInfo type;
};

struct StructTypeDecl {
    Token name;
    std::vector<StructFieldType> fields;
};

using ModuleValueExports = std::unordered_map<std::string, TypeBinding>;
using ModuleStructExports = std::unordered_map<std::string, StructTypeDecl>;

struct NamespaceImport {
    ModuleValueExports values;
    ModuleStructExports structs;
};

class ModuleSymbols {
public:
    void clear();

    bool markDirectImport(std::size_t importingModuleId, std::size_t importedModuleId);

    void recordValueExport(std::size_t moduleId, std::string name, TypeBinding binding);
    const ModuleValueExports* valueExports(std::size_t moduleId) const;

    void markLocalStruct(std::size_t moduleId, const std::string& name);
    bool isLocalStruct(std::size_t moduleId, const std::string& name) const;
    void recordStructExport(std::size_t moduleId, std::string name, StructTypeDecl declaration);
    const ModuleStructExports* structExports(std::size_t moduleId) const;

    bool hasNamespace(std::size_t moduleId, const std::string& alias) const;
    void recordNamespace(std::size_t moduleId, std::string alias, NamespaceImport imported);
    const NamespaceImport* namespaceImport(std::size_t moduleId, const std::string& alias) const;
};
```

Names may change during implementation if a clearer naming convention emerges, but the boundary should remain the same: module-symbol storage in `ModuleSymbols`, type-checker policy and diagnostics in `TypeChecker`.

## Shared Type Boundary

`ModuleSymbols` needs to store value bindings and struct shapes that are currently private nested `TypeChecker` structs. To avoid making `ModuleSymbols` depend on `TypeChecker`, move these data-only structs to a shared header:

- `TypeBinding` replaces `TypeChecker::Binding` for local and module value bindings.
- `StructFieldType` and `StructTypeDecl` become shared data-only structs.

`TypeChecker` may keep an internal alias such as `using Binding = TypeBinding;` if that keeps the migration small. Method metadata (`MethodInfo`) stays private to `TypeChecker` in this slice because method export/import is not implemented yet.

## Data Flow

`TypeChecker::check` clears `moduleSymbols_` with other per-run state.

`TypeChecker::checkImport` will:

1. ask `moduleSymbols_.markDirectImport(currentModuleId, resolvedModuleId)` whether a direct import is new;
2. check the imported module as it does today;
3. for namespace imports, build a `NamespaceImport` from `moduleSymbols_` export lookups and ask `declareNamespaceAlias` to record it;
4. for direct imports, iterate exported values and structs from `moduleSymbols_` and inject them into the current top-level scope or `structTypes_` exactly as today.

`TypeChecker::checkExport` will:

1. validate export eligibility using existing scope and struct lookup rules;
2. call `moduleSymbols_.recordValueExport(...)` for eligible values;
3. call `moduleSymbols_.recordStructExport(...)` for eligible local structs.

`TypeChecker::checkStructDeclaration` will record local module struct names through `moduleSymbols_.markLocalStruct(...)` when inside a module.

Qualified lookups (`alias.value`, `alias.Type`, namespaced constructor expressions, and qualified type annotations) will use a `TypeChecker::findNamespace` helper backed by `moduleSymbols_`.

## Error Handling

This refactor should not introduce new diagnostics. `ModuleSymbols` should not throw language diagnostics for user programs; it returns facts and status. `TypeChecker` remains responsible for all `TypeError` messages and source tokens, including namespace conflicts, duplicate structs, missing namespace members, and invalid exports.

Internal misuse, such as trying to access current module state without a module id, should either remain guarded in `TypeChecker` or use straightforward logic errors in tests. Avoid moving user-facing diagnostic construction into `ModuleSymbols`.

## Testing Strategy

Add focused C++ unit tests for `ModuleSymbols`:

- `clear()` removes exports, namespaces, local structs, and import tracking;
- `markDirectImport` returns true for the first direct import and false for duplicates;
- value exports can be recorded and queried without creating tables for missing modules;
- struct exports and local struct markers are independent;
- namespace aliases can be recorded and queried, and missing aliases return null/false.

Behavior preservation is verified by the existing full suite:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

No golden updates are expected. If any golden output changes, treat it as a regression unless explicitly re-scoped.

## Implementation Notes

- Move storage and simple table operations first; avoid opportunistic changes to import semantics.
- Keep `checkedModules_` and `moduleStack_` in `TypeChecker` for now because they control type-check traversal, not symbol storage.
- Keep `structTypes_` and `methods_` in `TypeChecker` for now because they are the active type-checking environment for the module currently being checked.
- Keep duplicate/conflict diagnostics in `TypeChecker` so messages and token locations stay stable.
- Add `src/ModuleSymbols.cpp` to `compiler_design` and a focused `module_symbols_tests` CTest target.

## Success Criteria

- `TypeChecker` no longer owns `moduleExports_`, `moduleStructExports_`, `moduleLocalStructNames_`, `moduleNamespaces_`, or `moduleImportedModules_` directly.
- `ModuleSymbols` owns those tables behind a small query/update API.
- Existing module import/export/namespace behavior is unchanged.
- Existing diagnostics and goldens are unchanged.
- Full verification passes and the working tree is clean after removing generated caches.
