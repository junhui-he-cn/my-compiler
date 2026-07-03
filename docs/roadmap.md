# Compiler Demo Roadmap

This roadmap captures a practical development sequence for growing the compiler demo from the current interpreter-oriented language into a more complete small language and backend playground.

## Guiding Principles

- Develop in vertical slices: update parser, AST, IR, interpreter, tests, grammar docs, and README together when a feature crosses layers.
- Keep golden tests as the main behavior contract.
- Prefer small semantic foundations before large syntax features.
- Do not document planned behavior as implemented behavior in `README.md` or `docs/language-grammar.ebnf`.
- Keep `AGENTS.md` updated when workflows or architecture conventions change.

## Recommended Phase Order

```text
1. Lexical scope
2. Basic type checker
3. Logical operators
4. While loops
5. Diagnostic cleanup
6. Functions
7. Arrays and indexing
8. Bytecode VM or backend expansion
```

## Phase 1: Lexical Scope — Implemented

Status: implemented. Blocks now introduce real variable scopes.

Implementation note: lexical names are resolved during type checking; generated IR uses unique internal variable names rather than runtime scope operations.

Suggested behavior:

- `{ ... }` creates a nested scope.
- `let` declares a variable in the current scope.
- Inner declarations may shadow outer declarations.
- Variable lookup searches from innermost scope outward.
- Assignment updates the nearest existing binding.
- Reading or assigning an undefined variable remains an error.

Likely touch points:

- `src/IRCompiler.cpp`
- `src/IRInterpreter.cpp`
- variable environment representation
- runtime-error golden fixtures
- `README.md`
- `docs/language-grammar.ebnf` if grammar wording changes

Why this first: functions, closures, type checking, and block-local variables all depend on clear scope semantics.

## Phase 2: Basic Type Checker — Implemented

Status: implemented for explicit `let` annotations. Unannotated variables are still accepted without full inference.

Start with these types:

- `number`
- `bool`
- `string`
- `nil` handling as a special initial simple case

Suggested checks:

- `let` initializer matches the annotation when present.
- Assignment value matches the existing variable type.
- Unary operators receive valid operand types.
- Binary operators receive valid operand types.
- `if` conditions have the intended condition type or truthiness rule.

Likely new files:

- `include/TypeChecker.hpp`
- `src/TypeChecker.cpp`

Likely test additions:

- successful typed declarations
- assignment type mismatch
- invalid unary/binary operands
- invalid condition type, if strict boolean conditions are chosen

Important design choice: decide whether type errors get a new golden fixture category or reuse an existing compile-error path.

## Phase 3: Logical Operators — Implemented

Status: implemented. The language supports `&&` and `||` short-circuit expressions using existing truthiness rules and returning the selected operand value.

Goal: add common boolean/control-flow expression operators.

Suggested syntax:

```text
a && b
a || b
```

Suggested semantics:

- `&&` and `||` short-circuit.
- `||` has lower precedence than `&&`.
- `&&` has lower precedence than equality.
- Result value follows the language's chosen truthiness/value rule.

Likely touch points:

- lexer keywords
- parser precedence methods
- AST expression node or desugared lowering strategy
- IR jump lowering
- interpreter execution
- AST/IR/run golden fixtures
- parse-error fixtures for malformed logical expressions

## Phase 4: While Loops — Implemented

Status: implemented. The language supports block-bodied `while` loops with truthy conditions. `break` and `continue` are not implemented yet.

Goal: support basic repeated control flow.

Suggested syntax:

```text
while condition {
  statement*
}
```

Suggested semantics:

- Evaluate condition before each iteration.
- Execute body while condition is truthy or boolean-true, depending on the chosen condition rule.
- Loop body uses the lexical scope behavior from Phase 1.

Likely touch points:

- lexer keyword: `while`
- parser statement handling
- `WhileStmt` AST node and printing
- IR backward jump support, if not already sufficient
- interpreter control flow through IR
- grammar and README docs
- golden tests for zero iterations, multiple iterations, and nested control flow

## Phase 5: Diagnostic Cleanup — Implemented

Status: implemented. Errors now use a shared diagnostic format with `Lex`, `Parse`, `Type`, `Compile`, and `Runtime` categories. Front-end diagnostics include `line:column` locations when available; snippets, carets, and multi-error recovery remain future improvements.

Goal: make errors more compiler-like and easier to test.

Suggested improvements:

- Introduce a common diagnostic representation.
- Preserve source line and column information consistently.
- Distinguish parse, type, compile, and runtime errors clearly.
- Make golden error output stable.
- Consider source snippets after the core diagnostic shape is stable.

Likely touch points:

- `ParseError`
- `IRCompileError`
- `IRRuntimeError`
- future `TypeError` or diagnostic type
- `src/main.cpp`
- parse/type/runtime error golden fixtures

This phase can happen before or after the basic type checker. Doing it before type checking may reduce rework.

## Phase 6: Functions

Goal: add reusable code and call frames.

Recommended split:

### Phase 6A: Function Declarations and Calls — Implemented

Status: implemented. The language supports named functions, calls, explicit `return`, bare `return;`, implicit `nil` returns, and recursion. Closures are not implemented yet.

Suggested syntax:

```text
fun add(a, b) {
  return a + b;
}

print add(1, 2);
```

Suggested features:

- function declaration statement
- call expression
- parameters and arguments
- `return` statement
- function values
- runtime call frames

### Phase 6B: Closures — Implemented

Status: implemented. Nested `fun` declarations capture enclosing local variables by reference. Captured variables stay alive through shared runtime cells, and closure reads/assignments observe the same cell.

### Phase 6C: Function Expressions / Lambdas

Future work. Add expression-level function literals after closure semantics are stable. This phase should reuse the same by-reference closure capture model introduced in Phase 6B.

## Phase 7: Arrays and Indexing — Implemented

Status: implemented. The language supports array literals, mixed element values, read-only indexing, chained indexing/calls, and runtime errors for invalid index operations. Array mutation, `push`, and `len` remain future work.

Suggested syntax:

```text
let xs = [1, 2, 3];
print xs[0];
```

Suggested features:

- array literals
- index expressions
- bounds checking
- runtime errors for invalid index types or out-of-range access
- optional later mutation syntax

Likely touch points:

- lexer tokens: brackets and comma
- parser expression grammar
- AST nodes
- `Value` representation
- IR operations or interpreter support
- type checker rules after Phase 2
- golden tests

## Phase 8: Bytecode VM or Backend Expansion

### Phase 8A: Bytecode VM — Implemented

Status: implemented. The compiler now has a parallel register-based bytecode backend. `--bytecode` prints lowered bytecode, and `--run-bytecode` executes it through the bytecode VM while preserving the existing `--ir` and `--run` paths.

The bytecode VM currently reuses the existing runtime `Value` representation. It includes VM heap and VM thread/frame boundaries so later phases can add GC, task scheduling, and JIT support without reworking the frontend.

Future Phase 8 follow-ups:

- GC: replace VM heap internals with tracing collection and explicit root scanning.
- Task scheduling: make VM threads schedulable with instruction budgets, yield points, and blocked states.
- JIT: compile hot `BytecodeFunction` units to native code using bytecode-level metadata.

## Near-Term Recommendation

Start with Phase 1: lexical scope. It unlocks cleaner semantics for future type checking, functions, closures, and block-local behavior.
