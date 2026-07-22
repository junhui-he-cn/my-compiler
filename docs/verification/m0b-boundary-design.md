# M0B Boundary Comparison and Failure Reports

Status: implemented against the M0A reference commit `6feb009`.

## Deliverable

M0B extends the verification inventory to revision
`m0b-2026-07-22-r1`. Every inventory case now records a deterministic
`boundary_sequence` and `terminal_boundary`. The supported pipeline boundaries
are:

```text
tokens -> ast -> semantic/diagnostic -> ir -> bytecode -> cdbc
       -> rust_decode -> vm_output
```

Named verification suites use the `verification` boundary. Existing AST, IR,
bytecode, module-interface, `.cdbc`, Rust decode, and VM assertions remain the
reference outputs; M0B adds five explicit lexical token cases in
`tests/boundary_cases.json` and their checked-in outputs under
`tests/boundary_references/tokens/`.

The canonical report from `tests/run_verification.py` now includes:

- each case's expected boundary sequence and terminal boundary;
- canonicalized failure text;
- `boundary_summary.supported_boundaries`;
- the first failing boundary ordered by pipeline dependency; and
- failure counts grouped by boundary.

`tests/boundary_comparison.py` owns the boundary order, diff generation, and
machine-readable path canonicalization. The reviewed allowlist in
`tests/boundary_allowlist.json` contains only checkout-root substitutions for
`tests/` and `vm-rs/` paths. No compiler or VM semantic behavior changes in
this slice.

## Migration

The existing specialized runners remain assertion owners. The canonical runner
continues to invoke their existing `CheckResult` logic and additionally invokes
`tests/run_boundary_tests.py` for the token corpus. Boundary metadata is added
to the report rather than replacing existing expected outputs. Runtime and VM
checks are not executed a second time merely to produce a boundary comparison.

The mismatch selftest injects an IR difference and verifies both the unified
diff and first-boundary attribution. The CTest target
`boundary_comparison_selftest` protects the allowlist and boundary ordering.

## Quantitative gate

The M0B baseline is bound to inventory revision `m0b-2026-07-22-r1` and
reference commit `6feb009`:

1. inventory validation covers 1,569 stable cases and requires a non-empty
   boundary sequence whose terminal boundary is included in that sequence;
2. the supported boundary set contains 13 named boundaries;
3. the five token reference cases pass with zero mismatches;
4. the canonical runner reports `1,569 passed, 0 failed` and zero untracked
   results, with no first failure;
5. CTest reports 17/17, including inventory, runner, and boundary selftests;
6. injected mismatch tests attribute the earliest failure to the IR boundary;
   and
7. the legacy runner set has zero unexplained pass/fail, exit-code, and
   failure-text differences.

The observed values and legacy projection are recorded in
`docs/verification/m0b-baseline.json`.

## Old-path deletion condition

M0B deletes no specialized runner or reference output. Boundary-aware reports
must cover every existing final-only category in two consecutive CI runs before
duplicate top-level reporting is removed. The checked-in reference outputs stay
available through M1F so semantic-path migration can compare against this
pre-M1 oracle.

## Explicit non-goals

M0B does not introduce HIR, alter type checking, change diagnostics, add new
bytecode instructions, negotiate artifact versions, or execute programs twice
for comparison. Malformed-input/property coverage belongs to M0C; semantic
decision policy belongs to M0.5A.
