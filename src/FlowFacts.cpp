#include "FlowFacts.hpp"

#include "Token.hpp"

#include <cstddef>
#include <utility>

namespace {

const Expr& ungrouped(const Expr& expression)
{
    const Expr* current = &expression;
    while (const auto* grouping = dynamic_cast<const GroupingExpr*>(current)) {
        current = grouping->expression.get();
    }
    return *current;
}

const VariableExpr* nilCheckedVariable(const Expr& left, const Expr& right)
{
    const auto* leftVariable = dynamic_cast<const VariableExpr*>(&left);
    const auto* rightLiteral = dynamic_cast<const LiteralExpr*>(&right);
    if (leftVariable && rightLiteral && rightLiteral->value == "nil") {
        return leftVariable;
    }

    const auto* rightVariable = dynamic_cast<const VariableExpr*>(&right);
    const auto* leftLiteral = dynamic_cast<const LiteralExpr*>(&left);
    if (rightVariable && leftLiteral && leftLiteral->value == "nil") {
        return rightVariable;
    }

    return nullptr;
}

class NarrowingStackGuard {
public:
    NarrowingStackGuard(std::vector<FlowNarrowing>& activeNarrowings, std::size_t savedSize)
        : activeNarrowings_(activeNarrowings)
        , savedSize_(savedSize)
    {
    }

    ~NarrowingStackGuard()
    {
        activeNarrowings_.resize(savedSize_);
    }

    NarrowingStackGuard(const NarrowingStackGuard&) = delete;
    NarrowingStackGuard& operator=(const NarrowingStackGuard&) = delete;

private:
    std::vector<FlowNarrowing>& activeNarrowings_;
    std::size_t savedSize_;
};

} // namespace

void FlowFacts::clear()
{
    activeNarrowings_.clear();
}

BranchFlowFacts FlowFacts::factsForIfCondition(
    const Expr& condition,
    const VariableNarrowingResolver& resolveVariableNarrowing) const
{
    const Expr& narrowedCondition = ungrouped(condition);

    if (const auto* logical = dynamic_cast<const LogicalExpr*>(&narrowedCondition)) {
        const BranchFlowFacts left = factsForIfCondition(*logical->left, resolveVariableNarrowing);
        const BranchFlowFacts right = factsForIfCondition(*logical->right, resolveVariableNarrowing);

        BranchFlowFacts result;
        if (logical->op.type == TokenType::AmpersandAmpersand) {
            result.thenNarrowings = left.thenNarrowings;
            result.thenNarrowings.insert(
                result.thenNarrowings.end(),
                right.thenNarrowings.begin(),
                right.thenNarrowings.end());
        } else if (logical->op.type == TokenType::PipePipe) {
            result.elseNarrowings = left.elseNarrowings;
            result.elseNarrowings.insert(
                result.elseNarrowings.end(),
                right.elseNarrowings.begin(),
                right.elseNarrowings.end());
        }
        return result;
    }

    const auto* binary = dynamic_cast<const BinaryExpr*>(&narrowedCondition);
    if (!binary || (binary->op.type != TokenType::BangEqual && binary->op.type != TokenType::EqualEqual)) {
        return BranchFlowFacts{};
    }

    const VariableExpr* variable = nilCheckedVariable(*binary->left, *binary->right);
    if (!variable) {
        return BranchFlowFacts{};
    }

    std::optional<FlowNarrowing> narrowing = resolveVariableNarrowing(*variable);
    if (!narrowing) {
        return BranchFlowFacts{};
    }

    BranchFlowFacts result;
    if (binary->op.type == TokenType::BangEqual) {
        result.thenNarrowings.push_back(std::move(*narrowing));
    } else {
        result.elseNarrowings.push_back(std::move(*narrowing));
    }
    return result;
}

std::optional<TypeInfo> FlowFacts::narrowedTypeFor(const std::string& resolvedName) const
{
    for (auto it = activeNarrowings_.rbegin(); it != activeNarrowings_.rend(); ++it) {
        if (it->resolvedName == resolvedName) {
            return it->type;
        }
    }
    return std::nullopt;
}

void FlowFacts::withNarrowings(
    const std::vector<FlowNarrowing>& narrowings,
    const std::function<void()>& body)
{
    if (narrowings.empty()) {
        body();
        return;
    }

    const std::size_t savedSize = activeNarrowings_.size();
    activeNarrowings_.insert(activeNarrowings_.end(), narrowings.begin(), narrowings.end());
    NarrowingStackGuard guard(activeNarrowings_, savedSize);
    body();
}
