# Recursive Struct Rules Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reject recursive named struct field types with clear type diagnostics while allowing non-recursive same-scope forward struct field references.

**Architecture:** Add a type-checker-only struct declaration state machine. Predeclare struct names in each statement list, resolve struct field annotations through a recursive checker that detects `Checking` states as cycles, and leave parser, AST, IR, bytecode, runtime, and VM behavior unchanged for successful programs.

**Tech Stack:** C++17 TypeChecker, existing AST `TypeAnnotation`, Python golden tests, README/roadmap documentation.

---

## File Structure

Create:

- `tests/golden/type_errors/recursive_struct_direct.cd` — direct `Node` field recursion.
- `tests/golden/type_errors/recursive_struct_direct.err` — expected recursive-field type diagnostic.
- `tests/golden/type_errors/recursive_struct_direct.exit` — expected exit status.
- `tests/golden/type_errors/recursive_struct_nullable.cd` — nullable `Node?` field recursion.
- `tests/golden/type_errors/recursive_struct_nullable.err` — expected recursive-field type diagnostic.
- `tests/golden/type_errors/recursive_struct_nullable.exit` — expected exit status.
- `tests/golden/type_errors/recursive_struct_array.cd` — `[Node]` field recursion.
- `tests/golden/type_errors/recursive_struct_array.err` — expected recursive-field type diagnostic.
- `tests/golden/type_errors/recursive_struct_array.exit` — expected exit status.
- `tests/golden/type_errors/recursive_struct_function_return.cd` — `fun(): Node` field recursion.
- `tests/golden/type_errors/recursive_struct_function_return.err` — expected recursive-field type diagnostic.
- `tests/golden/type_errors/recursive_struct_function_return.exit` — expected exit status.
- `tests/golden/type_errors/recursive_struct_function_parameter.cd` — `fun(Node): nil` field recursion.
- `tests/golden/type_errors/recursive_struct_function_parameter.err` — expected recursive-field type diagnostic.
- `tests/golden/type_errors/recursive_struct_function_parameter.exit` — expected exit status.
- `tests/golden/type_errors/recursive_struct_indirect.cd` — `A -> B -> A` recursion.
- `tests/golden/type_errors/recursive_struct_indirect.err` — expected recursive-field type diagnostic.
- `tests/golden/type_errors/recursive_struct_indirect.exit` — expected exit status.
- `tests/golden/struct_forward_field_reference/input.cd` — same-scope non-recursive forward struct reference success case.
- `tests/golden/struct_forward_field_reference/run.out` — expected runtime output.

Modify:

- `include/TypeChecker.hpp` — declare struct-check state, predeclare/check helpers, and recursive field annotation resolver.
- `src/TypeChecker.cpp` — predeclare struct declarations per statement list, detect direct/nested/indirect cycles, and support non-recursive forward field references.
- `README.md` — document that recursive struct field types are rejected and same-scope forward field references are allowed.
- `docs/roadmap.md` — mark recursive struct rules as defined for now.

Do not modify:

- `docs/language-grammar.ebnf` — syntax is unchanged.
- Parser, AST, IR, bytecode, `.cdbc`, or VM implementation files — behavior for successful programs is unchanged except type-checker acceptance of non-recursive forward field references.

---

### Task 1: Add failing recursive-struct and forward-reference goldens

**Files:**
- Create all fixtures listed in File Structure under `tests/golden/type_errors/` and `tests/golden/struct_forward_field_reference/`.

- [ ] **Step 1: Add direct recursion fixture**

```sh
cat > tests/golden/type_errors/recursive_struct_direct.cd <<'CASE'
struct Node { next: Node }
CASE
cat > tests/golden/type_errors/recursive_struct_direct.err <<'CASE'
Type error at 1:21: recursive struct field `next` references `Node`
CASE
cat > tests/golden/type_errors/recursive_struct_direct.exit <<'CASE'
1
CASE
```

- [ ] **Step 2: Add nullable recursion fixture**

```sh
cat > tests/golden/type_errors/recursive_struct_nullable.cd <<'CASE'
struct Node { next: Node? }
CASE
cat > tests/golden/type_errors/recursive_struct_nullable.err <<'CASE'
Type error at 1:21: recursive struct field `next` references `Node`
CASE
cat > tests/golden/type_errors/recursive_struct_nullable.exit <<'CASE'
1
CASE
```

- [ ] **Step 3: Add array recursion fixture**

```sh
cat > tests/golden/type_errors/recursive_struct_array.cd <<'CASE'
struct Node { children: [Node] }
CASE
cat > tests/golden/type_errors/recursive_struct_array.err <<'CASE'
Type error at 1:26: recursive struct field `children` references `Node`
CASE
cat > tests/golden/type_errors/recursive_struct_array.exit <<'CASE'
1
CASE
```

- [ ] **Step 4: Add function return recursion fixture**

```sh
cat > tests/golden/type_errors/recursive_struct_function_return.cd <<'CASE'
struct Node { makeNext: fun(): Node }
CASE
cat > tests/golden/type_errors/recursive_struct_function_return.err <<'CASE'
Type error at 1:32: recursive struct field `makeNext` references `Node`
CASE
cat > tests/golden/type_errors/recursive_struct_function_return.exit <<'CASE'
1
CASE
```

- [ ] **Step 5: Add function parameter recursion fixture**

```sh
cat > tests/golden/type_errors/recursive_struct_function_parameter.cd <<'CASE'
struct Node { acceptNext: fun(Node): nil }
CASE
cat > tests/golden/type_errors/recursive_struct_function_parameter.err <<'CASE'
Type error at 1:31: recursive struct field `acceptNext` references `Node`
CASE
cat > tests/golden/type_errors/recursive_struct_function_parameter.exit <<'CASE'
1
CASE
```

- [ ] **Step 6: Add indirect recursion fixture**

```sh
cat > tests/golden/type_errors/recursive_struct_indirect.cd <<'CASE'
struct A { b: B }
struct B { a: A }
CASE
cat > tests/golden/type_errors/recursive_struct_indirect.err <<'CASE'
Type error at 2:15: recursive struct field `a` references `A`
CASE
cat > tests/golden/type_errors/recursive_struct_indirect.exit <<'CASE'
1
CASE
```

- [ ] **Step 7: Add non-recursive forward-reference success fixture**

```sh
mkdir -p tests/golden/struct_forward_field_reference
cat > tests/golden/struct_forward_field_reference/input.cd <<'CASE'
struct Box { value: Value }
struct Value { n: number }
let box = Box { value: Value { n: 1 } };
print box.value.n;
CASE
cat > tests/golden/struct_forward_field_reference/run.out <<'CASE'
1
CASE
```

- [ ] **Step 8: Run focused tests and verify RED**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case recursive_struct_direct \
  --case recursive_struct_nullable \
  --case recursive_struct_array \
  --case recursive_struct_function_return \
  --case recursive_struct_function_parameter \
  --case recursive_struct_indirect \
  --case struct_forward_field_reference
```

Expected: command exits non-zero. Direct/nullable recursion currently report `unknown type` instead of recursive-field diagnostics. The forward-reference success fixture currently exits with `unknown type \`Value\``. Some nested fixtures may also report `unknown type` at the nested `Node` token. These are the expected RED failures.

- [ ] **Step 9: Commit failing tests**

```sh
git add \
  tests/golden/type_errors/recursive_struct_direct.* \
  tests/golden/type_errors/recursive_struct_nullable.* \
  tests/golden/type_errors/recursive_struct_array.* \
  tests/golden/type_errors/recursive_struct_function_return.* \
  tests/golden/type_errors/recursive_struct_function_parameter.* \
  tests/golden/type_errors/recursive_struct_indirect.* \
  tests/golden/struct_forward_field_reference
git commit -m "test: add recursive struct rule coverage"
```

---

### Task 2: Predeclare struct names per statement list

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add struct state declarations**

In `include/TypeChecker.hpp`, add `#include <set>` only if using `std::set`; the recommended implementation below does not need it. Keep existing includes.

Inside private `class TypeChecker`, after `using MethodTable = ...;`, add:

```cpp
    enum class StructCheckState {
        Declared,
        Checking,
        Checked,
    };
```

After `void checkStatement(const Stmt& statement);`, add:

```cpp
    void predeclareStructDeclarations(const std::vector<StmtPtr>& statements);
    void checkStatementList(const std::vector<StmtPtr>& statements);
```

After `void checkStructDeclaration(const StructDeclStmt& statement);`, add:

```cpp
    void predeclareStructDeclaration(const StructDeclStmt& statement);
```

Near existing private members `structTypes_` and `methods_`, add:

```cpp
    std::unordered_map<std::string, const StructDeclStmt*> structDeclarations_;
    std::unordered_map<std::string, StructCheckState> structCheckStates_;
```

- [ ] **Step 2: Clear new state at top-level check start**

In `TypeChecker::check`, after:

```cpp
    structTypes_.clear();
    methods_.clear();
```

add:

```cpp
    structDeclarations_.clear();
    structCheckStates_.clear();
```

- [ ] **Step 3: Preserve new state around module checking**

In `TypeChecker::checkModule`, after saving `savedStructTypes`, add:

```cpp
    std::unordered_map<std::string, const StructDeclStmt*> savedStructDeclarations = std::move(structDeclarations_);
    std::unordered_map<std::string, StructCheckState> savedStructCheckStates = std::move(structCheckStates_);
```

After resetting `structTypes_.clear();`, add:

```cpp
    structDeclarations_.clear();
    structCheckStates_.clear();
```

Before restoring `methods_`, restore the new state:

```cpp
    structDeclarations_ = std::move(savedStructDeclarations);
    structCheckStates_ = std::move(savedStructCheckStates);
```

- [ ] **Step 4: Implement predeclare and statement-list helpers**

In `src/TypeChecker.cpp`, after `makeResolvedName`, add:

```cpp
void TypeChecker::predeclareStructDeclaration(const StructDeclStmt& statement)
{
    if (structTypes_.find(statement.name.lexeme) != structTypes_.end()) {
        throw TypeError(statement.name, "duplicate struct `" + statement.name.lexeme + "`");
    }

    StructTypeDecl declaration{statement.name, {}};
    structTypes_.emplace(statement.name.lexeme, std::move(declaration));
    structDeclarations_.emplace(statement.name.lexeme, &statement);
    structCheckStates_.emplace(statement.name.lexeme, StructCheckState::Declared);

    if (!moduleStack_.empty()) {
        moduleSymbols_.markLocalStruct(moduleStack_.back(), statement.name.lexeme);
    }
}

void TypeChecker::predeclareStructDeclarations(const std::vector<StmtPtr>& statements)
{
    for (const auto& statement : statements) {
        if (const auto* structDecl = dynamic_cast<const StructDeclStmt*>(statement.get())) {
            predeclareStructDeclaration(*structDecl);
        }
    }
}

void TypeChecker::checkStatementList(const std::vector<StmtPtr>& statements)
{
    predeclareStructDeclarations(statements);
    for (const auto& statement : statements) {
        checkStatement(*statement);
    }
}
```

- [ ] **Step 5: Use statement-list helper at top-level, module, and block scopes**

In `TypeChecker::check`, replace the no-module loop:

```cpp
        for (const auto& statement : program.statements) {
            checkStatement(*statement);
        }
```

with:

```cpp
        checkStatementList(program.statements);
```

In `TypeChecker::checkModule`, replace:

```cpp
        for (const auto& statement : module.statements) {
            checkStatement(*statement);
        }
```

with:

```cpp
        checkStatementList(module.statements);
```

In `checkStatement` block handling, replace:

```cpp
        for (const auto& child : block->statements) {
            checkStatement(*child);
        }
```

with:

```cpp
        checkStatementList(block->statements);
```

- [ ] **Step 6: Make `checkStructDeclaration` tolerate predeclared structs**

Replace the first duplicate check in `checkStructDeclaration`:

```cpp
    if (structTypes_.find(statement.name.lexeme) != structTypes_.end()) {
        throw TypeError(statement.name, "duplicate struct `" + statement.name.lexeme + "`");
    }
```

with:

```cpp
    const auto state = structCheckStates_.find(statement.name.lexeme);
    if (state != structCheckStates_.end() && state->second == StructCheckState::Checked) {
        return;
    }
    if (state == structCheckStates_.end()) {
        predeclareStructDeclaration(statement);
    }
```

Leave field resolution as-is for this task. At the end of `checkStructDeclaration`, replace:

```cpp
    structTypes_.emplace(statement.name.lexeme, std::move(declaration));
    if (!moduleStack_.empty()) {
        moduleSymbols_.markLocalStruct(moduleStack_.back(), statement.name.lexeme);
    }
```

with:

```cpp
    structTypes_[statement.name.lexeme] = std::move(declaration);
    structCheckStates_[statement.name.lexeme] = StructCheckState::Checked;
```

- [ ] **Step 7: Build and run duplicate/forward focused tests**

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case named_struct_duplicate_decl \
  --case named_struct_types \
  --case struct_constructor_basic \
  --case struct_forward_field_reference
```

Expected: `struct_forward_field_reference` may still fail until Task 3 if forward annotation resolution needs recursive body checking. Duplicate and existing struct fixtures must pass.

- [ ] **Step 8: Commit predeclaration infrastructure**

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: predeclare struct names during type checking"
```

---

### Task 3: Detect recursive struct field annotations

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Declare recursive annotation helpers**

In `include/TypeChecker.hpp`, after `predeclareStructDeclaration(...)`, add:

```cpp
    TypeInfo resolveStructFieldAnnotation(const StructFieldDecl& field);
    TypeInfo resolveStructFieldAnnotation(const TypeAnnotation& typeName, const Token& fieldName);
    TypeInfo resolveSimpleStructFieldAnnotation(const TypeAnnotation& typeName, const Token& fieldName);
```

- [ ] **Step 2: Add field annotation resolver**

In `src/TypeChecker.cpp`, after `findStructType`, add:

```cpp
TypeInfo TypeChecker::resolveStructFieldAnnotation(const StructFieldDecl& field)
{
    return resolveStructFieldAnnotation(field.typeName, field.name);
}

TypeInfo TypeChecker::resolveStructFieldAnnotation(const TypeAnnotation& typeName, const Token& fieldName)
{
    if (typeName.kind == TypeAnnotation::Kind::Nullable) {
        return nullableType(resolveStructFieldAnnotation(*typeName.innerType, fieldName));
    }

    if (typeName.kind == TypeAnnotation::Kind::Array) {
        return arrayType(resolveStructFieldAnnotation(*typeName.elementType, fieldName));
    }

    if (typeName.kind == TypeAnnotation::Kind::Function) {
        std::vector<TypeInfo> parameterTypes;
        parameterTypes.reserve(typeName.parameterTypes.size());
        for (const TypeAnnotation& parameter : typeName.parameterTypes) {
            parameterTypes.push_back(resolveStructFieldAnnotation(parameter, fieldName));
        }
        return functionType(std::move(parameterTypes), resolveStructFieldAnnotation(*typeName.returnType, fieldName));
    }

    return resolveSimpleStructFieldAnnotation(typeName, fieldName);
}

TypeInfo TypeChecker::resolveSimpleStructFieldAnnotation(const TypeAnnotation& typeName, const Token& fieldName)
{
    if (typeName.kind == TypeAnnotation::Kind::Qualified) {
        return resolveAnnotation(typeName);
    }

    if (typeName.token.lexeme == "number") {
        return simpleType(StaticType::Number);
    }
    if (typeName.token.lexeme == "bool") {
        return simpleType(StaticType::Bool);
    }
    if (typeName.token.lexeme == "string") {
        return simpleType(StaticType::String);
    }
    if (typeName.token.lexeme == "nil") {
        return simpleType(StaticType::Nil);
    }

    const auto state = structCheckStates_.find(typeName.token.lexeme);
    if (state != structCheckStates_.end()) {
        if (state->second == StructCheckState::Checking) {
            throw TypeError(typeName.token,
                "recursive struct field `" + fieldName.lexeme + "` references `" + typeName.token.lexeme + "`");
        }
        if (state->second == StructCheckState::Declared) {
            const auto declaration = structDeclarations_.find(typeName.token.lexeme);
            if (declaration == structDeclarations_.end()) {
                throw TypeError(typeName.token, "unknown type `" + typeName.token.lexeme + "`");
            }
            checkStructDeclaration(*declaration->second);
        }
        return namedStructType(typeName.token.lexeme);
    }

    if (findStructType(typeName.token.lexeme)) {
        return namedStructType(typeName.token.lexeme);
    }

    throw TypeError(typeName.token, "unknown type `" + typeName.token.lexeme + "`");
}
```

- [ ] **Step 3: Mark checking state and use field resolver**

In `checkStructDeclaration`, after the state setup from Task 2 and before building `StructTypeDecl declaration`, add:

```cpp
    structCheckStates_[statement.name.lexeme] = StructCheckState::Checking;
```

Inside the field loop, replace:

```cpp
        declaration.fields.push_back(StructFieldType{field.name, resolveAnnotation(field.typeName)});
```

with:

```cpp
        declaration.fields.push_back(StructFieldType{field.name, resolveStructFieldAnnotation(field)});
```

Keep the end-of-function assignment to `StructCheckState::Checked`.

- [ ] **Step 4: Build and run recursive struct focused tests**

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case recursive_struct_direct \
  --case recursive_struct_nullable \
  --case recursive_struct_array \
  --case recursive_struct_function_return \
  --case recursive_struct_function_parameter \
  --case recursive_struct_indirect \
  --case struct_forward_field_reference \
  --case named_struct_unknown_type
```

Expected: all selected tests pass. The new recursive fixtures report recursive-field diagnostics. `named_struct_unknown_type` still reports `unknown type`.

- [ ] **Step 5: Run named struct regression tests**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case named_struct_types \
  --case struct_constructor_basic \
  --case struct_constructor_functions \
  --case struct_methods_basic \
  --case import_struct_method_direct \
  --case import_struct_method_namespace \
  --case re_export_struct_method
```

Expected: all selected tests pass.

- [ ] **Step 6: Commit recursive detection**

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: reject recursive struct fields"
```

---

### Task 4: Update documentation and roadmap

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README named struct limitations**

In `README.md`, replace this sentence in the named-struct paragraph:

```text
Named structs are static-only in this phase: runtime values remain anonymous struct values. Named constructor expressions such as `Person { name: "Ada", age: 36 }` infer the named static type, require an exact field match, and allow fields in any order. Annotated anonymous literals such as `let p: Person = { name: "Ada", age: 36 };` remain supported. Field access/assignment on known named struct values is statically checked. Constructor functions such as `Person(...)`, recursive struct types, and runtime type names are not implemented yet.
```

with:

```text
Named structs are static-only in this phase: runtime values remain anonymous struct values. Named constructor expressions such as `Person { name: "Ada", age: 36 }` infer the named static type, require an exact field match, and allow fields in any order. Annotated anonymous literals such as `let p: Person = { name: "Ada", age: 36 };` remain supported. Field annotations may refer to non-recursive struct names declared later in the same scope, but recursive struct field types such as `struct Node { next: Node? }` are explicitly rejected for now. Field access/assignment on known named struct values is statically checked. Constructor functions such as `Person(...)` and runtime type names are not implemented yet.
```

- [ ] **Step 2: Update roadmap Phase 12**

In `docs/roadmap.md`, replace:

```markdown
- Define recursive struct type rules before allowing recursive fields.
```

with:

```markdown
- Recursive struct field types are explicitly rejected for now; revisit only with
  a dedicated design for recursive initialization and runtime representation.
```

- [ ] **Step 3: Run documentation-adjacent focused tests**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case recursive_struct_direct \
  --case recursive_struct_nullable \
  --case recursive_struct_indirect \
  --case struct_forward_field_reference
```

Expected: all selected tests pass.

- [ ] **Step 4: Commit docs**

```sh
git add README.md docs/roadmap.md
git commit -m "docs: document recursive struct rules"
```

---

### Task 5: Full verification and cleanup

**Files:**
- No source changes expected.
- Remove: `tests/__pycache__/` if Python creates it.

- [ ] **Step 1: Check git status before verification**

```sh
git status --short
```

Expected: no output. If files are modified, inspect and commit intentional changes before continuing.

- [ ] **Step 2: Run full required verification**

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

Expected: every command exits 0. Record the exact summary output for the final response.

- [ ] **Step 3: Check final git status**

```sh
git status --short
```

Expected: no output after removing `tests/__pycache__/`.

- [ ] **Step 4: Prepare final implementation summary**

Report:

```text
Implemented recursive struct rules:
- recursive struct field annotations are rejected with clear type diagnostics;
- recursion is detected through nullable, array, and function field annotations;
- indirect same-scope struct cycles are rejected;
- non-recursive same-scope forward struct field references are accepted;
- README and roadmap updated.

Verification run:
- cmake -S . -B build: passed
- cmake --build build: passed
- ctest --test-dir build --output-on-failure: passed
- python3 tests/run_golden_tests.py ./build/compiler_design: passed
- python3 tests/run_golden_tests_selftest.py: passed
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: passed
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: passed
- cargo test --manifest-path vm-rs/Cargo.toml: passed
```
