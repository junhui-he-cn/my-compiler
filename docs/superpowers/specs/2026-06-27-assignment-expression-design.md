# Assignment Expression Design

## Summary

Add assignment expressions to the language so variables can be updated without reusing `let` declarations.

Supported form:

```text
name = expression
```

Assignment is an expression, not only a statement. It stores the right-hand value in an existing variable and evaluates to that same value.

Examples:

```text
let x = 1;
x = x + 2;
print x;
```

```text
let x = 0;
print x = 5;
print x;
```

```text
let a = 0;
let b = 0;
a = b = 7;
print a;
print b;
```

This change is intended to support later `while` loops naturally:

```text
let i = 0;
while i < 3 {
  print i;
  i = i + 1;
}
```

## Grammar

Current expression grammar starts at `equality`. Add an `assignment` precedence level below equality:

```ebnf
expression  = assignment ;

assignment  = identifier, "=", assignment
            | equality ;

equality    = comparison,
              { ( "==" | "!=" ), comparison } ;
```

Assignment is right-associative:

```text
a = b = 7
```

parses as:

```text
a = (b = 7)
```

The left side of `=` must be a variable identifier. These are invalid assignment targets:

```text
(a + b) = 1;
1 = x;
foo() = 2;  // calls are not currently in the language, but the same rule applies later
```

## Semantics

### Declaration vs Assignment

`let` creates or overwrites a variable in the current global variable table:

```text
let x = 1;
let x = 2;
```

Assignment updates an existing variable:

```text
let x = 1;
x = 2;
```

Assignment to an undefined variable is a runtime error:

```text
x = 1;
```

Expected runtime error:

```text
IR runtime error: undefined variable `x`
```

This keeps declaration and mutation distinct, which will make later lexical scopes and static type checking easier to define.

### Assignment Value

An assignment expression evaluates to the value assigned:

```text
let x = 0;
print x = 5;
```

prints:

```text
5
```

For chained assignment:

```text
let a = 0;
let b = 0;
a = b = 7;
```

execution order is:

1. Evaluate `7`.
2. Assign `7` to `b`; expression value is `7`.
3. Assign `7` to `a`; expression value is `7`.

## Lexer

No lexer changes are required. `=` is already tokenized as `TokenType::Equal`, and `==` remains `TokenType::EqualEqual`.

## AST

Add an assignment expression node:

```cpp
struct AssignExpr final : Expr {
    AssignExpr(Token name, ExprPtr value);
    void print(std::ostream& out) const override;

    Token name;
    ExprPtr value;
};
```

AST print format:

```text
(= name value)
```

Examples:

```text
x = x + 2;
```

prints in an expression statement as:

```text
Expr (= x (+ x 2))
```

```text
a = b = 7;
```

prints as:

```text
Expr (= a (= b 7))
```

## Parser

Change the expression entry point from:

```cpp
ExprPtr Parser::expression()
{
    return equality();
}
```

to:

```cpp
ExprPtr Parser::expression()
{
    return assignment();
}
```

Add:

```cpp
ExprPtr Parser::assignment();
```

Parsing algorithm:

1. Parse the left side with `equality()`.
2. If the next token is not `=`, return the left side.
3. If `=` is present, recursively parse the right side with `assignment()` to make assignment right-associative.
4. Check that the original left side is `VariableExpr`.
5. If it is a variable, return `AssignExpr(variableName, value)`.
6. Otherwise throw `ParseError` at the `=` token with message `invalid assignment target`.

The parser should not accept assignment as a new declaration form. These remain separate:

```text
let x = 1;  // declaration
x = 2;      // assignment expression statement
```

## IR

Add a distinct assignment operation instead of reusing `StoreVar`:

```cpp
enum class IROp {
    Constant,
    LoadVar,
    StoreVar,
    AssignVar,
    ...
};
```

Instruction conventions:

```text
StoreVar:  left = value register, operand = variable name index; creates or overwrites variable
AssignVar: left = value register, operand = variable name index; updates existing variable only
```

Add an emitter:

```cpp
void emitAssignVar(std::string name, IRRegister value);
```

IR printing should show the distinction:

```text
store_var  x, v0
assign_var x, v1
```

This distinction documents intent and lets the interpreter reject assignment to undefined variables.

## IR Compiler

Compile `AssignExpr` by:

1. Compiling the right-hand value expression.
2. Emitting `AssignVar` for the target name and value register.
3. Returning the same value register as the expression result.

Pseudo-code:

```cpp
if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
    const IRRegister value = compileExpression(*assign->value);
    ir_.emitAssignVar(assign->name.lexeme, value);
    return value;
}
```

Returning the right-hand register makes assignment usable inside larger expressions where the current grammar allows it, especially `print x = 5;`.

## IR Interpreter

`StoreVar` keeps current behavior:

```cpp
globals_.insert_or_assign(name, value);
```

`AssignVar` must check that the variable already exists:

```cpp
auto found = globals_.find(name);
if (found == globals_.end()) {
    throw IRRuntimeError("undefined variable `" + name + "`");
}
found->second = value;
```

The runtime error message intentionally matches undefined variable load errors.

## Tests

Add golden success cases, one runtime-error case, and one parse-error case.

### `tests/golden/assignment/input.cd`

```text
let x = 1;
x = x + 2;
print x;
print x = 5;
print x;
```

Expected runtime output:

```text
3
5
5
```

Expected AST includes:

```text
Expr (= x (+ x 2))
Print (= x 5)
```

Expected IR includes `assign_var` instructions.

### `tests/golden/chained_assignment/input.cd`

```text
let a = 0;
let b = 0;
a = b = 7;
print a;
print b;
```

Expected runtime output:

```text
7
7
```

Expected AST includes:

```text
Expr (= a (= b 7))
```

### `tests/golden/runtime_errors/assign_undefined.cd`

```text
missing = 1;
```

Expected stderr:

```text
IR runtime error: undefined variable `missing`
```

Expected exit code:

```text
1
```

### Invalid Assignment Target

Add parse-error fixture support to the golden runner so parser failures can be checked automatically. Use a new directory:

```text
tests/golden/parse_errors/
```

For a parse-error source file `invalid_assignment_target.cd`, use matching expected files:

```text
invalid_assignment_target.err
invalid_assignment_target.exit
```

The runner should execute the compiler in default AST mode, expect a non-zero exit code, compare stderr to `.err`, compare the exit code to `.exit`, and fail if stdout is non-empty.

`tests/golden/parse_errors/invalid_assignment_target.cd`:

```text
(x + 1) = 2;
```

Expected stderr:

```text
Parse error at line 1, column 9: invalid assignment target
```

Expected exit code:

```text
1
```

## Documentation

Update `docs/language-grammar.ebnf`:

```ebnf
expression  = assignment ;

assignment  = identifier, "=", assignment
            | equality ;
```

Update `README.md` supported expressions to mention assignment:

```text
Assignment: name = expression
```

Also document that assignment requires the variable to already exist; use `let` to declare variables. Update the README test section to mention `tests/golden/parse_errors` if parse-error fixture support is added there.

## Non-Goals

This change does not add:

- `+=`, `-=`, `*=`, or `/=` compound assignment.
- `++` or `--`.
- Destructuring assignment.
- Assignment to properties, indexes, or other l-values.
- Static definite-assignment analysis.
- Lexical scopes or shadowing changes.
- `while` loops. This change prepares for `while`, but does not implement it.
