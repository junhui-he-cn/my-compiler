#pragma once

#include <cstddef>
#include <functional>
#include <limits>

// IDs in this file are intentionally distinct C++ types.  Their numeric value
// is meaningful only inside the compiler snapshot that created them; it must
// not be used as a cache key or serialized into a bytecode artifact.
template <typename Tag>
struct SnapshotId {
    using value_type = std::size_t;
    static constexpr value_type invalidValue = std::numeric_limits<value_type>::max();

    value_type value = invalidValue;

    constexpr SnapshotId() = default;
    explicit constexpr SnapshotId(value_type value)
        : value(value)
    {
    }

    constexpr bool valid() const
    {
        return value != invalidValue;
    }

    constexpr explicit operator bool() const
    {
        return valid();
    }

    friend constexpr bool operator==(SnapshotId left, SnapshotId right)
    {
        return left.value == right.value;
    }

    friend constexpr bool operator!=(SnapshotId left, SnapshotId right)
    {
        return !(left == right);
    }

    friend constexpr bool operator<(SnapshotId left, SnapshotId right)
    {
        return left.value < right.value;
    }
};

struct SourceFileIdTag;
struct SyntaxNodeIdTag;
struct DeclarationIdTag;
struct SymbolIdTag;
struct BindingIdTag;
struct ScopeIdTag;

using SourceFileId = SnapshotId<SourceFileIdTag>;
using SyntaxNodeId = SnapshotId<SyntaxNodeIdTag>;
using DeclarationId = SnapshotId<DeclarationIdTag>;
using SymbolId = SnapshotId<SymbolIdTag>;
using BindingId = SnapshotId<BindingIdTag>;
using ScopeId = SnapshotId<ScopeIdTag>;

template <typename Tag>
struct SnapshotIdHash {
    std::size_t operator()(SnapshotId<Tag> id) const noexcept
    {
        return std::hash<std::size_t>{}(id.value);
    }
};

// A source position keeps the source-local byte offset together with the
// existing human-facing 1-based coordinates.  Ranges themselves use byte
// offsets; sourcePositionAt() in SourceMap.hpp is the canonical conversion
// back to line and column.
struct SourcePosition {
    std::size_t byte = 0;
    int line = 1;
    int column = 1;
};

struct SourceRange {
    SourceFileId source;
    // Half-open source-local byte interval [start, end).
    std::size_t start = 0;
    std::size_t end = 0;

    bool valid() const
    {
        return source.valid() && start <= end;
    }

    bool empty() const
    {
        return start == end;
    }
};
