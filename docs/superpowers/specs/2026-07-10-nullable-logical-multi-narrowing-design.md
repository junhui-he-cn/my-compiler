# Nullable Logical Multi-Narrowing Design

## Goal

Extend nullable logical narrowing so one `if` condition can prove multiple simple variables non-nil in the same branch.

## User-visible behavior

Supported examples:

```cd
if (a != nil && b != nil) {
  takesNumber(a);
  takesNumber(b);
}

if (a == nil || b == nil) {
  print 0;
} else {
  takesNumber(a);
  takesNumber(b);
}
```

The feature remains conservative:

- `&&` combines only then-branch non-nil proofs.
- `||` combines only else-branch non-nil proofs.
- Fields, array elements, loops, and post-branch flow remain out of scope.
- Runtime behavior is unchanged.

## Implementation approach

Replace `IfNarrowing`'s single optional `then` / `else` narrowing with vectors of narrowings. Direct nil comparisons produce a one-element vector. Logical `&&` concatenates then vectors from both operands. Logical `||` concatenates else vectors from both operands. Branch checking pushes all active narrowings and pops them after the branch body.

This keeps the existing simple-variable resolved-name model and avoids a full dataflow engine.

## Testing

Add a success fixture with both `&&` then-branch multi-variable narrowing and `||` else-branch multi-variable narrowing. Run it before implementation to confirm it fails because the second variable remains `number?`, then implement the minimal type-checker change.

## Documentation

Update README and roadmap wording from singular logical narrowing to multiple simple-variable narrowings in supported logical guards.
