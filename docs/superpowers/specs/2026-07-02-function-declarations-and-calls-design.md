# Function Declarations and Calls Design

## Goal

Implement Phase 6A from `docs/roadmap.md`: add named function declarations, call expressions, and explicit returns while leaving closures for a later phase.

This feature should integrate vertically with lexer, parser, AST printing, type checking/name resolution, IR compilation, IR interpretation, golden tests, grammar docs, README, and roadmap.

## Scope

Support:

```cd
fun add(a, b) {
  return a + b;
}

print add(1, 2);
```

Included in Phase 6A:

- Named function declarations: `fun name(params...) { declaration* }`
- Call expressions: `callee(args...)`
- Explicit returns: `return expression;`
- Bare returns: `return;`, returning `nil`
- Implicit `nil` return when execution reaches the end of a function body
- Recursive function calls
- Function declarations in top-level and block scopes
- Function values as runtime values

Excluded from Phase 6A:

- Closures / captured local variables
- Anonymous functions
- Function type annotations
- Parameter type annotations
- Return type annotations
- Methods/classes
- Default arguments
- Variadic functions
- Native/built-in functions beyond existing language behavior

## Syntax

Add keywords:

- `fun`
- `return`

Add token:

- `,`

Grammar additions:

```ebnf
declaration = funDecl
            | letDecl
            | statement ;

funDecl     = "fun", identifier,
              "(", [ parameters ], ")",
              block ;

parameters  = identifier, { ",", identifier } ;

statement   = printStmt
            | ifStmt
            | whileStmt
            | returnStmt
            | block
            | exprStmt ;

returnStmt  = "return", [ expression ], ";" ;

call        = primary,
              { "(", [ arguments ], ")" } ;

arguments   = expression, { ",", expression } ;
```

Expression precedence change:

```ebnf
unary       = ( "!" | "-" ), unary
            | call ;

call        = primary,
              { "(", [ arguments ], ")" } ;
```

Calls bind tighter than unary/binary operators, so:

```cd
add(1, 2) + 3
```

parses as:

```text
(+ (call add 1 2) 3)
```

## AST Design

Add expression node:

```cpp
struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, Token paren, std::vector<ExprPtr> arguments);
    void print(std::ostream& out) const override;

    ExprPtr callee;
    Token paren;
    std::vector<ExprPtr> arguments;
};
```

`paren` stores the closing parenthesis token for diagnostics such as wrong arity.

Add statement nodes:

```cpp
struct FunctionStmt final : Stmt {
    FunctionStmt(Token name, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};

struct ReturnStmt final : Stmt {
    ReturnStmt(Token keyword, ExprPtr value);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
    ExprPtr value;
};
```

AST print example:

```cd
fun add(a, b) {
  return a + b;
}
print add(1, 2);
```

prints:

```text
Program
  Fun add(a, b)
    Return (+ a b)
  Print (call add 1 2)
```

Bare return prints as:

```text
Return nil
```

## Static Name Resolution and Type Checking

The current type checker also resolves lexical names to unique internal variable names. Function support should extend that resolver without adding full function type inference.

### Static Function Type

Extend `StaticType` with a function type representation sufficient for arity checks.

A minimal approach:

```cpp
enum class StaticTypeKind { Unknown, Nil, Number, Bool, String, Function };

struct StaticType {
    StaticTypeKind kind;
    std::optional<std::size_t> arity;
};
```

If keeping the existing `enum class StaticType` is preferred for a smaller patch, add `Function` and store arity in the binding instead:

```cpp
struct Binding {
    StaticType type;
    std::string resolvedName;
    std::optional<std::size_t> arity;
};
```

The second approach is sufficient for Phase 6A.

### Declaration Rules

A function declaration:

1. Declares the function name in the current scope.
2. Records it as a function binding with known arity.
3. Checks the function body in a new function scope.
4. Defines parameters as function-local `unknown` bindings.

This enables recursion because the function name is available before the body is checked.

Same-scope duplicate declarations remain type errors:

```cd
fun f() {}
fun f() {} // type error
```

A function and variable in the same scope with the same name is also a duplicate declaration.

### Return Rules

Track a function-depth counter in the type checker.

- `return` outside a function is a type error:
  ```text
  Type error at 1:1: return outside function
  ```
- `return expression;` checks the expression.
- `return;` is valid and has value `nil` at runtime.
- No static return type inference/checking is done in Phase 6A.

### Call Rules

For `callee(args...)`:

- Check the callee expression.
- Check all argument expressions.
- If callee is a known function binding with known arity, argument count must match.
- If callee is a known non-function type, report a type error.
- If callee is `unknown`, allow the call statically and let runtime validate.
- Call result static type is `unknown`.

Example type error:

```cd
fun add(a, b) { return a + b; }
print add(1);
```

reports a wrong-arity type error before IR compilation.

### Closure Boundary Rule

Phase 6A does not support closures. Function bodies may use:

- Their parameters
- Locals declared inside the function body
- The function’s own name for recursion
- Global bindings

Function bodies may not capture non-global locals from enclosing block scopes.

Example allowed:

```cd
let g = 1;
fun f() {
  print g;
}
```

Example rejected:

```cd
{
  let x = 1;
  fun f() {
    print x;
  }
}
```

Diagnostic shape:

```text
Type error at 4:11: cannot capture local variable `x`
```

Implementation can track each binding’s scope depth and whether lookup crosses a function boundary. Global scope depth `0` remains accessible inside functions; non-global enclosing scopes are not capturable.

## Runtime Value Design

Extend `Value` with function values.

Suggested representation:

```cpp
struct FunctionValue {
    std::string name;
    std::size_t functionIndex;
    std::size_t arity;
};
```

`Value::Type` gains:

```cpp
Function
```

`valueToString(function)` prints:

```text
<fun add>
```

Function values are equal only if they refer to the same function index.

## IR Design

Extend `IRProgram` with a function table:

```cpp
struct IRFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<IRInstruction> instructions;
    std::size_t registerCount = 0;
};
```

Main program instructions remain as they are today. Function bodies compile into separate `IRFunction` entries.

Add IR operations:

- `MakeFunction`: creates a runtime function value from a function table index.
- `Call`: calls a callee register with zero or more argument registers and stores result in dest.
- `Return`: returns from the current function frame with an optional value register.

Because calls need multiple arguments, extend `IRInstruction` with:

```cpp
std::vector<IRRegister> arguments;
```

For non-call instructions this vector is empty.

IR printer examples:

```text
v0 = make_function $0 add/2
store_var @0 add#0, v0
v1 = load_var @1 add#0
v2 = constant #0 1
v3 = constant #1 2
v4 = call v1(v2, v3)
print v4

function $0 add(a#1, b#2)
0000  v0 = load_var @0 a#1
0001  v1 = load_var @1 b#2
0002  v2 = add v0, v1
0003  return v2
```

Exact formatting can follow existing IR style as long as golden output is stable.

## IR Compilation

Function declaration lowering:

1. Compile the function body into a new `IRFunction`.
2. Emit `MakeFunction` in the current instruction stream.
3. Store the resulting function value in the resolved function name.

Function body lowering:

- Parameters are treated as pre-bound local names in the function frame.
- Function body declarations compile in order.
- If no explicit `Return` is emitted on a path, append an implicit `return nil` at the end of the function body.

Return lowering:

- `return expr;` compiles `expr`, then emits `Return value`.
- `return;` emits a nil constant, then `Return nilReg`.

Call lowering:

- Compile callee expression.
- Compile each argument expression left-to-right.
- Emit `Call` with callee register and argument registers.

## Interpreter Design

Introduce call frames:

```cpp
struct Frame {
    std::vector<Value> registers;
    std::unordered_map<std::string, Value> locals;
};
```

The interpreter should execute main instructions in a main frame and function instructions in child frames.

Variable lookup rules at runtime:

1. Function frame locals/parameters first.
2. Main/global variables second.

Assignments:

- Assigning a function-local variable updates function locals.
- Assigning a global from inside a function updates global if the resolved name is global.
- Because compile-time resolution uses unique names and closure captures are disallowed, local/global distinction can be based on where the resolved name is bound.

Function call steps:

1. Validate callee value is a function.
2. Validate argument count equals arity.
3. Create a new frame with function register count.
4. Bind each parameter resolved name to the corresponding argument value.
5. Execute the function’s instructions.
6. If a `Return` instruction executes, return that value.
7. If instruction stream ends without return, return `nil`.

Runtime errors:

- Calling a non-function:
  ```text
  Runtime error: can only call functions
  ```
- Wrong arity if not caught statically:
  ```text
  Runtime error: expected 2 arguments but got 1
  ```

## Golden Test Coverage

Successful fixtures:

1. `function_call_add`
   ```cd
   fun add(a, b) {
     return a + b;
   }
   print add(1, 2);
   ```
   output: `3`

2. `function_return_nil`
   ```cd
   fun noop() {
     print "hi";
   }
   print noop();
   ```
   output:
   ```text
   hi
   nil
   ```

3. `function_bare_return`
   ```cd
   fun f() {
     return;
     print "no";
   }
   print f();
   ```
   output: `nil`

4. `function_recursion`
   ```cd
   fun count(n) {
     if n <= 0 {
       return 0;
     }
     return count(n - 1) + 1;
   }
   print count(3);
   ```
   output: `3`

5. `function_scope_global`
   ```cd
   let g = "global";
   fun show() {
     print g;
   }
   show();
   ```
   output: `global`

6. `function_ir`
   - Verify `make_function`, `call`, `return`, and function table printing.

Parse-error fixtures:

- Missing `)` after parameters.
- Bad comma in argument list or parameter list.

Type-error fixtures:

- `return` outside function.
- Duplicate function declaration in same scope.
- Function/variable duplicate in same scope.
- Wrong arity for known function.
- Unsupported closure capture of block-local variable.

Runtime-error fixtures:

- Calling a non-function value.
- Wrong arity through an unknown callee if a static case cannot catch it.

## Documentation Updates

Update `docs/language-grammar.ebnf` with `funDecl`, `returnStmt`, `call`, parameters, arguments, and comma syntax.

Update `README.md`:

- Add function declarations and return statements to supported statements.
- Add call expressions to supported expressions.
- Document `return;`, implicit `nil`, recursion, and no closures yet.

Update `docs/roadmap.md`:

- Mark Phase 6A implemented after implementation is complete.
- Leave Phase 6B closures pending.

Update `AGENTS.md` only if implementation establishes new function/IR workflow conventions that future agents need.

## Implementation Risk and Suggested Slicing

This is the largest language extension so far. Keep implementation as small vertical commits:

1. Lexer/parser/AST/type errors for syntax.
2. Runtime `Value::Function` representation.
3. IR function table and printer.
4. Function declaration lowering and function values.
5. Call frames and simple calls.
6. Return behavior and implicit nil.
7. Recursion and arity diagnostics.
8. Closure-boundary type errors.
9. Documentation.

Each slice should add or enable a focused set of golden tests.

## Non-Goals

- Do not implement closures.
- Do not capture non-global enclosing locals.
- Do not add anonymous functions.
- Do not add parameter or return type annotations.
- Do not add full function type inference.
- Do not add default or variadic arguments.
- Do not add native functions.
- Do not add classes, methods, or `this`.
- Do not change existing truthiness rules.
- Do not change CLI modes.
