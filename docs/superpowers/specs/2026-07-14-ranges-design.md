# Ranges Design

Date: 2026-07-14

## Goal

Add finite integer ranges as immutable, indexable collection values that can be
constructed by a native `range` helper and consumed by existing `len`, index,
`contains`, and `for-in` operations.

## Public semantics

`range(stop)` is equivalent to `range(0, stop, 1)`. `range(start, stop)` is
equivalent to `range(start, stop, 1)`. The three-argument form accepts an
explicit step. Ranges are half-open: ascending ranges contain values while
`value < stop`, and descending ranges contain values while `value > stop`.
Therefore `range(1, 5)` contains `1, 2, 3, 4`, while `range(5, 0, -2)` contains
`5, 3, 1`. A direction mismatch produces an empty range. Bounds and steps must
be finite integer numbers representable by the VM's signed integer range, and
the step cannot be zero.

Ranges are immutable values. `len(range)` returns the number of elements,
`range[index]` returns a numeric element using zero-based integer indexing,
and `contains(range, value)` / `range.contains(value)` test membership. Range
values can be used in `for-in`; the loop item has static type `number`. Range
equality compares start, stop, and step values. They print as
`range(start, stop, step)` and `typeOf` returns `"range"`.

## Pipeline

The C++ type checker adds `StaticType::Range` and recognizes `range` as a
shadowable native function with one to three numeric arguments. Existing IR and
bytecode `NativeCall`, `Len`, `Index`, and `AssertArray` instructions are
reused; `AssertArray` accepts both arrays and ranges to preserve `.cdbc`
compatibility. The Rust VM adds a compact `RangeValue` instead of materializing
an array, and performs length, index, and membership calculations arithmetically.

## Diagnostics

Stable runtime diagnostics for this slice are:

```text
range expects 1 to 3 arguments
range expects number as the first/second/third argument
range expects integer as the first/second/third argument
range bound out of range
range step must not be zero
range index must be number
range index must be integer
range index out of bounds
cannot assign range elements
len expects array, string, map, or range
contains expects array, map, or range as first argument
for-in expects array or range
```

Known static non-range values are rejected before lowering. Unknown values
remain runtime-checked by the existing assertion/index/native-call paths.
