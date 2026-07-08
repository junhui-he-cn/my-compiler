# Namespace Imports Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `import "path" as alias;` so exported module members can be accessed as `alias.member` without polluting the importing module's top-level scope.

**Architecture:** Extend import AST/parser state with an optional alias, add qualified type/constructor AST support, and resolve alias member access entirely during type checking. Namespace aliases are compile-time only: exported values resolve through `ResolvedNames` to existing imported binding names, and exported struct types are registered in the importing module under qualified keys such as `math.Point`, so IR/bytecode/VM need no namespace opcode.

**Tech Stack:** C++17 lexer/parser/AST/type checker/IR compiler, Python golden tests, bytecode artifact tests, Rust VM parity tests, Markdown/EBNF docs.

---

## File Structure

- `include/Token.hpp`, `src/Lexer.cpp`: add `As` keyword/token and token name printing.
- `include/Ast.hpp`, `src/Ast.cpp`: add optional import alias, qualified type annotations, and optional struct-constructor qualifier.
- `include/Parser.hpp`, `src/Parser.cpp`: parse `import "path" as alias;`, `alias.Type` type annotations, and `alias.Type { ... }` constructors.
- `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: track namespace aliases per module, resolve namespace member expressions/types/constructors, reject invalid namespace usage, and record namespace member resolved names.
- `src/IRCompiler.cpp`: load resolved namespace member bindings instead of emitting runtime field access when type checker recorded a namespaced field access.
- `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, `AGENTS.md`: document namespace imports.
- `tests/golden/`, `tests/bytecode_artifacts/`, `tests/cli_multi_source_tests.py`, `tests/run_rust_vm_tests.py`: add/update fixtures and parity coverage.

---

### Task 1: Add RED fixtures for namespace imports

**Files:**
- Create: `tests/golden/namespace_import_value/input.cd`
- Create: `tests/golden/namespace_import_value/lib.cd`
- Create: `tests/golden/namespace_import_value/run.out`
- Create: `tests/golden/namespace_import_function/input.cd`
- Create: `tests/golden/namespace_import_function/lib.cd`
- Create: `tests/golden/namespace_import_function/run.out`
- Create: `tests/golden/namespace_import_struct/input.cd`
- Create: `tests/golden/namespace_import_struct/lib.cd`
- Create: `tests/golden/namespace_import_struct/run.out`
- Create: `tests/golden/namespace_import_conflicting_exports/input.cd`
- Create: `tests/golden/namespace_import_conflicting_exports/a.cd`
- Create: `tests/golden/namespace_import_conflicting_exports/b.cd`
- Create: `tests/golden/namespace_import_conflicting_exports/run.out`
- Create: `tests/golden/parse_errors/import_alias_missing_name.cd`
- Create: `tests/golden/parse_errors/import_alias_missing_name.err`
- Create: `tests/golden/parse_errors/import_alias_missing_name.exit`
- Create: `tests/golden/type_errors/namespace_import_no_top_level_pollution.cd`
- Create: `tests/golden/type_errors/namespace_import_no_top_level_pollution.err`
- Create: `tests/golden/type_errors/namespace_import_no_top_level_pollution.exit`
- Create: `tests/golden/type_errors/namespace_import_private_member.cd`
- Create: `tests/golden/type_errors/namespace_import_private_member.err`
- Create: `tests/golden/type_errors/namespace_import_private_member.exit`
- Create: `tests/golden/type_errors/namespace_import_unknown_alias.cd`
- Create: `tests/golden/type_errors/namespace_import_unknown_alias.err`
- Create: `tests/golden/type_errors/namespace_import_unknown_alias.exit`
- Create: `tests/golden/type_errors/namespace_import_alias_as_value.cd`
- Create: `tests/golden/type_errors/namespace_import_alias_as_value.err`
- Create: `tests/golden/type_errors/namespace_import_alias_as_value.exit`
- Create: `tests/golden/type_errors/namespace_import_assign_alias.cd`
- Create: `tests/golden/type_errors/namespace_import_assign_alias.err`
- Create: `tests/golden/type_errors/namespace_import_assign_alias.exit`
- Create: `tests/golden/type_errors/namespace_import_unknown_type.cd`
- Create: `tests/golden/type_errors/namespace_import_unknown_type.err`
- Create: `tests/golden/type_errors/namespace_import_unknown_type.exit`
- Create: `tests/golden/type_errors/namespace_import_duplicate_alias.cd`
- Create: `tests/golden/type_errors/namespace_import_duplicate_alias.err`
- Create: `tests/golden/type_errors/namespace_import_duplicate_alias.exit`
- Create: `tests/bytecode_artifacts/namespace_imports/input.cd`
- Create: `tests/bytecode_artifacts/namespace_imports/lib.cd`
- Create: `tests/bytecode_artifacts/namespace_imports/run.out`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Create success fixtures**

Run this from the repository root:

```bash
mkdir -p tests/golden/namespace_import_value \
  tests/golden/namespace_import_function \
  tests/golden/namespace_import_struct \
  tests/golden/namespace_import_conflicting_exports

cat > tests/golden/namespace_import_value/input.cd <<'CD'
import "./lib.cd" as lib;
print lib.value;
CD
cat > tests/golden/namespace_import_value/lib.cd <<'CD'
let value = "namespaced";
export value;
CD
cat > tests/golden/namespace_import_value/run.out <<'OUT'
namespaced
OUT

cat > tests/golden/namespace_import_function/input.cd <<'CD'
import "./lib.cd" as math;
print math.add(2, 3);
CD
cat > tests/golden/namespace_import_function/lib.cd <<'CD'
fun add(a: number, b: number): number { return a + b; }
export add;
CD
cat > tests/golden/namespace_import_function/run.out <<'OUT'
5
OUT

cat > tests/golden/namespace_import_struct/input.cd <<'CD'
import "./lib.cd" as shapes;
let p: shapes.Point = shapes.Point { x: 3, y: 4 };
print p.x + p.y;
CD
cat > tests/golden/namespace_import_struct/lib.cd <<'CD'
struct Point { x: number, y: number }
export Point;
CD
cat > tests/golden/namespace_import_struct/run.out <<'OUT'
7
OUT

cat > tests/golden/namespace_import_conflicting_exports/input.cd <<'CD'
import "./a.cd" as a;
import "./b.cd" as b;
print a.value;
print b.value;
CD
cat > tests/golden/namespace_import_conflicting_exports/a.cd <<'CD'
let value = "a";
export value;
CD
cat > tests/golden/namespace_import_conflicting_exports/b.cd <<'CD'
let value = "b";
export value;
CD
cat > tests/golden/namespace_import_conflicting_exports/run.out <<'OUT'
a
b
OUT
```

- [ ] **Step 2: Create parse error fixture for missing alias name**

```bash
cat > tests/golden/parse_errors/import_alias_missing_name.cd <<'CD'
import "./lib.cd" as;
CD
cat > tests/golden/parse_errors/import_alias_missing_name.err <<'ERR'
Parse error at 1:21: expected namespace alias after `as`
ERR
cat > tests/golden/parse_errors/import_alias_missing_name.exit <<'ERR'
1
ERR
```

- [ ] **Step 3: Create namespace type error fixtures**

```bash
cat > tests/golden/type_errors/namespace_import_no_top_level_pollution.cd <<'CD'
import "../namespace_import_value/lib.cd" as lib;
print value;
CD
cat > tests/golden/type_errors/namespace_import_no_top_level_pollution.err <<'ERR'
Type error at /home/junhe/compiler/tests/golden/type_errors/namespace_import_no_top_level_pollution.cd:2:7: undefined variable `value`
  print value;
        ^
ERR
cat > tests/golden/type_errors/namespace_import_no_top_level_pollution.exit <<'ERR'
1
ERR

cat > tests/golden/type_errors/namespace_import_private_member.cd <<'CD'
import "../namespace_import_value/lib.cd" as lib;
print lib.hidden;
CD
cat > tests/golden/type_errors/namespace_import_private_member.err <<'ERR'
Type error at /home/junhe/compiler/tests/golden/type_errors/namespace_import_private_member.cd:2:11: module namespace `lib` has no exported member `hidden`
  print lib.hidden;
            ^
ERR
cat > tests/golden/type_errors/namespace_import_private_member.exit <<'ERR'
1
ERR

cat > tests/golden/type_errors/namespace_import_unknown_alias.cd <<'CD'
print missing.value;
CD
cat > tests/golden/type_errors/namespace_import_unknown_alias.err <<'ERR'
Type error at 1:7: undefined variable `missing`
ERR
cat > tests/golden/type_errors/namespace_import_unknown_alias.exit <<'ERR'
1
ERR

cat > tests/golden/type_errors/namespace_import_alias_as_value.cd <<'CD'
import "../namespace_import_value/lib.cd" as lib;
print lib;
CD
cat > tests/golden/type_errors/namespace_import_alias_as_value.err <<'ERR'
Type error at /home/junhe/compiler/tests/golden/type_errors/namespace_import_alias_as_value.cd:2:7: namespace alias `lib` is not a value
  print lib;
        ^
ERR
cat > tests/golden/type_errors/namespace_import_alias_as_value.exit <<'ERR'
1
ERR

cat > tests/golden/type_errors/namespace_import_assign_alias.cd <<'CD'
import "../namespace_import_value/lib.cd" as lib;
lib = 1;
CD
cat > tests/golden/type_errors/namespace_import_assign_alias.err <<'ERR'
Type error at /home/junhe/compiler/tests/golden/type_errors/namespace_import_assign_alias.cd:2:1: cannot assign to namespace alias `lib`
  lib = 1;
  ^
ERR
cat > tests/golden/type_errors/namespace_import_assign_alias.exit <<'ERR'
1
ERR

cat > tests/golden/type_errors/namespace_import_unknown_type.cd <<'CD'
import "../namespace_import_struct/lib.cd" as shapes;
let p: shapes.Missing = nil;
CD
cat > tests/golden/type_errors/namespace_import_unknown_type.err <<'ERR'
Type error at /home/junhe/compiler/tests/golden/type_errors/namespace_import_unknown_type.cd:2:15: module namespace `shapes` has no exported type `Missing`
  let p: shapes.Missing = nil;
                ^
ERR
cat > tests/golden/type_errors/namespace_import_unknown_type.exit <<'ERR'
1
ERR

cat > tests/golden/type_errors/namespace_import_duplicate_alias.cd <<'CD'
let lib = 1;
import "../namespace_import_value/lib.cd" as lib;
CD
cat > tests/golden/type_errors/namespace_import_duplicate_alias.err <<'ERR'
Type error at /home/junhe/compiler/tests/golden/type_errors/namespace_import_duplicate_alias.cd:2:43: namespace alias `lib` conflicts with an existing declaration
  import "../namespace_import_value/lib.cd" as lib;
                                            ^
ERR
cat > tests/golden/type_errors/namespace_import_duplicate_alias.exit <<'ERR'
1
ERR
```

- [ ] **Step 4: Add bytecode/Rust VM parity fixture**

```bash
mkdir -p tests/bytecode_artifacts/namespace_imports
cat > tests/bytecode_artifacts/namespace_imports/input.cd <<'CD'
import "./lib.cd" as lib;
print lib.answer();
CD
cat > tests/bytecode_artifacts/namespace_imports/lib.cd <<'CD'
let base = 39;
fun answer() { return base + 3; }
export answer;
CD
cat > tests/bytecode_artifacts/namespace_imports/run.out <<'OUT'
42
OUT
```

In `tests/run_rust_vm_tests.py`, add `namespace_imports` to the bytecode artifact/golden case list near `module_exports`. Use this exact entry in the existing list:

```python
            "namespace_imports",
```

- [ ] **Step 5: Run RED checks**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected before implementation: failures for `as` parsing, qualified type parsing, missing bytecode expected artifact, and possibly Rust VM case setup. Do not update goldens yet.

- [ ] **Step 6: Commit RED fixtures**

```bash
git add tests/golden tests/bytecode_artifacts tests/run_rust_vm_tests.py
git commit -m "test: add namespace import fixtures"
```

---

### Task 2: Parse namespace import syntax and qualified AST nodes

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add `As` token**

In `include/Token.hpp`, add `As` beside other keyword tokens, for example after `Import`:

```cpp
    Import,
    As,
    Export,
```

In `src/Lexer.cpp`, add the keyword mapping:

```cpp
        {"as", TokenType::As},
```

and add token name printing:

```cpp
    case TokenType::As:
        return "As";
```

- [ ] **Step 2: Extend AST type annotations and import/constructor nodes**

In `include/Ast.hpp`, change `TypeAnnotation::Kind` to include `Qualified`:

```cpp
enum class Kind {
    Simple,
    Qualified,
    Function,
    Array,
};
```

Add this factory declaration:

```cpp
static TypeAnnotation qualified(Token qualifier, Token name);
```

Add this member beside `token`:

```cpp
Token qualifier{TokenType::Identifier, "", 0, 0};
```

Change `StructConstructExpr` to store an optional qualifier:

```cpp
StructConstructExpr(std::optional<Token> qualifier, Token name, std::vector<StructField> fields);
std::optional<Token> qualifier;
Token name;
```

Change `ImportStmt` to store an optional alias:

```cpp
ImportStmt(Token keyword, Token path, std::optional<Token> alias);
Token keyword;
Token path;
std::optional<Token> alias;
```

- [ ] **Step 3: Update AST implementation**

In `src/Ast.cpp`, update `writeTypeAnnotation()` so the first branch is:

```cpp
if (annotation.kind == TypeAnnotation::Kind::Simple) {
    out << annotation.token.lexeme;
    return;
}

if (annotation.kind == TypeAnnotation::Kind::Qualified) {
    out << annotation.qualifier.lexeme << '.' << annotation.token.lexeme;
    return;
}
```

Add the qualified factory after `TypeAnnotation::simple()`:

```cpp
TypeAnnotation TypeAnnotation::qualified(Token qualifier, Token name)
{
    TypeAnnotation result;
    result.kind = Kind::Qualified;
    result.qualifier = std::move(qualifier);
    result.token = std::move(name);
    return result;
}
```

Replace the struct constructor implementation with:

```cpp
StructConstructExpr::StructConstructExpr(std::optional<Token> qualifier, Token name, std::vector<StructField> fields)
    : qualifier(std::move(qualifier))
    , name(std::move(name))
    , fields(std::move(fields))
{
}

void StructConstructExpr::print(std::ostream& out) const
{
    out << "(construct ";
    if (qualifier) {
        out << qualifier->lexeme << '.';
    }
    out << name.lexeme;
    for (const StructField& field : fields) {
        out << ' ' << field.name.lexeme << ": ";
        writeExpr(out, field.value);
    }
    out << ')';
}
```

Replace import constructor/printing with:

```cpp
ImportStmt::ImportStmt(Token keyword, Token path, std::optional<Token> alias)
    : keyword(std::move(keyword))
    , path(std::move(path))
    , alias(std::move(alias))
{
}

void ImportStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Import " << path.lexeme;
    if (alias) {
        out << " as " << alias->lexeme;
    }
    out << "\n";
}
```

Update compact import printing in `writeInlineStmt()`:

```cpp
if (const auto* import = dynamic_cast<const ImportStmt*>(&stmt)) {
    out << "(import " << import->path.lexeme;
    if (import->alias) {
        out << " as " << import->alias->lexeme;
    }
    out << ')';
    return;
}
```

- [ ] **Step 4: Update parser declarations**

In `include/Parser.hpp`, add:

```cpp
ExprPtr qualifiedStructConstructor();
bool isQualifiedStructConstructorStart() const;
```

near `structConstructor()`.

- [ ] **Step 5: Parse optional import aliases**

Replace `Parser::importDeclaration()` in `src/Parser.cpp` with:

```cpp
StmtPtr Parser::importDeclaration()
{
    Token keyword = previous();
    Token path = consume(TokenType::String, "expected import path string");
    std::optional<Token> alias;
    if (match(TokenType::As)) {
        alias = consume(TokenType::Identifier, "expected namespace alias after `as`");
    }
    consume(TokenType::Semicolon, alias ? "expected `;` after import alias" : "expected `;` after import path");
    return std::make_unique<ImportStmt>(std::move(keyword), std::move(path), std::move(alias));
}
```

- [ ] **Step 6: Parse qualified type annotations**

In `Parser::typeAnnotation()`, replace the final simple return with:

```cpp
Token name = consume(TokenType::Identifier, simpleTypeMessage);
if (match(TokenType::Dot)) {
    Token member = consume(TokenType::Identifier, "expected type name after `.`");
    return TypeAnnotation::qualified(std::move(name), std::move(member));
}
return TypeAnnotation::simple(std::move(name));
```

- [ ] **Step 7: Parse qualified struct constructors**

Replace `Parser::structConstructor()` with:

```cpp
ExprPtr Parser::structConstructor()
{
    Token name = consume(TokenType::Identifier, "expected struct constructor name");
    consume(TokenType::LeftBrace, "expected `{` after struct constructor name");
    std::vector<StructField> fields = structLiteralFields();
    consume(TokenType::RightBrace, "expected `}` after struct fields");
    return std::make_unique<StructConstructExpr>(std::nullopt, std::move(name), std::move(fields));
}
```

Add:

```cpp
bool Parser::isQualifiedStructConstructorStart() const
{
    return check(TokenType::Identifier)
        && current_ + 3 < tokens_.size()
        && tokens_[current_ + 1].type == TokenType::Dot
        && tokens_[current_ + 2].type == TokenType::Identifier
        && tokens_[current_ + 3].type == TokenType::LeftBrace;
}

ExprPtr Parser::qualifiedStructConstructor()
{
    Token qualifier = consume(TokenType::Identifier, "expected namespace alias before `.`");
    consume(TokenType::Dot, "expected `.` after namespace alias");
    Token name = consume(TokenType::Identifier, "expected struct constructor name after `.`");
    consume(TokenType::LeftBrace, "expected `{` after struct constructor name");
    std::vector<StructField> fields = structLiteralFields();
    consume(TokenType::RightBrace, "expected `}` after struct fields");
    return std::make_unique<StructConstructExpr>(std::move(qualifier), std::move(name), std::move(fields));
}
```

In `Parser::primary()`, before the existing simple struct constructor check, add:

```cpp
if (allowStructConstructors_ && isQualifiedStructConstructorStart()) {
    return qualifiedStructConstructor();
}
```

- [ ] **Step 8: Build and run RED/parse checks**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected after this task: compiler builds, parse error for missing alias can pass, but namespace import success/type fixtures still fail in type checking because aliases are not resolved yet.

- [ ] **Step 9: Commit parser/AST changes**

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/parse_errors/import_alias_missing_name.err
git commit -m "feat: parse namespace imports"
```

---

### Task 3: Type-check namespace aliases, members, and qualified structs

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Extend `ResolvedNames` for namespaced field access**

In `include/TypeChecker.hpp`, add public methods:

```cpp
bool hasFieldAccess(const FieldAccessExpr& expression) const;
const std::string& fieldAccessName(const FieldAccessExpr& expression) const;
```

Add private friend method and map:

```cpp
void recordFieldAccess(const FieldAccessExpr& expression, std::string name);
std::unordered_map<const FieldAccessExpr*, std::string> fieldAccessNames_;
```

In `src/TypeChecker.cpp`, implement:

```cpp
bool ResolvedNames::hasFieldAccess(const FieldAccessExpr& expression) const
{
    return fieldAccessNames_.find(&expression) != fieldAccessNames_.end();
}

const std::string& ResolvedNames::fieldAccessName(const FieldAccessExpr& expression) const
{
    return fieldAccessNames_.at(&expression);
}

void ResolvedNames::recordFieldAccess(const FieldAccessExpr& expression, std::string name)
{
    fieldAccessNames_.emplace(&expression, std::move(name));
}
```

and add this to `ResolvedNames::clear()`:

```cpp
fieldAccessNames_.clear();
```

- [ ] **Step 2: Add namespace alias state**

In `include/TypeChecker.hpp`, add private structs and fields:

```cpp
struct NamespaceImport {
    ExportTable values;
    std::unordered_map<std::string, StructTypeDecl> structs;
};

using NamespaceTable = std::unordered_map<std::string, NamespaceImport>;

std::unordered_map<std::size_t, NamespaceTable> moduleNamespaces_;
```

Add private helper declarations:

```cpp
NamespaceTable& currentNamespaceTable();
const NamespaceImport* findNamespace(const std::string& alias) const;
void declareNamespaceAlias(const ImportStmt& statement, NamespaceImport imported);
std::string qualifiedStructName(const Token& qualifier, const Token& name) const;
std::string structConstructorTypeName(const StructConstructExpr& expression) const;
```

- [ ] **Step 3: Reset namespace state**

In `TypeChecker::check()`, after `moduleLocalStructNames_.clear();`, add:

```cpp
moduleNamespaces_.clear();
```

- [ ] **Step 4: Implement namespace helper methods**

Add these methods before `checkImport()` in `src/TypeChecker.cpp`:

```cpp
TypeChecker::NamespaceTable& TypeChecker::currentNamespaceTable()
{
    if (moduleStack_.empty()) {
        throw TypeError("namespace imports require a module context");
    }
    return moduleNamespaces_[moduleStack_.back()];
}

const TypeChecker::NamespaceImport* TypeChecker::findNamespace(const std::string& alias) const
{
    if (moduleStack_.empty()) {
        return nullptr;
    }
    const auto table = moduleNamespaces_.find(moduleStack_.back());
    if (table == moduleNamespaces_.end()) {
        return nullptr;
    }
    const auto found = table->second.find(alias);
    return found == table->second.end() ? nullptr : &found->second;
}

std::string TypeChecker::qualifiedStructName(const Token& qualifier, const Token& name) const
{
    return qualifier.lexeme + "." + name.lexeme;
}

std::string TypeChecker::structConstructorTypeName(const StructConstructExpr& expression) const
{
    if (expression.qualifier) {
        return qualifiedStructName(*expression.qualifier, expression.name);
    }
    return expression.name.lexeme;
}

void TypeChecker::declareNamespaceAlias(const ImportStmt& statement, NamespaceImport imported)
{
    if (!statement.alias) {
        throw TypeError(statement.keyword, "internal error: namespace import without alias");
    }

    const Token& alias = *statement.alias;
    NamespaceTable& namespaces = currentNamespaceTable();
    if (namespaces.find(alias.lexeme) != namespaces.end()
        || currentScope().find(alias.lexeme) != currentScope().end()
        || structTypes_.find(alias.lexeme) != structTypes_.end()) {
        throw TypeError(alias, "namespace alias `" + alias.lexeme + "` conflicts with an existing declaration");
    }

    for (const auto& entry : imported.structs) {
        StructTypeDecl qualified = entry.second;
        qualified.name.lexeme = alias.lexeme + "." + entry.first;
        structTypes_.emplace(qualified.name.lexeme, std::move(qualified));
    }

    namespaces.emplace(alias.lexeme, std::move(imported));
}
```

- [ ] **Step 5: Update `checkImport()` for aliases**

In `TypeChecker::checkImport()`, after `checkModule(*imported);`, insert:

```cpp
    if (statement.alias) {
        NamespaceImport namespaceImport;
        const auto exports = moduleExports_.find(imported->moduleId);
        if (exports != moduleExports_.end()) {
            namespaceImport.values = exports->second;
        }
        const auto structExports = moduleStructExports_.find(imported->moduleId);
        if (structExports != moduleStructExports_.end()) {
            namespaceImport.structs = structExports->second;
        }
        declareNamespaceAlias(statement, std::move(namespaceImport));
        return;
    }
```

Leave the existing direct-import export insertion code below it unchanged.

- [ ] **Step 6: Reject alias-as-value and alias assignment**

In the `VariableExpr` branch of `checkExpressionInfo()`, before throwing undefined variable, add:

```cpp
        if (findNamespace(variable->name.lexeme)) {
            throw TypeError(variable->name, "namespace alias `" + variable->name.lexeme + "` is not a value");
        }
```

In the `AssignExpr` branch, before throwing undefined variable, add:

```cpp
        if (findNamespace(assign->name.lexeme)) {
            throw TypeError(assign->name, "cannot assign to namespace alias `" + assign->name.lexeme + "`");
        }
```

- [ ] **Step 7: Resolve namespaced field access**

At the start of the `FieldAccessExpr` branch in `checkExpressionInfo()`, before checking the object expression, add:

```cpp
        if (const auto* variable = dynamic_cast<const VariableExpr*>(field->object.get())) {
            if (const NamespaceImport* namespaceImport = findNamespace(variable->name.lexeme)) {
                const auto found = namespaceImport->values.find(field->name.lexeme);
                if (found == namespaceImport->values.end()) {
                    throw TypeError(field->name,
                        "module namespace `" + variable->name.lexeme + "` has no exported member `" + field->name.lexeme + "`");
                }
                resolvedNames_.recordFieldAccess(*field, found->second.resolvedName);
                return CheckedExpression{found->second.type};
            }
        }
```

- [ ] **Step 8: Resolve qualified struct constructors**

Replace `checkStructConstructor()` with:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkStructConstructor(const StructConstructExpr& expression)
{
    if (expression.qualifier) {
        const NamespaceImport* namespaceImport = findNamespace(expression.qualifier->lexeme);
        if (!namespaceImport) {
            throw TypeError(*expression.qualifier, "unknown module namespace `" + expression.qualifier->lexeme + "`");
        }
        if (namespaceImport->structs.find(expression.name.lexeme) == namespaceImport->structs.end()) {
            throw TypeError(expression.name,
                "module namespace `" + expression.qualifier->lexeme + "` has no exported type `" + expression.name.lexeme + "`");
        }
    }

    const std::string typeName = structConstructorTypeName(expression);
    const StructTypeDecl* structType = findStructType(typeName);
    if (!structType) {
        throw TypeError(expression.name, "unknown struct type `" + typeName + "`");
    }
    return checkNamedStructFields(expression.name, namedStructType(typeName), expression.fields);
}
```

- [ ] **Step 9: Resolve qualified type annotations**

At the start of `resolveAnnotation()` after the Array/Function branches and before builtin simple names, add:

```cpp
    if (typeName.kind == TypeAnnotation::Kind::Qualified) {
        const NamespaceImport* namespaceImport = findNamespace(typeName.qualifier.lexeme);
        if (!namespaceImport) {
            throw TypeError(typeName.qualifier, "unknown module namespace `" + typeName.qualifier.lexeme + "`");
        }
        if (namespaceImport->structs.find(typeName.token.lexeme) == namespaceImport->structs.end()) {
            throw TypeError(typeName.token,
                "module namespace `" + typeName.qualifier.lexeme + "` has no exported type `" + typeName.token.lexeme + "`");
        }
        return namedStructType(qualifiedStructName(typeName.qualifier, typeName.token));
    }
```

- [ ] **Step 10: Build and run type-focused tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: namespace import golden and type-error fixtures pass or show only AST/IR/bytecode missing output files that need intentional refresh.

- [ ] **Step 11: Commit type checker changes**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors
git commit -m "feat: type check namespace imports"
```

---

### Task 4: Lower namespaced member access in IR and refresh artifacts

**Files:**
- Modify: `src/IRCompiler.cpp`
- Modify: `tests/golden/**/ast.out`
- Modify: `tests/golden/**/ir.out`
- Modify: `tests/golden/**/bytecode.out`
- Modify: `tests/bytecode_artifacts/namespace_imports/expected.cdbc`

- [ ] **Step 1: Update `emitFieldAccess()`**

In `src/IRCompiler.cpp`, replace `emitFieldAccess()` with:

```cpp
IRRegister IRCompiler::emitFieldAccess(const FieldAccessExpr& expression)
{
    if (resolvedNames_->hasFieldAccess(expression)) {
        return ir_.emitLoadVar(resolvedNames_->fieldAccessName(expression));
    }
    IRRegister object = compileExpression(*expression.object);
    return ir_.emitField(object, expression.name.lexeme);
}
```

- [ ] **Step 2: Build and refresh golden outputs**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

Expected: namespace import success fixtures get expected AST/IR/bytecode files only if the runner creates them for cases with missing expected outputs. Review the diff and remove unrelated generated files if a full update creates outputs for unrelated cases.

- [ ] **Step 3: Refresh bytecode artifact**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --update
```

Expected: `tests/bytecode_artifacts/namespace_imports/expected.cdbc` is created. Review it and verify there is no namespace opcode.

- [ ] **Step 4: Verify focused behavior with full suites available in this repository**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: all three commands exit 0.

- [ ] **Step 5: Commit IR and refreshed outputs**

```bash
git add src/IRCompiler.cpp tests/golden tests/bytecode_artifacts tests/run_rust_vm_tests.py
git commit -m "test: refresh namespace import outputs"
```

---

### Task 5: Update documentation and roadmap

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update EBNF grammar**

In `docs/language-grammar.ebnf`, replace:

```ebnf
importDecl  = "import", string, ";" ;
```

with:

```ebnf
importDecl  = "import", string,
              [ "as", identifier ], ";" ;
```

Add qualified type support by changing `typeExpr` to include `qualifiedType` before `simpleType`:

```ebnf
typeExpr    = arrayType
            | functionType
            | qualifiedType
            | simpleType ;

qualifiedType = identifier, ".", identifier ;
```

Change struct constructor grammar to:

```ebnf
structConstructor = [ identifier, "." ], identifier, "{", [ fields ], "}" ;
```

- [ ] **Step 2: Update README syntax and source import docs**

In the syntax block, replace:

```text
import "path";
```

with:

```text
import "path" [as alias];
```

In the Source imports section, add this example after the direct import example:

```cd
// main.cd
import "./lib.cd" as lib;
print lib.visible();
```

Add this paragraph:

```markdown
Using `import "path" as alias;` keeps exported names out of the importing
file's top-level scope. Values and functions are accessed as `alias.name`, and
exported struct types may be used as `alias.Type` in annotations and
constructors such as `alias.Type { field: value }`.
```

Update the limitations paragraph to remove `import ... as name` from the not-implemented list while keeping re-export syntax, package search paths, separate compilation, and stdin imports as not implemented.

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, update Phase 14 status to say namespace imports are implemented. Replace the remaining future-work bullet for namespace imports with an implemented suggested feature bullet:

```markdown
- Namespace imports with `import "path" as name;` for qualified access to exported values, functions, and structs. Implemented.
```

Keep package/module search paths, re-export syntax, and separate compilation in remaining future work.

- [ ] **Step 4: Update AGENTS project memory**

In `AGENTS.md`, update the module semantics bullet to mention namespace imports:

```text
Top-level `import "path";` directives load dependency files relative to the importing file. Direct imports expose exported names in the importing module's top-level scope, while `import "path" as alias;` exposes them through qualified `alias.name` access. Imported files have module-private top-level scope; standalone `export name[, name...];` lists expose selected already-defined top-level declarations to importers, while private declarations remain visible only inside the imported file.
```

Keep the duplicate canonical import, stdin rejection, and remaining non-goals sentence.

- [ ] **Step 5: Verify docs and commit**

Run:

```bash
grep -R "import \.\.\. as name\|import \.\.\. as" -n README.md docs/roadmap.md AGENTS.md || true
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: no stale wording that says alias imports are unimplemented; golden tests still pass.

Commit:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document namespace imports"
```

---

### Task 6: Full verification and cleanup

**Files:**
- No source changes expected except generated cache cleanup.

- [ ] **Step 1: Run full verification**

Run:

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

- [ ] **Step 2: Check workspace**

Run:

```bash
git status --short
git diff --stat
```

Expected: no untracked generated files and no unstaged changes. If intentional changes remain, inspect with `git diff`, stage with `git add -A`, and commit with:

```bash
git commit -m "chore: finalize namespace imports"
```

- [ ] **Step 3: Report verification evidence**

In the final response, include each verification command and its result. Do not claim completion without the fresh verification output from Step 1.
