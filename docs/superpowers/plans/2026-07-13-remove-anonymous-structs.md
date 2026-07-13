# Remove Anonymous Structs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove source-language anonymous struct literals so users can only construct declared named structs with `Name { ... }` or `alias.Name { ... }`.

**Architecture:** Stop parsing `{ field: value }` as an expression, delete anonymous `StructExpr` AST/type-check/lowering paths, and migrate tests/docs to named constructors. Keep the shared field-list parser and keep `.cdbc`/Rust VM anonymous `struct { ... }` support as a low-level compatibility form.

**Tech Stack:** C++17 recursive-descent parser, AST/type checker/IR compiler, Python golden tests, bytecode artifact tests, Rust VM parity tests, Markdown docs.

---

## File Structure

Create:

- `tests/golden/parse_errors/anonymous_struct_literal.cd`
- `tests/golden/parse_errors/anonymous_struct_literal.err`
- `tests/golden/parse_errors/anonymous_struct_literal.exit`
- `tests/golden/parse_errors/anonymous_struct_annotated_initializer.cd`
- `tests/golden/parse_errors/anonymous_struct_annotated_initializer.err`
- `tests/golden/parse_errors/anonymous_struct_annotated_initializer.exit`
- `tests/golden/parse_errors/anonymous_struct_nested_literal.cd`
- `tests/golden/parse_errors/anonymous_struct_nested_literal.err`
- `tests/golden/parse_errors/anonymous_struct_nested_literal.exit`

Modify:

- `include/Ast.hpp` — remove anonymous `StructExpr` node.
- `src/Ast.cpp` — remove anonymous `StructExpr` constructor/printer.
- `include/Parser.hpp` — remove `structLiteral()` declaration.
- `src/Parser.cpp` — stop accepting `LeftBrace` in `primary()`; keep named constructor parsing.
- `include/TypeChecker.hpp` — remove `checkNamedStructLiteralInitializer` declaration.
- `src/TypeChecker.cpp` — remove annotated anonymous initializer special case and anonymous `StructExpr` expression checking.
- `include/IRCompiler.hpp` — remove `emitStruct(const StructExpr&)` declaration.
- `src/IRCompiler.cpp` — remove anonymous `StructExpr` lowering branch and helper.
- `README.md` — document only named struct constructors.
- `docs/language-grammar.ebnf` — remove anonymous `struct` expression production.
- `docs/roadmap.md` — mark source anonymous structs removed under Phase 12 current status.
- `AGENTS.md` — update project memory to say source construction requires declared named structs.
- Golden fixture inputs/expected outputs that currently use anonymous struct source literals.
- Bytecode artifact fixture inputs/expected `.cdbc`/`run.out` files that currently use anonymous struct source literals.

Do not modify:

- `docs/bytecode-text-format.md` — anonymous `.cdbc` struct instructions remain supported.
- `vm-rs/src/*` — Rust VM continues to parse/execute anonymous `.cdbc` structs for compatibility.
- `include/IR.hpp`, `src/IR.cpp`, `include/Bytecode.hpp`, `src/Bytecode.cpp`, `src/BytecodeTextEmitter.cpp` — anonymous low-level struct instructions remain available for artifacts/compatibility.

---

### Task 1: Add RED parse-error coverage and migrate source fixtures to named constructors

**Files:**
- Create: `tests/golden/parse_errors/anonymous_struct_literal.cd`
- Create: `tests/golden/parse_errors/anonymous_struct_literal.err`
- Create: `tests/golden/parse_errors/anonymous_struct_literal.exit`
- Create: `tests/golden/parse_errors/anonymous_struct_annotated_initializer.cd`
- Create: `tests/golden/parse_errors/anonymous_struct_annotated_initializer.err`
- Create: `tests/golden/parse_errors/anonymous_struct_annotated_initializer.exit`
- Create: `tests/golden/parse_errors/anonymous_struct_nested_literal.cd`
- Create: `tests/golden/parse_errors/anonymous_struct_nested_literal.err`
- Create: `tests/golden/parse_errors/anonymous_struct_nested_literal.exit`
- Modify: `tests/golden/struct_literals_field_access/input.cd`
- Modify: `tests/golden/struct_field_assignment/input.cd`
- Modify: `tests/golden/struct_identity_equality/input.cd`
- Modify: `tests/golden/native_stdlib_typeof/input.cd`
- Modify: `tests/golden/struct_runtime_type_name/input.cd`
- Modify: `tests/golden/runtime_errors/field_compound_dynamic_non_number_old.cd`
- Modify: `tests/golden/runtime_errors/struct_missing_field.cd`
- Modify: `tests/golden/runtime_errors/struct_field_assignment_missing_field.cd`
- Modify: `tests/golden/type_errors/named_struct_missing_field.cd`
- Modify: `tests/golden/type_errors/named_struct_extra_field.cd`
- Modify: `tests/golden/type_errors/named_struct_wrong_field_assignment_type.cd`
- Modify: `tests/golden/type_errors/named_struct_duplicate_literal_field.cd`
- Modify: `tests/golden/type_errors/named_struct_wrong_field_type.cd`
- Modify: `tests/golden/type_errors/named_struct_unknown_field_access.cd`
- Modify: `tests/golden/type_errors/named_struct_nominal_mismatch.cd`
- Modify: `tests/golden/type_errors/named_struct_unknown_field_assignment.cd`
- Modify: `tests/golden/named_struct_types/input.cd`
- Modify: `tests/bytecode_artifacts/structs/input.cd`
- Modify: `tests/bytecode_artifacts/struct_field_assignment/input.cd`
- Modify: `tests/bytecode_artifacts/native_stdlib_typeof/input.cd`
- Modify: `tests/bytecode_artifacts/native_stdlib_typeof/run.out`
- Modify: `tests/bytecode_artifacts/named_struct_types/input.cd`

- [ ] **Step 1: Add parse-error fixtures for removed anonymous struct syntax**

```sh
cat > tests/golden/parse_errors/anonymous_struct_literal.cd <<'CASE'
let p = { name: "Ada" };
CASE
cat > tests/golden/parse_errors/anonymous_struct_literal.err <<'CASE'
Parse error at 1:9: anonymous struct literals are not supported; use StructName { ... }
CASE
cat > tests/golden/parse_errors/anonymous_struct_literal.exit <<'CASE'
1
CASE

cat > tests/golden/parse_errors/anonymous_struct_annotated_initializer.cd <<'CASE'
struct Person { name: string }
let p: Person = { name: "Ada" };
CASE
cat > tests/golden/parse_errors/anonymous_struct_annotated_initializer.err <<'CASE'
Parse error at 2:17: anonymous struct literals are not supported; use StructName { ... }
CASE
cat > tests/golden/parse_errors/anonymous_struct_annotated_initializer.exit <<'CASE'
1
CASE

cat > tests/golden/parse_errors/anonymous_struct_nested_literal.cd <<'CASE'
struct Person { name: string }
print Person { name: { value: "Ada" } };
CASE
cat > tests/golden/parse_errors/anonymous_struct_nested_literal.err <<'CASE'
Parse error at 2:22: anonymous struct literals are not supported; use StructName { ... }
CASE
cat > tests/golden/parse_errors/anonymous_struct_nested_literal.exit <<'CASE'
1
CASE
```

- [ ] **Step 2: Rewrite successful anonymous struct fixtures as named constructor fixtures**

```sh
cat > tests/golden/struct_literals_field_access/input.cd <<'CASE'
struct Person { name: string, age: number }
struct Point { x: number, y: number }
struct Nested { point: Point, label: string }
struct Empty {}

let person = Person { name: "Ada", age: 36 };
print person.name;
print person.age;

let nested = Nested { point: Point { x: 1, y: 2 }, label: "p" };
print nested.point.x;
print Empty {};
CASE

cat > tests/golden/struct_field_assignment/input.cd <<'CASE'
struct Person { name: string, age: number }
struct Inner { x: number }
struct Outer { inner: Inner }
struct Box { x: number }

let person = Person { name: "Ada", age: 36 };
person.age = 37;
print person.age;
print person.age = 38;

let outer = Outer { inner: Inner { x: 1 } };
outer.inner.x = 2;
print outer.inner.x;

let a = Box { x: 1 };
let b = a;
b.x = 3;
print a.x;
CASE

cat > tests/golden/struct_identity_equality/input.cd <<'CASE'
struct Box { x: number }
let a = Box { x: 1 };
let b = a;
let c = Box { x: 1 };
print a == b;
print a == c;
fun id(value) {
  return value;
}
print id(a).x;
CASE

cat > tests/golden/native_stdlib_typeof/input.cd <<'CASE'
print typeOf(nil);
print typeOf(1);
print typeOf(true);
print typeOf("x");
print typeOf(fun () { return nil; });
print typeOf([1]);
struct Box { value: number }
print typeOf(Box { value: 1 });
let typeOf = fun (value) { return "shadowed"; };
print typeOf(123);
CASE
cat > tests/golden/native_stdlib_typeof/run.out <<'CASE'
nil
number
bool
string
function
array
Box
shadowed
CASE

cat > tests/golden/struct_runtime_type_name/input.cd <<'CASE'
struct Person { name: string }
let p = Person { name: "Ada" };
let q = p;
q.name = "Grace";
print typeOf(p);
print p.name;
CASE
cat > tests/golden/struct_runtime_type_name/run.out <<'CASE'
Person
Grace
CASE
```

- [ ] **Step 3: Rewrite runtime-error fixtures that used anonymous structs**

```sh
cat > tests/golden/runtime_errors/field_compound_dynamic_non_number_old.cd <<'CASE'
struct Counter { count: string }
let value = Counter { count: "bad" };
value.count += 1;
CASE

cat > tests/golden/runtime_errors/struct_missing_field.cd <<'CASE'
struct Person { name: string }
let person = Person { name: "Ada" };
print person.age;
CASE

cat > tests/golden/runtime_errors/struct_field_assignment_missing_field.cd <<'CASE'
struct Empty {}
let a = Empty {};
a.missing = 1;
CASE
```

- [ ] **Step 4: Rewrite named-struct type-error fixtures to use named constructors**

```sh
cat > tests/golden/type_errors/named_struct_missing_field.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada" };
CASE
cat > tests/golden/type_errors/named_struct_extra_field.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", age: 36, title: "Dr" };
CASE
cat > tests/golden/type_errors/named_struct_wrong_field_assignment_type.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", age: 36 };
p.age = "old";
CASE
cat > tests/golden/type_errors/named_struct_duplicate_literal_field.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", name: "Grace", age: 36 };
CASE
cat > tests/golden/type_errors/named_struct_wrong_field_type.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", age: "old" };
CASE
cat > tests/golden/type_errors/named_struct_unknown_field_access.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", age: 36 };
print p.title;
CASE
cat > tests/golden/type_errors/named_struct_nominal_mismatch.cd <<'CASE'
struct Person { name: string }
struct Pet { name: string }
let pet: Pet = Pet { name: "Milo" };
let person: Person = pet;
CASE
cat > tests/golden/type_errors/named_struct_unknown_field_assignment.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", age: 36 };
p.title = "Dr";
CASE
```

- [ ] **Step 5: Rewrite named struct success fixture with explicit constructor**

```sh
cat > tests/golden/named_struct_types/input.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", age: 36 };
p.age = 37;
print p.name;
print p.age;
CASE
```

- [ ] **Step 6: Rewrite bytecode artifact inputs to use named constructors**

```sh
cat > tests/bytecode_artifacts/structs/input.cd <<'CASE'
struct Person { name: string, age: number }
struct Point { x: number }
struct Nested { point: Point, label: string }
let person = Person { name: "Ada", age: 36 };
print person.name;
let nested = Nested { point: Point { x: 1 }, label: "p" };
print nested.point.x;
CASE

cat > tests/bytecode_artifacts/struct_field_assignment/input.cd <<'CASE'
struct Person { name: string, age: number }
let person = Person { name: "Ada", age: 36 };
person.age = 37;
print person.age;
CASE

cat > tests/bytecode_artifacts/native_stdlib_typeof/input.cd <<'CASE'
print typeOf(nil);
print typeOf(1);
print typeOf([1]);
struct Box { value: number }
print typeOf(Box { value: 1 });
CASE
cat > tests/bytecode_artifacts/native_stdlib_typeof/run.out <<'CASE'
nil
number
array
Box
CASE

cat > tests/bytecode_artifacts/named_struct_types/input.cd <<'CASE'
struct Person { name: string, age: number }
let p: Person = Person { name: "Ada", age: 36 };
p.age = 37;
print p.name;
print p.age;
CASE
```

- [ ] **Step 7: Run RED checks**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case anonymous_struct_literal \
  --case anonymous_struct_annotated_initializer \
  --case anonymous_struct_nested_literal \
  --case native_stdlib_typeof \
  --case struct_runtime_type_name \
  --case named_struct_types
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected before implementation: parse-error cases fail because anonymous struct literals are still accepted by the parser, and artifact tests may fail because expected files have not been refreshed yet. Named-constructor rewritten success cases should still compile.

- [ ] **Step 8: Commit RED coverage/input migration**

```sh
git add \
  tests/golden/parse_errors/anonymous_struct_literal.* \
  tests/golden/parse_errors/anonymous_struct_annotated_initializer.* \
  tests/golden/parse_errors/anonymous_struct_nested_literal.* \
  tests/golden/struct_literals_field_access/input.cd \
  tests/golden/struct_field_assignment/input.cd \
  tests/golden/struct_identity_equality/input.cd \
  tests/golden/native_stdlib_typeof/input.cd tests/golden/native_stdlib_typeof/run.out \
  tests/golden/struct_runtime_type_name/input.cd tests/golden/struct_runtime_type_name/run.out \
  tests/golden/runtime_errors/field_compound_dynamic_non_number_old.cd \
  tests/golden/runtime_errors/struct_missing_field.cd \
  tests/golden/runtime_errors/struct_field_assignment_missing_field.cd \
  tests/golden/type_errors/named_struct_missing_field.cd \
  tests/golden/type_errors/named_struct_extra_field.cd \
  tests/golden/type_errors/named_struct_wrong_field_assignment_type.cd \
  tests/golden/type_errors/named_struct_duplicate_literal_field.cd \
  tests/golden/type_errors/named_struct_wrong_field_type.cd \
  tests/golden/type_errors/named_struct_unknown_field_access.cd \
  tests/golden/type_errors/named_struct_nominal_mismatch.cd \
  tests/golden/type_errors/named_struct_unknown_field_assignment.cd \
  tests/golden/named_struct_types/input.cd \
  tests/bytecode_artifacts/structs/input.cd \
  tests/bytecode_artifacts/struct_field_assignment/input.cd \
  tests/bytecode_artifacts/native_stdlib_typeof/input.cd tests/bytecode_artifacts/native_stdlib_typeof/run.out \
  tests/bytecode_artifacts/named_struct_types/input.cd
git commit -m "test: migrate source fixtures to named structs"
```

---

### Task 2: Remove anonymous struct parsing and AST/type/lowering paths

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Remove anonymous `StructExpr` declaration**

In `include/Ast.hpp`, delete this block:

```cpp
struct StructExpr final : Expr {
    explicit StructExpr(std::vector<StructField> fields);
    void print(std::ostream& out) const override;

    std::vector<StructField> fields;
};
```

Keep `StructField` because named constructors still use it.

- [ ] **Step 2: Remove anonymous `StructExpr` implementation**

In `src/Ast.cpp`, delete:

```cpp
StructExpr::StructExpr(std::vector<StructField> fields)
    : fields(std::move(fields))
{
}

void StructExpr::print(std::ostream& out) const
{
    out << "(struct";
    for (const StructField& field : fields) {
        out << " " << field.name.lexeme << ": ";
        field.value->print(out);
    }
    out << ")";
}
```

- [ ] **Step 3: Remove parser declaration for anonymous struct literals**

In `include/Parser.hpp`, delete:

```cpp
    ExprPtr structLiteral();
```

Keep `structLiteralFields()` and named constructor methods.

- [ ] **Step 4: Reject `{` in expression position**

In `src/Parser.cpp`, delete the full `Parser::structLiteral()` function.

In `Parser::primary()`, replace:

```cpp
    if (match(TokenType::LeftBrace)) {
        return structLiteral();
    }
```

with:

```cpp
    if (check(TokenType::LeftBrace)) {
        throw ParseError(peek(), "anonymous struct literals are not supported; use StructName { ... }");
    }
```

Do not consume the `{`; this keeps the diagnostic location on the brace.

- [ ] **Step 5: Remove annotated anonymous initializer type-check special case**

In `include/TypeChecker.hpp`, delete the declaration:

```cpp
    CheckedExpression checkNamedStructLiteralInitializer(
        const LetStmt& statement,
        const TypeInfo& declared,
        const StructExpr& initializer);
```

In `src/TypeChecker.cpp`, in `TypeChecker::checkLetInitializer`, delete this block:

```cpp
    if (declared.kind == StaticType::Struct && declared.structName) {
        if (const auto* structLiteral = dynamic_cast<const StructExpr*>(statement.initializer.get())) {
            return checkNamedStructLiteralInitializer(statement, declared, *structLiteral);
        }
    }
```

Also delete the full `TypeChecker::checkNamedStructLiteralInitializer(...)` function.

- [ ] **Step 6: Remove anonymous `StructExpr` expression type-check branch**

In `src/TypeChecker.cpp`, in `TypeChecker::checkExpressionInfo`, delete:

```cpp
    if (const auto* structExpr = dynamic_cast<const StructExpr*>(&expression)) {
        for (const StructField& field : structExpr->fields) {
            checkExpression(*field.value);
        }
        return CheckedExpression{simpleType(StaticType::Struct)};
    }
```

- [ ] **Step 7: Remove anonymous struct lowering declaration and implementation**

In `include/IRCompiler.hpp`, delete:

```cpp
    IRRegister emitStruct(const StructExpr& expression);
```

In `src/IRCompiler.cpp`, in `IRCompiler::compileExpression`, delete:

```cpp
    if (const auto* structExpr = dynamic_cast<const StructExpr*>(&expression)) {
        return emitStruct(*structExpr);
    }
```

Also delete:

```cpp
IRRegister IRCompiler::emitStruct(const StructExpr& expression)
{
    return emitStructFields(expression.fields);
}
```

Keep `emitStructFields(...)` because named constructors still use it.

- [ ] **Step 8: Build and run focused GREEN checks**

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case anonymous_struct_literal \
  --case anonymous_struct_annotated_initializer \
  --case anonymous_struct_nested_literal
```

Expected: build succeeds and the three new parse-error fixtures pass.

- [ ] **Step 9: Commit parser/AST/type/lowering removal**

```sh
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp include/TypeChecker.hpp src/TypeChecker.cpp include/IRCompiler.hpp src/IRCompiler.cpp tests/golden/parse_errors/anonymous_struct_literal.* tests/golden/parse_errors/anonymous_struct_annotated_initializer.* tests/golden/parse_errors/anonymous_struct_nested_literal.*
git commit -m "feat: remove anonymous struct expressions"
```

---

### Task 3: Refresh migrated golden and bytecode artifact outputs

**Files:**
- Modify generated `ast.out`, `ir.out`, `bytecode.out`, and `run.out` files under changed success fixtures.
- Modify runtime/type/parse error expected files if diagnostic line locations changed.
- Modify `.cdbc` files under changed bytecode artifact fixtures.

- [ ] **Step 1: Refresh changed success golden outputs**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --update \
  --case struct_literals_field_access \
  --case struct_field_assignment \
  --case struct_identity_equality \
  --case native_stdlib_typeof \
  --case struct_runtime_type_name \
  --case named_struct_types
```

Expected: selected success fixtures refresh to named constructors and no anonymous `(struct ...)` AST nodes remain in these files.

- [ ] **Step 2: Refresh changed type/runtime error outputs if needed**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --update \
  --case field_compound_dynamic_non_number_old \
  --case struct_missing_field \
  --case struct_field_assignment_missing_field \
  --case named_struct_missing_field \
  --case named_struct_extra_field \
  --case named_struct_wrong_field_assignment_type \
  --case named_struct_duplicate_literal_field \
  --case named_struct_wrong_field_type \
  --case named_struct_unknown_field_access \
  --case named_struct_nominal_mismatch \
  --case named_struct_unknown_field_assignment
```

Expected: diagnostics remain semantically equivalent. If line/column changes because constructors add type names, review each `.err` or `.run.err` diff.

- [ ] **Step 3: Refresh changed bytecode artifact expected files**

```sh
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/structs/expected.cdbc tests/bytecode_artifacts/structs/input.cd
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/struct_field_assignment/expected.cdbc tests/bytecode_artifacts/struct_field_assignment/input.cd
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc tests/bytecode_artifacts/native_stdlib_typeof/input.cd
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/named_struct_types/expected.cdbc tests/bytecode_artifacts/named_struct_types/input.cd
```

Expected: these `.cdbc` files use named `struct nType { ... }` source emissions where structs are constructed.

- [ ] **Step 4: Check for leftover source anonymous struct literals**

```sh
python3 - <<'PY'
from pathlib import Path
roots = [Path('tests/golden'), Path('tests/bytecode_artifacts')]
patterns = ['= {', 'print {', 'return {']
for root in roots:
    for path in root.rglob('*.cd'):
        text = path.read_text()
        if any(pattern in text for pattern in patterns):
            print(path)
PY
```

Expected: only parse-error fixtures intentionally covering removed anonymous syntax may be printed. If success/type/runtime/artifact fixtures are printed, rewrite them before continuing.

- [ ] **Step 5: Run focused checks**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design \
  --case struct_literals_field_access \
  --case struct_field_assignment \
  --case struct_identity_equality \
  --case native_stdlib_typeof \
  --case struct_runtime_type_name \
  --case named_struct_types \
  --case anonymous_struct_literal \
  --case anonymous_struct_annotated_initializer \
  --case anonymous_struct_nested_literal
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens \
  --case struct_literals_field_access \
  --case struct_field_assignment \
  --case struct_identity_equality \
  --case native_stdlib_typeof \
  --case struct_runtime_type_name \
  --case named_struct_types
```

Expected: all selected checks pass.

- [ ] **Step 6: Commit refreshed outputs**

```sh
git add tests/golden tests/bytecode_artifacts
git commit -m "test: refresh named-only struct outputs"
```

---

### Task 4: Update user and project documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README type inference paragraph**

In `README.md`, replace the sentence fragment:

```text
Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and anonymous `struct`;
```

with:

```text
Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and named struct constructors;
```

- [ ] **Step 2: Update README struct section**

Replace the paragraph beginning `Struct literals use` with:

```markdown
Struct values are created with named constructor expressions such as `Person { name: "Ada", age: 36 }` after a matching `struct Person { ... }` declaration. Constructors preserve declared field behavior, require exact field names, and allow fields in any order. Field reads use `value.field`. Existing fields can be reassigned with `value.field = expression`; the assignment evaluates to the assigned value. Structs are reference values with identity equality, so aliases observe field mutation. Assigning a missing field is a runtime error when the target type is not statically known, and a type error when it is known.
```

Replace the named constructor paragraph with:

```markdown
Named constructor expressions infer the named static type and attach the named runtime type used by `typeOf`; all structs keep the same field-only print format. Annotated bindings must still use an explicit constructor, for example `let p: Person = Person { name: "Ada", age: 36 };`. Field annotations may refer to non-recursive struct names declared later in the same scope, but recursive struct field types such as `struct Node { next: Node? }` are explicitly rejected for now. Field access/assignment on known named struct values is statically checked. Anonymous source struct literals such as `{ name: "Ada" }` and constructor functions such as `Person(...)` are not implemented.
```

Replace the `typeOf` paragraph with:

```markdown
The debug native stdlib function `typeOf(value)` returns the current runtime type name as a string: primitive values report `"nil"`, `"number"`, `"bool"`, `"string"`, or `"function"`; arrays report `"array"`; named struct values report their runtime struct name such as `"Person"` or `"geo.Point"`. A user binding named `typeOf` shadows the builtin.
```

- [ ] **Step 3: Update language grammar**

In `docs/language-grammar.ebnf`, delete:

```ebnf
struct      = "{", [ fields ], "}" ;
```

In `primary`, remove the `| struct` alternative so the block becomes:

```ebnf
primary     = functionExpr
            | "false"
            | "true"
            | "nil"
            | number
            | string
            | array
            | structConstructor
            | identifier
            | "(", expression, ")" ;
```

Keep `fields` and `field` because named constructors use them.

- [ ] **Step 4: Update roadmap Phase 12 current status**

In `docs/roadmap.md`, under Phase 12 `Current status`, add:

```markdown
- Source-level anonymous struct literals are removed; users construct only
  declared named structs with `Name { ... }` or `alias.Name { ... }`.
```

- [ ] **Step 5: Update AGENTS project memory**

In `AGENTS.md`, replace wording that says `Runtime values currently include ... structs` only if needed to avoid implying source anonymous literals. Update the struct semantics bullet so it starts:

```markdown
- Source programs construct struct values only through declared named constructors such as `Name { field: value }` and `alias.Name { field: value }`; anonymous source struct literals such as `{ field: value }` are not supported. Field access `value.name` reads an existing field; ...
```

Keep the rest of the bullet's method/import/runtime-type-name content, adjusted for grammar.

- [ ] **Step 6: Run docs-focused checks**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case anonymous_struct_literal --case native_stdlib_typeof --case named_struct_types
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case native_stdlib_typeof --case named_struct_types
```

Expected: all selected checks pass.

- [ ] **Step 7: Commit docs**

```sh
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document named-only struct construction"
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

Expected: every command exits 0.

- [ ] **Step 3: Check final git status**

```sh
git status --short
```

Expected: no output after removing `tests/__pycache__/`.

- [ ] **Step 4: Prepare final summary**

Report:

```text
Implemented named-only source struct construction:
- removed anonymous source struct literals;
- kept named and namespaced constructors;
- kept low-level .cdbc anonymous struct compatibility;
- migrated tests/docs to named constructors;
- verified full C++/Python/Rust test suites.
```
