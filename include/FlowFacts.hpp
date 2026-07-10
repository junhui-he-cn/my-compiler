#pragma once

#include "Ast.hpp"
#include "TypeUtils.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

struct FlowNarrowing {
    std::string resolvedName;
    TypeInfo type;
};

struct BranchFlowFacts {
    std::vector<FlowNarrowing> thenNarrowings;
    std::vector<FlowNarrowing> elseNarrowings;
};

class FlowFacts {
public:
    using VariableNarrowingResolver = std::function<std::optional<FlowNarrowing>(const VariableExpr&)>;

    void clear();
    BranchFlowFacts factsForIfCondition(
        const Expr& condition,
        const VariableNarrowingResolver& resolveVariableNarrowing) const;
    std::optional<TypeInfo> narrowedTypeFor(const std::string& resolvedName) const;
    void withNarrowings(
        const std::vector<FlowNarrowing>& narrowings,
        const std::function<void()>& body);

private:
    std::vector<FlowNarrowing> activeNarrowings_;
};
