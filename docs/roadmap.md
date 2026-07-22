# Compiler Design Roadmap

This is the active planning document for the compiler and language. It is
organized around capabilities, migration boundaries, and measurable outcomes
rather than individual syntax forms or standard-library functions. The roadmap
was re-baselined on 2026-07-22 and refined into an execution plan on
2026-07-22.

The earlier numbered feature phases remain useful history, but they are not a
queue to continue indefinitely. Completed feature details are preserved in
`docs/roadmap-archive-2026-07-04.md` and
`docs/roadmap-backup-2026-07-10.md`. The implemented language contract remains
in `README.md` and `docs/language-grammar.ebnf`.

## Why the roadmap changed

The project has accumulated a broad and useful language surface: functions and
closures, generic types, enums and exhaustive matching, mutable collections,
named structs and methods, source imports, bytecode artifacts, and a Rust VM.
The marginal value of another isolated helper or syntax feature is now lower
than the value of improving the foundations shared by every future feature.

The current question is no longer only whether the language can grow. It is
whether it can grow with low repetition, low regression risk, and a verification
story that remains understandable as the implementation expands. The current
bottleneck is integration cost: type checking, name resolution, flow facts,
module metadata, and AST-to-IR lowering still meet in a large front-end
boundary. Source imports have module metadata, but compilation is still
fundamentally source-assembly based. The C++ compiler, `.cdbc` format, and Rust
VM have broad parity coverage, but their compatibility and debugging contracts
can be made more explicit.

From this point forward, a small user-visible feature is a means of validating
one of these capabilities, not a roadmap item by itself.

## Current baseline

The repository currently provides:

- a C++17 lexer, parser, type checker, register IR, and bytecode emitter;
- source spans and file-aware front-end diagnostics;
- lexical scopes, functions, closures, generic functions/enums/structs,
  nullable types, arrays, maps, ranges, named structs, methods, imports, and
  exhaustive pattern matching;
- a text `.cdbc` boundary and a standalone Rust VM with end-to-end execution
  and artifact-parity tests;
- module-interface metadata, import search paths, re-exports, warnings and
  sanitizer build options, and a broad golden-test suite.

The main architectural constraints are:

- the type checker is the largest front-end hotspot and owns several unrelated
  responsibilities;
- the type checker and IR compiler independently interpret the AST, so semantic
  decisions can be duplicated across passes;
- module interfaces exist, but they do not yet form a complete separate-
  compilation or incremental-build model;
- diagnostics, source mapping, runtime stack information, and artifact metadata
  are available in pieces rather than one consistently shared contract;
- formatter, REPL, language-server, and source-level debugging workflows are
  not yet productized.

## Execution model

Every milestone or vertical slice in this document must have four parts:

1. **Deliverable:** a boundary that can be merged, reviewed, and reverted on
   its own.
2. **Migration strategy:** how the new path coexists with the old path, how
   parity is checked, and how the cutover is performed.
3. **Quantitative gate:** a measurable target and a command or fixture set that
   proves it.
4. **Old-path deletion condition:** an explicit condition for removing the
   compatibility branch, duplicate implementation, or temporary adapter.

The old path is removed because the new path is proven, not because a refactor
has become inconvenient to maintain. During migration, run old and new paths in
shadow mode where possible and compare semantic output or generated IR; do not
execute a program twice merely to compare runtime side effects.

Each semantic slice must preserve the existing end-to-end chain:

```text
source -> parser -> semantic model -> IR -> .cdbc -> Rust VM
```

When a slice changes behavior across that chain, its proof includes the
relevant AST/IR/bytecode output, diagnostics, runtime goldens, artifact parser,
and Rust VM tests. A slice is not complete because one compiler binary accepts
the source.

## Prioritization model

Work should be selected by the outcome it unlocks, not by the novelty of its
surface syntax.

| Priority | Capability outcome | Why it comes first |
| --- | --- | --- |
| P0 | Verification foundation | Makes regressions, cross-backend mismatches, and malformed-input failures cheap to detect. |
| P0.5 | Semantic decisions and minimal artifact envelope | Removes ambiguity before HIR work and makes artifacts self-describing without promising compatibility yet. |
| P1 | Shared semantic front end | Removes duplicated type/name/lowering decisions and makes future features cheaper. |
| P1 | Coherent language semantics | Turns scattered type and control-flow rules into one predictable model. |
| P2 | Module graph and project model | Enables real reuse, separate compilation, and incremental work across files. |
| P2 | Formal artifact and runtime contract | Makes the C++ compiler and Rust VM independently evolvable and debuggable. |
| P3 | Independent developer tools | Delivers formatter, LSP, REPL, and debugger on their own dependency schedules. |
| Conditional | Advanced language and VM research | Starts only when a concrete use case and the preceding contracts justify it. |

The dependency shape is:

```text
verification foundation
        |
        v
semantic decisions + minimal artifact envelope
        |
        v
shared semantic front end ----> coherent language semantics ----> tooling
        |
        v
module graph and interfaces ----> separate/incremental compilation

verification foundation ----> formal artifact/runtime contract ----> measured optimization
```

The tracks are not required to be perfectly serial. The ordering identifies
which foundations must be in place before a later investment is likely to pay
off.

## Milestone 0: Verification foundation

**Objective:** make the current pipeline easy to verify, compare, and evolve.

M0 is test and observability infrastructure. It is not an artifact compatibility
promise and it does not freeze the `.cdbc` format. Formal artifact compatibility
belongs to Milestone 4.

### Deliverables

- one stage-aware conformance command covering parser/type errors, compiler
  output, bytecode artifacts, C++ output modes where applicable, and Rust VM
  execution;
- failure reports that identify the first disagreeing boundary rather than only
  reporting a final output mismatch;
- deterministic property/fuzz coverage for the lexer, parser, diagnostic
  recovery, and `.cdbc` parser, with the invariant that malformed input does not
  crash the compiler or VM;
- a verification matrix covering warning-enabled builds, sanitizers, C++ tests,
  Python goldens, artifact tests, Rust VM tests, and `cargo test`;
- recorded baseline measurements for compile time, test duration, artifact
  size, and representative runtime behavior.

### Migration strategy

Keep the existing runners as a reference while the unified command is built.
Run both orchestration paths against the current fixture corpus and compare
their pass/fail classification before changing any compiler behavior.

### Quantitative gate

- 100% of current test categories are reachable from the unified command;
- 100% of current fixtures have a stage/category label;
- the unified and legacy runners produce zero classification differences on the
  full current corpus;
- malformed-source and malformed-artifact tests have zero process crashes;
- the baseline measurement record is generated in CI and is reproducible on a
  clean checkout.

### Old-path deletion condition

Delete the legacy orchestration only after the unified command has passed the
full corpus in two consecutive CI runs, has equivalent failure diagnostics for
the existing categories, and has a documented escape hatch for adding a new
test category. Do not delete individual compiler or VM behavior based only on a
test-runner migration.

## Milestone 0.5: Semantic decisions and minimal artifact envelope

**Objective:** record the decisions that HIR and module work depend on, and make
the artifact boundary minimally self-describing before introducing a new
semantic representation.

M0.5 is deliberately between verification and architecture. It prevents an HIR
implementation from silently deciding language semantics while also avoiding a
premature public compatibility policy.

### Deliverables

Create a small decision log covering at least:

- evaluation order, side effects, short-circuiting, and error propagation;
- binding identity, lexical scope, closure capture, shared cells, and mutation;
- type identity, assignability, unknown/dynamic values, generic substitution,
  function compatibility, and variance policy;
- source-file/span ownership, diagnostic ownership, and source-to-artifact
  mapping;
- module identity, visibility, re-export behavior, and interface ownership;
- pattern bindings, guards, exhaustiveness, reachability, and nullable cases;
- the semantic boundary between native calls, IR operations, bytecode, and VM
  runtime behavior.

Define a minimum `.cdbc` envelope containing, at minimum:

- a magic/signature identifying the artifact family;
- an envelope/schema version, even if it is initially internal;
- artifact kind and target/runtime identity;
- feature or capability flags;
- deterministic section boundaries and lengths, with a policy for unknown
  sections;
- source/module identity and a marker for available debug metadata;
- a checksum or equivalent integrity field where the transport requires it.

The envelope is not yet a compatibility guarantee. It is a stable place to put
one later. M4 defines whether versions are compatible, how capabilities are
negotiated, and how old artifacts are migrated or rejected.

### Migration strategy

Keep the current versionless/text artifact reader behind an explicit legacy
adapter. Add the envelope reader and writer beside it, then regenerate a small
representative artifact corpus through the new path. Compare decoded sections
and execution behavior before migrating all golden artifacts.

### Quantitative gate

- every semantic domain listed above is marked resolved, intentionally open, or
  explicitly deferred before HIR implementation begins;
- 100% of representative artifact fixtures expose the minimum envelope fields;
- envelope parse/dump is byte-for-byte deterministic for 100% of the corpus;
- invalid envelope/version/section-length cases fail before VM execution;
- zero HIR design changes depend on an undocumented semantic decision.

### Old-path deletion condition

Do not remove the legacy artifact adapter until all current artifact fixtures
have an explicit migration status and the project has decided whether legacy
versionless artifacts are converted or permanently rejected. This deletion is
separate from M4's eventual compatibility-policy decision.

## Milestone 1: Shared semantic front end

**Objective:** establish one semantic model that the backend can consume rather
than making each later pass rediscover meaning from raw AST nodes.

M1 is not one large rewrite. It is a sequence of independently deliverable
vertical slices. Each slice changes one semantic family end to end, proves old
and new behavior in parallel, and deletes its duplicate path before the next
slice expands the boundary.

### M1A: Stable identities and source model

Introduce stable identities for source files, spans, syntax nodes, symbols,
bindings, modules, and declared types. The first proof slice covers lexical
bindings, variable reads, assignments, block scope, and file-aware diagnostics.

**Migration:** retain current string/pointer lookups while recording stable IDs;
shadow-compare binding resolution and source locations for the proof fixtures.

**Quantitative gate:** 100% of variable, assignment, scope, and direct
multi-file diagnostic fixtures use stable IDs; semantic resolution and old
lookup disagree zero times on the full proof corpus; all migrated diagnostics
retain their existing line/column/path output.

**Delete the old path when:** no migrated binding or diagnostic reaches the
legacy lookup through production code, and the proof corpus passes through the
unified conformance command.

### M1B: Declaration collection and name resolution

Separate declaration collection, lexical lookup, imported/exported symbols,
function signatures, struct/enum declarations, and method metadata from
expression checking. The proof slice covers declarations, calls, methods, and
namespace-qualified names without changing source syntax.

**Migration:** build the new symbol table from the existing AST, compare
resolved symbol IDs and signatures with the current `TypeChecker` results, and
switch one declaration family at a time. Keep module graph construction in M3;
this slice only establishes the semantic interface it will consume.

**Quantitative gate:** 100% of current function, method, struct, enum, import,
export, and namespace fixtures pass through the new declaration/resolution
path; zero unresolved-symbol mismatches remain in shadow mode; every resolved
call has a stable target ID before lowering.

**Delete the old path when:** each migrated declaration family has no fallback
name-resolution call site and its old symbol-table representation is no longer
read by IR lowering.

### M1C: Typed expressions and assignment targets

Introduce typed semantic nodes for literals, variables, calls, indexing, field
access, assignments, compound assignments, and native calls. The proof slice
must cover both statically known and dynamic runtime-validation paths.

**Migration:** type-check the existing AST once, materialize resolved targets
and types, and make IR lowering consume those results. Compare semantic dumps or
IR rather than executing programs twice.

**Quantitative gate:** 100% of existing expression, assignment, collection,
struct-field, and native-call fixtures pass through typed nodes; zero backend
lookups re-infer a binding or call target for migrated nodes; full-corpus old/new
IR comparison has zero unexplained differences.

**Delete the old path when:** `IRCompiler` no longer performs type/name
resolution for a migrated expression family and the corresponding AST-specific
re-inference helpers have zero production callers.

### M1D: Control flow, functions, and closures

Represent control-flow edges, returns, loop targets, function signatures,
closure captures, and shared-cell accesses in the semantic layer. The proof
slice covers `if`, loops, `break`/`continue`, functions, recursion, anonymous
functions, and nested closures.

**Migration:** keep the current lowering as the reference implementation;
materialize capture sets and control-flow targets first; then lower the same
semantic representation to existing IR operations and compare artifacts and
Rust VM behavior.

**Quantitative gate:** 100% of current control-flow/function/closure fixtures
use the new capture and control-flow metadata; every captured binding has one
stable binding ID; zero C++/Rust VM parity regressions; and all existing runtime
stack frames still have source locations.

**Delete the old path when:** capture discovery and loop-target resolution are
performed only by the semantic layer, with no AST re-walk in the backend for
migrated constructs.

### M1E: Types, generics, aggregates, and patterns

Move generic substitution, nullable compatibility, arrays, maps, ranges,
structs, enums, record/variant patterns, and exhaustiveness inputs behind the
same semantic type and pattern interfaces. This is the largest M1 slice, but it
is still delivered as one feature family at a time rather than as a checker
rewrite.

**Migration:** migrate the type utility and pattern-coverage decisions first;
preserve existing runtime representations and bytecode operations; use the
current end-to-end goldens as the compatibility oracle for each family.

**Quantitative gate:** 100% of current type-error and pattern fixtures are
classified by the shared type/pattern model; every generic instantiation used by
lowering has explicit substitutions; old/new semantic or IR comparison has zero
unexplained differences; and no new opcode is introduced solely to make the
semantic migration possible.

**Delete the old path when:** all migrated type and pattern decisions flow
through the shared model, local compatibility helpers have zero production
callers, and the backend consumes resolved aggregate/pattern operations rather
than reconstructing them from AST shape.

### M1F: Front-end cutover and cleanup

Make the semantic representation the only input to register-IR lowering, retain
the AST printer only for syntax inspection, and remove migration-only adapters.

**Migration:** perform a final full-corpus comparison, switch the CLI and all
test entry points to the semantic path, then land deletion as a separate
reviewable change so rollback remains possible.

**Quantitative gate:** 100% of successful, parse-error, type-error,
import-error, runtime-error, artifact, and Rust VM fixtures execute through the
new path; production legacy-path invocation count is zero; the full verification
matrix is green; and the measured number of duplicated semantic decision sites
is lower than the M0.5 baseline.

**Delete the old path when:** the gates above hold for two full verification
runs, the rollback point is recorded, and no compatibility adapter is still
needed by the CLI, tests, module loader, or artifact tools.

## Milestone 2: Coherent language semantics and analysis

**Objective:** make the language rules internally consistent before expanding
the surface area again.

### Deliverables

- unify type compatibility, generic substitution, function arity/compatibility,
  nullable values, collection element types, struct fields, enum payloads, and
  pattern bindings around the shared semantic type model;
- build a sound flow-analysis framework for branch, loop, return, mutation, and
  closure boundaries, including invalidation when aliases or calls can mutate
  state;
- share exhaustiveness and reachability reasoning between match statements,
  match expressions, nullable cases, guards, and future pattern forms;
- improve parser and type-checker recovery so independent diagnostics can be
  reported without corrupting later analysis, while retaining distinct parse,
  type, import, compile, and runtime error categories;
- specify evaluation order, aliasing, closure capture, mutation, runtime
  equality, and dynamic-value checks in one language-semantics document;
- decide recursive type support, richer narrowing, and remaining known
  limitations as deliberate language policy rather than incremental TODOs.

### Migration strategy

Add one analysis rule at a time on top of M1's semantic model. Keep existing
special cases as assertions or compatibility shims during migration, and add
positive, negative, mutation-invalidation, and recovery fixtures before changing
the default result.

### Quantitative gate

- every statement, expression, pattern, and type form in the implemented grammar
  appears in the analysis inventory;
- 100% of flow facts are produced by the shared dataflow engine, with zero
  production-only ad-hoc narrowing rules outside it;
- every documented diagnostic category has at least one positive and one
  negative regression fixture where applicable;
- multi-error ordering is deterministic across repeated runs;
- C++ artifact output and Rust VM behavior have zero unexplained semantic
  differences on the full corpus.

### Old-path deletion condition

Remove local `TypeChecker`/`FlowFacts` special cases only after their rule has a
shared analysis implementation, a mutation-invalidation test, and zero callers
outside the shared engine. Do not delete a conservative rule merely to make a
new feature type-check.

## Milestone 3: Module graph and project model

**Objective:** turn imports from source-loading conveniences into a dependable
module/build boundary.

### Deliverables

- evolve `FrontendSession` into an explicit module graph with stable module
  identities, dependency edges, import cycles, visibility, and source origins;
- make exported symbols, generic signatures, struct methods, enum metadata, and
  diagnostics first-class module-interface data;
- define interface format/versioning and compatibility rules before using
  interfaces for caching;
- compile modules independently, then add dependency-aware incremental rebuilds
  and cache invalidation; retain a clear single-file mode;
- formalize import resolution, search paths, re-exports, entry points, and
  future project/package manifests without ad-hoc filesystem scanning;
- preserve file-local diagnostics and deterministic module ordering in source,
  interface, and bytecode outputs.

### Migration strategy

Build the module graph beside the current source loader. For each import mode,
compare graph-derived interfaces, symbol visibility, diagnostics, and final
artifacts with the combined-source path. Migrate direct single-file inputs,
source imports, namespace imports, re-exports, and search paths separately.

### Quantitative gate

- 100% of current import, export, namespace, re-export, search-path, and cycle
  fixtures use graph-derived module identities;
- a no-change rebuild has a 100% interface/cache reuse rate for unchanged
  dependencies;
- a change to one leaf recompiles that leaf and its transitive dependents, but
  zero unrelated modules;
- module ordering and file-aware diagnostics are byte-for-byte stable across
  repeated builds;
- no imported program requires dependency source concatenation to be
  type-checked or lowered.

### Old-path deletion condition

Remove combined-source assembly from import builds only after all current
single-file, direct-multi-file, imported-file, and diagnostic-remapping tests
run through the graph. Keep a deliberately documented single-file adapter if it
does not rely on dependency concatenation; delete the old module loader only
after the interface and cache gates pass.

Package management and a large standard library are later product decisions.
They should consume this module boundary rather than define it.

## Milestone 4: Formal artifact and runtime contract

**Objective:** make the compiler/VM boundary versioned, validated, observable,
and safe to evolve. This milestone is where compatibility becomes an explicit
product contract; M0 only built verification infrastructure, and M0.5 only
defined the minimum envelope.

### Deliverables

- define `.cdbc` compatibility policy, including supported version ranges,
  major/minor rules, feature negotiation, deterministic encoding, and actionable
  rejection of unsupported artifacts;
- centralize bytecode validation and invariants before execution, with matching
  C++ emitter and Rust parser/formatter tests;
- define artifact migration or rejection policy for the M0.5 legacy format;
- standardize source/debug metadata so runtime errors and stack traces map back
  to modules, files, spans, functions, and calls;
- document and test the runtime model for shared cells, closures, references,
  mutable collections, native calls, and failure propagation;
- measure before adding constant folding, control-flow optimizations, register
  allocation, or other backend work;
- design memory ownership and garbage collection only after allocation and root
  requirements are measured.

### Migration strategy

Use the M0.5 envelope as the reader/writer boundary. During the transition,
support the explicitly documented legacy version in an adapter, emit the new
version by default, and run old/new decode and execution comparisons. Publish
the compatibility matrix before removing the adapter.

### Quantitative gate

- every emitted artifact is accepted or rejected by an explicit compatibility
  rule; no implicit versionless fallback remains;
- 100% of malformed-artifact fixtures fail before VM execution;
- canonical emission is byte-for-byte deterministic for 100% of the artifact
  corpus;
- 100% of runtime stack frames in the source-level test corpus map to a module,
  function, and source span;
- every backend optimization has a correctness fixture and a recorded
  improvement against the M0 baseline.

### Old-path deletion condition

Remove the legacy artifact parser/emitter only after the compatibility matrix,
migration/rejection behavior, and all artifact fixtures are checked in. Remove
temporary runtime/source-map adapters only after all stack frames use the shared
metadata path and the full C++/Rust conformance suite passes.

Task scheduling, async execution, and JIT metadata remain research topics until
the runtime contract and representative workloads justify them.

## Milestone 5: Independent developer-tool schedules

**Objective:** make the language practical to read, edit, experiment with, and
debug using the same compiler services as the CLI. These are separate products,
not one combined “toolchain” phase.

### Milestone 5A: Formatter

**Dependency:** stable parser, source spans, and the semantic/grammar contract.

Build a syntax-aware formatter with documented formatting stability. It should
operate on syntax, preserve the language's parse meaning, and avoid creating a
second grammar.

**Migration:** use the production lexer/parser and add formatting only after
parse-tree round trips are observable. Keep examples and golden inputs as the
formatter corpus.

**Quantitative gate:** `format(format(source)) == format(source)` for 100% of
the valid formatter corpus; 100% of formatted inputs parse successfully; and
formatted programs have zero AST/semantic-output differences from their
unformatted forms.

**Old-path deletion condition:** delete any formatter-specific parser or
grammar once the production parser covers the formatter corpus and the
idempotence gate passes.

### Milestone 5B: Language Server

**Dependency:** M1 semantic identities and M3 module graph; M2 diagnostics make
the service useful but need not be completely finished before a prototype.

Expose diagnostics, symbol lookup, definition/references, type information,
completion, and module navigation through a language-server-compatible service.

**Migration:** make the CLI and LSP consume the same source, semantic, module,
and diagnostic services. Do not reimplement name resolution or type inference
inside editor adapters.

**Quantitative gate:** on a representative multi-file corpus, 100% of exported
symbols and declarations have stable definition locations; diagnostics match
the CLI in message, severity, and span; and repeated queries do not reparse
unchanged modules unnecessarily.

**Old-path deletion condition:** remove editor-specific diagnostic and symbol
resolution code when all supported LSP queries use shared semantic IDs and
module interfaces.

### Milestone 5C: REPL / incremental evaluation

**Dependency:** M3 module/session boundaries and the M4 runtime contract.

Add a REPL or incremental evaluation mode with explicit session state, imports,
definitions, diagnostics, and runtime error behavior.

**Migration:** expose an incremental wrapper around the production front end and
VM. Keep session state separate from compiler-global state, and use the same
module identity and artifact rules as project builds.

**Quantitative gate:** 100% of the REPL transcript corpus is deterministic;
definitions persist exactly when specified; every accepted form has the same
semantic result as a corresponding file; and runtime errors leave the session
in a documented, testable state.

**Old-path deletion condition:** remove any one-off REPL parser/evaluator once
all transcript cases use the production semantic and runtime services.

### Milestone 5D: Source debugger and tracing

**Dependency:** M4 source/debug metadata and stable runtime stack behavior.

Add source-level stack inspection, function/line tracing, breakpoints or an
equivalent stepping protocol, and runtime-value inspection.

**Migration:** build on emitted debug metadata and VM events; do not create a
second source-location mapping scheme in the debugger.

**Quantitative gate:** 100% of frames in the debugger corpus map to source
  locations; stepping through the corpus produces deterministic event order;
  and inspected values agree with VM output for all supported runtime kinds.

**Old-path deletion condition:** remove temporary debug-location tables when
all debugger events use the M4 metadata contract and no runtime frame requires
fallback source guessing.

## Conditional research track

These are valid directions, but they are not the active queue:

- recursive structs and richer nominal/structural type relationships;
- protocols, traits, dynamic dispatch, inheritance, or overloading;
- strings as iterable values and a general iterator abstraction;
- garbage collection, task scheduling, async execution, and JIT compilation;
- package registries, dependency resolution, and large standard-library design.

Each item needs a separate design decision covering user value, semantics,
module/interface impact, diagnostics, runtime representation, quantitative
workload, and verification. It should enter the active roadmap only if the
shared foundations above make the investment tractable and a concrete use case
warrants it.

## Work admission rules

Before starting a substantial change, its design document must answer:

1. Which roadmap capability and which concrete milestone/slice does it advance?
2. Does it reduce duplicated compiler/VM work or unlock more than one future
   feature area?
3. What are its source, type, diagnostic, IR, bytecode, and runtime contracts?
4. What is the baseline, target metric, measurement command, and exit gate?
5. How does the new path coexist with the old path during migration?
6. What exact condition permits deleting the old path?
7. What behavior is intentionally not included?

An isolated builtin, syntax convenience, or VM opcode should not become the
next priority merely because it is easy to implement. It may still be used as
the smallest proof of a broader milestone.

## Near-term execution order

The next development sequence is:

```text
M0 verification foundation
-> M0.5 semantic decision log + minimum artifact envelope
-> M1A stable identities and source model
-> M1B declaration collection and name resolution
-> M1C typed expressions and assignment targets
-> M1D control flow, functions, and closures
-> M1E types, generics, aggregates, and patterns
-> M1F semantic-front-end cutover and old-path deletion
-> M2 language-wide analysis and semantic consistency
-> M3 module graph and interface-driven compilation
-> M4 formal artifact/runtime compatibility contract
```

Independent tool schedules after their dependencies:

```text
parser/source contract ------------------------------> M5A formatter
M1 semantic identities + M3 module graph ------------> M5B LSP
M3 module/session boundaries + M4 runtime contract --> M5C REPL
M4 source/debug metadata ----------------------------> M5D debugger
```

Keeping these schedules separate prevents one delayed tool from blocking the
others.

The immediate next project is therefore M0/M0.5: establish the verification
baseline, record the semantic decisions, and define the minimum artifact
envelope before writing HIR code.

## Metrics dashboard

Track these measures in each milestone report instead of counting APIs:

- **Correctness:** full-suite pass rate, unexplained cross-backend mismatches,
  malformed-input crashes, and deterministic-output failures;
- **Migration:** percentage of fixtures on the new path, shadow-mode mismatch
  count, production legacy-path invocation count, and deleted duplicate helpers;
- **Semantic architecture:** number of independent type/name/lowering decision
  sites, stable-ID coverage, and percentage of backend operations carrying
  resolved semantic data;
- **Modules:** unchanged-dependency cache hit rate, rebuild scope after a leaf
  change, graph diagnostic stability, and source-concatenation usage count;
- **Artifacts/runtime:** envelope coverage, invalid-artifact rejection rate,
  source-mapped frame coverage, artifact size, compile time, and representative
  runtime performance against the M0 baseline;
- **Tools:** formatter idempotence, CLI/LSP diagnostic equivalence, REPL
  transcript determinism, and debugger source-frame coverage.

The roadmap is successful when the compiler can grow in coherent vertical
slices without repeatedly redesigning its semantic model, module boundary, or
backend contract, and when every old implementation path has a visible reason
to remain or a verified condition for removal.
