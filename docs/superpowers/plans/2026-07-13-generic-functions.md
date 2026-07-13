# Generic Function Type Abstraction Implementation Plan

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add named generic functions with call-site type inference and recursive substitution while preserving the existing runtime and bytecode pipeline.

**Architecture:** Extend the C++ AST/parser with function type-parameter lists, represent type variables and generic signatures in TypeInfo, and centralize recursive inference/substitution in TypeChecker. Generic metadata travels through existing module interfaces; IR, .cdbc, and the Rust VM remain unchanged because generic functions execute as ordinary dynamic functions.

**Tech Stack:** C++17 AST/parser/type checker, existing TypeInfo utilities, Python golden fixtures, text .cdbc artifacts, Rust VM parity tests, CTest/Cargo.

---

### Task 1: Add failing type-system tests and generic fixtures

**Files:**

- Create: tests/type_utils_tests.cpp
- Modify: CMakeLists.txt
- Create: tests/golden/generic_functions/input.cd
- Create: tests/golden/generic_imports/input.cd
- Create: tests/golden/generic_imports/lib.cd
- Create: tests/golden/type_errors/generic_duplicate_type_parameter.cd
- Create: tests/golden/type_errors/generic_out_of_scope_type_parameter.cd
- Create: tests/golden/type_errors/generic_conflicting_inference.cd
- Create: tests/golden/type_errors/generic_uninferred_type_parameter.cd
- Create: tests/golden/type_errors/generic_monomorphic_assignment.cd

- [ ] Step 1: Add unit tests describing the TypeInfo API.

Create tests/type_utils_tests.cpp:

~~~cpp
#include "TypeUtils.hpp"

#include <cassert>

int main()
{
    const TypeInfo t = typeParameterType("T");
    assert(typeInfoName(t) == "T");
    assert(compatible(t, typeParameterType("T")));
    assert(!compatible(t, typeParameterType("U")));

    const TypeInfo identity = functionType({t}, t, {"T"});
    assert(typeInfoName(identity) == "fun<T>(T): T");

    const TypeInfo nested = arrayType(t);
    assert(compatible(nested, arrayType(typeParameterType("T"))));
    assert(!compatible(nested, arrayType(simpleType(StaticType::Number))));
    return 0;
}
~~~

- [ ] Step 2: Register the unit target before implementation.

Append this target and test to CMakeLists.txt:

~~~cmake
add_executable(type_utils_tests
    tests/type_utils_tests.cpp
    src/TypeUtils.cpp
)
target_include_directories(type_utils_tests PRIVATE include)
compiler_design_apply_warnings(type_utils_tests)
compiler_design_apply_sanitizers(type_utils_tests)
add_test(NAME type_utils COMMAND type_utils_tests)
~~~

- [ ] Step 3: Add success fixture sources.

tests/golden/generic_functions/input.cd:

~~~text
fun identity<T>(value: T): T {
  return value;
}
fun head<T>(values: [T]): T {
  return values[0];
}
fun same<T>(left: T, right: T): T {
  return left;
}
print identity(42);
print identity("hello");
print head([1, 2, 3]);
print same(true, true);
let alias = identity;
print alias("alias");
~~~

tests/golden/generic_imports/lib.cd:

~~~text
fun identity<T>(value: T): T {
  return value;
}
export identity;
~~~

tests/golden/generic_imports/input.cd:

~~~text
import "lib.cd";
import "lib.cd" as lib;
print identity(7);
print lib.identity("module");
~~~

- [ ] Step 4: Add type-error fixture sources.

~~~text
// tests/golden/type_errors/generic_duplicate_type_parameter.cd
fun duplicate<T, T>(value: T): T { return value; }
~~~

~~~text
// tests/golden/type_errors/generic_out_of_scope_type_parameter.cd
let value: T = 1;
~~~

~~~text
// tests/golden/type_errors/generic_conflicting_inference.cd
fun same<T>(left: T, right: T): T { return left; }
same(1, "x");
~~~

~~~text
// tests/golden/type_errors/generic_uninferred_type_parameter.cd
fun make<T>(): T? { return nil; }
make();
~~~

~~~text
// tests/golden/type_errors/generic_monomorphic_assignment.cd
fun identity<T>(value: T): T { return value; }
let typed: fun(number): number = identity;
~~~

- [ ] Step 5: Run the new unit target and parser fixture before implementation.

Run:

~~~bash
cmake -S . -B build
cmake --build build --target type_utils_tests
~~~

Expected: compilation fails because typeParameterType and the generic functionType overload do not exist. Running compiler_design on generic_functions/input.cd should also fail before the new parser support.

### Task 2: Add generic function syntax to the AST and parser

**Files:**

- Modify: include/Ast.hpp (FunctionStmt)
- Modify: src/Ast.cpp (FunctionStmt constructor and printer)
- Modify: include/Parser.hpp (type-parameter helper declaration)
- Modify: src/Parser.cpp (functionDeclaration and helper)

- [ ] Step 1: Extend FunctionStmt with type-parameter tokens.

Use this shape:

~~~cpp
FunctionStmt(
    Token name,
    std::vector<Token> typeParameters,
    std::vector<Parameter> parameters,
    std::optional<TypeAnnotation> returnTypeName,
    std::vector<StmtPtr> body);

Token name;
std::vector<Token> typeParameters;
std::vector<Parameter> parameters;
~~~

Move typeParameters in the constructor before parameters.

- [ ] Step 2: Parse an optional type-parameter list after a function name.

Declare std::vector<Token> typeParameters(); in Parser.hpp and implement:

~~~cpp
std::vector<Token> Parser::typeParameters()
{
    std::vector<Token> parameters;
    if (!match(TokenType::Less)) {
        return parameters;
    }
    do {
        parameters.push_back(consume(
            TokenType::Identifier,
            "expected type parameter name after < or ,"));
    } while (match(TokenType::Comma));
    consume(TokenType::Greater, "expected > after type parameters");
    return parameters;
}
~~~

Call it immediately after consuming the function name and pass the vector into FunctionStmt. Do not alter method or function-expression parsing.

- [ ] Step 3: Print type parameters in AST output.

In FunctionStmt::print, emit the list between the name and ordinary parameter list:

~~~cpp
out << "Fun " << name.lexeme;
if (!typeParameters.empty()) {
    out << '<';
    for (std::size_t i = 0; i < typeParameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << typeParameters[i].lexeme;
    }
    out << '>';
}
writeParameterList(out, parameters);
~~~

- [ ] Step 4: Build and verify parsing reaches type checking.

Run:

~~~bash
cmake --build build --target compiler_design
./build/compiler_design --tokens tests/golden/generic_functions/input.cd
~~~

Expected: tokens include Less, Identifier, and Greater after each generic function name; normal compilation reaches the type checker and reports unknown type T until Task 4.

- [ ] Step 5: Commit the AST/parser slice.

~~~bash
git add include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse generic function parameters"
~~~

### Task 3: Extend TypeInfo and add utility behavior

**Files:**

- Modify: include/TypeUtils.hpp
- Modify: src/TypeUtils.cpp
- Modify: tests/type_utils_tests.cpp to keep assertions aligned with final
  TypeInfo formatting
- Modify: CMakeLists.txt only to retain the target from Task 1

- [ ] Step 1: Add type-variable and generic-signature fields.

Extend the model:

~~~cpp
enum class StaticType {
    Unknown,
    Nil,
    Number,
    Bool,
    String,
    Function,
    Array,
    Struct,
    Nullable,
    TypeParameter,
};

struct TypeInfo {
    StaticType kind = StaticType::Unknown;
    std::vector<TypeInfo> parameterTypes;
    std::shared_ptr<TypeInfo> returnType;
    std::optional<std::string> structName;
    std::shared_ptr<TypeInfo> elementType;
    std::shared_ptr<TypeInfo> nullableOf;
    std::optional<std::string> typeParameterName;
    std::vector<std::string> genericParameters;
};

TypeInfo typeParameterType(std::string name);
TypeInfo functionType(
    std::vector<TypeInfo> parameterTypes,
    TypeInfo returnType,
    std::vector<std::string> genericParameters = {});
~~~

Keep existing two-argument functionType calls source-compatible via the default third argument.

- [ ] Step 2: Implement formatting and compatibility.

TypeInfoName must return the type-parameter name for TypeParameter and print function signatures as fun<T, U>(...). In compatible, TypeParameter values are compatible only when both names match. Recursive arrays, nullable types, and function children continue to call compatible. Unknown remains the existing wildcard; concrete types are not compatible with an unsubstituted type parameter.

- [ ] Step 3: Run focused utility tests.

~~~bash
cmake --build build --target type_utils_tests
ctest --test-dir build -R '^type_utils$' --output-on-failure
~~~

Expected: the executable passes all assertions, including fun<T>(T): T formatting and recursive type-variable compatibility.

- [ ] Step 4: Commit the type model.

~~~bash
git add include/TypeUtils.hpp src/TypeUtils.cpp tests/type_utils_tests.cpp CMakeLists.txt
git commit -m "feat: represent generic type parameters"
~~~

### Task 4: Implement generic scope, inference, substitution, and call checking

**Files:**

- Modify: include/TypeChecker.hpp
- Modify: src/TypeChecker.cpp

- [ ] Step 1: Add generic-scope and inference helper declarations.

Add to TypeChecker:

~~~cpp
using TypeSubstitutions = std::unordered_map<std::string, TypeInfo>;

void beginTypeParameterScope(const std::vector<Token>& parameters);
void endTypeParameterScope();
const TypeInfo* findTypeParameter(const std::string& name) const;
std::vector<std::string> typeParameterNames(const std::vector<Token>& parameters) const;
void inferTypeArguments(
    const TypeInfo& expected,
    const TypeInfo& actual,
    TypeSubstitutions& substitutions,
    const Token& callToken) const;
TypeInfo substituteTypeParameters(
    const TypeInfo& type,
    const TypeSubstitutions& substitutions) const;
CheckedExpression checkFunctionCall(
    const Token& callToken,
    const TypeInfo& calleeType,
    const std::vector<ExprPtr>& arguments);
~~~

Store a stack of unordered_map<string, TypeInfo> scopes. beginTypeParameterScope rejects duplicate names and maps each name to typeParameterType(name); resolveAnnotation checks this stack before primitive and struct lookup.

- [ ] Step 2: Resolve generic declarations and retain their signatures.

In checkFunction, push the declaration's type-parameter scope before resolving parameter and return annotations. Create the binding with:

~~~cpp
functionType(
    declaredParameterTypes,
    expectedReturnType ? *expectedReturnType : unknownType(),
    typeParameterNames(statement.typeParameters))
~~~

Check the body while the scope is active, then update the stored binding with the inferred return type and the same generic-parameter list before popping the scope. Reject a nested function signature that retains a type parameter from an enclosing generic scope; a nested generic declaration must own every parameter name it uses.

- [ ] Step 3: Implement recursive unification.

The core of inferTypeArguments follows this shape:

~~~cpp
if (expected.kind == StaticType::TypeParameter && expected.typeParameterName) {
    if (!isKnown(actual)) {
        return;
    }
    const auto [it, inserted] = substitutions.emplace(
        *expected.typeParameterName, actual);
    if (!inserted
        && (!compatible(it->second, actual)
            || !compatible(actual, it->second))) {
        throw TypeError(callToken,
            "type parameter " + *expected.typeParameterName
                + " inferred as " + typeInfoName(it->second)
                + " and " + typeInfoName(actual));
    }
    return;
}
if (expected.kind == StaticType::Array && actual.kind == StaticType::Array
    && expected.elementType && actual.elementType) {
    inferTypeArguments(
        *expected.elementType, *actual.elementType,
        substitutions, callToken);
    return;
}
if (expected.kind == StaticType::Nullable && expected.nullableOf) {
    if (actual.kind == StaticType::Nil) {
        return;
    }
    if (actual.kind == StaticType::Nullable && actual.nullableOf) {
        inferTypeArguments(
            *expected.nullableOf, *actual.nullableOf,
            substitutions, callToken);
    } else {
        inferTypeArguments(
            *expected.nullableOf, actual, substitutions, callToken);
    }
    return;
}
if (expected.kind == StaticType::Function && actual.kind == StaticType::Function
    && hasFunctionSignature(expected) && hasFunctionSignature(actual)
    && expected.parameterTypes.size() == actual.parameterTypes.size()) {
    for (std::size_t i = 0; i < expected.parameterTypes.size(); ++i) {
        inferTypeArguments(
            expected.parameterTypes[i], actual.parameterTypes[i],
            substitutions, callToken);
    }
    inferTypeArguments(
        *expected.returnType, *actual.returnType,
        substitutions, callToken);
}
~~~

Concrete shapes fall through to compatibility after substitution. Unknown arguments never create a substitution.

- [ ] Step 4: Implement recursive substitution.

substituteTypeParameters returns a copied tree, replacing a TypeParameter when its name is present and recursively rebuilding array, nullable, and function children. Preserve struct names and generic-parameter metadata. An unsubstituted variable remains unchanged so the caller can report missing inference.

- [ ] Step 5: Route local and namespace calls through one generic-aware helper.

checkFunctionCall must verify function kind, complete signature, and arity; for a generic signature it checks arguments without expected context, infers substitutions, reports missing parameters as cannot infer type parameter <name>, substitutes parameter types, and runs existing compatibility checks. For ordinary signatures it preserves current contextual argument checking. It returns the substituted generic return type or the ordinary return type.

Call this helper from checkCall and from the namespace-import alias.name(...) branch in checkMemberCall. Generic aliases from unannotated let bindings retain TypeInfo automatically.

- [ ] Step 6: Reject generic-to-monomorphic assignment explicitly.

Before ordinary checkAssignable logic in annotated let initialization and assignment, detect a generic function value assigned to a function type with no generic parameters and throw:

~~~text
cannot assign generic function to monomorphic function type
~~~

Unannotated bindings copy the generic TypeInfo unchanged.

- [ ] Step 7: Run targeted compiler checks.

~~~bash
cmake --build build --target compiler_design type_utils_tests
./build/compiler_design tests/golden/generic_functions/input.cd
./build/compiler_design tests/golden/generic_imports/input.cd
~~~

Expected: both programs type-check and print ASTs containing Fun identity<T>. The five type-error fixtures each exit nonzero with Type error diagnostics.

- [ ] Step 8: Commit the checker implementation.

~~~bash
git add include/TypeChecker.hpp src/TypeChecker.cpp include/TypeUtils.hpp src/TypeUtils.cpp
git commit -m "feat: infer generic function type arguments"
~~~

### Task 5: Add module-interface and focused golden expectations

**Files:**

- Modify: tests/module_interface_emitter_tests.cpp
- Create/update: tests/golden/generic_functions/ast.out, ir.out, bytecode.out, run.out
- Create/update: tests/golden/generic_imports/ast.out, ir.out, bytecode.out, module-interface.out, run.out
- Create/update: tests/golden/type_errors/generic_*.err and generic_*.exit
- Create/update: tests/bytecode_artifacts/generic_functions/input.cd, expected.cdbc, run.out

- [ ] Step 1: Extend module-interface unit coverage.

Add this value export to the existing module-interface test:

~~~cpp
module.values.push_back(ModuleInterfaceValue{
    "identity",
    functionType({typeParameterType("T")},
                 typeParameterType("T"), {"T"})});
~~~

Extend the sorted expected output with:

~~~text
  export value identity: fun<T>(T): T
~~~

- [ ] Step 2: Seed success markers and refresh compiler goldens.

Create these run.out files before refreshing compiler goldens.

tests/golden/generic_functions/run.out:

~~~text
42
hello
1
true
alias
~~~

tests/golden/generic_imports/run.out:

~~~text
7
module
~~~

Then run:

~~~bash
python3 tests/run_golden_tests.py ./build/compiler_design \
  --update --update-missing --case generic_functions --case generic_imports
~~~

Review AST output for generic names, IR/bytecode output for unchanged ordinary call instructions, and module-interface output for fun<T>(T): T.

- [ ] Step 3: Generate type-error goldens.

~~~bash
python3 tests/run_golden_tests.py ./build/compiler_design --update --case generic_
~~~

Expected: five .err/.exit pairs are generated under tests/golden/type_errors with Type error diagnostics and no stdout. Review that conflict and inference messages use the exact wording from Task 4.

- [ ] Step 4: Generate and execute the bytecode artifact fixture.

Copy the success input with:

~~~bash
mkdir -p tests/bytecode_artifacts/generic_functions
cp tests/golden/generic_functions/input.cd tests/bytecode_artifacts/generic_functions/input.cd
~~~

Then emit the artifact from that path and record its output:

~~~bash
tmp_artifact=$(mktemp)
./build/compiler_design --emit-bytecode "$tmp_artifact" \
  tests/bytecode_artifacts/generic_functions/input.cd
cargo run --quiet --manifest-path vm-rs/Cargo.toml -- run "$tmp_artifact" \
  > /tmp/generic_functions_run.out
cp /tmp/generic_functions_run.out tests/bytecode_artifacts/generic_functions/run.out
cp "$tmp_artifact" tests/bytecode_artifacts/generic_functions/expected.cdbc
rm -f /tmp/generic_functions_run.out "$tmp_artifact"
~~~

The artifact must contain ordinary make_function/call instructions and no generic opcode or runtime metadata.

- [ ] Step 5: Run focused integration checks.

~~~bash
python3 tests/run_golden_tests.py ./build/compiler_design --case generic
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens \
  --case generic_functions --case generic_imports \
  --case generic_duplicate_type_parameter \
  --case generic_out_of_scope_type_parameter \
  --case generic_conflicting_inference \
  --case generic_uninferred_type_parameter \
  --case generic_monomorphic_assignment
~~~

Expected: all new compiler, artifact, and Rust VM checks pass with no VM source changes.

### Task 6: Update grammar, documentation, and roadmap status

**Files:**

- Modify: docs/language-grammar.ebnf
- Modify: README.md
- Modify: AGENTS.md
- Modify: docs/roadmap.md

- [ ] Step 1: Document the function-declaration grammar.

Update docs/language-grammar.ebnf:

~~~ebnf
functionDecl = "fun", identifier, [ typeParameters ], "(",
               [ parameter, { ",", parameter } ], ")",
               [ ":", typeAnnotation ], block ;
typeParameters = "<", identifier, { ",", identifier }, ">" ;
~~~

Keep function-expression and call rules unchanged.

- [ ] Step 2: Update README and AGENTS semantics.

Add the identity<T> syntax and call-site inference examples. State that [T] works inside generic annotations, aliases retain generic signatures, and explicit type arguments, generic methods/lambdas, constraints, and generic containers are not implemented. Update the matching type-system and function bullets in AGENTS.md.

- [ ] Step 3: Mark Phase 9 and Phase 17 in the roadmap.

Mark the first generic-function slice implemented while preserving inference boundaries and higher-order collection deferral. Update Phase 17 to say generic function/type abstraction is complete for this slice and maps/ranges/iterators remain future work.

- [ ] Step 4: Commit goldens and documentation.

~~~bash
git add tests/golden tests/bytecode_artifacts/generic_functions \
  tests/module_interface_emitter_tests.cpp docs/language-grammar.ebnf \
  README.md AGENTS.md docs/roadmap.md
git commit -m "test: cover generic function inference"
~~~

### Task 7: Run the complete verification gate and finish

**Files:**

- Modify: none except generated-cache cleanup

- [ ] Step 1: Run all required checks.

~~~bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
~~~

Expected: every command exits zero; report exact pass counts and confirm the Rust VM has no source changes for this compile-time-only slice.

- [ ] Step 2: Inspect final diff and status.

~~~bash
git status --short --branch
git diff HEAD~5..HEAD --check
git log -8 --oneline
~~~

Expected: only generic AST/parser/type-checker utilities, tests, fixtures, and docs are present; no build products or Python caches are tracked.
