#include "Ast.hpp"

#include <string>

namespace {

void writeIndent(std::ostream& out, int indent)
{
    for (int i = 0; i < indent; ++i) {
        out << "  ";
    }
}

void writeExpr(std::ostream& out, const ExprPtr& expr)
{
    if (expr) {
        expr->print(out);
    } else {
        out << "nil";
    }
}

} // namespace

LiteralExpr::LiteralExpr(std::string value)
    : value(std::move(value))
{
}

void LiteralExpr::print(std::ostream& out) const
{
    out << value;
}

VariableExpr::VariableExpr(Token name)
    : name(std::move(name))
{
}

void VariableExpr::print(std::ostream& out) const
{
    out << name.lexeme;
}

UnaryExpr::UnaryExpr(Token op, ExprPtr right)
    : op(std::move(op))
    , right(std::move(right))
{
}

void UnaryExpr::print(std::ostream& out) const
{
    out << '(' << op.lexeme << ' ';
    writeExpr(out, right);
    out << ')';
}

BinaryExpr::BinaryExpr(ExprPtr left, Token op, ExprPtr right)
    : left(std::move(left))
    , op(std::move(op))
    , right(std::move(right))
{
}

void BinaryExpr::print(std::ostream& out) const
{
    out << '(' << op.lexeme << ' ';
    writeExpr(out, left);
    out << ' ';
    writeExpr(out, right);
    out << ')';
}

GroupingExpr::GroupingExpr(ExprPtr expression)
    : expression(std::move(expression))
{
}

void GroupingExpr::print(std::ostream& out) const
{
    out << "(group ";
    writeExpr(out, expression);
    out << ')';
}

LetStmt::LetStmt(Token name, std::optional<Token> typeName, ExprPtr initializer)
    : name(std::move(name))
    , typeName(std::move(typeName))
    , initializer(std::move(initializer))
{
}

void LetStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Let " << name.lexeme;
    if (typeName) {
        out << ": " << typeName->lexeme;
    }
    out << " = ";
    writeExpr(out, initializer);
    out << '\n';
}

PrintStmt::PrintStmt(ExprPtr expression)
    : expression(std::move(expression))
{
}

void PrintStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Print ";
    writeExpr(out, expression);
    out << '\n';
}

ExpressionStmt::ExpressionStmt(ExprPtr expression)
    : expression(std::move(expression))
{
}

void ExpressionStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Expr ";
    writeExpr(out, expression);
    out << '\n';
}

BlockStmt::BlockStmt(std::vector<StmtPtr> statements)
    : statements(std::move(statements))
{
}

void BlockStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Block\n";
    for (const auto& statement : statements) {
        statement->print(out, indent + 1);
    }
}

IfStmt::IfStmt(ExprPtr condition, StmtPtr thenBranch, StmtPtr elseBranch)
    : condition(std::move(condition))
    , thenBranch(std::move(thenBranch))
    , elseBranch(std::move(elseBranch))
{
}

void IfStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "If ";
    writeExpr(out, condition);
    out << '\n';

    writeIndent(out, indent + 1);
    out << "Then\n";
    if (thenBranch) {
        thenBranch->print(out, indent + 2);
    }

    if (elseBranch) {
        writeIndent(out, indent + 1);
        out << "Else\n";
        elseBranch->print(out, indent + 2);
    }
}

void Program::print(std::ostream& out) const
{
    out << "Program\n";
    for (const auto& statement : statements) {
        statement->print(out, 1);
    }
}
