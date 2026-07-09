# Struct Methods Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add first-slice user-defined methods for local named structs via `impl Name { fun method(...) ... }`, with `this` inside methods and `receiver.method(args...)` lowered to ordinary function calls.

**Architecture:** Extend lexer/parser/AST with `impl` declarations and method declarations. Type checking records a per-struct method table, injects implicit `this` into method scopes, and resolves `MemberCallExpr` either as a namespace call, existing builtin member sugar, or a static named-struct method call. IR lowering emits each method as a hidden function and compiles method calls by loading that hidden function, evaluating the receiver once, prepending it to explicit arguments, and emitting the existing `call` IR.

**Tech Stack:** C++17 compiler front-end, recursive-descent parser, AST/type checker/IR compiler, Python golden tests, bytecode artifact tests, Rust VM parity tests.

---

## File Structure

- `include/Token.hpp`, `src/Lexer.cpp`: add reserved `impl` token and token name.
- `include/Ast.hpp`, `src/Ast.cpp`: add `MethodDecl` and `ImplStmt` AST structures and printer output.
- `include/Parser.hpp`, `src/Parser.cpp`: parse top-level `impl Name { fun method(...) ... }` declarations.
- `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: add method metadata, resolved method names/parameters, `this` binding, method-body checking, and struct method-call resolution.
- `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: compile impl methods as hidden functions and lower struct method calls with receiver prepending.
- `tests/golden/struct_methods_*`: success fixtures for reading fields, mutating fields, parameters/returns, and preserving existing member-call behavior.
- `tests/golden/type_errors/struct_method_*`: type-error fixtures for unknown structs/methods, duplicate/conflicting names, `this` rules, arity/type errors, and return mismatches.
- `tests/bytecode_artifacts/struct_methods_*`: `.cdbc` artifact coverage for method read/mutation.
- `README.md`, `docs/language-grammar.ebnf`, `docs/roadmap.md`, `AGENTS.md`: document implemented syntax and limitations.

---

### Task 1: RED success fixtures for struct methods

**Files:**
- Create: `tests/golden/struct_methods_basic/input.cd`
- Create: `tests/golden/struct_methods_basic/run.out`
- Create: `tests/golden/struct_methods_basic/ast.out`
- Create: `tests/golden/struct_methods_basic/ir.out`
- Create: `tests/golden/struct_methods_mutation/input.cd`
- Create: `tests/golden/struct_methods_mutation/run.out`
- Create: `tests/golden/struct_methods_mutation/ast.out`
- Create: `tests/golden/struct_methods_mutation/ir.out`
- Create: `tests/golden/struct_methods_arguments/input.cd`
- Create: `tests/golden/struct_methods_arguments/run.out`
- Create: `tests/golden/struct_methods_arguments/ast.out`
- Create: `tests/golden/struct_methods_arguments/ir.out`

- [ ] **Step 1: Create basic method-read fixture**

Create `tests/golden/struct_methods_basic/input.cd`:

```cd
struct Person { name: string, age: number }

impl Person {
  fun label(): string {
    return this.name + "!";
  }

  fun nextAge(): number {
    return this.age + 1;
  }
}

let p: Person = Person { name: "Ada", age: 36 };
print p.label();
print p.nextAge();
```

Create `tests/golden/struct_methods_basic/run.out`:

```text
Ada!
37
```

Create empty expected AST/IR files for RED:

```bash
: > tests/golden/struct_methods_basic/ast.out
: > tests/golden/struct_methods_basic/ir.out
```

- [ ] **Step 2: Create method mutation fixture**

Create `tests/golden/struct_methods_mutation/input.cd`:

```cd
struct Counter { value: number }

impl Counter {
  fun incBy(delta: number): nil {
    this.value = this.value + delta;
    return nil;
  }

  fun valueNow(): number {
    return this.value;
  }
}

let c: Counter = Counter { value: 1 };
let alias = c;
c.incBy(4);
print c.valueNow();
print alias.value;
```

Create `tests/golden/struct_methods_mutation/run.out`:

```text
5
5
```

Create empty expected AST/IR files:

```bash
: > tests/golden/struct_methods_mutation/ast.out
: > tests/golden/struct_methods_mutation/ir.out
```

- [ ] **Step 3: Create method arguments and call-result fixture**

Create `tests/golden/struct_methods_arguments/input.cd`:

```cd
struct Box { value: number }

impl Box {
  fun add(amount: number): number {
    return this.value + amount;
  }
}

let b: Box = Box { value: 10 };
let result: number = b.add(5) + 1;
print result;
```

Create `tests/golden/struct_methods_arguments/run.out`:

```text
16
```

Create empty expected AST/IR files:

```bash
: > tests/golden/struct_methods_arguments/ast.out
: > tests/golden/struct_methods_arguments/ir.out
```

- [ ] **Step 4: Verify RED for success fixtures**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. New fixtures fail at parse time because `impl` is still an identifier and `impl Person { ... }` is not valid current syntax.

- [ ] **Step 5: Commit RED success fixtures**

```bash
git add tests/golden/struct_methods_basic tests/golden/struct_methods_mutation tests/golden/struct_methods_arguments
git commit -m "test: add struct method success fixtures"
```

---

### Task 2: RED type-error fixtures

**Files:**
- Create: `tests/golden/type_errors/struct_method_unknown_impl_type.cd`
- Create: `tests/golden/type_errors/struct_method_unknown_impl_type.err`
- Create: `tests/golden/type_errors/struct_method_unknown_impl_type.exit`
- Create: `tests/golden/type_errors/struct_method_duplicate.cd`
- Create: `tests/golden/type_errors/struct_method_duplicate.err`
- Create: `tests/golden/type_errors/struct_method_duplicate.exit`
- Create: `tests/golden/type_errors/struct_method_field_conflict.cd`
- Create: `tests/golden/type_errors/struct_method_field_conflict.err`
- Create: `tests/golden/type_errors/struct_method_field_conflict.exit`
- Create: `tests/golden/type_errors/struct_method_builtin_conflict.cd`
- Create: `tests/golden/type_errors/struct_method_builtin_conflict.err`
- Create: `tests/golden/type_errors/struct_method_builtin_conflict.exit`
- Create: `tests/golden/type_errors/struct_method_this_outside.cd`
- Create: `tests/golden/type_errors/struct_method_this_outside.err`
- Create: `tests/golden/type_errors/struct_method_this_outside.exit`
- Create: `tests/golden/type_errors/struct_method_this_parameter.cd`
- Create: `tests/golden/type_errors/struct_method_this_parameter.err`
- Create: `tests/golden/type_errors/struct_method_this_parameter.exit`
- Create: `tests/golden/type_errors/struct_method_unknown_method.cd`
- Create: `tests/golden/type_errors/struct_method_unknown_method.err`
- Create: `tests/golden/type_errors/struct_method_unknown_method.exit`
- Create: `tests/golden/type_errors/struct_method_unknown_receiver.cd`
- Create: `tests/golden/type_errors/struct_method_unknown_receiver.err`
- Create: `tests/golden/type_errors/struct_method_unknown_receiver.exit`
- Create: `tests/golden/type_errors/struct_method_wrong_arity.cd`
- Create: `tests/golden/type_errors/struct_method_wrong_arity.err`
- Create: `tests/golden/type_errors/struct_method_wrong_arity.exit`
- Create: `tests/golden/type_errors/struct_method_wrong_argument.cd`
- Create: `tests/golden/type_errors/struct_method_wrong_argument.err`
- Create: `tests/golden/type_errors/struct_method_wrong_argument.exit`
- Create: `tests/golden/type_errors/struct_method_return_mismatch.cd`
- Create: `tests/golden/type_errors/struct_method_return_mismatch.err`
- Create: `tests/golden/type_errors/struct_method_return_mismatch.exit`

- [ ] **Step 1: Create impl declaration validation fixtures**

Create `tests/golden/type_errors/struct_method_unknown_impl_type.cd`:

```cd
impl Missing {
  fun value(): number { return 1; }
}
```

Create `tests/golden/type_errors/struct_method_unknown_impl_type.err`:

```text
Type error at 1:6: unknown struct type `Missing` in impl
```

Create `tests/golden/type_errors/struct_method_unknown_impl_type.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_duplicate.cd`:

```cd
struct Person { name: string }

impl Person {
  fun greet(): string { return this.name; }
  fun greet(): string { return this.name; }
}
```

Create `tests/golden/type_errors/struct_method_duplicate.err`:

```text
Type error at 5:7: duplicate method `greet` for struct `Person`
```

Create `tests/golden/type_errors/struct_method_duplicate.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_field_conflict.cd`:

```cd
struct Person { name: string }

impl Person {
  fun name(): string { return this.name; }
}
```

Create `tests/golden/type_errors/struct_method_field_conflict.err`:

```text
Type error at 4:7: method `name` conflicts with field `name` on struct `Person`
```

Create `tests/golden/type_errors/struct_method_field_conflict.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_builtin_conflict.cd`:

```cd
struct Bag { value: number }

impl Bag {
  fun len(): number { return this.value; }
}
```

Create `tests/golden/type_errors/struct_method_builtin_conflict.err`:

```text
Type error at 4:7: method `len` conflicts with builtin member call `len`
```

Create `tests/golden/type_errors/struct_method_builtin_conflict.exit`:

```text
1
```

- [ ] **Step 2: Create `this` rule fixtures**

Create `tests/golden/type_errors/struct_method_this_outside.cd`:

```cd
print this;
```

Create `tests/golden/type_errors/struct_method_this_outside.err`:

```text
Type error at 1:7: undefined variable `this`
```

Create `tests/golden/type_errors/struct_method_this_outside.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_this_parameter.cd`:

```cd
struct Person { name: string }

impl Person {
  fun bad(this: string): string { return this; }
}
```

Create `tests/golden/type_errors/struct_method_this_parameter.err`:

```text
Type error at 4:11: variable `this` already declared in this scope
```

Create `tests/golden/type_errors/struct_method_this_parameter.exit`:

```text
1
```

- [ ] **Step 3: Create method-call validation fixtures**

Create `tests/golden/type_errors/struct_method_unknown_method.cd`:

```cd
struct Person { name: string }
impl Person { fun greet(): string { return this.name; } }
let p: Person = Person { name: "Ada" };
print p.missing();
```

Create `tests/golden/type_errors/struct_method_unknown_method.err`:

```text
Type error at 4:16: struct `Person` has no method `missing`
```

Create `tests/golden/type_errors/struct_method_unknown_method.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_unknown_receiver.cd`:

```cd
fun id(value) { return value; }
id(1).missing();
```

Create `tests/golden/type_errors/struct_method_unknown_receiver.err`:

```text
Type error at 2:14: can only call methods on known named structs
```

Create `tests/golden/type_errors/struct_method_unknown_receiver.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_wrong_arity.cd`:

```cd
struct Box { value: number }
impl Box { fun add(amount: number): number { return this.value + amount; } }
let b: Box = Box { value: 1 };
print b.add();
```

Create `tests/golden/type_errors/struct_method_wrong_arity.err`:

```text
Type error at 4:12: expected 1 arguments but got 0
```

Create `tests/golden/type_errors/struct_method_wrong_arity.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_wrong_argument.cd`:

```cd
struct Box { value: number }
impl Box { fun add(amount: number): number { return this.value + amount; } }
let b: Box = Box { value: 1 };
print b.add("bad");
```

Create `tests/golden/type_errors/struct_method_wrong_argument.err`:

```text
Type error at 4:18: argument 1 expects number, got string
```

Create `tests/golden/type_errors/struct_method_wrong_argument.exit`:

```text
1
```

Create `tests/golden/type_errors/struct_method_return_mismatch.cd`:

```cd
struct Person { name: string }

impl Person {
  fun bad(): number { return this.name; }
}
```

Create `tests/golden/type_errors/struct_method_return_mismatch.err`:

```text
Type error at 4:23: cannot return string from function returning number
```

Create `tests/golden/type_errors/struct_method_return_mismatch.exit`:

```text
1
```

- [ ] **Step 4: Verify RED for type-error fixtures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. New `impl` fixtures fail at parse time until lexer/parser support exists; `this` outside method may already pass as an undefined variable fixture.

- [ ] **Step 5: Commit RED type-error fixtures**

```bash
git add tests/golden/type_errors/struct_method_*
git commit -m "test: add struct method type-error fixtures"
```

---

### Task 3: Lexer, parser, and AST for `impl`

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add `Impl` token**

In `include/Token.hpp`, insert `Impl` in the keyword area next to `Import`/`In`:

```cpp
    If,
    Impl,
    Import,
    In,
```

In `src/Lexer.cpp::identifier`, add the keyword entry:

```cpp
        {"impl", TokenType::Impl},
```

In `src/Lexer.cpp::tokenTypeName`, add:

```cpp
    case TokenType::Impl:
        return "Impl";
```

- [ ] **Step 2: Add AST declarations**

In `include/Ast.hpp`, add `MethodDecl` before `Stmt`, and `ImplStmt` near `StructDeclStmt`:

```cpp
struct MethodDecl {
    MethodDecl(Token name, std::vector<Parameter> parameters, std::optional<TypeAnnotation> returnTypeName, std::vector<StmtPtr> body);

    Token name;
    std::vector<Parameter> parameters;
    std::optional<TypeAnnotation> returnTypeName;
    std::vector<StmtPtr> body;
};

struct ImplStmt final : Stmt {
    ImplStmt(Token typeName, std::vector<MethodDecl> methods);
    void print(std::ostream& out, int indent) const override;

    Token typeName;
    std::vector<MethodDecl> methods;
};
```

Place `MethodDecl` after `StmtPtr` is defined so `std::vector<StmtPtr>` is available.

- [ ] **Step 3: Implement AST constructors and printing**

In `src/Ast.cpp`, add a helper printer for method bodies near `FunctionStmt::print`:

```cpp
MethodDecl::MethodDecl(Token name, std::vector<Parameter> parameters, std::optional<TypeAnnotation> returnTypeName, std::vector<StmtPtr> body)
    : name(std::move(name))
    , parameters(std::move(parameters))
    , returnTypeName(std::move(returnTypeName))
    , body(std::move(body))
{
}

ImplStmt::ImplStmt(Token typeName, std::vector<MethodDecl> methods)
    : typeName(std::move(typeName))
    , methods(std::move(methods))
{
}

void ImplStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Impl " << typeName.lexeme << '\n';
    for (const MethodDecl& method : methods) {
        writeIndent(out, indent + 1);
        out << "Method " << method.name.lexeme << '(';
        for (std::size_t i = 0; i < method.parameters.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << method.parameters[i].name.lexeme;
            if (method.parameters[i].typeName) {
                out << ": ";
                writeTypeAnnotation(out, *method.parameters[i].typeName);
            }
        }
        out << ')';
        if (method.returnTypeName) {
            out << ": ";
            writeTypeAnnotation(out, *method.returnTypeName);
        }
        out << '\n';
        for (const auto& statement : method.body) {
            statement->print(out, indent + 2);
        }
    }
}
```

If helper names differ, use the existing AST printer helpers in `src/Ast.cpp` (`writeIndent`, `writeTypeAnnotation`, and statement `print`).

- [ ] **Step 4: Add parser declarations**

In `include/Parser.hpp`, add private declarations:

```cpp
StmtPtr implDeclaration();
MethodDecl methodDeclaration();
```

- [ ] **Step 5: Parse top-level impl declarations**

In `Parser::declaration`, after struct declarations and before function declarations, add:

```cpp
    if (match(TokenType::Impl)) {
        if (blockDepth_ != 0) {
            throw ParseError(previous(), "`impl` is only allowed at top level");
        }
        return implDeclaration();
    }
```

Add parser implementations:

```cpp
StmtPtr Parser::implDeclaration()
{
    Token typeName = consume(TokenType::Identifier, "expected struct name after `impl`");
    consume(TokenType::LeftBrace, "expected `{` after impl type name");
    std::vector<MethodDecl> methods;
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        methods.push_back(methodDeclaration());
    }
    consume(TokenType::RightBrace, "expected `}` after impl methods");
    return std::make_unique<ImplStmt>(std::move(typeName), std::move(methods));
}

MethodDecl Parser::methodDeclaration()
{
    consume(TokenType::Fun, "expected `fun` method declaration in impl block");
    Token name = consume(TokenType::Identifier, "expected method name after `fun`");
    consume(TokenType::LeftParen, "expected `(` after method name");
    std::vector<Parameter> parsedParameters = parameters();
    consume(TokenType::RightParen, "expected `)` after method parameters");
    std::optional<TypeAnnotation> returnTypeName = optionalReturnType();
    consume(TokenType::LeftBrace, "expected `{` before method body");
    ++blockDepth_;
    std::vector<StmtPtr> body = blockStatements();
    --blockDepth_;
    return MethodDecl(std::move(name), std::move(parsedParameters), std::move(returnTypeName), std::move(body));
}
```

- [ ] **Step 6: Build and verify parser RED advances to type checker**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. New success fixtures now parse and print AST, but type checking fails with `unsupported statement node` for `ImplStmt` until Task 4.

- [ ] **Step 7: Commit parser/AST slice**

```bash
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse struct impl blocks"
```

---

### Task 4: Type-check impl declarations and method bodies

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Extend `ResolvedNames` for methods and member-call receiver mode**

In `include/TypeChecker.hpp`, add public accessors:

```cpp
const std::string& methodName(const MethodDecl& method) const;
const std::vector<std::string>& methodParameterNames(const MethodDecl& method) const;
bool memberCallPassesReceiver(const MemberCallExpr& expression) const;
```

Add private recorders and maps:

```cpp
void recordMethod(const MethodDecl& method, std::string name);
void recordMethodParameters(const MethodDecl& method, std::vector<std::string> names);
void recordMemberCallCallee(const MemberCallExpr& expression, std::string name, bool passesReceiver);

std::unordered_map<const MethodDecl*, std::string> methodNames_;
std::unordered_map<const MethodDecl*, std::vector<std::string>> methodParameterNames_;
std::unordered_map<const MemberCallExpr*, bool> memberCallPassesReceiver_;
```

Update the existing `recordMemberCallCallee(const MemberCallExpr&, std::string)` implementation to take the new `bool passesReceiver` parameter and store it in `memberCallPassesReceiver_`. Update the existing namespace-call site to pass `false`.

- [ ] **Step 2: Add type checker method metadata**

In `TypeChecker` private section, add:

```cpp
struct MethodInfo {
    const MethodDecl* declaration = nullptr;
    TypeInfo receiverType;
    std::vector<TypeInfo> parameterTypes;
    TypeInfo returnType;
    std::string resolvedName;
};

using MethodTable = std::unordered_map<std::string, std::unordered_map<std::string, MethodInfo>>;

void checkImpl(const ImplStmt& statement);
void registerMethodSignature(const StructTypeDecl& structType, const ImplStmt& statement, const MethodDecl& method);
void checkMethodBody(const std::string& structName, const MethodInfo& method);
const MethodInfo* findMethod(const std::string& structName, const std::string& methodName) const;
bool isBuiltinMemberName(const std::string& name) const;

MethodTable methods_;
```

Clear `methods_` in `TypeChecker::check`. Save/restore it in `checkModule` alongside `structTypes_` so method tables remain module-private in this first slice.

- [ ] **Step 3: Handle `ImplStmt` in statement checking**

In `TypeChecker::checkStatement`, after struct declarations and before function declarations, add:

```cpp
    if (const auto* impl = dynamic_cast<const ImplStmt*>(&statement)) {
        checkImpl(*impl);
        return;
    }
```

- [ ] **Step 4: Implement method registration validation**

Add helper:

```cpp
bool TypeChecker::isBuiltinMemberName(const std::string& name) const
{
    return name == "push" || name == "pop" || name == "len" || name == "substr" || name == "charAt";
}
```

Add `registerMethodSignature`:

```cpp
void TypeChecker::registerMethodSignature(const StructTypeDecl& structType, const ImplStmt& statement, const MethodDecl& method)
{
    auto& structMethods = methods_[statement.typeName.lexeme];
    if (structMethods.find(method.name.lexeme) != structMethods.end()) {
        throw TypeError(method.name, "duplicate method `" + method.name.lexeme + "` for struct `" + statement.typeName.lexeme + "`");
    }
    if (findStructField(structType, method.name.lexeme)) {
        throw TypeError(method.name,
            "method `" + method.name.lexeme + "` conflicts with field `" + method.name.lexeme + "` on struct `" + statement.typeName.lexeme + "`");
    }
    if (isBuiltinMemberName(method.name.lexeme)) {
        throw TypeError(method.name, "method `" + method.name.lexeme + "` conflicts with builtin member call `" + method.name.lexeme + "`");
    }

    std::vector<TypeInfo> parameterTypes;
    parameterTypes.reserve(method.parameters.size());
    for (const Parameter& parameter : method.parameters) {
        parameterTypes.push_back(parameter.typeName ? resolveAnnotation(*parameter.typeName) : unknownType());
    }

    std::optional<TypeInfo> expectedReturnType;
    if (method.returnTypeName) {
        expectedReturnType = resolveAnnotation(*method.returnTypeName);
    }

    MethodInfo info;
    info.declaration = &method;
    info.receiverType = namedStructType(statement.typeName.lexeme);
    info.parameterTypes = std::move(parameterTypes);
    info.returnType = expectedReturnType ? *expectedReturnType : unknownType();
    info.resolvedName = makeResolvedName("__method_" + statement.typeName.lexeme + "_" + method.name.lexeme);
    resolvedNames_.recordMethod(method, info.resolvedName);
    structMethods.emplace(method.name.lexeme, std::move(info));
}
```

- [ ] **Step 5: Implement method body checking with implicit `this`**

Add `checkMethodBody`:

```cpp
void TypeChecker::checkMethodBody(const std::string& structName, const MethodInfo& method)
{
    const MethodDecl& declaration = *method.declaration;

    beginScope();
    ++functionDepth_;
    const std::size_t enclosingLoopDepth = loopDepth_;
    loopDepth_ = 0;

    std::vector<std::string> parameterNames;
    Token thisToken{TokenType::Identifier, "this", declaration.name.line, declaration.name.column};
    Binding thisBinding = declareVariable(thisToken, method.receiverType, true);
    parameterNames.push_back(thisBinding.resolvedName);

    for (std::size_t i = 0; i < declaration.parameters.size(); ++i) {
        const Parameter& parameter = declaration.parameters[i];
        Binding parameterBinding = declareVariable(parameter.name, method.parameterTypes[i], parameter.typeName.has_value());
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordMethodParameters(declaration, std::move(parameterNames));

    std::optional<TypeInfo> expectedReturnType;
    if (declaration.returnTypeName) {
        expectedReturnType = method.returnType;
    }

    const TypeInfo returnType = checkFunctionBody(
        declaration.body,
        expectedReturnType,
        declaration.name,
        structName + "." + declaration.name.lexeme);

    loopDepth_ = enclosingLoopDepth;
    --functionDepth_;
    endScope();

    auto& stored = methods_[structName][declaration.name.lexeme];
    stored.returnType = returnType;
}
```

This deliberately declares `this` before explicit parameters so a method parameter named `this` produces the existing duplicate declaration diagnostic.

- [ ] **Step 6: Implement `checkImpl` and method lookup**

Add:

```cpp
const TypeChecker::MethodInfo* TypeChecker::findMethod(const std::string& structName, const std::string& methodName) const
{
    const auto structFound = methods_.find(structName);
    if (structFound == methods_.end()) {
        return nullptr;
    }
    const auto methodFound = structFound->second.find(methodName);
    return methodFound == structFound->second.end() ? nullptr : &methodFound->second;
}

void TypeChecker::checkImpl(const ImplStmt& statement)
{
    const StructTypeDecl* structType = findStructType(statement.typeName.lexeme);
    if (!structType) {
        throw TypeError(statement.typeName, "unknown struct type `" + statement.typeName.lexeme + "` in impl");
    }

    for (const MethodDecl& method : statement.methods) {
        registerMethodSignature(*structType, statement, method);
    }
    for (const MethodDecl& method : statement.methods) {
        const MethodInfo* info = findMethod(statement.typeName.lexeme, method.name.lexeme);
        if (!info) {
            throw TypeError(method.name, "internal error: missing method signature");
        }
        checkMethodBody(statement.typeName.lexeme, *info);
    }
}
```

- [ ] **Step 7: Build and run goldens to reach method-call RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL. Impl declarations and method bodies type-check where calls are absent, but method-call fixtures still fail with unknown member call or compile errors until Task 5 resolves `MemberCallExpr` for named structs.

- [ ] **Step 8: Commit type-checking impl body slice**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: type check struct impl methods"
```

---

### Task 5: Resolve and lower struct method calls

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Add method-call branch to `checkMemberCall`**

In `TypeChecker::checkMemberCall`, keep namespace handling first and builtin handling second. Before the final unknown-member error, add named struct handling:

```cpp
    const CheckedExpression receiver = checkExpressionInfo(*expression.receiver);
    if (receiver.type.kind == StaticType::Struct && receiver.type.structName) {
        const MethodInfo* method = findMethod(*receiver.type.structName, name);
        if (!method) {
            throw TypeError(expression.paren, "struct `" + *receiver.type.structName + "` has no method `" + name + "`");
        }
        if (expression.arguments.size() != method->parameterTypes.size()) {
            throw TypeError(expression.paren,
                "expected " + std::to_string(method->parameterTypes.size()) + " arguments but got " + std::to_string(expression.arguments.size()));
        }
        for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
            const CheckedExpression argument = checkExpressionInfo(*expression.arguments[i], &method->parameterTypes[i]);
            if (!compatible(method->parameterTypes[i], argument.type)) {
                throw TypeError(expression.paren,
                    "argument " + std::to_string(i + 1) + " expects " + typeInfoName(method->parameterTypes[i])
                        + ", got " + typeInfoName(argument.type));
            }
        }
        resolvedNames_.recordMemberCallCallee(expression, method->resolvedName, true);
        return CheckedExpression{method->returnType};
    }

    if (receiver.type.kind == StaticType::Unknown) {
        throw TypeError(expression.paren, "can only call methods on known named structs");
    }
```

Ensure builtin member-call branches do not call `checkExpressionInfo(*expression.receiver)` twice for the method path. If needed, keep builtin branches as-is and accept a separate receiver check only in this method branch.

- [ ] **Step 2: Update namespace call record site**

In the existing namespace branch in `checkMemberCall`, update:

```cpp
resolvedNames_.recordMemberCallCallee(expression, found->second.resolvedName, false);
```

This preserves `math.add(2, 3)` lowering without prepending `math` as a receiver argument.

- [ ] **Step 3: Compile impl methods as hidden functions**

In `include/IRCompiler.hpp`, add:

```cpp
void compileImpl(const ImplStmt& statement);
void compileMethod(const MethodDecl& method);
```

In `IRCompiler::compileStatement`, after struct declarations, add:

```cpp
    if (const auto* impl = dynamic_cast<const ImplStmt*>(&statement)) {
        compileImpl(*impl);
        return;
    }
```

Implement:

```cpp
void IRCompiler::compileImpl(const ImplStmt& statement)
{
    for (const MethodDecl& method : statement.methods) {
        compileMethod(method);
    }
}

void IRCompiler::compileMethod(const MethodDecl& method)
{
    const std::string methodName = resolvedNames_->methodName(method);
    IRRegister placeholder = ir_.emitConstant(Value::nil());
    ir_.emitStoreVar(methodName, placeholder);

    std::vector<std::string> parameters = resolvedNames_->methodParameterNames(method);
    ir_.beginFunction(method.name.lexeme, std::move(parameters));

    std::vector<LoopContext> enclosingLoopContexts = std::move(loopContexts_);
    loopContexts_.clear();
    for (const auto& statement : method.body) {
        compileStatement(*statement);
    }
    IRRegister nilValue = ir_.emitConstant(Value::nil());
    ir_.emitReturn(nilValue);
    loopContexts_ = std::move(enclosingLoopContexts);

    const std::size_t functionIndex = ir_.endFunction();
    IRRegister value = ir_.emitMakeFunction(functionIndex);
    ir_.emitAssignVar(methodName, value);
}
```

- [ ] **Step 4: Lower struct method calls with receiver prepending**

Update `IRCompiler::emitMemberCall` first branch:

```cpp
    if (resolvedNames_->hasMemberCallCallee(expression)) {
        const IRRegister callee = ir_.emitLoadVar(resolvedNames_->memberCallCalleeName(expression));
        std::vector<IRRegister> arguments;
        if (resolvedNames_->memberCallPassesReceiver(expression)) {
            arguments.push_back(compileExpression(*expression.receiver));
        }
        for (const auto& argument : expression.arguments) {
            arguments.push_back(compileExpression(*argument));
        }
        return ir_.emitCall(callee, std::move(arguments));
    }
```

Namespace calls keep `memberCallPassesReceiver == false`; struct method calls use `true`.

- [ ] **Step 5: Run goldens and inspect diagnostic columns**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: FAIL only for new empty AST/IR files and any type-error `.err` files whose exact token column differs. If existing namespace or builtin member-call tests fail, fix before proceeding.

- [ ] **Step 6: Update new expected outputs intentionally**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS for golden tests. Review only relevant diffs:

```bash
git diff -- tests/golden/struct_methods_basic tests/golden/struct_methods_mutation tests/golden/struct_methods_arguments tests/golden/type_errors/struct_method_*
```

If `--update` creates unrelated generated `bytecode.out` files or broad parse/type error rewrites, revert unrelated changes with:

```bash
git checkout -- tests/golden/parse_errors tests/golden/type_errors
git clean -fd tests/golden
```

Then manually update only `tests/golden/type_errors/struct_method_*.err` based on direct command output.

- [ ] **Step 7: Commit method-call lowering slice**

```bash
git add include/TypeChecker.hpp src/TypeChecker.cpp include/IRCompiler.hpp src/IRCompiler.cpp tests/golden/struct_methods_basic tests/golden/struct_methods_mutation tests/golden/struct_methods_arguments tests/golden/type_errors/struct_method_*
git commit -m "feat: lower struct method calls"
```

---

### Task 6: Regression coverage for existing member-call forms

**Files:**
- Modify or verify existing: `tests/golden/member_calls_arrays/`
- Modify or verify existing: `tests/golden/member_calls_strings/`
- Modify or verify existing: `tests/golden/member_calls_shadowing/`
- Modify or verify existing: `tests/golden/namespace_import_function/`

- [ ] **Step 1: Run focused existing member-call regressions**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: PASS after Task 5. Specifically confirm these fixtures are included in the passing count and do not appear in failures:

```text
member_calls_arrays
member_calls_strings
member_calls_shadowing
namespace_import_function
```

- [ ] **Step 2: If needed, add explicit no-regression fixture for namespace method-like calls**

Only if `namespace_import_function` is not already sufficient, create `tests/golden/struct_methods_namespace_regression/input.cd`:

```cd
import "./lib.cd" as math;
print math.add(2, 3);
```

Create `tests/golden/struct_methods_namespace_regression/lib.cd`:

```cd
fun add(a: number, b: number): number { return a + b; }
export add;
```

Create `tests/golden/struct_methods_namespace_regression/run.out`:

```text
5
```

Create empty `ast.out` and `ir.out`, run `python3 tests/run_golden_tests.py ./build/compiler_design --update`, then review only this fixture. If the existing `namespace_import_function` fixture already covers the behavior, skip creating this duplicate fixture.

- [ ] **Step 3: Commit regression fixture only if created**

If Step 2 created files, run:

```bash
git add tests/golden/struct_methods_namespace_regression
git commit -m "test: cover namespace call with struct methods"
```

If no files were created, do not commit.

---

### Task 7: Bytecode artifact and Rust VM parity coverage

**Files:**
- Create: `tests/bytecode_artifacts/struct_methods_basic/input.cd`
- Create: `tests/bytecode_artifacts/struct_methods_basic/run.out`
- Create: `tests/bytecode_artifacts/struct_methods_basic/expected.cdbc`
- Create: `tests/bytecode_artifacts/struct_methods_mutation/input.cd`
- Create: `tests/bytecode_artifacts/struct_methods_mutation/run.out`
- Create: `tests/bytecode_artifacts/struct_methods_mutation/expected.cdbc`

- [ ] **Step 1: Create bytecode artifact inputs**

Create `tests/bytecode_artifacts/struct_methods_basic/input.cd`:

```cd
struct Person { name: string }
impl Person { fun label(): string { return this.name + "!"; } }
let p: Person = Person { name: "Ada" };
print p.label();
```

Create `tests/bytecode_artifacts/struct_methods_basic/run.out`:

```text
Ada!
```

Create `tests/bytecode_artifacts/struct_methods_mutation/input.cd`:

```cd
struct Counter { value: number }
impl Counter { fun inc(): nil { this.value = this.value + 1; return nil; } }
let c: Counter = Counter { value: 1 };
c.inc();
print c.value;
```

Create `tests/bytecode_artifacts/struct_methods_mutation/run.out`:

```text
2
```

- [ ] **Step 2: Generate expected `.cdbc` artifacts with compiler**

Run:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/struct_methods_basic/expected.cdbc tests/bytecode_artifacts/struct_methods_basic/input.cd
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/struct_methods_mutation/expected.cdbc tests/bytecode_artifacts/struct_methods_mutation/input.cd
```

Expected: both commands exit 0 and create `expected.cdbc` files.

- [ ] **Step 3: Verify artifact and Rust VM parity**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: PASS. Artifact text should contain existing function/call/field operations and no new bytecode opcode.

- [ ] **Step 4: Review artifacts**

Run:

```bash
cat tests/bytecode_artifacts/struct_methods_basic/expected.cdbc
cat tests/bytecode_artifacts/struct_methods_mutation/expected.cdbc
git diff -- tests/bytecode_artifacts/struct_methods_basic tests/bytecode_artifacts/struct_methods_mutation
```

Expected: hidden method functions appear as normal bytecode functions, and method calls use existing `call` instructions.

- [ ] **Step 5: Commit artifact coverage**

```bash
git add tests/bytecode_artifacts/struct_methods_basic tests/bytecode_artifacts/struct_methods_mutation
git commit -m "test: cover struct methods in bytecode artifacts"
```

---

### Task 8: Documentation updates

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README**

Add a concise section near struct/member-call docs:

```markdown
Named structs may define first-slice methods in top-level `impl` blocks:

```cd
struct Person { name: string }
impl Person {
  fun greet(): string { return this.name; }
}
let p: Person = Person { name: "Ada" };
print p.greet();
```

Methods are statically resolved on known named struct receiver types. Inside a method, `this` is the receiver and has the impl struct type; field assignment through `this.field = value` mutates the receiver. This first slice does not support methods on anonymous or imported/namespaced structs, method export/import behavior, inheritance, overloading, dynamic dispatch, static methods, or function-valued field calls.
```

If nested fenced blocks are awkward in `README.md`, use indented code blocks instead of nested fences.

- [ ] **Step 2: Update EBNF**

In `docs/language-grammar.ebnf`, add `implDecl` to `declaration` and add definitions:

```ebnf
declaration = importDecl
            | exportDecl
            | structDecl
            | implDecl
            | funDecl
            | letDecl
            | statement ;

implDecl    = "impl", identifier,
              "{", { methodDecl }, "}" ;

methodDecl  = "fun", identifier,
              "(", [ parameters ], ")",
              [ ":", typeExpr ],
              block ;
```

Keep the existing `memberCall` grammar unchanged.

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, update Phase 12 status to include:

```markdown
Phase 12F is implemented: local named structs can define statically resolved methods in `impl` blocks, with `this` bound to the receiver and method calls lowered to ordinary function calls. Methods on imported structs, method export/import behavior, dynamic dispatch, inheritance, and overloading remain future work.
```

Update near-term queue only if it still lists struct methods as pending.

- [ ] **Step 4: Update AGENTS.md**

In current language semantics, add:

```markdown
- Local named structs may define methods in top-level `impl Name { fun method(...) ... }` blocks. Method calls `receiver.method(args...)` are statically resolved for known named struct receiver types and lower to ordinary function calls with the receiver passed as implicit `this`. Methods on imported/namespaced structs, method export/import behavior, dynamic dispatch, inheritance, overloading, and function-valued field calls are not implemented yet.
```

- [ ] **Step 5: Commit docs**

```bash
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document struct methods"
```

---

### Task 9: Full verification and cleanup

**Files:**
- No planned edits.

- [ ] **Step 1: Run full verification suite**

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

Expected: every command exits 0. Golden, artifact, and Rust VM counts may increase because of new fixtures; record the actual counts from output.

- [ ] **Step 2: Check workspace state**

Run:

```bash
git status --short
```

Expected: clean working tree. If files remain modified, review and commit focused changes before reporting completion.

- [ ] **Step 3: Final report**

Report exact verification evidence using this shape:

```text
Implemented first-slice struct methods.
Verification run:
- cmake -S . -B build: PASS
- cmake --build build: PASS
- ctest --test-dir build --output-on-failure: PASS
- python3 tests/run_golden_tests.py ./build/compiler_design: PASS (<actual count>)
- python3 tests/run_golden_tests_selftest.py: PASS
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: PASS (<actual count>)
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: PASS (<actual count>)
- cargo test --manifest-path vm-rs/Cargo.toml: PASS
- git status --short: clean
```

If any command fails, do not claim completion. Fix the failure or report the failing command and relevant output.

---

## Self-Review

- Spec coverage: The plan covers `impl` parsing, AST printing, method table validation, implicit `this`, struct method-call type checking, hidden-function lowering, namespace/builtin member-call preservation, success/type-error/artifact/Rust VM coverage, and docs.
- Placeholder scan: No forbidden placeholder text or unspecified test instructions remain. Each code-edit task includes concrete snippets and exact file paths.
- Type consistency: The plan consistently uses `MethodDecl`, `ImplStmt`, `MethodInfo`, `checkImpl`, `compileImpl`, `compileMethod`, and `memberCallPassesReceiver`.
- Scope check: The plan intentionally excludes imported/namespaced methods, export behavior, dynamic dispatch, inheritance, overloading, static methods, function-valued field calls, and new bytecode opcodes, matching the approved first slice.
