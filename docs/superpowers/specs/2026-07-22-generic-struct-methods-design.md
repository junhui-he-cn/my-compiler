# Generic Struct Method Design

Date: 2026-07-22

## Goal

Allow methods to be defined once for every instantiation of a nominal generic
struct, while reusing the existing method-call lowering and erased runtime
representation.

```cd
struct Box<T> { value: T }

impl Box<T> {
  fun get(): T { return this.value; }
  fun wrap<U>(value: U): Box<U> {
    return Box<U> { value: value };
  }
}

let box: Box<number> = Box { value: 1 };
print box.get();
print box.wrap<string>("text").value;
```

## Static semantics

- A generic `impl` header must bind every struct type parameter in declaration
  order, using the same names: `impl Box<T>`. A non-generic struct keeps the
  existing `impl Box` form and cannot provide an `impl` type-parameter list.
- Receiver bounds come from the struct declaration. Header bounds, when
  written, must agree with the declared bounds; method type parameters must not
  reuse receiver parameter names.
- The receiver type parameters are implicit method-call parameters. A method
  declared with receiver type `Box<T>` specializes its parameter and return
  types from the actual receiver `Box<number>` before ordinary method generic
  argument inference runs.
- Methods may still declare their own generic parameters. Explicit method type
  arguments and inference use the existing function-call rules after receiver
  specialization.
- Field access and assignment through `this` use the substituted receiver
  field types, so `this.value` has type `T` in `Box<T>` methods.
- Exported methods preserve the generic receiver type in module symbols and
  module-interface output. Direct imports, namespace imports, and re-exports
  specialize the same way as local methods.

## Runtime and non-goals

- No new IR, bytecode, opcode, VM behavior, or runtime generic metadata is
  needed. All instantiations call the existing erased method function.
- This slice does not add map APIs, recursive structs, inheritance, overloads,
  dynamic dispatch, or generic protocols.

## Diagnostics and verification

Cover local receiver specialization, generic method inference and explicit
arguments, field mutation through `this`, bounds, receiver-header arity/name
mismatches, method type-parameter name collisions, direct and namespace
imports, re-exports, module-interface output, and rejection of type arguments
on non-generic `impl` blocks. Existing non-generic method outputs must remain
unchanged.
