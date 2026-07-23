# Source metadata

M1A1 adds source metadata beside the existing compiler representations.

- `SourceFileId`, `SyntaxNodeId`, `DeclarationId`, `SymbolId`, `BindingId`, and
  `ScopeId` are domain-typed, snapshot-local IDs. Their numeric values are only
  meaningful during one front-end/type-checker snapshot and are never artifact,
  cache, or cross-build keys.
- `SourceRange` is a source-local half-open byte interval `[start, end)` with a
  `SourceFileId`. `Token`, AST nodes, bindings, and located diagnostics retain
  these ranges while the existing `SourceSpan` and string resolved names stay
  available for compatibility.
- `sourcePositionAt` converts a byte offset to the existing 1-based line and
  column coordinates. The end offset is allowed to equal the source length, so
  an end position can identify the point immediately after the final byte.
- Direct multi-file inputs keep each token and diagnostic range local to its
  original file even though the parser still consumes the combined source.
  User-facing diagnostics continue to use the established path/line/column
  format.

The current proof slice covers lexical declarations, block scopes, variable
reads, assignments, and direct multi-file diagnostics. Bytecode emission does
not serialize these snapshot-local identities; artifact-local debug identity
remains owned by M4A/M4B.

## Declaration index (M1B initial slice)

`DeclarationIndex` is a snapshot-local AST index built beside the existing
`TypeChecker` path. It records declaration IDs and symbols for lexical values,
parameters, structs, enums, methods, and namespace aliases; scope parents and
lexical lookup tables; function/method signatures; import/export metadata; and
the targets of variable reads, ordinary assignments, and compound assignments.
Struct and enum declarations retain their AST records for field and variant
metadata, while signatures retain their type parameters, parameters, and
optional return annotations.

The checker exposes the index and a shadow-comparison count. During this
migration slice, type and namespace qualifiers are not treated as value reads,
and OR-pattern occurrences share one declaration record. The old
`ResolvedNames` implementation remains the behavior oracle; module graph
resolution and materialization of imported value symbols are deferred to M3A.
The index IDs are not serialized into `.cdbc` artifacts or used as cache keys.

## Lossless source view

`FrontendSession::losslessSourceView()` groups `LosslessPiece` values by
`SourceFileId`. Existing lexer tokens become token pieces; every byte gap
between adjacent token ranges becomes one or more trivia pieces. Line comments
are classified from those already-known gaps, while their bytes and all other
trivia are copied directly from `SourceFile::text`. No second lexer or parser
grammar is involved. Concatenating a file view's pieces reproduces the exact
original source bytes, including comments, whitespace, and comment placement.
