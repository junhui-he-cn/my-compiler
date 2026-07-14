# Generic Methods Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add generic methods to named structs with inferred and explicit type arguments while preserving ordinary method lowering and module behavior.

**Architecture:** Extend method AST and exported method metadata with ordered type-parameter names. Resolve method signatures and bodies inside a method-local type-parameter scope, then adapt the existing generic function-call checker for member calls. IR, bytecode, and Rust VM remain unchanged because method type arguments are erased before lowering.

**Tech Stack:** C++17 parser/AST/type checker, existing TypeInfo generic inference, Python golden fixtures, text `.cdbc` artifacts, Rust VM parity tests, Markdown language docs.

---

### Task 1: Add red fixtures for generic methods

**Files:**

- Create: `tests/golden/generic_methods/input.cd`
- Create: `tests/golden/generic_method_imports/input.cd`
- Create: `tests/golden/generic_method_imports/lib.cd`
- Create: `tests/golden/type_errors/generic_method_duplicate_type_parameter.cd`
- Create: `tests/golden/type_errors/generic_method_conflicting_inference.cd`
- Create: `tests/golden/type_errors/generic_method_uninferred_type_parameter.cd`
- Create: `tests/golden/type_errors/generic_method_argument_mismatch.cd`
- Create: `tests/golden/type_errors/generic_method_explicit_non_generic.cd`

- [x] **Step 1: Write the local success source**

Create `tests/golden/generic_methods/input.cd`:

```cd
struct Box { value: number }

impl Box {
  fun echo<T>(value: T): T {
    return value;
  }

  fun head<T>(values: [T]): T {
    return values[0];
  }

  fun make<T>(): T? {
    return nil;
  }
}

let box: Box = Box { value: 0 };
print box.echo<number>(42);
print box.echo("hello");
print box.head<number>([3, 4]);
print box.make<string>();
```

- [x] **Step 2: Write the imported success source**

Create `tests/golden/generic_method_imports/lib.cd`:

```cd
struct Box { value: number }

impl Box {
  fun echo<T>(value: T): T {
    return value;
  }
}

export Box;
```

Create `tests/golden/generic_method_imports/input.cd`:

```cd
import "lib.cd";
import "lib.cd" as lib;

let direct: Box = Box { value: 0 };
let namespaced: lib.Box = lib.Box { value: 0 };
print direct.echo<number>(7);
print namespaced.echo<string>("module");
```

- [x] **Step 3: Write type-error sources**

Use these exact cases:

```cd
// generic_method_duplicate_type_parameter.cd
struct Box { value: number }
impl Box {
  fun duplicate<T, T>(value: T): T { return value; }
}
```

```cd
// generic_method_conflicting_inference.cd
struct Box { value: number }
impl Box {
  fun same<T>(left: T, right: T): T { return left; }
}
let box: Box = Box { value: 0 };
box.same(1, "x");
```

```cd
// generic_method_uninferred_type_parameter.cd
struct Box { value: number }
impl Box {
  fun make<T>(): T? { return nil; }
}
let box: Box = Box { value: 0 };
box.make();
```

```cd
// generic_method_argument_mismatch.cd
struct Box { value: number }
impl Box {
  fun echo<T>(value: T): T { return value; }
}
let box: Box = Box { value: 0 };
box.echo<number>("bad");
```

```cd
// generic_method_explicit_non_generic.cd
struct Box { value: number }
impl Box {
  fun value(): number { return this.value; }
}
let box: Box = Box { value: 1 };
box.value<number>();
```

- [x] **Step 4: Verify the red state**

Run:

```sh
./build/compiler_design tests/golden/generic_methods/input.cd
./build/compiler_design tests/golden/type_errors/generic_method_conflicting_inference.cd
```

Expected: the first command fails while parsing `fun echo<T>` in an impl
block, and the second fails before the intended inference diagnostic because
generic method syntax is not yet accepted. Do not add expected output files
until implementation makes the cases pass.

### Task 2: Store and parse method type parameters

**Files:**

- Modify: `include/Ast.hpp` (`MethodDecl`)
- Modify: `src/Ast.cpp` (`MethodDecl` constructor and `ImplStmt::print`)
- Modify: `src/Parser.cpp` (`Parser::methodDeclaration`)

- [x] **Step 1: Extend `MethodDecl`**

Use the same field order as `FunctionStmt`:

```cpp
MethodDecl(
    Token name,
    std::vector<Token> typeParameters,
    std::vector<Parameter> parameters,
    std::optional<TypeAnnotation> returnTypeName,
    std::vector<StmtPtr> body);

Token name;
std::vector<Token> typeParameters;
std::vector<Parameter> parameters;
```

Move `typeParameters` in the constructor and keep existing method fields
unchanged.

- [x] **Step 2: Parse the optional list**

Immediately after consuming the method name, call the existing parser helper:

```cpp
Token name = consume(TokenType::Identifier, "expected method name after `fun`");
std::vector<Token> parsedTypeParameters = typeParameters();
consume(TokenType::LeftParen, "expected `(` after method name");
```

Pass `parsedTypeParameters` to the new constructor. Function declarations and
function expressions continue using their existing paths.

- [x] **Step 3: Print the method list**

Print `<T, U>` after the method name and before its parameter list, matching
the existing `FunctionStmt` output. The local fixture must contain lines such
as `Method echo<T>(value: T): T` and `Method head<T>(values: [T]): T`.

- [x] **Step 4: Verify parser-only progress**

Run:

```sh
cmake --build build --target compiler_design
./build/compiler_design tests/golden/generic_methods/input.cd
```

Expected: parsing succeeds and type checking now reports an unknown or
unsupported method type behavior rather than a parse error.

### Task 3: Carry generic method metadata through modules and interfaces

**Files:**

- Modify: `include/TypeCheckerTypes.hpp` (`MethodSignature`)
- Modify: `include/ModuleInterface.hpp` (`ModuleInterfaceMethod`)
- Modify: `include/TypeChecker.hpp` (`MethodInfo`)
- Modify: `src/TypeChecker.cpp` (method registration, import/qualification, and interface construction)
- Modify: `src/ModuleInterfaceEmitter.cpp` (method signature text)
- Modify: `tests/module_interface_emitter_tests.cpp` (generic method assertion)

- [x] **Step 1: Add ordered names**

Append `std::vector<std::string> genericParameters;` to `MethodSignature`,
`ModuleInterfaceMethod`, and `TypeChecker::MethodInfo`. Appending preserves
existing aggregate initializers.

- [x] **Step 2: Register and copy the names**

When registering a method, set `info.genericParameters` from
`typeParameterNames(method.typeParameters)`. Copy it in
`methodSignatureFromInfo` and `methodInfoFromSignature`. Namespace
qualification must preserve the vector while recursively qualifying only
receiver, parameter, and return types.

- [x] **Step 3: Include method names in module interfaces**

When building `ModuleInterfaceMethod`, pass the copied generic-parameter
vector. Update the emitter to write `method echo<T>(T): T`; ordinary methods
retain their current output.

- [x] **Step 4: Add the focused emitter test**

Add a `ModuleInterfaceMethod{"echo", {typeParameterType("T")},
typeParameterType("T"), {"T"}}` entry and assert that the sorted interface
contains `method echo<T>(T): T`.

- [x] **Step 5: Run the focused unit test**

Run:

```sh
cmake --build build --target module_interface_emitter_tests
ctest --test-dir build -R '^module_interface_emitter$' --output-on-failure
```

Expected: the existing interface assertions and the new generic method line
pass.

### Task 4: Reuse generic call checking for method calls

**Files:**

- Modify: `include/TypeChecker.hpp` (remove or replace `checkMethodArguments`)
- Modify: `src/TypeChecker.cpp` (`registerMethodSignature`, `checkMethodBody`, `checkStructMethodCall`)

- [x] **Step 1: Resolve signatures in a method type scope**

In `registerMethodSignature`, call `beginTypeParameterScope(method.typeParameters)`
before resolving parameter and return annotations, set the method's generic
parameter vector, then call `endTypeParameterScope()` after resolution. Keep
duplicate detection in the shared scope helper.

- [x] **Step 2: Keep the scope active while checking the body**

In `checkMethodBody`, call `beginTypeParameterScope(declaration.typeParameters)`
before body checking and `endTypeParameterScope()` after storing the inferred
return type. This makes body annotations such as `let copy: T = value;`
resolve consistently with the signature.

- [x] **Step 3: Adapt the member call**

Replace the existing explicit-argument rejection and bespoke argument loop in
`checkStructMethodCall` with:

```cpp
const TypeInfo signature = functionType(
    method->parameterTypes,
    method->returnType,
    method->genericParameters);
const CheckedExpression result = checkFunctionCall(
    expression.paren,
    signature,
    expression.typeArguments,
    expression.arguments);
resolvedNames_.recordMemberCallCallee(expression, method->resolvedName, true);
return result;
```

The receiver is still checked by `checkMemberCall` before this helper, and IR
continues to prepend it as the implicit first runtime argument. Non-generic
methods therefore retain existing arity and diagnostics.

- [x] **Step 4: Build and run focused source checks**

Run:

```sh
cmake --build build --target compiler_design
./build/compiler_design tests/golden/generic_methods/input.cd
./build/compiler_design tests/golden/type_errors/generic_method_conflicting_inference.cd
```

Expected: the success source type-checks and the error source reports the
generic conflicting-inference diagnostic at the method call.

### Task 5: Add golden, artifact, and documentation coverage

**Files:**

- Create: expected files under `tests/golden/generic_methods/`
- Create: expected files under `tests/golden/generic_method_imports/`
- Create: `.err` and `.exit` beside the five type-error fixtures
- Create: `tests/bytecode_artifacts/generic_methods/input.cd`
- Create: `tests/bytecode_artifacts/generic_methods/expected.cdbc`
- Create: `tests/bytecode_artifacts/generic_methods/run.out`
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`

- [x] **Step 1: Generate and review success outputs**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --update --update-missing --case generic_methods
python3 tests/run_golden_tests.py ./build/compiler_design --update --update-missing --case generic_method_imports
```

Confirm AST output preserves `<T>` and call-site type arguments while IR and
bytecode show ordinary method calls with the receiver as the first argument.

- [x] **Step 2: Generate and review type errors**

Run each type-error fixture once, capture its stable stderr and exit code into
the adjacent `.err` and `.exit` files, and verify no fixture emits stdout. The
expected messages are duplicate parameter, conflicting inference, missing
inference, substituted argument mismatch, and `function is not generic`.

- [x] **Step 3: Add bytecode parity**

Copy the local source into the artifact fixture and run:

```sh
./build/compiler_design --emit-bytecode /tmp/generic-methods.cdbc tests/bytecode_artifacts/generic_methods/input.cd
cp /tmp/generic-methods.cdbc tests/bytecode_artifacts/generic_methods/expected.cdbc
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case generic_methods --goldens
```

Confirm the artifact has no type-argument metadata or new opcode.

- [x] **Step 4: Synchronize docs**

Document generic method declarations, inferred calls, explicit calls, and
imported method behavior in `README.md`; add the optional method type-parameter
production to `docs/language-grammar.ebnf`; and remove generic methods from the
remaining Phase 9 work in `docs/roadmap.md` while leaving generic lambdas,
constraints, and broader generic containers planned.

- [x] **Step 5: Review documentation and diff hygiene**

Run:

```sh
rg -n -i 'generic method|echo<T>|method.*type|generic lambdas' README.md AGENTS.md docs/language-grammar.ebnf docs/roadmap.md
git diff --check
```

### Task 6: Run the completion gate and push

**Files:**

- Verify: `build/`, `tests/`, `vm-rs/`

- [x] **Step 1: Run focused checks**

```sh
cmake --build build
    ctest --test-dir build -R 'type_utils|module_interface_emitter' --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design --case generic_method
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case generic_methods --goldens
```

- [x] **Step 2: Run the full repository gate**

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

- [x] **Step 3: Commit the focused slice**

```sh
git add docs/superpowers/specs/2026-07-14-generic-methods-design.md docs/superpowers/plans/2026-07-14-generic-methods.md include/Ast.hpp src/Ast.cpp src/Parser.cpp include/TypeCheckerTypes.hpp include/ModuleInterface.hpp include/TypeChecker.hpp src/TypeChecker.cpp src/ModuleInterfaceEmitter.cpp README.md docs/language-grammar.ebnf docs/roadmap.md tests/golden tests/bytecode_artifacts/generic_methods
git commit -m "feat: add generic methods"
```

- [x] **Step 4: Push the completed commit**

```sh
git push
```
