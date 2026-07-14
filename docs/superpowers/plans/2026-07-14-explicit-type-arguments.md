# Explicit Generic Call Arguments Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Add explicit type arguments to calls of the existing generic named-function slice while preserving ordinary runtime calls.

**Architecture:** Extend call AST nodes with type-annotation metadata, parse the canonical compact name<T>(...) postfix form with balanced-angle lookahead, and seed the existing generic substitution map from explicit arguments. IR, bytecode, artifacts, and the Rust VM remain unchanged because type arguments are erased before lowering.

**Tech Stack:** C++17 compiler front end, Python golden fixtures, C++ IR/bytecode printers, Rust CDBC VM parity tests, Markdown language documentation.

---

### Task 1: Add red fixtures

**Files:**
- Create: tests/golden/generic_explicit_type_arguments/input.cd
- Create: tests/golden/type_errors/generic_explicit_wrong_arity.cd
- Create: tests/golden/type_errors/generic_explicit_non_generic.cd
- Create: tests/golden/type_errors/generic_explicit_argument_mismatch.cd
- Create: tests/golden/type_errors/generic_explicit_unknown_type.cd

- [x] **Step 1: Write the success source fixture**

Create the success fixture with identity, array-head, return-only, and alias
calls:

~~~cd
fun identity<T>(value: T): T {
  return value;
}
fun head<T>(values: [T]): T {
  return values[0];
}
fun make<T>(): T? {
  return nil;
}
print identity<number>(42);
print identity<string>("hello");
print head<[number]>([1, 2]);
print make<number>();
let alias = identity;
print alias<bool>(true);
~~~

- [x] **Step 2: Write type-error fixtures**

Use these sources:

~~~cd
fun identity<T>(value: T): T { return value; }
print identity<number, string>(1);
~~~

for wrong arity;

~~~cd
fun identity(value: number): number { return value; }
print identity<number>(1);
~~~

for a non-generic function;

~~~cd
fun identity<T>(value: T): T { return value; }
print identity<number>("bad");
~~~

for an explicit argument mismatch; and:

~~~cd
fun identity<T>(value: T): T { return value; }
print identity<Missing>(1);
~~~

for an unknown type argument.

- [x] **Step 3: Verify the red state**

Run:

~~~sh
./build/compiler_design tests/golden/generic_explicit_type_arguments/input.cd
./build/compiler_design tests/golden/type_errors/generic_explicit_argument_mismatch.cd
~~~

Expected: both commands exit non-zero with parse diagnostics because the
current parser does not accept explicit call type arguments.

### Task 2: Extend the call AST and postfix parser

**Files:**
- Modify: include/Ast.hpp call nodes
- Modify: src/Ast.cpp call printing
- Modify: include/Parser.hpp parser declarations
- Modify: src/Parser.cpp call postfix parsing

- [x] **Step 1: Store and print type arguments**

Add vector<TypeAnnotation> typeArguments to CallExpr and MemberCallExpr, and
accept it in both constructors before the runtime argument vector. Print it
after the callee/name using the existing annotation printer. The AST line for
identity<number>(42) must be:

~~~text
Print (call identity<number> 42)
~~~

- [x] **Step 2: Parse the explicit postfix**

Add these private methods:

~~~cpp
bool isExplicitTypeArgumentCall(const Expr& callee) const;
std::vector<TypeAnnotation> explicitTypeArguments();
ExprPtr finishCall(ExprPtr callee, std::vector<TypeAnnotation> typeArguments);
~~~

The lookahead only accepts a variable or field callee, requires the less-than
token to be directly adjacent to the callee name, scans balanced angle tokens,
and requires the matching greater-than token to be followed by a left
parenthesis. This preserves ordinary comparison expressions. The parser
consumes one or more existing typeAnnotation values separated by commas.
Ordinary calls pass an empty type-argument vector.

- [x] **Step 3: Run the parser-focused command**

~~~sh
cmake --build build
./build/compiler_design tests/golden/generic_explicit_type_arguments/input.cd
~~~

Expected: parsing succeeds and type checking reports the not-yet-supported
explicit type-argument behavior.

### Task 3: Add explicit substitution to type checking

**Files:**
- Modify: include/TypeChecker.hpp checkFunctionCall declaration
- Modify: src/TypeChecker.cpp generic call, builtin, namespace, and member-call checks

- [x] **Step 1: Thread type arguments through call checking**

Change checkFunctionCall to accept const vector<TypeAnnotation>&
typeArguments. Pass the AST vector from checkCall and namespace member calls.

- [x] **Step 2: Seed the existing substitution map**

For a non-empty type-argument vector, require a complete function signature and
a non-empty generic-parameter list. Require equal list lengths, resolve each
annotation with resolveAnnotation, and bind each generic parameter to the
resolved TypeInfo. Skip inferTypeArguments when explicit bindings exist;
retain the current inference path for ordinary generic calls. Reuse the
existing substituted argument and return compatibility checks.

Use these diagnostics:

~~~text
function is not generic
explicit type arguments require a known function signature
expected N type arguments but got M
argument N expects <type>, got <type>
~~~

Reject explicit arguments on builtins, builtin member calls, enum constructors,
and ordinary struct methods. Namespace-exported generic functions use the same
checkFunctionCall path as local functions.

- [x] **Step 3: Run focused type-error commands**

~~~sh
./build/compiler_design tests/golden/type_errors/generic_explicit_wrong_arity.cd
./build/compiler_design tests/golden/type_errors/generic_explicit_non_generic.cd
./build/compiler_design tests/golden/type_errors/generic_explicit_argument_mismatch.cd
./build/compiler_design tests/golden/type_errors/generic_explicit_unknown_type.cd
~~~

Expected: diagnostics match the design document and no IR or runtime changes
are needed.

### Task 4: Add goldens and parity coverage

**Files:**
- Create: tests/golden/generic_explicit_type_arguments AST, IR, bytecode, and run outputs
- Create: tests/bytecode_artifacts/generic_explicit_type_arguments input, artifact, and run outputs
- Create: type-error err and exit files beside the four fixtures

- [x] **Step 1: Generate and review compiler goldens**

Copy the success input to the bytecode-artifact fixture and run:

~~~sh
python3 tests/run_golden_tests.py ./build/compiler_design --update --update-missing --case generic_explicit_type_arguments
python3 tests/run_golden_tests.py ./build/compiler_design --update --case generic_explicit
~~~

Review that the AST preserves type arguments while IR and bytecode contain
ordinary call instructions with no type metadata.

- [x] **Step 2: Generate and review the CDBC artifact**

~~~sh
./build/compiler_design --emit-bytecode /tmp/generic-explicit.cdbc tests/bytecode_artifacts/generic_explicit_type_arguments/input.cd
cp /tmp/generic-explicit.cdbc tests/bytecode_artifacts/generic_explicit_type_arguments/expected.cdbc
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case generic_explicit_type_arguments --goldens
~~~

Confirm that the artifact has no type-argument section or new opcode.

### Task 5: Update language documentation

**Files:**
- Modify: README.md
- Modify: docs/language-grammar.ebnf
- Modify: docs/roadmap.md

- [x] **Step 1: Document the implemented syntax**

Document identity<number>(42) and namespace calls in README.md. Add an
optional call type-argument production near the call-expression grammar.
Remove explicit type arguments from remaining Phase 9 work while retaining
generic methods, constraints, and broader generic container syntax.

- [x] **Step 2: Check documentation consistency**

~~~sh
rg -n -i 'explicit type arguments|generic methods|identity<number>|call.*type' README.md AGENTS.md docs/language-grammar.ebnf docs/roadmap.md
git diff --check
~~~

Expected: user-facing docs describe the syntax and roadmap/AGENTS limitations
no longer call explicit type arguments unimplemented.

### Task 6: Run the completion verification suite

**Files:**
- Verify: build/, tests/, vm-rs/

- [x] **Step 1: Run focused checks**

~~~sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --case generic_explicit
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case generic_explicit_type_arguments --goldens
~~~

- [x] **Step 2: Run the full repository gate**

~~~sh
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

- [x] **Step 3: Inspect final workspace state**

~~~sh
git diff --check
git status --short
~~~

The only intentional changes are the explicit-type-argument implementation,
tests, and associated design/documentation updates.
