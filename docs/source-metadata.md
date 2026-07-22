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
