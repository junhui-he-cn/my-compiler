# Imported Struct Methods Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Support statically resolved method calls on exported/imported named structs, including namespaced struct receiver types.

**Architecture:** Add data-only method signature exports to `ModuleSymbols`, then have `TypeChecker` snapshot local struct method signatures when a struct is exported and import them for direct and namespace imports. Keep method lowering unchanged: member calls continue to load the hidden resolved method function and pass the receiver as implicit `this`.

**Tech Stack:** C++17, CMake, recursive-descent compiler front end, existing golden tests, `.cdbc` bytecode path, Rust VM parity tests.

---

## File Structure

- Modify `include/TypeCheckerTypes.hpp`: add shared `MethodSignature` data type.
- Modify `include/ModuleSymbols.hpp` and `src/ModuleSymbols.cpp`: add module method export tables and namespace method metadata storage.
- Modify `tests/module_symbols_tests.cpp`: add focused method export and namespace method storage assertions.
- Modify `include/TypeChecker.hpp` and `src/TypeChecker.cpp`: convert between private `MethodInfo` and shared `MethodSignature`, export/import method signatures, and qualify namespaced method signature types.
- Add success fixtures under `tests/golden/` for direct import, namespace import, mutation, and namespaced struct-typed signatures.
- Add type-error fixtures under `tests/golden/type_errors/` for wrong argument type and unknown namespaced method.
- Modify `README.md`: update struct/module docs to say imported and namespaced struct methods are supported.

---

### Task 1: Add failing tests for method exports and imported method calls

**Files:**
- Modify: `tests/module_symbols_tests.cpp`
- Create: `tests/golden/import_struct_method_direct/input.cd`
- Create: `tests/golden/import_struct_method_direct/lib.cd`
- Create: `tests/golden/import_struct_method_direct/run.out`
- Create: `tests/golden/import_struct_method_namespace/input.cd`
- Create: `tests/golden/import_struct_method_namespace/lib.cd`
- Create: `tests/golden/import_struct_method_namespace/run.out`
- Create: `tests/golden/import_struct_method_mutation/input.cd`
- Create: `tests/golden/import_struct_method_mutation/lib.cd`
- Create: `tests/golden/import_struct_method_mutation/run.out`
- Create: `tests/golden/import_struct_method_namespaced_signature/input.cd`
- Create: `tests/golden/import_struct_method_namespaced_signature/lib.cd`
- Create: `tests/golden/import_struct_method_namespaced_signature/run.out`
- Create: `tests/golden/type_errors/import_struct_method_wrong_argument.cd`
- Create: `tests/golden/type_errors/import_struct_method_wrong_argument.err`
- Create: `tests/golden/type_errors/import_struct_method_wrong_argument.exit`
- Create: `tests/golden/type_errors/import_struct_method_namespace_unknown.cd`
- Create: `tests/golden/type_errors/import_struct_method_namespace_unknown.err`
- Create: `tests/golden/type_errors/import_struct_method_namespace_unknown.exit`

- [ ] **Step 1: Confirm workspace state**

Run:

```sh
git status --short
```

Expected: no output.

- [ ] **Step 2: Add ModuleSymbols method export assertions**

In `tests/module_symbols_tests.cpp`, add this helper after `structDecl`:

```cpp
MethodSignature methodSignature(std::string resolvedName, TypeInfo returnType)
{
    MethodSignature result;
    result.receiverType = namedStructType("Person");
    result.parameterTypes = {simpleType(StaticType::Number)};
    result.returnType = std::move(returnType);
    result.resolvedName = std::move(resolvedName);
    return result;
}
```

Add this test before `test_clear_removes_all_tables`:

```cpp
void test_method_exports_are_recorded_with_struct_names()
{
    ModuleSymbols symbols;

    assert(symbols.methodExports(3) == nullptr);
    symbols.recordMethodExport(3, "Person", "ageNext", methodSignature("__method_Person_ageNext#0", simpleType(StaticType::Number)));

    const ModuleMethodExports* exports = symbols.methodExports(3);
    assert(exports != nullptr);
    assert(exports->size() == 1);
    assert(exports->at("Person").size() == 1);
    assert(exports->at("Person").at("ageNext").resolvedName == "__method_Person_ageNext#0");
    assert(exports->at("Person").at("ageNext").returnType.kind == StaticType::Number);
}
```

In `test_namespace_aliases_are_recorded_and_queried`, add method metadata to `imported` before `recordNamespace`:

```cpp
    imported.methods["Person"].emplace("ageNext", methodSignature("__method_Person_ageNext#0", simpleType(StaticType::Number)));
```

Then add these assertions after the existing struct assertion:

```cpp
    assert(found->methods.at("Person").at("ageNext").resolvedName == "__method_Person_ageNext#0");
    assert(found->methods.at("Person").at("ageNext").returnType.kind == StaticType::Number);
```

In `test_clear_removes_all_tables`, add this before `symbols.clear();`:

```cpp
    symbols.recordMethodExport(1, "Person", "ageNext", methodSignature("__method_Person_ageNext#0", simpleType(StaticType::Number)));
```

Add this assertion after `assert(symbols.structExports(1) == nullptr);`:

```cpp
    assert(symbols.methodExports(1) == nullptr);
```

Add this call in `main()` before `test_clear_removes_all_tables();`:

```cpp
    test_method_exports_are_recorded_with_struct_names();
```

- [ ] **Step 3: Add direct import success fixture**

Create `tests/golden/import_struct_method_direct/lib.cd`:

```text
struct Point { x: number, y: number }

impl Point {
  fun sum(): number {
    return this.x + this.y;
  }
}

export Point;
```

Create `tests/golden/import_struct_method_direct/input.cd`:

```text
import "./lib.cd";
let p: Point = Point { x: 3, y: 4 };
print p.sum();
```

Create `tests/golden/import_struct_method_direct/run.out`:

```text
7
```

- [ ] **Step 4: Add namespace import success fixture**

Create `tests/golden/import_struct_method_namespace/lib.cd`:

```text
struct Point { x: number, y: number }

impl Point {
  fun sum(): number {
    return this.x + this.y;
  }
}

export Point;
```

Create `tests/golden/import_struct_method_namespace/input.cd`:

```text
import "./lib.cd" as shapes;
let p: shapes.Point = shapes.Point { x: 5, y: 6 };
print p.sum();
```

Create `tests/golden/import_struct_method_namespace/run.out`:

```text
11
```

- [ ] **Step 5: Add imported mutation success fixture**

Create `tests/golden/import_struct_method_mutation/lib.cd`:

```text
struct Counter { value: number }

impl Counter {
  fun inc(delta: number): number {
    this.value += delta;
    return this.value;
  }
}

export Counter;
```

Create `tests/golden/import_struct_method_mutation/input.cd`:

```text
import "./lib.cd";
let c: Counter = Counter { value: 1 };
let alias = c;
print c.inc(4);
print alias.value;
```

Create `tests/golden/import_struct_method_mutation/run.out`:

```text
5
5
```

- [ ] **Step 6: Add namespaced struct-typed signature success fixture**

Create `tests/golden/import_struct_method_namespaced_signature/lib.cd`:

```text
struct Point { x: number, y: number }
struct Box { point: Point }

impl Box {
  fun replace(point: Point): Point {
    let old: Point = this.point;
    this.point = point;
    return old;
  }
}

export Point, Box;
```

Create `tests/golden/import_struct_method_namespaced_signature/input.cd`:

```text
import "./lib.cd" as shapes;
let box: shapes.Box = shapes.Box { point: shapes.Point { x: 1, y: 2 } };
let old: shapes.Point = box.replace(shapes.Point { x: 3, y: 4 });
print old.x + old.y;
print box.point.x + box.point.y;
```

Create `tests/golden/import_struct_method_namespaced_signature/run.out`:

```text
3
7
```

- [ ] **Step 7: Add imported wrong-argument type error fixture**

Create `tests/golden/type_errors/import_struct_method_wrong_argument.cd`:

```text
import "../import_struct_method_direct/lib.cd";
let p: Point = Point { x: 1, y: 2 };
print p.sum(1);
```

Create `tests/golden/type_errors/import_struct_method_wrong_argument.err`:

```text
Type error at <repo>/tests/golden/type_errors/import_struct_method_wrong_argument.cd:3:12: expected 0 arguments but got 1
```

Create `tests/golden/type_errors/import_struct_method_wrong_argument.exit`:

```text
1
```

- [ ] **Step 8: Add namespaced unknown method type error fixture**

Create `tests/golden/type_errors/import_struct_method_namespace_unknown.cd`:

```text
import "../import_struct_method_namespace/lib.cd" as shapes;
let p: shapes.Point = shapes.Point { x: 1, y: 2 };
print p.missing();
```

Create `tests/golden/type_errors/import_struct_method_namespace_unknown.err`:

```text
Type error at <repo>/tests/golden/type_errors/import_struct_method_namespace_unknown.cd:3:16: struct `shapes.Point` has no method `missing`
```

Create `tests/golden/type_errors/import_struct_method_namespace_unknown.exit`:

```text
1
```

- [ ] **Step 9: Run focused tests to verify RED**

Run:

```sh
cmake -S . -B build
cmake --build build --target module_symbols_tests compiler_design
ctest --test-dir build --output-on-failure -R '^module_symbols$'
python3 tests/run_golden_tests.py ./build/compiler_design --case import_struct_method_
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case import_struct_method_direct --case import_struct_method_namespace --case import_struct_method_mutation --case import_struct_method_namespaced_signature
```

Expected: `module_symbols_tests` fails to compile because `MethodSignature` / method export APIs do not exist yet, or the new import method golden/type-error cases fail because imported method metadata is missing. This is the required RED state.

---

### Task 2: Add shared method metadata and ModuleSymbols storage

**Files:**
- Modify: `include/TypeCheckerTypes.hpp`
- Modify: `include/ModuleSymbols.hpp`
- Modify: `src/ModuleSymbols.cpp`
- Test: `tests/module_symbols_tests.cpp`

- [ ] **Step 1: Add `MethodSignature` to shared type-checker types**

In `include/TypeCheckerTypes.hpp`, add this struct after `StructTypeDecl`:

```cpp
struct MethodSignature {
    TypeInfo receiverType;
    std::vector<TypeInfo> parameterTypes;
    TypeInfo returnType;
    std::string resolvedName;
};
```

- [ ] **Step 2: Add method export aliases and namespace storage**

In `include/ModuleSymbols.hpp`, add these aliases after `ModuleStructExports`:

```cpp
using StructMethodTable = std::unordered_map<std::string, MethodSignature>;
using ModuleMethodExports = std::unordered_map<std::string, StructMethodTable>;
```

Add this field to `NamespaceImport`:

```cpp
    ModuleMethodExports methods;
```

Add these public methods after `structExports`:

```cpp
    void recordMethodExport(std::size_t moduleId, std::string structName, std::string methodName, MethodSignature signature);
    const ModuleMethodExports* methodExports(std::size_t moduleId) const;
```

Add this private member after `structExports_`:

```cpp
    std::unordered_map<std::size_t, ModuleMethodExports> methodExports_;
```

- [ ] **Step 3: Implement method export storage**

In `src/ModuleSymbols.cpp`, add this line to `clear()` after `structExports_.clear();`:

```cpp
    methodExports_.clear();
```

Add these definitions after `structExports`:

```cpp
void ModuleSymbols::recordMethodExport(std::size_t moduleId, std::string structName, std::string methodName, MethodSignature signature)
{
    methodExports_[moduleId][std::move(structName)].emplace(std::move(methodName), std::move(signature));
}

const ModuleMethodExports* ModuleSymbols::methodExports(std::size_t moduleId) const
{
    const auto found = methodExports_.find(moduleId);
    return found == methodExports_.end() ? nullptr : &found->second;
}
```

- [ ] **Step 4: Run ModuleSymbols tests to verify GREEN**

Run:

```sh
cmake -S . -B build
cmake --build build --target module_symbols_tests
ctest --test-dir build --output-on-failure -R '^module_symbols$'
```

Expected: `module_symbols` passes.

- [ ] **Step 5: Commit ModuleSymbols method metadata**

Run:

```sh
git add include/TypeCheckerTypes.hpp include/ModuleSymbols.hpp src/ModuleSymbols.cpp tests/module_symbols_tests.cpp
git commit -m "refactor: add module method symbols"
```

Expected: commit succeeds.

---

### Task 3: Integrate method exports into TypeChecker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test: focused imported-method goldens

- [ ] **Step 1: Add helper declarations to TypeChecker**

In `include/TypeChecker.hpp`, add these private declarations near the existing method helpers:

```cpp
    MethodSignature methodSignatureFromInfo(const MethodInfo& method) const;
    MethodInfo methodInfoFromSignature(const MethodSignature& signature) const;
    TypeInfo qualifyNamespaceType(const TypeInfo& type, const std::string& alias, const ModuleStructExports& structs) const;
    MethodSignature qualifyNamespaceMethodSignature(const MethodSignature& signature, const std::string& alias, const ModuleStructExports& structs) const;
    void importMethodExports(const Token& diagnosticToken, const ModuleMethodExports& methodExports, const std::string* namespaceAlias = nullptr, const ModuleStructExports* namespaceStructs = nullptr);
    void recordStructMethodExports(std::size_t moduleId, const std::string& structName);
```

- [ ] **Step 2: Implement MethodInfo/MethodSignature conversion**

In `src/TypeChecker.cpp`, add these definitions after `findMethod`:

```cpp
MethodSignature TypeChecker::methodSignatureFromInfo(const MethodInfo& method) const
{
    MethodSignature signature;
    signature.receiverType = method.receiverType;
    signature.parameterTypes = method.parameterTypes;
    signature.returnType = method.returnType;
    signature.resolvedName = method.resolvedName;
    return signature;
}

TypeChecker::MethodInfo TypeChecker::methodInfoFromSignature(const MethodSignature& signature) const
{
    MethodInfo info;
    info.receiverType = signature.receiverType;
    info.parameterTypes = signature.parameterTypes;
    info.returnType = signature.returnType;
    info.resolvedName = signature.resolvedName;
    return info;
}
```

- [ ] **Step 3: Implement namespace type qualification**

Add these definitions after the conversion helpers:

```cpp
TypeInfo TypeChecker::qualifyNamespaceType(const TypeInfo& type, const std::string& alias, const ModuleStructExports& structs) const
{
    TypeInfo result = type;
    if (result.kind == StaticType::Struct && result.structName && structs.find(*result.structName) != structs.end()) {
        result.structName = alias + "." + *result.structName;
        return result;
    }
    if (result.kind == StaticType::Array && result.elementType) {
        result.elementType = std::make_shared<TypeInfo>(qualifyNamespaceType(*result.elementType, alias, structs));
        return result;
    }
    if (isNullable(result) && result.nullableOf) {
        result.nullableOf = std::make_shared<TypeInfo>(qualifyNamespaceType(*result.nullableOf, alias, structs));
        return result;
    }
    if (result.kind == StaticType::Function && result.returnType) {
        for (TypeInfo& parameter : result.parameterTypes) {
            parameter = qualifyNamespaceType(parameter, alias, structs);
        }
        result.returnType = std::make_shared<TypeInfo>(qualifyNamespaceType(*result.returnType, alias, structs));
    }
    return result;
}

MethodSignature TypeChecker::qualifyNamespaceMethodSignature(const MethodSignature& signature, const std::string& alias, const ModuleStructExports& structs) const
{
    MethodSignature result = signature;
    result.receiverType = qualifyNamespaceType(result.receiverType, alias, structs);
    for (TypeInfo& parameter : result.parameterTypes) {
        parameter = qualifyNamespaceType(parameter, alias, structs);
    }
    result.returnType = qualifyNamespaceType(result.returnType, alias, structs);
    return result;
}
```

- [ ] **Step 4: Implement method import/export helpers**

Add these definitions after namespace qualification:

```cpp
void TypeChecker::importMethodExports(
    const Token& diagnosticToken,
    const ModuleMethodExports& methodExports,
    const std::string* namespaceAlias,
    const ModuleStructExports* namespaceStructs)
{
    for (const auto& structEntry : methodExports) {
        std::string structName = structEntry.first;
        if (namespaceAlias) {
            structName = *namespaceAlias + "." + structName;
        }
        auto& table = methods_[structName];
        for (const auto& methodEntry : structEntry.second) {
            MethodSignature signature = methodEntry.second;
            if (namespaceAlias && namespaceStructs) {
                signature = qualifyNamespaceMethodSignature(signature, *namespaceAlias, *namespaceStructs);
            }
            if (table.find(methodEntry.first) != table.end()) {
                throw TypeError(diagnosticToken, "duplicate method `" + methodEntry.first + "` for struct `" + structName + "`");
            }
            table.emplace(methodEntry.first, methodInfoFromSignature(signature));
        }
    }
}

void TypeChecker::recordStructMethodExports(std::size_t moduleId, const std::string& structName)
{
    const auto methods = methods_.find(structName);
    if (methods == methods_.end()) {
        return;
    }
    for (const auto& method : methods->second) {
        moduleSymbols_.recordMethodExport(moduleId, structName, method.first, methodSignatureFromInfo(method.second));
    }
}
```

- [ ] **Step 5: Copy method exports into namespace imports**

In `TypeChecker::checkImport`, inside the `if (statement.alias)` block, after copying `structExports`, add:

```cpp
        if (const ModuleMethodExports* methodExports = moduleSymbols_.methodExports(imported->moduleId)) {
            namespaceImport.methods = *methodExports;
        }
```

- [ ] **Step 6: Register namespaced methods in `declareNamespaceAlias`**

In `TypeChecker::declareNamespaceAlias`, after the loop that registers qualified structs and before `moduleSymbols_.recordNamespace`, add:

```cpp
    importMethodExports(alias, imported.methods, &alias.lexeme, &imported.structs);
```

- [ ] **Step 7: Import direct method exports**

In `TypeChecker::checkImport`, after the direct struct export loop, add:

```cpp
    if (const ModuleMethodExports* methodExports = moduleSymbols_.methodExports(imported->moduleId)) {
        importMethodExports(statement.keyword, *methodExports);
    }
```

- [ ] **Step 8: Record method exports when exporting a struct**

In `TypeChecker::checkExport`, inside the `if (const StructTypeDecl* structType = findStructType(name.lexeme))` block immediately after `moduleSymbols_.recordStructExport(...)`, add:

```cpp
                    recordStructMethodExports(moduleId, name.lexeme);
```

- [ ] **Step 9: Run focused imported method tests**

Run:

```sh
cmake -S . -B build
cmake --build build --target compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design --case import_struct_method_
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case import_struct_method_direct --case import_struct_method_namespace --case import_struct_method_mutation --case import_struct_method_namespaced_signature
```

Expected: all focused imported-method checks pass. If the namespaced signature fixture fails, inspect `qualifyNamespaceType` recursion for struct names nested in arrays, nullable, and function types.

- [ ] **Step 10: Commit TypeChecker integration**

Run:

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/import_struct_method_direct tests/golden/import_struct_method_namespace tests/golden/import_struct_method_mutation tests/golden/import_struct_method_namespaced_signature tests/golden/type_errors/import_struct_method_wrong_argument.cd tests/golden/type_errors/import_struct_method_wrong_argument.err tests/golden/type_errors/import_struct_method_wrong_argument.exit tests/golden/type_errors/import_struct_method_namespace_unknown.cd tests/golden/type_errors/import_struct_method_namespace_unknown.err tests/golden/type_errors/import_struct_method_namespace_unknown.exit
git commit -m "feat: support imported struct methods"
```

Expected: commit succeeds.

---

### Task 4: Documentation and roadmap update

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README struct method documentation**

In `README.md`, replace this sentence in the named struct methods section:

```text
Inside a method, `this` has the impl struct type; field assignment through `this.field = value` mutates the receiver. Methods on anonymous or imported/namespaced structs, method export/import behavior, inheritance, overloading, dynamic dispatch, static methods, and function-valued field calls are not implemented yet.
```

with:

```text
Inside a method, `this` has the impl struct type; field assignment through `this.field = value` mutates the receiver. Methods on exported/imported named structs are available through direct imports and namespace imports, so `import "path";` supports `value.method()` and `import "path" as alias;` supports methods on `alias.Type` values. Methods on anonymous structs, inheritance, overloading, dynamic dispatch, static methods, and function-valued field calls are not implemented yet.
```

- [ ] **Step 2: Update README module documentation**

In `README.md`, replace this sentence in the source imports section:

```text
Namespaced imports expose exported declarations only through the alias, and exported struct types may be used as `alias.Type` in annotations and constructors such as `alias.Type { field: value }`:
```

with:

```text
Namespaced imports expose exported declarations only through the alias, and exported struct types may be used as `alias.Type` in annotations, constructors such as `alias.Type { field: value }`, and method calls on `alias.Type` values:
```

- [ ] **Step 3: Update roadmap M1 first item**

In `docs/roadmap.md`, delete this M1 item:

```text
1. Support methods on exported/imported structs, including namespaced structs.
```

Renumber the remaining M1 items so they start at 1.

- [ ] **Step 4: Verify documentation diff**

Run:

```sh
git diff -- README.md docs/roadmap.md
```

Expected: only imported/namespaced struct method docs and the completed roadmap item changed.

- [ ] **Step 5: Commit docs**

Run:

```sh
git add README.md docs/roadmap.md
git commit -m "docs: document imported struct methods"
```

Expected: commit succeeds.

---

### Task 5: Full verification and cleanup

**Files:**
- Verify: whole repository
- Clean: `tests/__pycache__/`

- [ ] **Step 1: Run standard full verification**

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

- [ ] **Step 2: Run sanitizer CTest because C++ type-checker code changed**

Run:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
rm -rf build-sanitize
```

Expected: sanitizer configure/build/CTest exits 0 with `10/10` CTest pass.

- [ ] **Step 3: Confirm final workspace state**

Run:

```sh
git status --short
```

Expected: no output. If files remain modified, inspect them and either commit intentional changes or revert accidental changes.

- [ ] **Step 4: Report final verification evidence**

In the final response, list each verification command and observed pass counts. Do not claim completion without fresh verification evidence.

---

## Self-Review Notes

- Spec coverage: Tasks 1-3 implement method metadata exports/imports for direct and namespace imports, including namespaced type rewriting. Task 4 updates user docs and roadmap. Task 5 runs required standard and sanitizer verification.
- Placeholder scan: every task has concrete files, code snippets, commands, and expected results; no placeholders remain.
- Type/name consistency: the plan consistently uses `MethodSignature`, `StructMethodTable`, `ModuleMethodExports`, `recordMethodExport`, `methodExports`, `methodSignatureFromInfo`, `methodInfoFromSignature`, `qualifyNamespaceType`, `qualifyNamespaceMethodSignature`, `importMethodExports`, and `recordStructMethodExports`.
