# M0D Verification Matrix and Measurement Baseline

Status: implemented against reference commit `79fbfa0`.

## Deliverable

M0D adds the machine-readable matrix in `tests/verification_matrix.json`, its
validator in `tests/verification_matrix.py`, the executor and measurement
reporter in `tests/run_verification_matrix.py`, and the selftest in
`tests/verification_matrix_selftest.py`. The matrix revision is
`m0d-2026-07-22-r1`, bound to verification inventory revision
`m0d-2026-07-22-r1`.

The matrix has ten cells:

| Cell family | Cells | Execution ownership |
| --- | --- | --- |
| build | default, warnings, sanitizers | clean matrix runner or named CI wrapper |
| canonical runner | canonical verification | matrix runner, with an optional existing-report projection |
| C++ tests | default CTest, sanitizer CTest | canonical runner and sanitizer wrapper |
| Python/backend suites | golden, artifact, Rust VM, Cargo | canonical runner projections |

The canonical cell owns the existing assertion runners. The projection cells
retain their direct command and runner name so coverage is reviewable, but are
marked `covered` and are not executed a second time by the matrix. Warning and
sanitizer cells point to the explicit CI wrapper names in `.github/workflows`.
This makes every cell reachable either from `run_verification.py` or from a
named environment-specific build/test wrapper.

The five fixed runtime workloads are `arithmetic`, `collection_helpers`,
`maps`, `native_stdlib_math`, and `unicode_strings`. Each is compiled once,
executed three times by the Rust VM, and accepted only when stdout, stderr, and
exit status match the checked-in expectation.

## Measurement policy

The report separates deterministic fields from wall-clock observations:

| Measurement | Definition | Samples/statistic | Comparison |
| --- | --- | --- | --- |
| compile time | clean CMake configure plus build for default, warnings, and sanitizers | one observed sample per build | informational, 50% tolerance |
| test duration | canonical verification plus sanitizer CTest; projections count once | one observed aggregate | informational, 50% tolerance |
| artifact size | checked-in `expected.cdbc` corpus count, bytes, and path/content SHA-256 | exact | zero-byte tolerance |
| runtime workload | Rust VM execution after one compiler emission | three samples, median per workload | informational, 50% tolerance |

Clean measurements use temporary build directories so stale incremental output
cannot be mistaken for a clean compile. Reuse mode is the CI projection: it
reuses the existing compiler and canonical JSON report, records the environment
wrapper cells as delegated, and still validates the fixed workload set. A
future baseline comparison may use `--baseline`; performance comparison is
only meaningful when toolchain, build mode, host family, and machine match.
`--strict-performance` turns an out-of-tolerance comparable observation into a
failure. Artifact corpus changes are always deterministic differences.

The environment record includes commit, CMake/C++/Python/Cargo/Rust versions,
host characteristics, build mode, and CMake cache mode values. The baseline
report is therefore an observation tied to a concrete machine, not a promise
that hosted CI wall-clock times are identical.

## Migration and verification

Existing CI commands remain assertion owners. The normal job runs the existing
canonical command, then records a matrix projection from its report; the
sanitizer job remains the named wrapper for the sanitizer build and CTest cell.
The direct legacy commands remain available while the canonical matrix proves
equivalent coverage and failure diagnostics.

The M0D baseline is `docs/verification/m0d-baseline.json`. Its observed gate is:

- 10/10 matrix cells pass;
- 5/5 runtime workloads pass with three observations each;
- the canonical inventory reports 1,659/1,659 passed with zero untracked results;
- the artifact corpus contains 58 files and 129,946 bytes with the recorded
  SHA-256; and
- the clean report records all required toolchain, host, repetition, statistic,
  and tolerance metadata.

The relevant commands are:

```sh
python3 tests/verification_matrix.py
python3 tests/run_verification_matrix.py --mode clean --report docs/verification/m0d-baseline.json
ctest --test-dir build --output-on-failure
python3 tests/run_verification.py ./build/compiler_design vm-rs --report build/verification-report.json
```

## Old-path deletion condition

No compiler, VM, or specialized assertion path is deleted in M0D. Duplicate CI
steps may be removed only after the canonical matrix has equivalent coverage,
case classification, and failure diagnostics in two consecutive CI runs. The
versioned matrix schema and baseline remain after any future CI consolidation;
only the redundant wrapper invocation is eligible for deletion.

## Explicit non-goals

M0D does not change language semantics, diagnostics, bytecode format, Rust VM
behavior, or performance claims. It does not replace the specialized runners,
turn rolling hosted measurements into release gates, or introduce a benchmark
framework beyond the five fixed workloads.
