# Named Struct Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 12C named struct types as static field-shape declarations with exact struct literal initialization checks and static field access/assignment checks.

**Architecture:** Add `struct` declarations to the front end and represent them as compile-time-only AST statements. Extend `TypeInfo` so `StaticType::Struct` can optionally carry a nominal struct name, then have the type checker own a `structTypes_` table for field-shape checking while reusing existing anonymous struct runtime values, IR, bytecode, and Rust VM behavior. IR compilation skips struct declarations; all runtime execution remains based on existing `struct`, `field`, and `assign_field` operations.

**Tech Stack:** C++17 lexer/parser/AST/type checker/IR compiler, Python golden tests, `.cdbc` bytecode artifact tests, Rust VM parity tests.

---

## File Structure

- `include/Token.hpp`, `src/Lexer.cpp`: add `struct` keyword token.
- `include/Ast.hpp`, `src/Ast.cpp`: add `StructDeclStmt` and `StructFieldDecl`, plus AST printing.
- `include/Parser.hpp`, `src/Parser.cpp`: parse top-level `struct Name { field: Type, ... }` declarations.
- `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: add named struct type metadata, nominal compatibility, struct declaration checking, exact literal initialization, field access typing, and field assignment checks.
- `src/IRCompiler.cpp`: ignore compile-time-only `StructDeclStmt`.
- `tests/golden/named_struct_types/`: success fixture with AST/IR/bytecode/run outputs.
- `tests/golden/type_errors/`: named struct type error fixtures.
- `tests/golden/parse_errors/`: struct declaration parse error fixtures.
- `tests/bytecode_artifacts/named_struct_types/`: `.cdbc` artifact and Rust VM run fixture.
- `tests/run_rust_vm_tests.py`: add `named_struct_types` to golden allowlist.
- `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, `AGENTS.md`: user-facing and project memory documentation.

---

### Task 1: RED success fixture for named struct types

**Files:**
- Create: `tests/golden/named_struct_types/input.cd`
- Create: `tests/golden/named_struct_types/ast.out`
- Create: `tests/golden/named_struct_types/run.out`

- [ ] **Step 1: Write the success fixture input**

Create `tests/golden/named_struct_types/input.cd`:

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

- [ ] **Step 2: Write the initial AST expectation**

Create `tests/golden/named_struct_types/ast.out`:

```text
Program
  Struct Person {name: string, age: number}
  Let p: Person = (struct age: 36 name: "Ada")
  Print (field p name)
  Expr (= (field p age) 37)
  Print (field p age)
  Fun birthday(person: Person): Person
    Expr (= (field person age) (+ (field person age) 1))
    Return person
  Let older: Person = (call birthday p)
  Print (field older age)
```

- [ ] **Step 3: Write the run expectation**

Create `tests/golden/named_struct_types/run.out`:

```text
Ada
37
38
```

- [ ] **Step 4: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL for `named_struct_types` because `struct` is not yet parsed as a declaration and/or `Person` is still an unknown type.

- [ ] **Step 5: Commit the RED fixture**

```bash
git add tests/golden/named_struct_types/input.cd tests/golden/named_struct_types/ast.out tests/golden/named_struct_types/run.out
git commit -m "test: add named struct type fixture"
```

---

### Task 2: Lexer, AST, parser, and IR skip for struct declarations

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Add the token**

In `include/Token.hpp`, add `Struct` in the keyword group after `Return` and before `While`:

```cpp
    Return,
    Struct,
    While,
```

In `src/Lexer.cpp`, add the keyword mapping in `Lexer::identifier()`'s keyword table:

```cpp
        {"struct", TokenType::Struct},
```

In `src/Token.cpp` or the existing token-name implementation file, add:

```cpp
    case TokenType::Struct:
        return "Struct";
```

- [ ] **Step 2: Add AST declarations**

In `include/Ast.hpp`, add after `StructExpr` or before `Stmt` declarations:

```cpp
struct StructFieldDecl {
    Token name;
    TypeAnnotation typeName;
};
```

Add this statement node near other `Stmt` subclasses:

```cpp
struct StructDeclStmt final : Stmt {
    StructDeclStmt(Token name, std::vector<StructFieldDecl> fields);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::vector<StructFieldDecl> fields;
};
```

- [ ] **Step 3: Implement AST printing**

In `src/Ast.cpp`, add this constructor and printer near other statement implementations:

```cpp
StructDeclStmt::StructDeclStmt(Token name, std::vector<StructFieldDecl> fields)
    : name(std::move(name))
    , fields(std::move(fields))
{
}

void StructDeclStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Struct " << name.lexeme << " {";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << fields[i].name.lexeme << ": ";
        writeTypeAnnotation(out, fields[i].typeName);
    }
    out << "}\n";
}
```

Update `writeInlineStmt` with this branch before the final fallback:

```cpp
    if (const auto* structDecl = dynamic_cast<const StructDeclStmt*>(&stmt)) {
        out << "(struct " << structDecl->name.lexeme;
        for (const StructFieldDecl& field : structDecl->fields) {
            out << ' ' << field.name.lexeme << ": ";
            writeTypeAnnotation(out, field.typeName);
        }
        out << ')';
        return;
    }
```

- [ ] **Step 4: Declare parser methods**

In `include/Parser.hpp`, add private methods near `functionDeclaration()` and `letDeclaration()`:

```cpp
    StmtPtr structDeclaration();
    std::vector<StructFieldDecl> structFields();
    StructFieldDecl structField();
```

- [ ] **Step 5: Parse struct declarations**

In `src/Parser.cpp`, update `Parser::declaration()` before `fun`/`let` handling:

```cpp
    if (match(TokenType::Struct)) {
        return structDeclaration();
    }
```

Add these methods after `functionDeclaration()`:

```cpp
StmtPtr Parser::structDeclaration()
{
    Token name = consume(TokenType::Identifier, "expected struct name after `struct`");
    consume(TokenType::LeftBrace, "expected `{` after struct name");
    std::vector<StructFieldDecl> fields = structFields();
    consume(TokenType::RightBrace, "expected `}` after struct fields");
    return std::make_unique<StructDeclStmt>(std::move(name), std::move(fields));
}

std::vector<StructFieldDecl> Parser::structFields()
{
    std::vector<StructFieldDecl> fields;
    if (!check(TokenType::RightBrace)) {
        do {
            fields.push_back(structField());
        } while (match(TokenType::Comma));
    }
    return fields;
}

StructFieldDecl Parser::structField()
{
    Token name = consume(TokenType::Identifier, "expected struct field name");
    consume(TokenType::Colon, "expected `:` after struct field name");
    TypeAnnotation typeName = typeAnnotation("expected struct field type after `:`");
    return StructFieldDecl{std::move(name), std::move(typeName)};
}
```

- [ ] **Step 6: Skip struct declarations in IR compilation**

In `src/IRCompiler.cpp`, add this early branch in `IRCompiler::compileStatement()` before function handling:

```cpp
    if (dynamic_cast<const StructDeclStmt*>(&statement)) {
        return;
    }
```

- [ ] **Step 7: Build and verify parser RED advances**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: build succeeds. `named_struct_types` still fails with a type checker error such as `unsupported statement node` or `unknown type `Person`` until type checker support lands.

- [ ] **Step 8: Commit parser/AST support**

```bash
git add include/Token.hpp src/Lexer.cpp src/Token.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp src/IRCompiler.cpp
git commit -m "feat: parse named struct declarations"
```

---

### Task 3: Add named struct type model and declaration table

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/named_struct_unknown_type.cd`
- Create: `tests/golden/type_errors/named_struct_unknown_type.err`
- Create: `tests/golden/type_errors/named_struct_unknown_type.exit`
- Create: `tests/golden/type_errors/named_struct_duplicate_decl.cd`
- Create: `tests/golden/type_errors/named_struct_duplicate_decl.err`
- Create: `tests/golden/type_errors/named_struct_duplicate_decl.exit`
- Create: `tests/golden/type_errors/named_struct_duplicate_field.cd`
- Create: `tests/golden/type_errors/named_struct_duplicate_field.err`
- Create: `tests/golden/type_errors/named_struct_duplicate_field.exit`

- [ ] **Step 1: Add type-error fixtures**

Create `tests/golden/type_errors/named_struct_unknown_type.cd`:

```cd
let p: Person = nil;
```

Create `tests/golden/type_errors/named_struct_unknown_type.err`:

```text
Type error at 1:8: unknown type `Person`
```

Create `tests/golden/type_errors/named_struct_unknown_type.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_duplicate_decl.cd`:

```cd
struct Person { name: string }
struct Person { age: number }
```

Create `tests/golden/type_errors/named_struct_duplicate_decl.err`:

```text
Type error at 2:8: duplicate struct `Person`
```

Create `tests/golden/type_errors/named_struct_duplicate_decl.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_duplicate_field.cd`:

```cd
struct Person { name: string, name: number }
```

Create `tests/golden/type_errors/named_struct_duplicate_field.err`:

```text
Type error at 1:31: duplicate field `name` in struct `Person`
```

Create `tests/golden/type_errors/named_struct_duplicate_field.exit`:

```text
1
```

- [ ] **Step 2: Extend `TypeInfo`**

In `include/TypeChecker.hpp`, add `#include <string>` is already present and add an optional name field to `TypeInfo`:

```cpp
struct TypeInfo {
    StaticType kind = StaticType::Unknown;
    std::vector<TypeInfo> parameterTypes;
    std::shared_ptr<TypeInfo> returnType;
    std::optional<std::string> structName;
};
```

- [ ] **Step 3: Add struct type metadata to `TypeChecker`**

In `include/TypeChecker.hpp`, add private structs inside `TypeChecker`:

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

Add private methods:

```cpp
    void checkStructDeclaration(const StructDeclStmt& statement);
    const StructTypeDecl* findStructType(const std::string& name) const;
```

Add a member near scopes:

```cpp
    std::unordered_map<std::string, StructTypeDecl> structTypes_;
```

- [ ] **Step 4: Add helper constructors and names**

In `src/TypeChecker.cpp`, update helper constructors so they initialize `structName` through aggregate initialization or assignment. Add after `simpleType`:

```cpp
TypeInfo namedStructType(std::string name)
{
    TypeInfo result;
    result.kind = StaticType::Struct;
    result.structName = std::move(name);
    return result;
}
```

Update `functionType`, `unknownType`, `simpleType`, and `functionWithoutSignature` to leave `structName` empty.

Update `typeInfoName` before the function-special case:

```cpp
    if (type.kind == StaticType::Struct && type.structName) {
        return *type.structName;
    }
```

- [ ] **Step 5: Update compatibility for nominal struct types**

In `src/TypeChecker.cpp`, update `compatible` after the `expected.kind != actual.kind` check and before function handling:

```cpp
    if (expected.kind == StaticType::Struct) {
        if (expected.structName || actual.structName) {
            return expected.structName == actual.structName;
        }
        return true;
    }
```

This makes two named structs compatible only when their names match. It keeps anonymous struct compatible with anonymous struct.

- [ ] **Step 6: Clear struct declarations at check start**

In `TypeChecker::check`, add:

```cpp
    structTypes_.clear();
```

beside `resolvedNames_.clear()`, `scopes_.clear()`, and counter resets.

- [ ] **Step 7: Dispatch struct declaration checking**

In `TypeChecker::checkStatement`, add this branch before function handling:

```cpp
    if (const auto* structDecl = dynamic_cast<const StructDeclStmt*>(&statement)) {
        checkStructDeclaration(*structDecl);
        return;
    }
```

- [ ] **Step 8: Implement struct declaration checking**

In `src/TypeChecker.cpp`, add near `checkFunction`:

```cpp
const TypeChecker::StructTypeDecl* TypeChecker::findStructType(const std::string& name) const
{
    const auto found = structTypes_.find(name);
    if (found == structTypes_.end()) {
        return nullptr;
    }
    return &found->second;
}

void TypeChecker::checkStructDeclaration(const StructDeclStmt& statement)
{
    if (structTypes_.find(statement.name.lexeme) != structTypes_.end()) {
        throw TypeError(statement.name, "duplicate struct `" + statement.name.lexeme + "`");
    }

    StructTypeDecl declaration{statement.name, {}};
    std::unordered_map<std::string, Token> fieldNames;
    for (const StructFieldDecl& field : statement.fields) {
        if (fieldNames.find(field.name.lexeme) != fieldNames.end()) {
            throw TypeError(field.name,
                "duplicate field `" + field.name.lexeme + "` in struct `" + statement.name.lexeme + "`");
        }
        fieldNames.emplace(field.name.lexeme, field.name);
        declaration.fields.push_back(StructFieldType{field.name, resolveAnnotation(field.typeName)});
    }

    structTypes_.emplace(statement.name.lexeme, std::move(declaration));
}
```

- [ ] **Step 9: Resolve named struct annotations**

In `resolveAnnotation`, before throwing unknown type, add:

```cpp
    if (findStructType(typeName.token.lexeme)) {
        return namedStructType(typeName.token.lexeme);
    }
```

- [ ] **Step 10: Run verification for declaration/type support**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: duplicate and unknown type fixtures pass. `named_struct_types` may still fail because exact literal shape checking and field typing are not implemented.

- [ ] **Step 11: Commit named type declaration support**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/named_struct_unknown_type.* tests/golden/type_errors/named_struct_duplicate_decl.* tests/golden/type_errors/named_struct_duplicate_field.*
git commit -m "feat: register named struct types"
```

---

### Task 4: Exact struct literal initialization checks

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/named_struct_missing_field.cd`
- Create: `tests/golden/type_errors/named_struct_missing_field.err`
- Create: `tests/golden/type_errors/named_struct_missing_field.exit`
- Create: `tests/golden/type_errors/named_struct_extra_field.cd`
- Create: `tests/golden/type_errors/named_struct_extra_field.err`
- Create: `tests/golden/type_errors/named_struct_extra_field.exit`
- Create: `tests/golden/type_errors/named_struct_wrong_field_type.cd`
- Create: `tests/golden/type_errors/named_struct_wrong_field_type.err`
- Create: `tests/golden/type_errors/named_struct_wrong_field_type.exit`
- Create: `tests/golden/type_errors/named_struct_duplicate_literal_field.cd`
- Create: `tests/golden/type_errors/named_struct_duplicate_literal_field.err`
- Create: `tests/golden/type_errors/named_struct_duplicate_literal_field.exit`
- Create: `tests/golden/type_errors/named_struct_nominal_mismatch.cd`
- Create: `tests/golden/type_errors/named_struct_nominal_mismatch.err`
- Create: `tests/golden/type_errors/named_struct_nominal_mismatch.exit`

- [ ] **Step 1: Add field-shape type-error fixtures**

Create `tests/golden/type_errors/named_struct_missing_field.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada" };
```

Create `tests/golden/type_errors/named_struct_missing_field.err`:

```text
Type error at 2:17: missing field `age` for struct `Person`
```

Create `tests/golden/type_errors/named_struct_missing_field.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_extra_field.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: 36, title: "Dr" };
```

Create `tests/golden/type_errors/named_struct_extra_field.err`:

```text
Type error at 2:42: extra field `title` for struct `Person`
```

Create `tests/golden/type_errors/named_struct_extra_field.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_wrong_field_type.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: "old" };
```

Create `tests/golden/type_errors/named_struct_wrong_field_type.err`:

```text
Type error at 2:33: field `age` expects number, got string
```

Create `tests/golden/type_errors/named_struct_wrong_field_type.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_duplicate_literal_field.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", name: "Grace", age: 36 };
```

Create `tests/golden/type_errors/named_struct_duplicate_literal_field.err`:

```text
Type error at 2:33: duplicate field `name` in struct literal
```

Create `tests/golden/type_errors/named_struct_duplicate_literal_field.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_nominal_mismatch.cd`:

```cd
struct Person { name: string }
struct Pet { name: string }
let pet: Pet = { name: "Milo" };
let person: Person = pet;
```

Create `tests/golden/type_errors/named_struct_nominal_mismatch.err`:

```text
Type error at 4:5: cannot initialize `person` of type Person with Pet
```

Create `tests/golden/type_errors/named_struct_nominal_mismatch.exit`:

```text
1
```

- [ ] **Step 2: Declare literal check helpers**

In `include/TypeChecker.hpp`, add private methods:

```cpp
    CheckedExpression checkNamedStructLiteralInitializer(
        const LetStmt& statement,
        const TypeInfo& declared,
        const StructExpr& initializer);
    const StructFieldType* findStructField(const StructTypeDecl& structType, const std::string& name) const;
```

- [ ] **Step 3: Implement field lookup helper**

In `src/TypeChecker.cpp`, add:

```cpp
const TypeChecker::StructFieldType* TypeChecker::findStructField(
    const StructTypeDecl& structType,
    const std::string& name) const
{
    for (const StructFieldType& field : structType.fields) {
        if (field.name.lexeme == name) {
            return &field;
        }
    }
    return nullptr;
}
```

- [ ] **Step 4: Implement exact literal checking**

In `src/TypeChecker.cpp`, add:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkNamedStructLiteralInitializer(
    const LetStmt& statement,
    const TypeInfo& declared,
    const StructExpr& initializer)
{
    const StructTypeDecl* structType = declared.structName ? findStructType(*declared.structName) : nullptr;
    if (!structType) {
        throw TypeError(statement.name, "unknown struct type `" + typeInfoName(declared) + "`");
    }

    std::unordered_map<std::string, const StructField*> literalFields;
    for (const StructField& field : initializer.fields) {
        if (literalFields.find(field.name.lexeme) != literalFields.end()) {
            throw TypeError(field.name, "duplicate field `" + field.name.lexeme + "` in struct literal");
        }
        literalFields.emplace(field.name.lexeme, &field);
    }

    for (const StructFieldType& expectedField : structType->fields) {
        const auto found = literalFields.find(expectedField.name.lexeme);
        if (found == literalFields.end()) {
            throw TypeError(statement.name,
                "missing field `" + expectedField.name.lexeme + "` for struct `" + structType->name.lexeme + "`");
        }
        const CheckedExpression actual = checkExpressionInfo(*found->second->value);
        if (!compatible(expectedField.type, actual.type)) {
            throw TypeError(found->second->name,
                "field `" + expectedField.name.lexeme + "` expects " + typeInfoName(expectedField.type)
                    + ", got " + typeInfoName(actual.type));
        }
    }

    for (const StructField& field : initializer.fields) {
        if (!findStructField(*structType, field.name.lexeme)) {
            throw TypeError(field.name,
                "extra field `" + field.name.lexeme + "` for struct `" + structType->name.lexeme + "`");
        }
    }

    return CheckedExpression{declared};
}
```

- [ ] **Step 5: Call exact checking from let initialization**

In `TypeChecker::checkLetInitializer`, move `resolveAnnotation` before `checkExpressionInfo` and handle named struct literal initializers:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkLetInitializer(const LetStmt& statement)
{
    if (!statement.typeName) {
        return checkExpressionInfo(*statement.initializer);
    }

    const TypeInfo declared = resolveAnnotation(*statement.typeName);
    if (declared.kind == StaticType::Struct && declared.structName) {
        if (const auto* structLiteral = dynamic_cast<const StructExpr*>(statement.initializer.get())) {
            return checkNamedStructLiteralInitializer(statement, declared, *structLiteral);
        }
    }

    const CheckedExpression initializer = checkExpressionInfo(*statement.initializer);
    checkAssignable(
        statement.name,
        "cannot initialize `" + statement.name.lexeme + "` of type " + typeInfoName(declared)
            + " with " + typeInfoName(initializer.type),
        declared,
        initializer.type);
    return CheckedExpression{declared};
}
```

- [ ] **Step 6: Verify shape checks**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: field-shape type-error fixtures pass. The success fixture may advance to field access/assignment type failures until Task 5.

- [ ] **Step 7: Commit exact literal checks**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/named_struct_missing_field.* tests/golden/type_errors/named_struct_extra_field.* tests/golden/type_errors/named_struct_wrong_field_type.* tests/golden/type_errors/named_struct_duplicate_literal_field.* tests/golden/type_errors/named_struct_nominal_mismatch.*
git commit -m "feat: check named struct literal shapes"
```

---

### Task 5: Static field access and field assignment for named structs

**Files:**
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/type_errors/named_struct_unknown_field_access.cd`
- Create: `tests/golden/type_errors/named_struct_unknown_field_access.err`
- Create: `tests/golden/type_errors/named_struct_unknown_field_access.exit`
- Create: `tests/golden/type_errors/named_struct_unknown_field_assignment.cd`
- Create: `tests/golden/type_errors/named_struct_unknown_field_assignment.err`
- Create: `tests/golden/type_errors/named_struct_unknown_field_assignment.exit`
- Create: `tests/golden/type_errors/named_struct_wrong_field_assignment_type.cd`
- Create: `tests/golden/type_errors/named_struct_wrong_field_assignment_type.err`
- Create: `tests/golden/type_errors/named_struct_wrong_field_assignment_type.exit`
- Refresh/Create: `tests/golden/named_struct_types/ir.out`
- Refresh/Create: `tests/golden/named_struct_types/bytecode.out`

- [ ] **Step 1: Add field access/assignment error fixtures**

Create `tests/golden/type_errors/named_struct_unknown_field_access.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: 36 };
print p.missing;
```

Create `tests/golden/type_errors/named_struct_unknown_field_access.err`:

```text
Type error at 3:9: struct `Person` has no field `missing`
```

Create `tests/golden/type_errors/named_struct_unknown_field_access.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_unknown_field_assignment.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: 36 };
p.missing = 1;
```

Create `tests/golden/type_errors/named_struct_unknown_field_assignment.err`:

```text
Type error at 3:3: struct `Person` has no field `missing`
```

Create `tests/golden/type_errors/named_struct_unknown_field_assignment.exit`:

```text
1
```

Create `tests/golden/type_errors/named_struct_wrong_field_assignment_type.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: 36 };
p.age = "old";
```

Create `tests/golden/type_errors/named_struct_wrong_field_assignment_type.err`:

```text
Type error at 3:3: field `age` expects number, got string
```

Create `tests/golden/type_errors/named_struct_wrong_field_assignment_type.exit`:

```text
1
```

- [ ] **Step 2: Update `FieldAccessExpr` checking**

In `TypeChecker::checkExpressionInfo`, replace the current `FieldAccessExpr` branch with:

```cpp
    if (const auto* field = dynamic_cast<const FieldAccessExpr*>(&expression)) {
        const TypeInfo object = checkExpression(*field->object);
        if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
            throw TypeError(field->name, "can only access fields on structs");
        }
        if (object.kind == StaticType::Struct && object.structName) {
            const StructTypeDecl* structType = findStructType(*object.structName);
            const StructFieldType* structField = structType ? findStructField(*structType, field->name.lexeme) : nullptr;
            if (!structField) {
                throw TypeError(field->name,
                    "struct `" + *object.structName + "` has no field `" + field->name.lexeme + "`");
            }
            return CheckedExpression{structField->type};
        }
        return CheckedExpression{unknownType()};
    }
```

- [ ] **Step 3: Update `checkFieldAssignment`**

Replace `TypeChecker::checkFieldAssignment` with:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkFieldAssignment(const FieldAssignExpr& expression)
{
    const TypeInfo object = checkExpression(*expression.object);
    const CheckedExpression value = checkExpressionInfo(*expression.value);

    if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
        throw TypeError(expression.name, "can only assign fields on structs");
    }

    if (object.kind == StaticType::Struct && object.structName) {
        const StructTypeDecl* structType = findStructType(*object.structName);
        const StructFieldType* structField = structType ? findStructField(*structType, expression.name.lexeme) : nullptr;
        if (!structField) {
            throw TypeError(expression.name,
                "struct `" + *object.structName + "` has no field `" + expression.name.lexeme + "`");
        }
        if (!compatible(structField->type, value.type)) {
            throw TypeError(expression.name,
                "field `" + expression.name.lexeme + "` expects " + typeInfoName(structField->type)
                    + ", got " + typeInfoName(value.type));
        }
        return CheckedExpression{structField->type};
    }

    return value;
}
```

- [ ] **Step 4: Verify success and errors, then refresh generated outputs**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: if only `ir.out` and `bytecode.out` are missing for `named_struct_types`, refresh them:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

Then review only the new/changed files:

```bash
git diff -- tests/golden/named_struct_types/ir.out tests/golden/named_struct_types/bytecode.out tests/golden/named_struct_types/ast.out tests/golden/named_struct_types/run.out
```

Expected: `StructDeclStmt` produces no IR or bytecode instructions; field reads and assignment use existing `field` and `assign_field` operations.

- [ ] **Step 5: Run full golden verification**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass.

- [ ] **Step 6: Commit field typing support**

```bash
git add src/TypeChecker.cpp tests/golden/type_errors/named_struct_unknown_field_access.* tests/golden/type_errors/named_struct_unknown_field_assignment.* tests/golden/type_errors/named_struct_wrong_field_assignment_type.* tests/golden/named_struct_types
git commit -m "feat: type check named struct fields"
```

---

### Task 6: Bytecode artifact and Rust VM parity coverage

**Files:**
- Create: `tests/bytecode_artifacts/named_struct_types/input.cd`
- Create: `tests/bytecode_artifacts/named_struct_types/run.out`
- Create/refresh: `tests/bytecode_artifacts/named_struct_types/expected.cdbc`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Add artifact fixture input and run output**

Create `tests/bytecode_artifacts/named_struct_types/input.cd`:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: 36 };
p.age = 37;
print p.name;
print p.age;
```

Create `tests/bytecode_artifacts/named_struct_types/run.out`:

```text
Ada
37
```

- [ ] **Step 2: Generate expected `.cdbc`**

Run:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/named_struct_types/expected.cdbc tests/bytecode_artifacts/named_struct_types/input.cd
```

Expected: `expected.cdbc` contains no instruction for the `struct Person` declaration, and contains existing `struct`, `assign_field`, and `field` instructions.

- [ ] **Step 3: Add golden fixture to Rust VM allowlist**

In `tests/run_rust_vm_tests.py`, add `"named_struct_types",` to `golden_allowlist` near other language golden fixtures:

```python
            "named_struct_types",
```

- [ ] **Step 4: Verify artifact and Rust VM coverage**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case named_struct_types
```

Expected: artifact tests pass, and the Rust VM named struct golden case prints `Ada`, `37`, `38` from `tests/golden/named_struct_types/run.out`.

- [ ] **Step 5: Commit parity coverage**

```bash
git add tests/bytecode_artifacts/named_struct_types tests/run_rust_vm_tests.py
git commit -m "test: add named struct bytecode parity"
```

---

### Task 7: Parse-error fixtures for struct declarations

**Files:**
- Create: `tests/golden/parse_errors/struct_decl_missing_name.cd`
- Create: `tests/golden/parse_errors/struct_decl_missing_name.err`
- Create: `tests/golden/parse_errors/struct_decl_missing_name.exit`
- Create: `tests/golden/parse_errors/struct_decl_missing_left_brace.cd`
- Create: `tests/golden/parse_errors/struct_decl_missing_left_brace.err`
- Create: `tests/golden/parse_errors/struct_decl_missing_left_brace.exit`
- Create: `tests/golden/parse_errors/struct_decl_missing_field_type.cd`
- Create: `tests/golden/parse_errors/struct_decl_missing_field_type.err`
- Create: `tests/golden/parse_errors/struct_decl_missing_field_type.exit`
- Create: `tests/golden/parse_errors/struct_decl_missing_right_brace.cd`
- Create: `tests/golden/parse_errors/struct_decl_missing_right_brace.err`
- Create: `tests/golden/parse_errors/struct_decl_missing_right_brace.exit`

- [ ] **Step 1: Add parse-error inputs**

Create `tests/golden/parse_errors/struct_decl_missing_name.cd`:

```cd
struct { name: string }
```

Create `tests/golden/parse_errors/struct_decl_missing_left_brace.cd`:

```cd
struct Person name: string }
```

Create `tests/golden/parse_errors/struct_decl_missing_field_type.cd`:

```cd
struct Person { name: }
```

Create `tests/golden/parse_errors/struct_decl_missing_right_brace.cd`:

```cd
struct Person { name: string
```

- [ ] **Step 2: Capture exact parser diagnostics**

Run these commands to get exact line/column output:

```bash
./build/compiler_design tests/golden/parse_errors/struct_decl_missing_name.cd >/tmp/struct_missing_name.out 2>/tmp/struct_missing_name.err; cat /tmp/struct_missing_name.err
./build/compiler_design tests/golden/parse_errors/struct_decl_missing_left_brace.cd >/tmp/struct_missing_left_brace.out 2>/tmp/struct_missing_left_brace.err; cat /tmp/struct_missing_left_brace.err
./build/compiler_design tests/golden/parse_errors/struct_decl_missing_field_type.cd >/tmp/struct_missing_field_type.out 2>/tmp/struct_missing_field_type.err; cat /tmp/struct_missing_field_type.err
./build/compiler_design tests/golden/parse_errors/struct_decl_missing_right_brace.cd >/tmp/struct_missing_right_brace.out 2>/tmp/struct_missing_right_brace.err; cat /tmp/struct_missing_right_brace.err
```

Expected diagnostic shapes:

```text
Parse error at 1:8: expected struct name after `struct`, found LeftBrace `{`
Parse error at 1:15: expected `{` after struct name, found Identifier `name`
Parse error at 1:23: expected struct field type after `:`, found RightBrace `}`
Parse error at 2:1: expected `}` after struct fields, found EndOfFile
```

- [ ] **Step 3: Write `.err` and `.exit` files**

Write each `.err` file with the exact output from Step 2. Write each `.exit` file as:

```text
1
```

- [ ] **Step 4: Verify parse errors**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all parse-error fixtures pass.

- [ ] **Step 5: Commit parse-error coverage**

```bash
git add tests/golden/parse_errors/struct_decl_missing_name.* tests/golden/parse_errors/struct_decl_missing_left_brace.* tests/golden/parse_errors/struct_decl_missing_field_type.* tests/golden/parse_errors/struct_decl_missing_right_brace.*
git commit -m "test: cover named struct parse errors"
```

---

### Task 8: Documentation updates

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar docs**

In `docs/language-grammar.ebnf`, update declarations:

```ebnf
declaration = structDecl
            | funDecl
            | letDecl
            | statement ;
```

Add:

```ebnf
structDecl  = "struct", identifier, "{", [ structFields ], "}" ;

structFields = structField,
               { ",", structField } ;

structField = identifier, ":", typeExpr ;
```

Keep `struct` expression grammar as struct literal syntax and avoid naming conflict by placing declaration grammar near `funDecl`.

- [ ] **Step 2: Update README language docs**

In `README.md`, update the struct section with:

```markdown
Named struct declarations define static field shapes:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: 36 };
print p.name;
p.age = 37;
```

Named structs are static-only in this phase: runtime values remain anonymous struct values. Literal initialization of a named struct requires an exact field match, field order does not matter, and field access/assignment on known named struct values is statically checked. Constructor syntax such as `Person { ... }`, methods, recursive struct types, and runtime type names are not implemented yet.
```

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, update Phase 12 status to state:

```markdown
Phase 12C is implemented: named struct declarations define static field shapes, named struct annotations check exact literal initialization, and known named struct field access/assignment is statically checked. Constructor syntax, methods, recursive structs, runtime type names, and field creation remain future work.
```

- [ ] **Step 4: Update AGENTS project memory**

In `AGENTS.md`, update current language semantics to mention:

```markdown
- Named struct declarations `struct Name { field: type, ... }` define static field shapes. Named struct type annotations check exact struct literal initialization, and known named struct field access/assignment is statically checked. Runtime struct values remain anonymous reference values; constructor syntax and recursive struct types are not implemented yet.
```

- [ ] **Step 5: Verify docs do not claim unsupported features**

Run:

```bash
rg -n "Person \{|constructor|recursive|runtime type" README.md docs/roadmap.md AGENTS.md docs/language-grammar.ebnf
```

Expected: constructor and recursive mentions are limitations, not supported syntax claims.

- [ ] **Step 6: Commit docs**

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document named struct types"
```

---

### Task 9: Full verification and cleanup

**Files:**
- No source edits expected.
- Remove: `tests/__pycache__/` if created.

- [ ] **Step 1: Run full verification**

Run from repo root:

```bash
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

- [ ] **Step 2: Inspect workspace state**

Run:

```bash
git status --short
git log --oneline -8
```

Expected: worktree is clean; recent commits cover tests, parser/AST, type checker, parity, docs, and plan/spec.

- [ ] **Step 3: Final report**

Report exact command results:

```text
Implemented Phase 12C named struct types.
Verification:
- cmake -S . -B build: PASS
- cmake --build build: PASS
- ctest --test-dir build --output-on-failure: PASS
- python3 tests/run_golden_tests.py ./build/compiler_design: PASS
- python3 tests/run_golden_tests_selftest.py: PASS
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: PASS
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: PASS
- cargo test --manifest-path vm-rs/Cargo.toml: PASS
```

---

## Self-Review

- Spec coverage: Tasks cover struct declaration syntax, named type annotations, exact literal checking, static field access/assignment, compile-time-only IR behavior, success/type/parse tests, bytecode artifact/Rust VM parity, and documentation.
- Placeholder scan: The plan contains no `TBD`, `TODO`, or vague "write tests" steps; fixtures, commands, and code snippets are explicit.
- Type consistency: The plan consistently uses `StructDeclStmt`, `StructFieldDecl`, `StructTypeDecl`, `StructFieldType`, `TypeInfo::structName`, `namedStructType`, `checkNamedStructLiteralInitializer`, and `findStructField`.
