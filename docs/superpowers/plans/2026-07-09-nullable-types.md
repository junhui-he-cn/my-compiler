# Nullable Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add explicit nullable type annotations `T?` so the type checker can distinguish values that may be `nil` from non-null values.

**Architecture:** Add a `?` token and nullable type wrapper in AST/type annotations. Resolve nullable annotations into a `StaticType::Nullable` `TypeInfo` wrapper, then centralize all assignment/argument/return/field/array compatibility behavior in `compatible(expected, actual)`; no IR, bytecode, or runtime changes are required.

**Tech Stack:** C++17 lexer/parser/AST/type checker, Python golden tests, docs.

---

## File Structure

- Modify `include/Token.hpp`, `src/Lexer.cpp`: add `Question` token for `?`.
- Modify `include/Ast.hpp`, `src/Ast.cpp`: add `TypeAnnotation::Kind::Nullable`, `TypeAnnotation::nullable`, and printing support.
- Modify `src/Parser.cpp`: parse nullable postfix `?` for simple, qualified, and array type annotations; function parameter and return nullable types work through recursive type parsing, while nullable function values remain out of scope.
- Modify `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: add `StaticType::Nullable`, `TypeInfo::nullableOf`, type-name printing, annotation resolution, and compatibility rules.
- Create success fixtures under `tests/golden/nullable_*`.
- Create parse/type-error fixtures under `tests/golden/parse_errors/nullable_*` and `tests/golden/type_errors/nullable_*`.
- Modify `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, `AGENTS.md`: document implemented nullable type annotations.

---

### Task 1: RED nullable success and error fixtures

**Files:**
- Create: `tests/golden/nullable_basic/input.cd`
- Create: `tests/golden/nullable_basic/ast.out`
- Create: `tests/golden/nullable_basic/run.out`
- Create: `tests/golden/nullable_functions/input.cd`
- Create: `tests/golden/nullable_functions/ast.out`
- Create: `tests/golden/nullable_functions/run.out`
- Create: `tests/golden/nullable_collections_structs/input.cd`
- Create: `tests/golden/nullable_collections_structs/ast.out`
- Create: `tests/golden/nullable_collections_structs/run.out`
- Create: `tests/golden/parse_errors/nullable_double_question.cd`
- Create: `tests/golden/parse_errors/nullable_double_question.err`
- Create: `tests/golden/parse_errors/nullable_double_question.exit`
- Create: `tests/golden/type_errors/nullable_nil_to_number.cd`
- Create: `tests/golden/type_errors/nullable_nil_to_number.err`
- Create: `tests/golden/type_errors/nullable_nil_to_number.exit`
- Create: `tests/golden/type_errors/nullable_to_nonnullable_assignment.cd`
- Create: `tests/golden/type_errors/nullable_to_nonnullable_assignment.err`
- Create: `tests/golden/type_errors/nullable_to_nonnullable_assignment.exit`
- Create: `tests/golden/type_errors/nullable_argument_to_nonnullable.cd`
- Create: `tests/golden/type_errors/nullable_argument_to_nonnullable.err`
- Create: `tests/golden/type_errors/nullable_argument_to_nonnullable.exit`
- Create: `tests/golden/type_errors/nullable_wrong_inner_type.cd`
- Create: `tests/golden/type_errors/nullable_wrong_inner_type.err`
- Create: `tests/golden/type_errors/nullable_wrong_inner_type.exit`
- Create: `tests/golden/type_errors/nullable_nil_to_nonnullable_struct_field.cd`
- Create: `tests/golden/type_errors/nullable_nil_to_nonnullable_struct_field.err`
- Create: `tests/golden/type_errors/nullable_nil_to_nonnullable_struct_field.exit`

- [ ] **Step 1: Add basic nullable success fixture**

Create `tests/golden/nullable_basic/input.cd`:

```cd
let age: number? = nil;
print age;
age = 42;
print age;
age = nil;
print age;
let label: string? = "ok";
print label;
```

Create `tests/golden/nullable_basic/ast.out`:

```text
Program
  Let age: number? = nil
  Print age
  Expr (= age 42)
  Print age
  Expr (= age nil)
  Print age
  Let label: string? = "ok"
  Print label
```

Create `tests/golden/nullable_basic/run.out`:

```text
nil
42
nil
ok
```

- [ ] **Step 2: Add function nullable success fixture**

Create `tests/golden/nullable_functions/input.cd`:

```cd
fun maybe(flag: bool): number? {
  if flag {
    return 7;
  }
  return nil;
}

fun describe(value: number?): string? {
  if value == nil {
    return nil;
  }
  return "number";
}

print maybe(true);
print maybe(false);
print describe(3);
print describe(nil);
```

Create `tests/golden/nullable_functions/ast.out`:

```text
Program
  Fun maybe(flag: bool): number?
    If flag
      Block
        Return 7
    Return nil
  Fun describe(value: number?): string?
    If (== value nil)
      Block
        Return nil
    Return "number"
  Print (call maybe true)
  Print (call maybe false)
  Print (call describe 3)
  Print (call describe nil)
```

Create `tests/golden/nullable_functions/run.out`:

```text
7
nil
number
nil
```

- [ ] **Step 3: Add collection and struct nullable success fixture**

Create `tests/golden/nullable_collections_structs/input.cd`:

```cd
struct Box { value: number? }
let box: Box = Box { value: nil };
print box.value;
box.value = 5;
print box.value;
let maybeBox: Box? = nil;
print maybeBox;
let values: [number?] = [1, nil, 3];
print values;
push(values, nil);
print values;
let maybeValues: [number]? = nil;
print maybeValues;
```

Create `tests/golden/nullable_collections_structs/ast.out`:

```text
Program
  Struct Box {value: number?}
  Let box: Box = (construct Box value: nil)
  Print (. box value)
  Expr (= (. box value) 5)
  Print (. box value)
  Let maybeBox: Box? = nil
  Print maybeBox
  Let values: [number?] = (array 1 nil 3)
  Print values
  Expr (call push values nil)
  Print values
  Let maybeValues: [number]? = nil
  Print maybeValues
```

Create `tests/golden/nullable_collections_structs/run.out`:

```text
nil
5
nil
[1, nil, 3]
[1, nil, 3, nil]
nil
```

- [ ] **Step 4: Add malformed nullable parse-error fixture**

Create `tests/golden/parse_errors/nullable_double_question.cd`:

```cd
let x: number?? = nil;
```

Create `tests/golden/parse_errors/nullable_double_question.err`:

```text
Parse error at 1:15: expected `=` after variable declaration
```

Create `tests/golden/parse_errors/nullable_double_question.exit`:

```text
1
```

- [ ] **Step 5: Add non-nullable nil type-error fixture**

Create `tests/golden/type_errors/nullable_nil_to_number.cd`:

```cd
let x: number = nil;
```

Create `tests/golden/type_errors/nullable_nil_to_number.err`:

```text
Type error at 1:5: cannot initialize `x` of type number with nil
```

Create `tests/golden/type_errors/nullable_nil_to_number.exit`:

```text
1
```

- [ ] **Step 6: Add nullable-to-nonnullable assignment type-error fixture**

Create `tests/golden/type_errors/nullable_to_nonnullable_assignment.cd`:

```cd
let maybe: number? = 1;
let x: number = maybe;
```

Create `tests/golden/type_errors/nullable_to_nonnullable_assignment.err`:

```text
Type error at 2:5: cannot initialize `x` of type number with number?
```

Create `tests/golden/type_errors/nullable_to_nonnullable_assignment.exit`:

```text
1
```

- [ ] **Step 7: Add nullable argument to nonnullable parameter fixture**

Create `tests/golden/type_errors/nullable_argument_to_nonnullable.cd`:

```cd
fun takesNumber(value: number) { print value; }
let maybe: number? = 1;
takesNumber(maybe);
```

Create `tests/golden/type_errors/nullable_argument_to_nonnullable.err`:

```text
Type error at 3:12: argument 1 expects number, got number?
```

Create `tests/golden/type_errors/nullable_argument_to_nonnullable.exit`:

```text
1
```

- [ ] **Step 8: Add wrong inner type fixture**

Create `tests/golden/type_errors/nullable_wrong_inner_type.cd`:

```cd
let value: number? = "x";
```

Create `tests/golden/type_errors/nullable_wrong_inner_type.err`:

```text
Type error at 1:5: cannot initialize `value` of type number? with string
```

Create `tests/golden/type_errors/nullable_wrong_inner_type.exit`:

```text
1
```

- [ ] **Step 9: Add non-nullable struct field nil fixture**

Create `tests/golden/type_errors/nullable_nil_to_nonnullable_struct_field.cd`:

```cd
struct Box { value: number }
let box: Box = Box { value: nil };
```

Create `tests/golden/type_errors/nullable_nil_to_nonnullable_struct_field.err`:

```text
Type error at 2:22: field `value` expects number, got nil
```

Create `tests/golden/type_errors/nullable_nil_to_nonnullable_struct_field.exit`:

```text
1
```

- [ ] **Step 10: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: nullable success fixtures and `nullable_double_question` fail before lexer/parser support for `?`. Some type-error fixtures that do not require `?`, such as `nullable_nil_to_number`, may already pass; that is acceptable.

- [ ] **Step 11: Commit RED fixtures**

```bash
git add tests/golden/nullable_* tests/golden/parse_errors/nullable_* tests/golden/type_errors/nullable_*
git commit -m "test: add nullable type fixtures"
```

---

### Task 2: Lexer, AST, and parser support for nullable type annotations

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add `Question` token**

In `include/Token.hpp`, add `Question` after `Dot`:

```cpp
    Comma,
    Dot,
    Question,
```

In `src/Lexer.cpp`, add a scanner branch after `.`:

```cpp
    case '?':
        addToken(TokenType::Question);
        break;
```

In `tokenTypeName`, add:

```cpp
    case TokenType::Question:
        return "Question";
```

- [ ] **Step 2: Add nullable type annotation AST node**

In `include/Ast.hpp`, add `Nullable` to `TypeAnnotation::Kind` after `Array`:

```cpp
        Function,
        Array,
        Nullable,
```

Add a factory declaration after `array`:

```cpp
    static TypeAnnotation nullable(Token token, TypeAnnotation innerType);
```

Add storage after `elementType`:

```cpp
    std::shared_ptr<TypeAnnotation> innerType;
```

- [ ] **Step 3: Print nullable type annotations**

In `src/Ast.cpp`, update `writeTypeAnnotation` after the array branch:

```cpp
    if (annotation.kind == TypeAnnotation::Kind::Nullable) {
        writeTypeAnnotation(out, *annotation.innerType);
        out << '?';
        return;
    }
```

Add this factory after `TypeAnnotation::array`:

```cpp
TypeAnnotation TypeAnnotation::nullable(Token token, TypeAnnotation innerType)
{
    TypeAnnotation result;
    result.kind = Kind::Nullable;
    result.token = std::move(token);
    result.innerType = std::make_shared<TypeAnnotation>(std::move(innerType));
    return result;
}
```

- [ ] **Step 4: Parse nullable postfix for non-function type expressions**

In `src/Parser.cpp`, update `Parser::typeAnnotation` inline. Replace the array branch body with:

```cpp
        TypeAnnotation annotation = TypeAnnotation::array(std::move(bracket), std::move(elementType));
        if (match(TokenType::Question)) {
            annotation = TypeAnnotation::nullable(previous(), std::move(annotation));
        }
        return annotation;
```

Keep the function type branch returning `TypeAnnotation::function(...)` without accepting a postfix `?` after the whole function type.

For simple and qualified types, replace the current direct returns with:

```cpp
    TypeAnnotation annotation;
    if (match(TokenType::Dot)) {
        Token member = consume(TokenType::Identifier, "expected type name after `.`");
        annotation = TypeAnnotation::qualified(std::move(name), std::move(member));
    } else {
        annotation = TypeAnnotation::simple(std::move(name));
    }
    if (match(TokenType::Question)) {
        annotation = TypeAnnotation::nullable(previous(), std::move(annotation));
    }
    return annotation;
```

This supports `number?`, `module.Type?`, `[number]?`, `[number?]`, `fun(number?): string?`, and rejects `number??` because the second `?` remains where `=` or another delimiter is expected.

- [ ] **Step 5: Verify parser/AST GREEN and type checker RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: nullable AST output and `nullable_double_question` parse error pass. Nullable fixtures still fail in type checking because `TypeChecker::resolveAnnotation` does not handle `TypeAnnotation::Kind::Nullable` yet.

- [ ] **Step 6: Commit parser slice**

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp src/Parser.cpp tests/golden/nullable_* tests/golden/parse_errors/nullable_double_question.*
git commit -m "feat: parse nullable type annotations"
```

---

### Task 3: Type checker nullable compatibility

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add nullable type storage**

In `include/TypeChecker.hpp`, add `Nullable` after `Struct` in `StaticType`:

```cpp
    Array,
    Struct,
    Nullable,
```

Add nullable storage to `TypeInfo` after `elementType`:

```cpp
    std::shared_ptr<TypeInfo> nullableOf;
```

- [ ] **Step 2: Add nullable helper functions**

In `src/TypeChecker.cpp`, add this helper after `functionType`:

```cpp
TypeInfo nullableType(TypeInfo innerType)
{
    TypeInfo result;
    result.kind = StaticType::Nullable;
    result.nullableOf = std::make_shared<TypeInfo>(std::move(innerType));
    return result;
}
```

Add this helper after `hasFunctionSignature`:

```cpp
bool isNullable(const TypeInfo& type)
{
    return type.kind == StaticType::Nullable && type.nullableOf != nullptr;
}
```

- [ ] **Step 3: Print nullable type names**

In `src/TypeChecker.cpp`, update `typeInfoName` before the struct branch:

```cpp
    if (isNullable(type)) {
        return typeInfoName(*type.nullableOf) + "?";
    }
```

Update `staticTypeName` with a `Nullable` case:

```cpp
    case StaticType::Nullable:
        return "nullable";
```

- [ ] **Step 4: Implement nullable compatibility**

In `src/TypeChecker.cpp`, update `compatible` after the unknown-type check and before the kind equality check:

```cpp
    if (isNullable(expected)) {
        if (actual.kind == StaticType::Nil) {
            return true;
        }
        if (isNullable(actual)) {
            return compatible(*expected.nullableOf, *actual.nullableOf);
        }
        return compatible(*expected.nullableOf, actual);
    }
    if (isNullable(actual)) {
        return false;
    }
```

Keep the existing struct, array, and function compatibility logic unchanged after this block.

- [ ] **Step 5: Resolve nullable annotations**

In `src/TypeChecker.cpp`, update `TypeChecker::resolveAnnotation` at the start, before the array branch:

```cpp
    if (typeName.kind == TypeAnnotation::Kind::Nullable) {
        return nullableType(resolveAnnotation(*typeName.innerType));
    }
```

- [ ] **Step 6: Verify nullable type checking GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all nullable success, parse-error, and type-error fixtures pass, and all existing golden tests still pass.

If any new nullable fixture has a column mismatch, update only the new nullable `.err` file to match current diagnostics, then rerun the command.

- [ ] **Step 7: Commit type checker slice**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/nullable_* tests/golden/type_errors/nullable_*
git commit -m "feat: type check nullable annotations"
```

---

### Task 4: Documentation and final verification

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update EBNF**

In `docs/language-grammar.ebnf`, replace the type grammar with:

```ebnf
typeExpr    = nullableType ;

nullableType = primaryType,
               [ "?" ] ;

primaryType = arrayType
            | functionType
            | qualifiedType
            | simpleType ;

arrayType   = "[", typeExpr, "]" ;
```

Keep `functionType` return and argument productions pointing to `typeExpr` so nullable parameter and return types work.

Add a short comment near this grammar:

```ebnf
(* `T?` means `T` or `nil`. This slice supports nullable function parameter and
   return types, but not nullable function values such as `(fun(number): string)?`. *)
```

- [ ] **Step 2: Update README type annotation section**

In `README.md`, replace the sentence ending with “Generic types and nullable type syntax are not implemented yet.” with text that says:

```markdown
Nullable annotations use postfix `?`, for example `number?`, `Person?`, `[number?]`, and `[number]?`. A value of type `T?` may be either `nil` or a `T`; `nil` is not assignable to non-nullable `T`, and `T?` is not assignable back to `T` without a future narrowing feature. Function parameter and return annotations may use nullable types, such as `fun(value: number?): string?`. Generic types and flow-sensitive nullable narrowing are not implemented yet.
```

- [ ] **Step 3: Add README nullable example**

In `README.md`, after the type annotation paragraph, add:

```markdown
Examples of nullable annotations:

```cd
let age: number? = nil;
age = 42;
age = nil;
let values: [number?] = [1, nil, 3];
let maybeValues: [number]? = nil;
```
```

Ensure the markdown fencing is valid: the outer plan fence is illustrative; the actual README should contain one ` ```cd ` fence and one closing ` ``` `.

- [ ] **Step 4: Update roadmap Phase 9**

In `docs/roadmap.md`, update the Phase 9 status paragraph to add:

```markdown
Phase 9G is implemented: nullable annotations use postfix `?`, allowing `nil` only where `T?` is expected while keeping flow-sensitive narrowing as future work.
```

In the near-term queue and recommendation sections, remove Phase 9G and make Phase 12E member calls the next recommendation.

- [ ] **Step 5: Update AGENTS project memory**

In `AGENTS.md`, update the type semantics bullet to mention nullable annotations:

```markdown
Nullable annotations use postfix `?`, such as `number?`, `Person?`, `[number?]`, and `[number]?`. `nil` is assignable to `T?`, `T` is assignable to `T?`, and `T?` is not assignable to non-nullable `T`; flow-sensitive narrowing is not implemented yet.
```

Also update the roadmap hints if they still list nullable as future work.

- [ ] **Step 6: Run full verification**

Run exactly:

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
git status --short
```

Expected:

- CMake configure and build exit 0.
- CTest reports all tests passed.
- Golden tests report 0 failed.
- Golden selftests report OK.
- Bytecode artifact tests report 0 failed.
- Rust VM golden tests report 0 failed.
- Cargo tests report all tests passed.
- `git status --short` shows only intentional documentation changes before the final commit.

- [ ] **Step 7: Commit docs**

```bash
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document nullable types"
```

- [ ] **Step 8: Run fresh final verification after docs commit**

Run exactly:

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
git status --short
```

Expected: all commands exit 0 and `git status --short` is clean.

- [ ] **Step 9: Report results**

Report:

- All commits created during the nullable phase.
- Exact verification commands and pass/fail counts.
- Final `git status --short` result.
