# Array For-In Loops Design

## Goal

Add a focused array iteration form:

```cd
for item in values {
  print item;
}
```

This phase is Phase 11C in the language roadmap. It should make array traversal ergonomic while reusing the existing loop-control, array indexing, type checking, IR, bytecode, and Rust VM foundations.

## User-Facing Semantics

### Syntax

The new loop form is:

```cd
for name in expression {
  declaration*
}
```

Examples:

```cd
let xs = [1, 2, 3];
for x in xs {
  print x;
}
```

Only arrays are iterable in this phase. Strings, structs, ranges, maps, and custom iterators are out of scope.

### Loop Variable Scope

The loop variable is a fresh binding scoped to the loop body only:

```cd
for x in [1] {
  print x; // ok
}
print x; // type error: undefined variable
```

The binding is recreated/updated for each iteration and may shadow an outer variable:

```cd
let x = 99;
for x in [1, 2] {
  print x; // 1, then 2
}
print x; // 99
```

A body-local declaration with the same name as the loop variable is a same-scope duplicate declaration error, consistent with block scope rules.

### Iteration Order and Mutation During Iteration

Iteration visits array elements in increasing index order starting at `0`.

The loop takes a length snapshot before the first iteration. Appending to the iterated array during the loop does not extend the number of iterations:

```cd
let xs = [1, 2];
for x in xs {
  print x;
  push(xs, 99);
}
// prints only 1 and 2
```

Element reads happen at the current index during each iteration. Mutating not-yet-visited existing elements may be observed when their turn is reached. Removing elements during iteration is not statically prohibited; if a later snapshot index no longer exists, the existing runtime array bounds check reports the indexing error.

This rule keeps loop termination deterministic while preserving arrays as mutable reference values.

### Break and Continue

`break;` exits the nearest `while`, C-style `for`, or `for-in` loop.

`continue;` in a `for-in` loop skips the rest of the body and proceeds to the next element. The internal index increment happens before the next condition check. This mirrors the existing C-style `for` behavior where `continue` runs the increment clause.

Nested functions inside a `for-in` body cannot break or continue the enclosing loop, matching current loop-control rules.

### Static and Runtime Checks

If the iterable expression is statically known and not an array, type checking fails:

```cd
for x in 123 {
  print x;
}
```

Suggested diagnostic:

```text
Type error at 1:10: for-in expects array, got number
```

If the iterable type is unknown, runtime checks enforce array-ness. The runtime error should be stable across the C++ IR interpreter and Rust VM:

```text
Runtime error: for-in expects array
```

If the iterable is a typed array, the loop variable has the known element type inside the body:

```cd
let xs: [number] = [1, 2];
for x in xs {
  let y: number = x; // ok
}
```

If the iterable is an untyped/mixed array or unknown array element type, the loop variable type is unknown.

## Grammar

`for` statements become a disambiguated pair:

```ebnf
forStmt      = "for", ( forInClause | cForClause ), block ;
forInClause  = identifier, "in", expression ;
cForClause   = [ forInitializer ], ";",
               [ expression ], ";",
               [ expression ] ;
```

The parser can disambiguate by checking for `identifier "in"` immediately after `for`. Otherwise it parses the existing C-style form.

`in` becomes a reserved keyword token in this phase. It is used only by `for-in` syntax.

## AST and Printing

Add a statement node:

```cpp
struct ForInStmt final : Stmt {
    ForInStmt(Token keyword, Token variable, ExprPtr iterable, StmtPtr body);

    Token keyword;
    Token variable;
    ExprPtr iterable;
    StmtPtr body;
};
```

AST printing should be compact and consistent with existing loop output, for example:

```text
ForIn x in xs
  Block
    Print x
```

Exact formatting can follow existing `ForStmt` and `WhileStmt` indentation conventions.

## Type Checker Design

Type checking should:

1. Check the iterable expression once before entering the loop body scope.
2. If the iterable type is known and not `array`, report `for-in expects array, got <type>` at the `in` token or a stable nearby token.
3. Begin a new scope for the loop body.
4. Declare the loop variable in that body scope.
   - If the iterable is `[T]`, bind the loop variable as `T`.
   - Otherwise bind it as `unknown`.
5. Increment loop depth while checking the body so `break` and `continue` work.
6. Restore loop depth when checking nested functions, matching existing loop-control behavior.

The loop variable should not escape after the loop because its scope is the body scope.

## IR Lowering Design

Lower `for item in iterable { body }` directly to IR equivalent to:

```cd
{
  let __iter = iterable;
  let __index = 0;
  let __len = len(__iter);
  while __index < __len {
    let item = __iter[__index];
    body
    __index = __index + 1;
  }
}
```

Implementation details:

- Evaluate the iterable expression exactly once.
- Store it in a compiler-generated temporary local/cell so array reference semantics are preserved.
- Emit a runtime array check before reading length/indexing, or rely on an explicit new IR helper if existing `len` would produce `len expects array or string`. Prefer a dedicated IR-level array check or runtime helper only if needed to keep the specified error message `for-in expects array`.
- Snapshot length before the loop condition.
- At each iteration, read `iter[index]` and bind/store the loop variable before compiling the body.
- `break` patches to the loop end.
- `continue` patches to the internal increment block, then condition check.

No bytecode opcode is required if lowering uses existing IR operations plus any necessary existing runtime checks. If a new IR check operation is introduced to get a precise error message, it must be lowered to bytecode and implemented in Rust VM as part of the same slice.

## Bytecode and Rust VM

The bytecode path should remain semantically aligned with `--run`.

Preferred implementation:

- Reuse existing bytecode operations for constants, variable storage, array indexing, len, comparison, jumps, and assignment.
- Avoid a dedicated bytecode `for_in` opcode.
- Add Rust VM support only if the C++ bytecode compiler emits a new IR/bytecode check operation. Otherwise existing Rust VM instructions should already execute the lowered loop.

Rust VM parity tests should include the success fixtures that exercise `for-in`, `break`, `continue`, empty arrays, typed arrays, and mutation-during-iteration length snapshot behavior.

## Testing Strategy

### Success Goldens

Add focused fixtures such as:

- `for_in_basic`: prints elements of `[1, 2, 3]`.
- `for_in_sum`: accumulates a sum.
- `for_in_break`: exits early.
- `for_in_continue`: skips selected elements.
- `for_in_empty`: body does not run.
- `for_in_shadow_outer`: loop variable shadows an outer variable without mutating it.
- `for_in_typed_array`: typed array element flows into body type checks.
- `for_in_length_snapshot`: `push` inside the body does not extend iteration count.

Include `ast.out`, `ir.out`, `bytecode.out`, and `run.out` where useful. Add representative cases to Rust VM golden parity.

### Type Errors

Add fixtures for:

- Static non-array iterable: `for x in 123 { print x; }`.
- Loop variable escapes body: `for x in [1] { } print x;`.
- Duplicate loop variable declaration in body: `for x in [1] { let x = 2; }`.
- Typed array element mismatch inside body, for example assigning `x` from `[number]` to `string`.
- `break` / `continue` inside nested functions in a `for-in` body remains a type error.

### Runtime Errors

Add a dynamic non-array iterable case:

```cd
fun id(x) { return x; }
for x in id(123) {
  print x;
}
```

Expected:

```text
Runtime error: for-in expects array
```

If array shrinkage during iteration is left to existing bounds checks, add a runtime-error fixture documenting the current behavior only if it is stable and useful.

### Parse Errors

Add fixtures for malformed syntax:

- `for in xs { }` — missing loop variable.
- `for x xs { }` — missing `in` or C-style semicolons.
- `for x in { }` — missing iterable expression.
- `for x in xs print x;` — missing block body.

## Documentation

Update:

- `docs/language-grammar.ebnf` with the new `for-in` grammar and `in` keyword usage.
- `README.md` loop section with syntax, scope, break/continue behavior, and length snapshot rule.
- `AGENTS.md` current semantics with the new loop feature and its limitations.
- `docs/roadmap.md` Phase 11 status to mark Phase 11C implemented after completion.

## Open Questions Resolved for This Slice

- **Can `for-in` iterate strings?** No.
- **Can it iterate ranges or maps?** No.
- **Does appended data extend the loop?** No, length is snapshotted before iteration.
- **Does mutation of existing future elements affect values read later?** Yes, because each element is read at its turn.
- **Is `in` reserved?** Yes, it becomes a keyword for this syntax.
- **Does the loop variable escape?** No, it is scoped to the loop body.
