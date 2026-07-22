# Array `flatMap` Design

Date: 2026-07-22

## Goal

Add a one-level flattening companion to the existing callback-based array
helpers without introducing a new collection value or bytecode opcode.

## Public API

The shadowable function form and unshadowed member form are:

```cd
print flatMap([1, 2], fun (value: number): [number] {
  return [value, value * 10];
});
print [3].flatMap(fun (value: number): [number] { return [value + 1]; });
```

`flatMap(array, callback)` and `array.flatMap(callback)` invoke a one-argument
callback from left to right over a snapshot of the source array. Each callback
result must be an array; its elements are appended to a fresh output array and
only that outer array is flattened. The output preserves callback result order,
and mutations of the source during callback execution do not change the
iteration boundary. Callback errors propagate through the existing call stack.

The function-style name is shadowable. Member-call sugar always resolves to
the builtin.

## Static and runtime checks

Known array and callback types are checked statically. The callback must be
non-generic, have arity one, accept the known source element type, and return
an array when its return type is known. A known callback return type of `[T]`
produces a known `[T]` result; unknown element types remain dynamic.

Unknown values defer validation to runtime. Runtime errors use these stable
messages:

```text
flatMap expects 2 arguments
flatMap expects array as first argument
flatMap expects function as second argument
flatMap expects callback with 1 argument
flatMap expects callback to return array
```

## Compiler and runtime architecture

`flatMap` reuses the native registry, generic `native_call` IR and bytecode
lowering, and the existing member-call receiver convention. The Rust VM takes
an element snapshot, invokes the callback, validates each returned value as an
array, and appends its shallow elements to a new array. No new IR, bytecode, or
runtime value representation is needed.

## Coverage

The slice covers function and member forms, shadowing, one-level flattening,
empty arrays, source snapshots, static callback checks, dynamic runtime errors,
bytecode artifact output, and Rust VM execution parity.
