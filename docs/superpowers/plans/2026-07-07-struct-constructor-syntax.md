# Struct Constructor Syntax Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `Name { ... }` named struct constructor expressions that infer the named struct static type while reusing existing runtime struct values.

**Architecture:** Add a `StructConstructExpr` AST node parsed from `Identifier LeftBrace ... RightBrace`. Type checking reuses a shared exact-field named-struct validator and returns `namedStructType(name)`. IR lowering emits the same `Struct` IR as anonymous struct literals, so bytecode and Rust VM behavior stay unchanged except for new source programs.

**Tech Stack:** C++17 recursive-descent parser, AST printer, TypeChecker, register IR compiler, C++ IR interpreter, bytecode artifact emitter, Rust VM parity tests, Python golden tests, CMake/CTest.

---

## File Structure

- Modify `include/Ast.hpp`: add `StructConstructExpr` alongside `StructExpr`.
- Modify `src/Ast.cpp`: construct and print `(construct Name field: value ...)`.
- Modify `include/Parser.hpp`: add `structConstructor()` and optionally `structLiteralFields()` helper declarations.
- Modify `src/Parser.cpp`: parse `Identifier "{" fields "}"` before variable expressions.
- Modify `include/TypeChecker.hpp`: add `checkStructConstructor()` and refactor named struct field checking to accept a token and field vector.
- Modify `src/TypeChecker.cpp`: validate constructors against `structTypes_`, return named struct type, keep annotated anonymous literal behavior.
- Modify `include/IRCompiler.hpp`: add a field-vector helper or constructor emitter declaration.
- Modify `src/IRCompiler.cpp`: lower `StructConstructExpr` with the existing struct IR path.
- Modify `docs/language-grammar.ebnf`, `README.md`, and `docs/roadmap.md` after behavior is implemented.
- Modify `tests/run_rust_vm_tests.py`: add constructor success case to the Rust VM golden allow-list.
- Create success and error fixtures under `tests/golden/`.

---

### Task 1: Parse and print constructor expressions

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Create: `tests/golden/parse_errors/struct_constructor_missing_value.cd`
- Create: `tests/golden/parse_errors/struct_constructor_missing_value.err`
- Create: `tests/golden/parse_errors/struct_constructor_missing_value.exit`

- [ ] **Step 1: Write constructor-specific parse-error fixture**

Create `tests/golden/parse_errors/struct_constructor_missing_value.cd`:

```cd
struct Person { name: string }
let p = Person { name: };
```

Create `tests/golden/parse_errors/struct_constructor_missing_value.err`:

```text
Parse error at 2:24: expected expression, found RightBrace `}`
```

Create `tests/golden/parse_errors/struct_constructor_missing_value.exit`:

```text
1
```

- [ ] **Step 2: Run focused golden tests and confirm RED**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. `struct_constructor_missing_value` should fail with a different parse message because `Person { ... }` is not yet parsed as a constructor expression.

- [ ] **Step 3: Add `StructConstructExpr` to `include/Ast.hpp`**

Add this node immediately after `StructExpr`:

```cpp
struct StructConstructExpr final : Expr {
    StructConstructExpr(Token name, std::vector<StructField> fields);
    void print(std::ostream& out) const override;

    Token name;
    std::vector<StructField> fields;
};
```

- [ ] **Step 4: Add constructor AST implementation in `src/Ast.cpp`**

Add this constructor and printer immediately after `StructExpr::print`:

```cpp
StructConstructExpr::StructConstructExpr(Token name, std::vector<StructField> fields)
    : name(std::move(name))
    , fields(std::move(fields))
{
}

void StructConstructExpr::print(std::ostream& out) const
{
    out << "(construct " << name.lexeme;
    for (const StructField& field : fields) {
        out << ' ' << field.name.lexeme << ": ";
        writeExpr(out, field.value);
    }
    out << ')';
}
```

- [ ] **Step 5: Add parser declarations in `include/Parser.hpp`**

Change the expression helper section to include these declarations:

```cpp
ExprPtr arrayLiteral(Token bracket);
std::vector<StructField> structLiteralFields();
ExprPtr structLiteral();
ExprPtr structConstructor();
ExprPtr functionExpression();
ExprPtr primary();
```

- [ ] **Step 6: Refactor field parsing and parse constructors in `src/Parser.cpp`**

Replace `Parser::structLiteral` with this helper plus wrapper:

```cpp
std::vector<StructField> Parser::structLiteralFields()
{
    std::vector<StructField> fields;
    if (!check(TokenType::RightBrace)) {
        do {
            Token name = consume(TokenType::Identifier, "expected struct field name");
            consume(TokenType::Colon, "expected `:` after struct field name");
            ExprPtr value = expression();
            fields.push_back(StructField{std::move(name), std::move(value)});
        } while (match(TokenType::Comma));
    }
    return fields;
}

ExprPtr Parser::structLiteral()
{
    std::vector<StructField> fields = structLiteralFields();
    consume(TokenType::RightBrace, "expected `}` after struct fields");
    return std::make_unique<StructExpr>(std::move(fields));
}
```

Add this constructor parser after `structLiteral()`:

```cpp
ExprPtr Parser::structConstructor()
{
    Token name = consume(TokenType::Identifier, "expected struct constructor name");
    consume(TokenType::LeftBrace, "expected `{` after struct constructor name");
    std::vector<StructField> fields = structLiteralFields();
    consume(TokenType::RightBrace, "expected `}` after struct fields");
    return std::make_unique<StructConstructExpr>(std::move(name), std::move(fields));
}
```

In `Parser::primary`, add this branch before the existing `Identifier` branch:

```cpp
if (check(TokenType::Identifier) && checkNext(TokenType::LeftBrace)) {
    return structConstructor();
}
```

Keep the existing anonymous struct branch unchanged:

```cpp
if (match(TokenType::LeftBrace)) {
    return structLiteral();
}
```

- [ ] **Step 7: Run parser/AST verification**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS for the constructor parse-error fixture and no regressions. If the parse-error fixture reports the same location and reason but the token spelling differs, update only `tests/golden/parse_errors/struct_constructor_missing_value.err`.

- [ ] **Step 8: Commit parser and AST support**

Run:

```bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp tests/golden/parse_errors/struct_constructor_missing_value.*
git commit -m "feat: parse struct constructor expressions"
```

---

### Task 2: Type check constructor expressions

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Create: `tests/golden/struct_constructor_basic/input.cd`
- Create: `tests/golden/struct_constructor_basic/ast.out`
- Create: `tests/golden/type_errors/struct_constructor_unknown_type.cd`
- Create: `tests/golden/type_errors/struct_constructor_unknown_type.err`
- Create: `tests/golden/type_errors/struct_constructor_unknown_type.exit`
- Create: `tests/golden/type_errors/struct_constructor_missing_field.cd`
- Create: `tests/golden/type_errors/struct_constructor_missing_field.err`
- Create: `tests/golden/type_errors/struct_constructor_missing_field.exit`
- Create: `tests/golden/type_errors/struct_constructor_extra_field.cd`
- Create: `tests/golden/type_errors/struct_constructor_extra_field.err`
- Create: `tests/golden/type_errors/struct_constructor_extra_field.exit`
- Create: `tests/golden/type_errors/struct_constructor_wrong_field_type.cd`
- Create: `tests/golden/type_errors/struct_constructor_wrong_field_type.err`
- Create: `tests/golden/type_errors/struct_constructor_wrong_field_type.exit`
- Create: `tests/golden/type_errors/struct_constructor_duplicate_field.cd`
- Create: `tests/golden/type_errors/struct_constructor_duplicate_field.err`
- Create: `tests/golden/type_errors/struct_constructor_duplicate_field.exit`

- [ ] **Step 1: Write constructor success AST and type-error fixtures**

Create `tests/golden/struct_constructor_basic/input.cd`:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada", age: 36 };
print p.name;
p.age = 37;
print p.age;
```

Create `tests/golden/struct_constructor_basic/ast.out`:

```text
Program
  Struct Person {name: string, age: number}
  Let p = (construct Person name: "Ada" age: 36)
  Print (field p name)
  Expr (= (field p age) 37)
  Print (field p age)
```

Create `tests/golden/type_errors/struct_constructor_unknown_type.cd`:

```cd
let p = Person { name: "Ada" };
```

Create `tests/golden/type_errors/struct_constructor_unknown_type.err`:

```text
Type error at 1:9: unknown struct type `Person`
```

Create `tests/golden/type_errors/struct_constructor_unknown_type.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_constructor_missing_field.cd`:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada" };
```

Create `tests/golden/type_errors/struct_constructor_missing_field.err`:

```text
Type error at 2:9: missing field `age` for struct `Person`
```

Create `tests/golden/type_errors/struct_constructor_missing_field.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_constructor_extra_field.cd`:

```cd
struct Person { name: string }
let p = Person { name: "Ada", age: 36 };
```

Create `tests/golden/type_errors/struct_constructor_extra_field.err`:

```text
Type error at 2:31: extra field `age` for struct `Person`
```

Create `tests/golden/type_errors/struct_constructor_extra_field.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_constructor_wrong_field_type.cd`:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada", age: "old" };
```

Create `tests/golden/type_errors/struct_constructor_wrong_field_type.err`:

```text
Type error at 2:31: field `age` expects number, got string
```

Create `tests/golden/type_errors/struct_constructor_wrong_field_type.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_constructor_duplicate_field.cd`:

```cd
struct Person { name: string }
let p = Person { name: "Ada", name: "Grace" };
```

Create `tests/golden/type_errors/struct_constructor_duplicate_field.err`:

```text
Type error at 2:31: duplicate field `name` in struct literal
```

Create `tests/golden/type_errors/struct_constructor_duplicate_field.exit`:

```text
1
```

- [ ] **Step 2: Run golden tests and confirm RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. `struct_constructor_basic` and constructor type-error fixtures should not yet pass because `StructConstructExpr` has no type checker branch.

- [ ] **Step 3: Update declarations in `include/TypeChecker.hpp`**

Replace the existing `checkNamedStructLiteralInitializer` declaration with these declarations:

```cpp
CheckedExpression checkNamedStructFields(
    const Token& diagnosticToken,
    const TypeInfo& declared,
    const std::vector<StructField>& fields);
CheckedExpression checkNamedStructLiteralInitializer(
    const LetStmt& statement,
    const TypeInfo& declared,
    const StructExpr& initializer);
CheckedExpression checkStructConstructor(const StructConstructExpr& expression);
```

- [ ] **Step 4: Refactor named struct field checking in `src/TypeChecker.cpp`**

Replace the body/signature of `checkNamedStructLiteralInitializer` with a shared field checker plus a thin wrapper:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkNamedStructFields(
    const Token& diagnosticToken,
    const TypeInfo& declared,
    const std::vector<StructField>& fields)
{
    const StructTypeDecl* structType = declared.structName ? findStructType(*declared.structName) : nullptr;
    if (!structType) {
        throw TypeError(diagnosticToken, "unknown struct type `" + typeInfoName(declared) + "`");
    }

    std::unordered_map<std::string, const StructField*> literalFields;
    for (const StructField& field : fields) {
        if (literalFields.find(field.name.lexeme) != literalFields.end()) {
            throw TypeError(field.name, "duplicate field `" + field.name.lexeme + "` in struct literal");
        }
        literalFields.emplace(field.name.lexeme, &field);
    }

    for (const StructFieldType& expectedField : structType->fields) {
        const auto found = literalFields.find(expectedField.name.lexeme);
        if (found == literalFields.end()) {
            throw TypeError(diagnosticToken,
                "missing field `" + expectedField.name.lexeme + "` for struct `" + structType->name.lexeme + "`");
        }
        const CheckedExpression actual = checkExpressionInfo(*found->second->value, &expectedField.type);
        if (!compatible(expectedField.type, actual.type)) {
            throw TypeError(found->second->name,
                "field `" + expectedField.name.lexeme + "` expects " + typeInfoName(expectedField.type)
                    + ", got " + typeInfoName(actual.type));
        }
    }

    for (const StructField& field : fields) {
        if (!findStructField(*structType, field.name.lexeme)) {
            throw TypeError(field.name,
                "extra field `" + field.name.lexeme + "` for struct `" + structType->name.lexeme + "`");
        }
    }

    return CheckedExpression{declared};
}

TypeChecker::CheckedExpression TypeChecker::checkNamedStructLiteralInitializer(
    const LetStmt& statement,
    const TypeInfo& declared,
    const StructExpr& initializer)
{
    return checkNamedStructFields(statement.name, declared, initializer.fields);
}
```

- [ ] **Step 5: Add constructor checker in `src/TypeChecker.cpp`**

Add this method after `checkNamedStructLiteralInitializer`:

```cpp
TypeChecker::CheckedExpression TypeChecker::checkStructConstructor(const StructConstructExpr& expression)
{
    const StructTypeDecl* structType = findStructType(expression.name.lexeme);
    if (!structType) {
        throw TypeError(expression.name, "unknown struct type `" + expression.name.lexeme + "`");
    }
    return checkNamedStructFields(expression.name, namedStructType(expression.name.lexeme), expression.fields);
}
```

- [ ] **Step 6: Dispatch constructor expressions in `checkExpressionInfo`**

In `TypeChecker::checkExpressionInfo`, after the anonymous `StructExpr` branch and before field access, add:

```cpp
if (const auto* construct = dynamic_cast<const StructConstructExpr*>(&expression)) {
    return checkStructConstructor(*construct);
}
```

- [ ] **Step 7: Run type checker verification**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS for `struct_constructor_basic` AST output and constructor type-error fixtures. No IR or run output is expected yet, so constructor lowering is not required in this task.

- [ ] **Step 8: Commit constructor type checking**

Run:

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/struct_constructor_basic tests/golden/type_errors/struct_constructor_unknown_type.* tests/golden/type_errors/struct_constructor_missing_field.* tests/golden/type_errors/struct_constructor_extra_field.* tests/golden/type_errors/struct_constructor_wrong_field_type.* tests/golden/type_errors/struct_constructor_duplicate_field.*
git commit -m "feat: type check struct constructors"
```

---

### Task 3: Lower constructors through existing struct IR

**Files:**
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Create: `tests/golden/struct_constructor_basic/ir.out`
- Create: `tests/golden/struct_constructor_basic/bytecode.out`
- Create: `tests/golden/struct_constructor_basic/run.out`
- Create: `tests/golden/struct_constructor_functions/input.cd`
- Create: `tests/golden/struct_constructor_functions/ast.out`
- Create: `tests/golden/struct_constructor_functions/ir.out`
- Create: `tests/golden/struct_constructor_functions/bytecode.out`
- Create: `tests/golden/struct_constructor_functions/run.out`

- [ ] **Step 1: Add runtime success fixtures**

Create `tests/golden/struct_constructor_basic/run.out`:

```text
Ada
37
```

Create `tests/golden/struct_constructor_functions/input.cd`:

```cd
struct Person { name: string, age: number }

fun birthday(person: Person): Person {
  person.age = person.age + 1;
  return person;
}

fun makePerson(): Person {
  return Person { age: 36, name: "Ada" };
}

let p = birthday(makePerson());
print p.name;
print p.age;
```

Create `tests/golden/struct_constructor_functions/ast.out`:

```text
Program
  Struct Person {name: string, age: number}
  Fun birthday(person: Person): Person
    Expr (= (field person age) (+ (field person age) 1))
    Return person
  Fun makePerson(): Person
    Return (construct Person age: 36 name: "Ada")
  Let p = (call birthday (call makePerson))
  Print (field p name)
  Print (field p age)
```

Create `tests/golden/struct_constructor_functions/run.out`:

```text
Ada
37
```

Do not hand-write `ir.out` or `bytecode.out` yet. They should be generated only after lowering is implemented and reviewed.

- [ ] **Step 2: Run golden tests and confirm RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL for `--run`, `--ir`, or `--bytecode` paths involving constructor expressions because IR lowering does not support `StructConstructExpr` yet.

- [ ] **Step 3: Add IR compiler declarations in `include/IRCompiler.hpp`**

Replace the existing struct emitter declaration with these two declarations:

```cpp
IRRegister emitStructFields(const std::vector<StructField>& fields);
IRRegister emitStruct(const StructExpr& expression);
IRRegister emitStructConstructor(const StructConstructExpr& expression);
```

- [ ] **Step 4: Dispatch and lower constructors in `src/IRCompiler.cpp`**

In `IRCompiler::compileExpression`, after the `StructExpr` branch, add:

```cpp
if (const auto* construct = dynamic_cast<const StructConstructExpr*>(&expression)) {
    return emitStructConstructor(*construct);
}
```

Replace `emitStruct` with this helper plus two wrappers:

```cpp
IRRegister IRCompiler::emitStructFields(const std::vector<StructField>& fields)
{
    std::vector<std::size_t> names;
    std::vector<IRRegister> values;
    names.reserve(fields.size());
    values.reserve(fields.size());
    for (const StructField& field : fields) {
        names.push_back(ir_.addName(field.name.lexeme));
        values.push_back(compileExpression(*field.value));
    }
    return ir_.emitStruct(std::move(names), std::move(values));
}

IRRegister IRCompiler::emitStruct(const StructExpr& expression)
{
    return emitStructFields(expression.fields);
}

IRRegister IRCompiler::emitStructConstructor(const StructConstructExpr& expression)
{
    return emitStructFields(expression.fields);
}
```

- [ ] **Step 5: Generate and review IR/bytecode goldens**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

Expected: the update command exits 0 and creates only intentional missing expected files or changed files for constructor fixtures. Immediately inspect:

```bash
git diff -- tests/golden/struct_constructor_basic tests/golden/struct_constructor_functions | sed -n '1,240p'
git status --short
```

Keep only these intended files:

```text
tests/golden/struct_constructor_basic/ir.out
tests/golden/struct_constructor_basic/bytecode.out
tests/golden/struct_constructor_functions/ir.out
tests/golden/struct_constructor_functions/bytecode.out
```

If `--update` creates unrelated golden files, remove them with `git clean -fd <paths>` and restore unrelated modifications with `git restore <paths>` before continuing.

- [ ] **Step 6: Run golden tests after generated outputs**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS with constructor AST, IR, bytecode, and run fixtures.

- [ ] **Step 7: Commit constructor lowering and runtime coverage**

Run:

```bash
git add include/IRCompiler.hpp src/IRCompiler.cpp tests/golden/struct_constructor_basic tests/golden/struct_constructor_functions
git commit -m "feat: lower struct constructors"
```

---

### Task 4: Update docs, roadmap, and Rust VM parity

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Update language grammar**

In `docs/language-grammar.ebnf`, add constructor syntax near the existing struct literal grammar:

```ebnf
structConstructor = identifier, "{", [ fields ], "}" ;
```

Then update `primary` so the relevant section is:

```ebnf
primary     = functionExpr
            | false
            | true
            | nil
            | number
            | string
            | array
            | structConstructor
            | struct
            | identifier
            | "(", expression, ")" ;
```

- [ ] **Step 2: Update README named struct section**

Replace the named struct example in `README.md` with:

````markdown
Named struct declarations define static field shapes:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada", age: 36 };
print p.name;
p.age = 37;
```

Named structs are static-only in this phase: runtime values remain anonymous struct values. Named constructor expressions such as `Person { name: "Ada", age: 36 }` infer the named static type, require an exact field match, and allow fields in any order. Annotated anonymous literals such as `let p: Person = { name: "Ada", age: 36 };` remain supported. Field access/assignment on known named struct values is statically checked. Constructor functions such as `Person(...)`, methods, recursive struct types, and runtime type names are not implemented yet.
````

- [ ] **Step 3: Update roadmap Phase 12 status**

In `docs/roadmap.md`, update the Phase 12 status paragraph to include:

```markdown
Phase 12D is implemented: named struct constructor expressions `Name { ... }` infer named struct types while reusing anonymous runtime struct values.
```

Also update the remaining future-work sentence so constructor syntax is no longer listed as future work.

- [ ] **Step 4: Add Rust VM parity allow-list entry**

In `tests/run_rust_vm_tests.py`, add this case name to `golden_allowlist`:

```python
"struct_constructor_functions",
```

Place it near existing struct cases, for example after `"struct_identity_equality",`.

- [ ] **Step 5: Run docs/parity verification**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: PASS. Rust VM tests include `struct_constructor_functions` and match `run.out`.

- [ ] **Step 6: Commit docs and parity updates**

Run:

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md tests/run_rust_vm_tests.py
git commit -m "docs: document struct constructors"
```

---

### Task 5: Run full verification and cleanup

**Files:**
- No planned source edits unless verification exposes a concrete failure.

- [ ] **Step 1: Run the full verification set from `AGENTS.md`**

Run from the repository root:

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

Expected: every command exits with status 0.

- [ ] **Step 2: Inspect git status**

Run:

```bash
git status --short
```

Expected: no untracked `tests/__pycache__/` and no unintended build artifacts. Source, docs, and golden changes should already be committed, unless Step 1 required a focused fix.

- [ ] **Step 3: Resolve any verification-only leftovers**

If `git status --short` shows only Python cache files, clean them and re-check:

```bash
rm -rf tests/__pycache__
git status --short
```

Expected: no output. If any other files remain, stop and inspect them before reporting completion; do not create a catch-all commit.

- [ ] **Step 4: Report exact verification results**

In the handoff message, list each command from Step 1 and whether it passed. Mention that constructor values reuse existing anonymous struct runtime representation, IR struct op, bytecode struct op, and Rust VM execution path.
