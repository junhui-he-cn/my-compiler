# Module Symbols Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract module-level symbol table storage from `TypeChecker` into a focused `ModuleSymbols` subsystem without changing module/import/export behavior.

**Architecture:** Introduce shared data-only type-checker symbol structs, then add `ModuleSymbols` to own module exports, namespace imports, local struct markers, and direct-import dedup state. `TypeChecker` keeps AST traversal, scope/type policy, diagnostics, `checkedModules_`, `moduleStack_`, active `structTypes_`, and `methods_`; it delegates only module table storage and lookup to `ModuleSymbols`.

**Tech Stack:** C++17, CMake, existing `TypeUtils`/`Token` types, C++ assertion tests, Python golden tests, Rust VM integration tests.

---

## File Structure

- Create `include/TypeCheckerTypes.hpp`: shared data-only `TypeBinding`, `StructFieldType`, and `StructTypeDecl` structs.
- Create `include/ModuleSymbols.hpp`: public `NamespaceImport`, module export table aliases, and `ModuleSymbols` API.
- Create `src/ModuleSymbols.cpp`: storage implementation for module symbols.
- Create `tests/module_symbols_tests.cpp`: focused assertions for `ModuleSymbols` behavior.
- Modify `CMakeLists.txt`: compile `src/ModuleSymbols.cpp` into `compiler_design`; add `module_symbols_tests` CTest target.
- Modify `include/TypeChecker.hpp`: include `ModuleSymbols.hpp`, replace private data-only structs with aliases, remove module table members, and add `ModuleSymbols moduleSymbols_`.
- Modify `src/TypeChecker.cpp`: delegate module exports/imports/namespaces/local-struct markers/direct-import dedup to `moduleSymbols_`.

---

### Task 1: Add ModuleSymbols tests before implementation

**Files:**
- Create: `tests/module_symbols_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Confirm a clean workspace**

Run:

```sh
git status --short
```

Expected: no output.

- [ ] **Step 2: Add the focused ModuleSymbols C++ test**

Create `tests/module_symbols_tests.cpp` with this exact content:

```cpp
#include "ModuleSymbols.hpp"
#include "Token.hpp"
#include "TypeUtils.hpp"

#include <cassert>
#include <string>
#include <utility>

namespace {

Token token(std::string lexeme)
{
    return Token{TokenType::Identifier, std::move(lexeme), 1, 1};
}

TypeBinding binding(std::string resolvedName, StaticType kind)
{
    TypeBinding result;
    result.type = simpleType(kind);
    result.resolvedName = std::move(resolvedName);
    return result;
}

StructTypeDecl structDecl(std::string name)
{
    return StructTypeDecl{token(std::move(name)), {StructFieldType{token("field"), simpleType(StaticType::Number)}}};
}

void test_direct_import_deduplicates_per_importing_module()
{
    ModuleSymbols symbols;

    assert(symbols.markDirectImport(1, 2));
    assert(!symbols.markDirectImport(1, 2));
    assert(symbols.markDirectImport(1, 3));
    assert(symbols.markDirectImport(2, 2));
}

void test_value_exports_are_recorded_and_missing_modules_return_null()
{
    ModuleSymbols symbols;

    assert(symbols.valueExports(7) == nullptr);
    symbols.recordValueExport(7, "answer", binding("answer#0", StaticType::Number));

    const ModuleValueExports* exports = symbols.valueExports(7);
    assert(exports != nullptr);
    assert(exports->size() == 1);
    assert(exports->at("answer").resolvedName == "answer#0");
    assert(exports->at("answer").type.kind == StaticType::Number);
}

void test_struct_exports_and_local_struct_markers_are_independent()
{
    ModuleSymbols symbols;

    assert(!symbols.isLocalStruct(4, "Person"));
    symbols.markLocalStruct(4, "Person");
    assert(symbols.isLocalStruct(4, "Person"));
    assert(symbols.structExports(4) == nullptr);

    symbols.recordStructExport(4, "Person", structDecl("Person"));
    const ModuleStructExports* exports = symbols.structExports(4);
    assert(exports != nullptr);
    assert(exports->size() == 1);
    assert(exports->at("Person").name.lexeme == "Person");
}

void test_namespace_aliases_are_recorded_and_queried()
{
    ModuleSymbols symbols;
    NamespaceImport imported;
    imported.values.emplace("value", binding("value#0", StaticType::String));
    imported.structs.emplace("Person", structDecl("Person"));

    assert(!symbols.hasNamespace(9, "lib"));
    assert(symbols.namespaceImport(9, "lib") == nullptr);

    symbols.recordNamespace(9, "lib", std::move(imported));

    assert(symbols.hasNamespace(9, "lib"));
    const NamespaceImport* found = symbols.namespaceImport(9, "lib");
    assert(found != nullptr);
    assert(found->values.at("value").type.kind == StaticType::String);
    assert(found->structs.at("Person").name.lexeme == "Person");
}

void test_clear_removes_all_tables()
{
    ModuleSymbols symbols;
    symbols.markDirectImport(1, 2);
    symbols.recordValueExport(1, "value", binding("value#0", StaticType::Number));
    symbols.markLocalStruct(1, "Person");
    symbols.recordStructExport(1, "Person", structDecl("Person"));
    NamespaceImport imported;
    imported.values.emplace("value", binding("value#1", StaticType::String));
    symbols.recordNamespace(1, "lib", std::move(imported));

    symbols.clear();

    assert(symbols.markDirectImport(1, 2));
    assert(symbols.valueExports(1) == nullptr);
    assert(!symbols.isLocalStruct(1, "Person"));
    assert(symbols.structExports(1) == nullptr);
    assert(!symbols.hasNamespace(1, "lib"));
    assert(symbols.namespaceImport(1, "lib") == nullptr);
}

} // namespace

int main()
{
    test_direct_import_deduplicates_per_importing_module();
    test_value_exports_are_recorded_and_missing_modules_return_null();
    test_struct_exports_and_local_struct_markers_are_independent();
    test_namespace_aliases_are_recorded_and_queried();
    test_clear_removes_all_tables();
}
```

- [ ] **Step 3: Register the failing test target in CMake**

Add this block after the `flow_facts` CTest registration in `CMakeLists.txt`:

```cmake
add_executable(module_symbols_tests
    tests/module_symbols_tests.cpp
    src/ModuleSymbols.cpp
    src/TypeUtils.cpp
)
target_include_directories(module_symbols_tests PRIVATE include)
add_test(NAME module_symbols COMMAND module_symbols_tests)
```

Do not add `src/ModuleSymbols.cpp` to `compiler_design` yet in this task.

- [ ] **Step 4: Run the focused build to verify RED**

Run:

```sh
cmake -S . -B build
cmake --build build --target module_symbols_tests
```

Expected: build fails because `include/ModuleSymbols.hpp` or `src/ModuleSymbols.cpp` does not exist yet.

---

### Task 2: Implement shared type structs and standalone ModuleSymbols

**Files:**
- Create: `include/TypeCheckerTypes.hpp`
- Create: `include/ModuleSymbols.hpp`
- Create: `src/ModuleSymbols.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/module_symbols_tests.cpp`

- [ ] **Step 1: Add shared type-checker symbol structs**

Create `include/TypeCheckerTypes.hpp` with this exact content:

```cpp
#pragma once

#include "Token.hpp"
#include "TypeUtils.hpp"

#include <cstddef>
#include <string>
#include <vector>

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
```

- [ ] **Step 2: Add the ModuleSymbols public header**

Create `include/ModuleSymbols.hpp` with this exact content:

```cpp
#pragma once

#include "TypeCheckerTypes.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

private:
    std::unordered_map<std::size_t, ModuleValueExports> valueExports_;
    std::unordered_map<std::size_t, ModuleStructExports> structExports_;
    std::unordered_map<std::size_t, std::unordered_set<std::string>> localStructNames_;
    std::unordered_map<std::size_t, std::unordered_map<std::string, NamespaceImport>> namespaces_;
    std::unordered_map<std::size_t, std::unordered_set<std::size_t>> directImports_;
};
```

- [ ] **Step 3: Add the ModuleSymbols implementation**

Create `src/ModuleSymbols.cpp` with this exact content:

```cpp
#include "ModuleSymbols.hpp"

#include <utility>

void ModuleSymbols::clear()
{
    valueExports_.clear();
    structExports_.clear();
    localStructNames_.clear();
    namespaces_.clear();
    directImports_.clear();
}

bool ModuleSymbols::markDirectImport(std::size_t importingModuleId, std::size_t importedModuleId)
{
    return directImports_[importingModuleId].insert(importedModuleId).second;
}

void ModuleSymbols::recordValueExport(std::size_t moduleId, std::string name, TypeBinding binding)
{
    valueExports_[moduleId].emplace(std::move(name), std::move(binding));
}

const ModuleValueExports* ModuleSymbols::valueExports(std::size_t moduleId) const
{
    const auto found = valueExports_.find(moduleId);
    return found == valueExports_.end() ? nullptr : &found->second;
}

void ModuleSymbols::markLocalStruct(std::size_t moduleId, const std::string& name)
{
    localStructNames_[moduleId].insert(name);
}

bool ModuleSymbols::isLocalStruct(std::size_t moduleId, const std::string& name) const
{
    const auto found = localStructNames_.find(moduleId);
    return found != localStructNames_.end() && found->second.find(name) != found->second.end();
}

void ModuleSymbols::recordStructExport(std::size_t moduleId, std::string name, StructTypeDecl declaration)
{
    structExports_[moduleId].emplace(std::move(name), std::move(declaration));
}

const ModuleStructExports* ModuleSymbols::structExports(std::size_t moduleId) const
{
    const auto found = structExports_.find(moduleId);
    return found == structExports_.end() ? nullptr : &found->second;
}

bool ModuleSymbols::hasNamespace(std::size_t moduleId, const std::string& alias) const
{
    return namespaceImport(moduleId, alias) != nullptr;
}

void ModuleSymbols::recordNamespace(std::size_t moduleId, std::string alias, NamespaceImport imported)
{
    namespaces_[moduleId].emplace(std::move(alias), std::move(imported));
}

const NamespaceImport* ModuleSymbols::namespaceImport(std::size_t moduleId, const std::string& alias) const
{
    const auto table = namespaces_.find(moduleId);
    if (table == namespaces_.end()) {
        return nullptr;
    }
    const auto found = table->second.find(alias);
    return found == table->second.end() ? nullptr : &found->second;
}
```

- [ ] **Step 4: Add ModuleSymbols to the main compiler target**

In `CMakeLists.txt`, insert `src/ModuleSymbols.cpp` in the `compiler_design` source list after `src/Lexer.cpp`:

```cmake
    src/Lexer.cpp
    src/ModuleSymbols.cpp
    src/NativeStdlib.cpp
```

- [ ] **Step 5: Run the focused ModuleSymbols test**

Run:

```sh
cmake -S . -B build
cmake --build build --target module_symbols_tests
ctest --test-dir build --output-on-failure -R '^module_symbols$'
```

Expected: build succeeds and CTest reports `module_symbols` passed.

- [ ] **Step 6: Commit the standalone component**

Run:

```sh
git add CMakeLists.txt include/TypeCheckerTypes.hpp include/ModuleSymbols.hpp src/ModuleSymbols.cpp tests/module_symbols_tests.cpp
git commit -m "refactor: add module symbols table"
```

Expected: commit succeeds.

---

### Task 3: Move shared type structs out of TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: build

- [ ] **Step 1: Include ModuleSymbols from TypeChecker**

In `include/TypeChecker.hpp`, add this include near the other project includes:

```cpp
#include "ModuleSymbols.hpp"
```

- [ ] **Step 2: Replace private data-only structs with aliases**

In `include/TypeChecker.hpp`, delete the private nested `Binding`, `StructFieldType`, and `StructTypeDecl` struct definitions.

Add these aliases in the same private area before `FunctionReturnContext`:

```cpp
    using Binding = TypeBinding;
    using StructFieldType = ::StructFieldType;
    using StructTypeDecl = ::StructTypeDecl;
```

Keep `FunctionReturnContext`, `MethodInfo`, and `IndexTargetTypes` private in `TypeChecker`.

- [ ] **Step 3: Remove module table type aliases from TypeChecker**

In `include/TypeChecker.hpp`, delete these aliases and nested struct:

```cpp
    using ExportTable = std::unordered_map<std::string, Binding>;

    struct NamespaceImport {
        ExportTable values;
        std::unordered_map<std::string, StructTypeDecl> structs;
    };

    using NamespaceTable = std::unordered_map<std::string, NamespaceImport>;
```

Do not remove `using Scope` or `using MethodTable`.

- [ ] **Step 4: Remove `currentNamespaceTable` declaration**

Delete this private method declaration from `include/TypeChecker.hpp`:

```cpp
    NamespaceTable& currentNamespaceTable();
```

Keep these declarations:

```cpp
    const NamespaceImport* findNamespace(const std::string& alias) const;
    void declareNamespaceAlias(const ImportStmt& statement, NamespaceImport imported);
```

They now refer to the `NamespaceImport` type from `ModuleSymbols.hpp`.

- [ ] **Step 5: Build to catch type alias mistakes**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design
```

Expected: build may still fail if `TypeChecker.cpp` references removed nested `NamespaceTable` or module table members; the next task fixes those references. If it succeeds, continue anyway.

Do not commit this task yet unless the build succeeds without Task 4 changes. The expected clean commit is after TypeChecker integration.

---

### Task 4: Integrate ModuleSymbols into TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: focused module/import golden and VM checks

- [ ] **Step 1: Replace module table members with ModuleSymbols**

In `include/TypeChecker.hpp`, delete these members:

```cpp
    std::unordered_map<std::size_t, ExportTable> moduleExports_;
    std::unordered_map<std::size_t, std::unordered_map<std::string, StructTypeDecl>> moduleStructExports_;
    std::unordered_map<std::size_t, std::unordered_set<std::string>> moduleLocalStructNames_;
    std::unordered_map<std::size_t, NamespaceTable> moduleNamespaces_;
    std::unordered_map<std::size_t, std::unordered_set<std::size_t>> moduleImportedModules_;
```

Add this member in their place:

```cpp
    ModuleSymbols moduleSymbols_;
```

- [ ] **Step 2: Clear module symbols at the start of each check**

In `src/TypeChecker.cpp`, inside `TypeChecker::check`, replace these lines:

```cpp
    moduleExports_.clear();
    moduleStructExports_.clear();
    moduleLocalStructNames_.clear();
    moduleNamespaces_.clear();
    moduleImportedModules_.clear();
```

with:

```cpp
    moduleSymbols_.clear();
```

- [ ] **Step 3: Remove `currentNamespaceTable` implementation and update `findNamespace`**

Delete the full `TypeChecker::currentNamespaceTable` implementation from `src/TypeChecker.cpp`.

Replace `TypeChecker::findNamespace` with:

```cpp
const NamespaceImport* TypeChecker::findNamespace(const std::string& alias) const
{
    if (moduleStack_.empty()) {
        return nullptr;
    }
    return moduleSymbols_.namespaceImport(moduleStack_.back(), alias);
}
```

- [ ] **Step 4: Update namespace alias recording**

In `TypeChecker::declareNamespaceAlias`, replace:

```cpp
    NamespaceTable& namespaces = currentNamespaceTable();
    if (namespaces.find(alias.lexeme) != namespaces.end()
```

with:

```cpp
    if (moduleStack_.empty()) {
        throw TypeError(statement.keyword, "namespace imports require a module context");
    }

    const std::size_t moduleId = moduleStack_.back();
    if (moduleSymbols_.hasNamespace(moduleId, alias.lexeme)
```

At the end of the function, replace:

```cpp
    namespaces.emplace(alias.lexeme, std::move(imported));
```

with:

```cpp
    moduleSymbols_.recordNamespace(moduleId, alias.lexeme, std::move(imported));
```

- [ ] **Step 5: Update direct-import dedup in `checkImport`**

In `TypeChecker::checkImport`, replace the direct import dedup block:

```cpp
    if (!statement.alias && moduleImportedModules_[currentModuleId].find(statement.resolvedModuleId)
        != moduleImportedModules_[currentModuleId].end()) {
        return;
    }
    if (!statement.alias) {
        moduleImportedModules_[currentModuleId].insert(statement.resolvedModuleId);
    }
```

with:

```cpp
    if (!statement.alias && !moduleSymbols_.markDirectImport(currentModuleId, statement.resolvedModuleId)) {
        return;
    }
```

- [ ] **Step 6: Update namespace import export copying**

In the `if (statement.alias)` branch of `checkImport`, replace module export map lookups with:

```cpp
        NamespaceImport namespaceImport;
        if (const ModuleValueExports* exports = moduleSymbols_.valueExports(imported->moduleId)) {
            namespaceImport.values = *exports;
        }
        if (const ModuleStructExports* structExports = moduleSymbols_.structExports(imported->moduleId)) {
            namespaceImport.structs = *structExports;
        }
        declareNamespaceAlias(statement, std::move(namespaceImport));
        return;
```

- [ ] **Step 7: Update direct import export iteration**

In `checkImport`, replace direct value export iteration with:

```cpp
    if (const ModuleValueExports* exports = moduleSymbols_.valueExports(imported->moduleId)) {
        for (const auto& entry : *exports) {
            Token name{TokenType::Identifier, entry.first, statement.keyword.line, statement.keyword.column};
            declareImportedVariable(name, entry.second);
        }
    }
```

Replace direct struct export iteration with:

```cpp
    if (const ModuleStructExports* structExports = moduleSymbols_.structExports(imported->moduleId)) {
        for (const auto& entry : *structExports) {
            if (structTypes_.find(entry.first) != structTypes_.end()) {
                Token name{TokenType::Identifier, entry.first, statement.keyword.line, statement.keyword.column};
                throw TypeError(name, "duplicate struct `" + entry.first + "`");
            }
            structTypes_.emplace(entry.first, entry.second);
        }
    }
```

- [ ] **Step 8: Update exports to record through ModuleSymbols**

In `TypeChecker::checkExport`, replace:

```cpp
                    moduleExports_[moduleId].emplace(name.lexeme, *binding);
```

with:

```cpp
                    moduleSymbols_.recordValueExport(moduleId, name.lexeme, *binding);
```

Replace the local struct/export block:

```cpp
            const auto localStructs = moduleLocalStructNames_.find(moduleId);
            if (localStructs != moduleLocalStructNames_.end()
                && localStructs->second.find(name.lexeme) != localStructs->second.end()) {
                if (const StructTypeDecl* structType = findStructType(name.lexeme)) {
                    moduleStructExports_[moduleId].emplace(name.lexeme, *structType);
                    exported = true;
                }
            }
```

with:

```cpp
            if (moduleSymbols_.isLocalStruct(moduleId, name.lexeme)) {
                if (const StructTypeDecl* structType = findStructType(name.lexeme)) {
                    moduleSymbols_.recordStructExport(moduleId, name.lexeme, *structType);
                    exported = true;
                }
            }
```

- [ ] **Step 9: Update local struct markers**

In `TypeChecker::checkStructDeclaration`, replace:

```cpp
        moduleLocalStructNames_[moduleStack_.back()].insert(statement.name.lexeme);
```

with:

```cpp
        moduleSymbols_.markLocalStruct(moduleStack_.back(), statement.name.lexeme);
```

- [ ] **Step 10: Run focused module checks**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design module_symbols_tests
ctest --test-dir build --output-on-failure -R '^module_symbols$'
python3 tests/run_golden_tests.py ./build/compiler_design --case module_exports
python3 tests/run_golden_tests.py ./build/compiler_design --case namespace_imports
python3 tests/run_golden_tests.py ./build/compiler_design --case import
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case module_exports
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case namespace_imports
```

Expected: all commands pass and no golden files change.

- [ ] **Step 11: Check old TypeChecker module table state is gone**

Run:

```sh
grep -R -n "moduleExports_\|moduleStructExports_\|moduleLocalStructNames_\|moduleNamespaces_\|moduleImportedModules_\|currentNamespaceTable\|TypeChecker::Namespace" include/TypeChecker.hpp src/TypeChecker.cpp
```

Expected: no output.

- [ ] **Step 12: Commit TypeChecker integration**

Run:

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "refactor: delegate module symbols from type checker"
```

Expected: commit succeeds.

---

### Task 5: Full verification and cleanup

**Files:**
- Verify: whole repository
- Clean: `tests/__pycache__/`

- [ ] **Step 1: Run the full verification suite**

Run:

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

Expected: every command exits 0.

- [ ] **Step 2: Confirm no accidental files remain**

Run:

```sh
git status --short
```

Expected: no output. If source files remain modified, inspect and commit them. If golden files changed, treat them as regressions unless explicitly required by the implementation.

- [ ] **Step 3: Report final verification evidence**

In the final response, list each verification command and observed pass counts. Do not claim completion without this fresh evidence.

---

## Self-Review Notes

- Spec coverage: Tasks 1-2 add the standalone subsystem and tests; Tasks 3-4 move shared data structs and delegate all module table state from `TypeChecker`; Task 5 runs the required full verification suite.
- Red-flag scan: every task names exact files, code snippets, commands, and expected outcomes; no deferred work remains.
- Type consistency: the plan consistently uses `TypeBinding`, `StructFieldType`, `StructTypeDecl`, `ModuleValueExports`, `ModuleStructExports`, `NamespaceImport`, `ModuleSymbols::markDirectImport`, and `ModuleSymbols moduleSymbols_`.
