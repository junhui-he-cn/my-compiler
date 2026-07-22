# M0C Deterministic Malformed-Input and Property Harness

Status: implemented against the M0B reference commit `30ae329`.

## Deliverable

M0C adds the fixed corpus in `tests/malformed_cases.json`, deterministic case
expansion in `tests/malformed_corpus.py`, and the bounded runner
`tests/run_malformed_tests.py`. The corpus revision is
`m0c-2026-07-22-r1` and contains 88 stable cases:

| Corpus family | Cases |
| --- | ---: |
| Explicit lexer/parser/recovery stdin seeds | 6 |
| Existing parse-error fixture family | 66 |
| Mutated `.cdbc` parser inputs | 16 |
| **Total** | **88** |

The `.cdbc` cases mutate four checked-in valid artifacts with fixed operations:
bad header, truncation, unknown opcode, and trailing garbage. They invoke the
Rust VM's `dump` parser only; malformed inputs are never executed by the VM.

Every case runs twice with the same seed and is classified as `pass`,
`timeout`, `crash`, `non_deterministic`, `unexpected_accept`,
`unexpected_stdout`, or `input_too_large`. The manifest bounds each input to
8,192 bytes, each process to two seconds, and the complete corpus to 120
seconds. The JSON report records both observations, exit codes, canonicalized
stdout/stderr, classification, seed, and limits.

The deterministic `minimize_text` reducer removes chunks in a stable order.
`tests/malformed_tests_selftest.py` injects a predicate failure, reduces
`prefix/needle/suffix` to `needle`, and saves the minimized input in a
temporary fixture, proving the minimize-and-save path without relying on an
external fuzzing engine.

## Migration

The existing compiler, parser, diagnostic, artifact, and Rust VM production
paths are unchanged. The malformed runner is an additional canonical suite;
the canonical verification command writes `build/malformed-report.json` beside
the overall verification report. CI also runs the direct malformed command and
uploads both reports. Existing CTest and specialized runners remain in place.

The runner uses the M0B path canonicalization allowlist for deterministic
diagnostic comparison and does not execute a malformed artifact. A failure can
optionally be minimized and saved with `--failure-dir`; no failure fixtures are
generated automatically during a normal verification run.

## Quantitative gate

The M0C baseline is bound to inventory revision `m0c-2026-07-22-r1` and
reference commit `30ae329`:

1. corpus expansion produces exactly 88 unique case IDs in sorted order;
2. all 88 cases remain within the declared input and process budgets;
3. repeated observations are identical for all cases;
4. malformed compiler inputs are rejected without stdout, and all malformed
   `.cdbc` inputs are rejected by Rust `dump` before execution;
5. the malformed report records `88 passed`, `0 failed`, `0 timeouts`,
   `0 crashes`, and `0 non_deterministic` cases;
6. CTest reports 18/18, including the corpus and minimizer selftest; and
7. the canonical inventory reports `1,658 passed`, `0 failed`, and zero
   untracked results.

The observed values and legacy command projection are recorded in
`docs/verification/m0c-baseline.json`.

## Old-path deletion condition

M0C deletes no compiler or VM path. The harness remains an additive bounded
verification component. Temporary mutation or minimization adapters may be
removed only after the same seed corpus, budget, and failure reproduction are
provided by the canonical harness.

## Explicit non-goals

M0C does not change accepted syntax, diagnostics, bytecode format, Rust parser
semantics, or runtime behavior. It does not claim coverage for randomized
inputs, unbounded fuzzing, performance benchmarking, or sanitizer builds beyond
the bounded process classification. Those concerns remain governed by M0D and
later milestones.
