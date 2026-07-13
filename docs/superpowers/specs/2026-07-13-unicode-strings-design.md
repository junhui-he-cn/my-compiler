# Unicode String Semantics Design

Date: 2026-07-13  
Status: approved for implementation planning

## Goal

Make string length and slicing predictable for valid UTF-8 source programs.
The language will treat a Unicode scalar value (a Unicode code point encoded in
UTF-8) as the unit used by string length and string indexing operations. This
changes the current byte-oriented behavior of `len`, `substr`, and `charAt`
without adding new syntax or bytecode instructions.

The first slice deliberately does not define grapheme-cluster segmentation,
normalization, locale-sensitive behavior, collation, or regular expressions.

## User-visible semantics

Language strings are UTF-8 text. For a string `s`:

- `len(s)` returns the number of Unicode scalar values in `s` as a number.
- `substr(s, start, length)` interprets `start` and `length` as scalar-value
  offsets and returns exactly `length` complete scalar values beginning at
  `start`.
- `charAt(s, index)` interprets `index` as a scalar-value offset and returns
  one complete scalar value as a string.
- The equivalent member forms (`s.len()`, `s.substr(...)`, and
  `s.charAt(...)`) have exactly the same semantics as their function forms.
- ASCII behavior is unchanged. For example, `len("hello")` remains `5`.
- Combining sequences and zero-width-joiner sequences are counted one scalar at
  a time. For example, a base letter followed by a combining accent counts as
  two scalar values; this slice does not attempt user-perceived grapheme
  segmentation.
- Empty strings have length zero. `substr(s, len(s), 0)` returns the empty
  string. `charAt` has no valid index for an empty string.

The existing argument-count, argument-type, finite-integer, and bounds error
messages remain stable. Bounds are checked against scalar-value length rather
than byte length. No new diagnostic category is introduced.

## Architecture and data flow

The C++ compiler already stores string literals as `std::string` UTF-8 bytes,
emits those bytes as escaped `.cdbc` string constants, and has no separate
string runtime or string-index opcode. The compiler pipeline therefore remains
structurally unchanged:

```text
UTF-8 source literal
  -> C++ token/IR/bytecode string constant (bytes preserved)
  -> .cdbc string constant
  -> Rust Value::String
  -> Rust VM native len/substr/charAt helpers
```

The Rust VM will centralize scalar counting and scalar-index-to-byte-offset
conversion in small internal helpers. `len`, `substr`, and `charAt` will use
those helpers so their boundaries cannot diverge. Slicing will use validated
`char_indices` boundaries and will never split a UTF-8 encoding. Existing
native-call dispatch, member-call lowering, shadowing rules, bytecode format,
and runtime diagnostic source locations remain unchanged.

Valid UTF-8 is a precondition for string runtime operations. This slice does
not add a new source-encoding diagnostic or attempt to repair malformed input;
the existing artifact/parser UTF-8 handling remains authoritative for malformed
byte streams.

## Error behavior

The current messages and argument validation are retained:

- `substr` still requires three arguments, a string receiver, and numeric
  `start`/`length` values.
- `charAt` still requires two arguments, a string receiver, and a numeric index.
- Non-finite or fractional offsets use the existing `expects integer ...`
  messages.
- Negative offsets, offsets past the scalar boundary, and lengths that extend
  past the end use the existing `... out of bounds` messages.

Because byte offsets are no longer exposed, a valid scalar index can never
produce the old `produced invalid utf-8` failure. That implementation-only
failure is removed from the normal path; malformed artifact input remains a
parser/IO concern outside this feature.

## Testing strategy

Tests will be written before implementation for the following matrix:

1. Existing ASCII native and member-call fixtures continue to pass unchanged.
2. A UTF-8 golden program exercises `len`, `substr`, `charAt`, concatenation,
   and member-call forms with Chinese text and an emoji; expected output is
   shared by the C++ compiler/Rust VM golden runner.
3. A scalar-boundary case covers combining marks or a ZWJ sequence and records
   the intentional scalar (not grapheme) count.
4. Runtime-error fixtures cover scalar-index bounds, length bounds, negative
   offsets, and non-integer offsets on non-ASCII strings.
5. A bytecode artifact fixture verifies emitted UTF-8 constants and the stable
   `.cdbc` text remain unchanged structurally while the Rust VM executes the
   new semantics.
6. Focused Rust unit tests cover scalar counting, boundary conversion,
   multi-byte `substr`, single-scalar `charAt`, empty strings, and all relevant
   bounds. The full CTest, golden, artifact, Rust VM, and Cargo suites remain
   the completion gate.

## Documentation updates

- Replace the byte-length/byte-offset statements in `README.md` with the scalar
  value contract and examples.
- Mark Phase 16 in `docs/roadmap.md` as implemented and retain its explicit
  non-goals.
- Update any active string builtin documentation that still promises byte
  offsets; historical design/plan documents remain historical records.
- No `.cdbc` format version or opcode changes are needed, but the format docs
  will note that string constants are UTF-8 and runtime string operations use
  scalar-value offsets.

## Alternatives considered

### Grapheme clusters

Using a grapheme segmentation library would better match user-perceived
characters, but introduces a dependency and Unicode-version-sensitive behavior
that is disproportionate to this first slice. It remains a possible future
extension with a dedicated design.

### Byte-oriented strings

Keeping byte offsets would minimize code changes but leaves non-ASCII behavior
surprising and can split UTF-8 sequences. It does not satisfy the roadmap goal
of predictable Unicode string behavior.

