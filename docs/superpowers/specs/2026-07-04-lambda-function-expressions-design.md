# Lambda / Function Expressions Design

Date: 2026-07-04

## Goal

Implement Phase 6C function expressions, also called lambdas, as expression-level anonymous functions that reuse the existing function, closure, IR, bytecode, and runtime call machinery.

This phase adds no new runtime value kind and no new IR or bytecode opcode. A lambda should compile to the same function table plus `make_function` representation used by named functions.

## Scope

In scope:

- Function expression syntax:

  ```text
  fun (parameter*) { declaration* }
  ```

- Function expressions in ordinary expression positions:

  ```text
  let inc = fun (x) {
    return x + 1;
  };

  print inc(41);
  print (fun (x) { return x * 2; })(3);
  ```

- Closures that capture enclosing variables by reference through existing shared cells.
- Function expressions with zero or more parameters.
- `return` statements inside function expression bodies.
- AST, parser, type checker, IR compiler, bytecode output, IR interpreter, and bytecode VM parity.
- Golden coverage for AST, IR, bytecode, `--run`, and `--run-bytecode`.
- Parse/type error coverage for malformed lambdas and duplicate parameters.
- Documentation updates for grammar, README, roadmap, and AGENTS.

Out of scope:

- Expression-body lambdas such as `fun (x) => x + 1`.
- A new `lambda` keyword.
- `|x| { ... }` syntax.
- Function type annotations.
- Return type inference.
- Generic function types.
- New runtime function value representation.
- New IR or bytecode opcodes.

## Syntax and Parsing

Function declarations remain statements:

```text
fun add(a, b) {
  return a + b;
}
```

Function expressions omit the name and appear as expressions:

```text
fun (a, b) {
  return a + b;
}
```

The parser distinguishes the forms by context:

- In declaration position, `fun` starts a function declaration and must be followed by an identifier.
- In expression/primary position, `fun` starts a function expression and must be followed by `(`.

This means a top-level anonymous function statement without a semicolon is still a parse error through the declaration path:

```text
fun (x) { return x; }
```

The first implementation should keep that behavior. Supporting bare lambda expression statements beginning with `fun` would require changing declaration disambiguation and is out of scope.

Valid expression-statement form:

```text
(fun (x) { return x; });
```

or:

```text
let id = fun (x) { return x; };
```

Grammar update:

```text
primary      = functionExpr
             | "false"
             | "true"
             | "nil"
             | number
             | string
             | array
             | identifier
             | "(", expression, ")" ;

functionExpr = "fun", "(", [ parameters ], ")", block ;
```

## AST

Add:

```cpp
struct FunctionExpr final : Expr {
    FunctionExpr(Token keyword, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out) const override;

    Token keyword;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};
```

The `keyword` token provides a stable source location for diagnostics and resolved-name bookkeeping.

Suggested AST print shape:

```text
(fun (x) (return (+ x 1)))
```

The exact format should be deterministic and consistent with existing AST printer style. Golden files are the behavior contract.

## Type Checking and Name Resolution

Function expressions should return `StaticType::Function`.

The type checker should create resolved-name metadata for `FunctionExpr` values, parallel to existing `FunctionStmt` metadata:

```cpp
const std::string& functionName(const FunctionExpr& expression) const;
const std::vector<std::string>& parameterNames(const FunctionExpr& expression) const;

void recordFunction(const FunctionExpr& expression, std::string name);
void recordParameters(const FunctionExpr& expression, std::vector<std::string> names);
```

Function expression checking:

1. Generate an internal function name. Use a stable runtime/debug display name such as `<lambda>` and avoid exposing counter suffixes in normal printed function values.
2. Record the function expression name in `ResolvedNames`.
3. Enter a new function scope.
4. Declare parameters in that scope as `StaticType::Unknown`.
5. Record parameter resolved names in `ResolvedNames`.
6. Increment `functionDepth_` while checking the body so `return` is legal.
7. Check body declarations/statements.
8. Restore the previous function depth and scope.
9. Return `StaticType::Function`.

Duplicate parameter names should continue to use the existing same-scope duplicate declaration diagnostic:

```text
Type error at <line>:<column>: duplicate declaration `<name>`
```

`return` outside any function or function expression remains a type error:

```text
Type error at <line>:<column>: return outside function
```

No return type is inferred in this phase.

## IR Lowering

Add `IRCompiler::emitFunctionExpr()` or equivalent.

Named function declaration lowering currently needs a placeholder binding so recursion can refer to the function by name:

```text
store nil into function name
compile function body
make_function
assign function name
```

Function expressions do not declare a source-level name, so lowering is simpler:

```text
compile lambda body into IR function table
emit make_function for that function index
return the resulting register
```

Example:

```text
let add = fun (a, b) {
  return a + b;
};
```

Expected IR shape:

```text
v0 = make_function $0 <lambda>/2
store_var add#0, v0

function $0 <lambda>/2
...
```

The exact instruction numbering/registers are implementation-defined and covered by goldens.

Function expressions are anonymous. Direct self-recursion inside a lambda is not supported in this phase unless the lambda is assigned to a variable and refers to that variable from its body after normal name resolution permits it.

## Runtime and Closure Semantics

Function expressions produce ordinary function runtime values.

Closure capture should match nested named functions:

```text
fun makeAdder(n) {
  return fun (x) {
    return x + n;
  };
}
```

When `make_function` executes, the interpreter/VM captures visible local and closure cells by reference. Assignments through those cells must be visible to closures returned from function expressions:

```text
fun counter() {
  let n = 0;
  return fun () {
    n = n + 1;
    return n;
  };
}

let next = counter();
print next();  // 1
print next();  // 2
```

Printing a function expression value should use the existing `valueToString()` function format. Anonymous functions should display as:

```text
<fun <lambda>>
```

Avoid including generated numeric ids in normal user-visible function names, so golden output remains stable.

## Bytecode

No new bytecode opcode is required.

The existing pipeline remains:

```text
AST -> IRProgram -> BytecodeProgram -> BytecodeVM
```

Function expressions lower to existing IR operations, then to existing bytecode operations:

- `MakeFunction`
- `Call`
- `Return`
- `LoadVar`
- `StoreVar`
- `AssignVar`

`--run-bytecode` should match `--run` for all lambda fixtures.

## Diagnostics

Recommended parse errors:

- In declaration position:

  ```text
  fun (x) { return x; }
  ```

  remains:

  ```text
  Parse error at <line>:<column>: expected function name after `fun`
  ```

- Missing `)` after parameters:

  ```text
  let f = fun (x { return x; };
  ```

  should report:

  ```text
  Parse error at <line>:<column>: expected `)` after function parameters
  ```

- Missing `{` before body:

  ```text
  let f = fun (x) return x;
  ```

  should report:

  ```text
  Parse error at <line>:<column>: expected `{` before function body
  ```

Recommended type error:

```text
let f = fun (x, x) { return x; };
```

```text
Type error at <line>:<column>: duplicate declaration `x`
```

## Golden Tests

Add success fixtures:

### `lambda_basic`

```text
let inc = fun (x) {
  return x + 1;
};
print inc(41);
```

Expected run output:

```text
42
```

### `lambda_immediate_call`

```text
print (fun (x) {
  return x * 2;
})(3);
```

Expected run output:

```text
6
```

### `lambda_closure`

```text
fun makeAdder(n) {
  return fun (x) {
    return x + n;
  };
}

let add10 = makeAdder(10);
print add10(5);
```

Expected run output:

```text
15
```

### `lambda_mutable_closure`

```text
fun counter() {
  let n = 0;
  return fun () {
    n = n + 1;
    return n;
  };
}

let next = counter();
print next();
print next();
```

Expected run output:

```text
1
2
```

For representative success fixtures, include:

- `ast.out`
- `ir.out`
- `bytecode.out`
- `run.out`
- `run_bytecode.out`

Add parse/type error fixtures for malformed parameter lists, missing body braces, and duplicate lambda parameters.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf`
- `README.md`
- `docs/roadmap.md`
- `AGENTS.md`

Roadmap should mark Phase 6C as implemented after the implementation is complete.

README should remove the statement that lambda/function-expression syntax is not implemented and document the new expression form.

AGENTS should add function expressions to current language semantics and remind future agents that function-like backend changes must keep IR interpreter and bytecode VM behavior aligned.

## Success Criteria

Phase 6C is complete when:

- `fun (params) { body }` parses as an expression in primary-expression positions.
- Function expressions can be assigned, called, immediately called, returned, and stored in arrays like other function values.
- Function expressions support by-reference closure capture through existing shared runtime cells.
- `return` is legal inside function expression bodies and illegal outside functions.
- Duplicate lambda parameters are type errors.
- `--run` and `--run-bytecode` outputs match for lambda fixtures.
- No new IR or bytecode opcode is introduced.
- Existing tests continue to pass.
- New golden tests cover AST, IR, bytecode, run output, parse errors, and type errors where relevant.
- Documentation reflects implemented behavior without describing future lambda features as complete.

