# M0.5A Semantic Decision Baseline

Status: implemented against reference commit `7e46d36`.

## Purpose and scope

This decision log freezes the current semantic contract before shared HIR work.
The machine-readable source is
`docs/decisions/m05a-semantic-decisions.json`; this page explains its ownership
and migration rules. The bound verification inventory is
`m0d-2026-07-22-r1`.

The log contains 16 entries across the eight required domains:

| Domain | Resolved | Deferred/open | M1-blocking |
| --- | ---: | ---: | ---: |
| evaluation and effects | 2 | 0 | 2 |
| binding and scope | 2 | 0 | 2 |
| types | 3 | 0 | 3 |
| source and ranges | 1 | 1 | 1 |
| diagnostics | 1 | 0 | 1 |
| modules | 1 | 1 | 1 |
| patterns | 2 | 0 | 2 |
| backend boundary | 2 | 0 | 2 |
| **Total** | **14** | **2** | **14** |

All M1-blocking entries are `resolved`. The two deferred entries are lossless
trivia ownership (`M1A2`) and interface-driven separate compilation
(`M3A/M3B`); both include an explicit closure condition and are not implicit
HIR decisions.

## Decisions that M1 must preserve

- Evaluation is source-ordered, logical operators short-circuit, and current
  assignment lowering order is observable. Runtime errors propagate without
  implicit recovery.
- Lexical binding resolution selects the nearest declaration before IR
  generation. Closures capture shared cells by reference, while nested
  functions own independent loop/control-flow contexts.
- Named structs/enums are nominal and generic arguments are invariant metadata;
  unknown types defer applicable checks to runtime; nullable narrowing remains
  limited to the documented simple-variable branches.
- Source files own original path/text. User-facing coordinates remain one-based;
  M1A1 adds half-open byte ranges without serializing snapshot-local identities.
- Diagnostic phase ownership and parser recovery remain stable, including file
  context, source snippets, caret positions, and exit behavior.
- Canonical module paths, import visibility, cycle rejection, namespace aliases,
  and re-export forwarding remain unchanged.
- Pattern bindings are arm-local, guards do not provide coverage, and OR
  alternatives must bind the same compatible names.
- Native helpers use the existing `native_call` boundary, `len` retains its
  legacy dedicated operation, and optional cdbc debug metadata keeps its
  one-based source coordinates and local artifact indexes.

## Duplicated decision-site baseline

The source-site inventory is intentionally a reproducible measurement, not a
claim that every matching line is a separate semantic rule. It records the
AST-specific locations that M1 will migrate or delete:

| Category | Matches | SHA-256 |
| --- | ---: | --- |
| type-checking | 104 | `fd22bf3e8da256997a11c187f940ca1ea6b2b699119b8013214d0870225318d9` |
| name-resolution | 86 | `5d607342922daccf9816badbbd521bdbe56c0750ec9980414e23d6d01470e94b` |
| lowering-dispatch | 100 | `a137f9e5079ed653de29d19165ab4e6e04e569d8e29e7dd7a7a5d48971f3223d` |
| **Total** | **290** | per-category digests |

Reproduce it with:

```sh
python3 tests/semantic_decisions.py
python3 tests/semantic_decision_inventory.py \
  --report build/m05a-duplicate-decision-inventory.json
python3 tests/semantic_decisions_selftest.py
```

The scanner reads the category/file/pattern manifest from the JSON decision
log, sorts file/line/pattern matches, normalizes recorded whitespace, and
hashes the canonical match tuples. Future M1 slices should record their
consumed decision IDs and update the inventory only when a production site is
actually migrated or removed.

## Migration and deletion rules

M0.5A changes no compiler, artifact, or VM behavior. M1 structural work must
compare semantic outputs, IR, artifacts, diagnostics, and runtime parity against
the M0B/M0D corpus while citing the decision IDs it preserves. An intentional
semantic change belongs to M2 or a reviewed decision revision.

The duplicate sites are not deleted by documentation alone. Each decision
family in the JSON log has an owner and an old-path deletion condition. The
current baseline is complete when the scanner remains reproducible, each M1
slice names its consumed entries, and no migrated production path falls back to
the old AST-specific type/name/lowering interpretation.

## Verification gate

The checked-in baseline is `docs/verification/m05a-baseline.json`. The gate is:

- the decision log validates with 16 entries and all eight domains;
- all 14 M1-blocking entries are resolved;
- both deferred entries name their closing milestone and condition;
- the duplicate-site inventory reproduces 290 matches and the three recorded
  category digests; and
- the six selftests pass.
