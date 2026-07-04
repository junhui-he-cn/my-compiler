#include "Ast.hpp"

#include <string>
#include <utility>

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

void writeParameterList(std::ostream& out, const std::vector<Token>& parameters)
{
    out << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << parameters[i].lexeme;
    }
    out << ')';
}

void writeInlineStmt(std::ostream& out, const Stmt& stmt)
{
    if (const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
        out << "(return ";
        writeExpr(out, returnStmt->value);
        out << ')';
        return;
    }

    if (const auto* expressionStmt = dynamic_cast<const ExpressionStmt*>(&stmt)) {
        out << "(expr ";
        writeExpr(out, expressionStmt->expression);
        out << ')';
        return;
    }

    if (const auto* printStmt = dynamic_cast<const PrintStmt*>(&stmt)) {
        out << "(print ";
        writeExpr(out, printStmt->expression);
        out << ')';
        return;
    }

    if (const auto* letStmt = dynamic_cast<const LetStmt*>(&stmt)) {
        out << "(let " << letStmt->name.lexeme;
        if (letStmt->typeName) {
            out << ": " << letStmt->typeName->lexeme;
        }
        out << " = ";
        writeExpr(out, letStmt->initializer);
        out << ')';
        return;
    }

    if (const auto* blockStmt = dynamic_cast<const BlockStmt*>(&stmt)) {
        out << "(block";
        for (const auto& child : blockStmt->statements) {
            out << ' ';
            writeInlineStmt(out, *child);
        }
        out << ')';
        return;
    }

    if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        out << "(if ";
        writeExpr(out, ifStmt->condition);
        out << ' ';
        writeInlineStmt(out, *ifStmt->thenBranch);
        if (ifStmt->elseBranch) {
            out << ' ';
            writeInlineStmt(out, *ifStmt->elseBranch);
        }
        out << ')';
        return;
    }

    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        out << "(while ";
        writeExpr(out, whileStmt->condition);
        out << ' ';
        writeInlineStmt(out, *whileStmt->body);
        out << ')';
        return;
    }

    if (const auto* functionStmt = dynamic_cast<const FunctionStmt*>(&stmt)) {
        out << "(fun " << functionStmt->name.lexeme << ' ';
        writeParameterList(out, functionStmt->parameters);
        for (const auto& child : functionStmt->body) {
            out << ' ';
            writeInlineStmt(out, *child);
        }
        out << ')';
        return;
    }

    out << "(stmt)";
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

AssignExpr::AssignExpr(Token name, ExprPtr value)
    : name(std::move(name))
    , value(std::move(value))
{
}

void AssignExpr::print(std::ostream& out) const
{
    out << "(= " << name.lexeme << ' ';
    writeExpr(out, value);
    out << ')';
}

IndexAssignExpr::IndexAssignExpr(ExprPtr collection, Token bracket, ExprPtr index, ExprPtr value)
    : collection(std::move(collection))
    , bracket(std::move(bracket))
    , index(std::move(index))
    , value(std::move(value))
{
}

void IndexAssignExpr::print(std::ostream& out) const
{
    out << "(= (index ";
    writeExpr(out, collection);
    out << ' ';
    writeExpr(out, index);
    out << ") ";
    writeExpr(out, value);
    out << ')';
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

LogicalExpr::LogicalExpr(ExprPtr left, Token op, ExprPtr right)
    : left(std::move(left))
    , op(std::move(op))
    , right(std::move(right))
{
}

void LogicalExpr::print(std::ostream& out) const
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

CallExpr::CallExpr(ExprPtr callee, Token paren, std::vector<ExprPtr> arguments)
    : callee(std::move(callee))
    , paren(std::move(paren))
    , arguments(std::move(arguments))
{
}

void CallExpr::print(std::ostream& out) const
{
    out << "(call ";
    writeExpr(out, callee);
    for (const auto& argument : arguments) {
        out << ' ';
        writeExpr(out, argument);
    }
    out << ')';
}

ArrayExpr::ArrayExpr(std::vector<ExprPtr> elements)
    : elements(std::move(elements))
{
}

void ArrayExpr::print(std::ostream& out) const
{
    out << "(array";
    for (const auto& element : elements) {
        out << ' ';
        writeExpr(out, element);
    }
    out << ')';
}

IndexExpr::IndexExpr(ExprPtr collection, Token bracket, ExprPtr index)
    : collection(std::move(collection))
    , bracket(std::move(bracket))
    , index(std::move(index))
{
}

void IndexExpr::print(std::ostream& out) const
{
    out << "(index ";
    writeExpr(out, collection);
    out << ' ';
    writeExpr(out, index);
    out << ')';
}

FunctionExpr::FunctionExpr(Token keyword, std::vector<Token> parameters, std::vector<StmtPtr> body)
    : keyword(std::move(keyword))
    , parameters(std::move(parameters))
    , body(std::move(body))
{
}

void FunctionExpr::print(std::ostream& out) const
{
    out << "(fun ";
    writeParameterList(out, parameters);
    for (const auto& statement : body) {
        out << ' ';
        writeInlineStmt(out, *statement);
    }
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

WhileStmt::WhileStmt(ExprPtr condition, StmtPtr body)
    : condition(std::move(condition))
    , body(std::move(body))
{
}

void WhileStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "While ";
    writeExpr(out, condition);
    out << '\n';

    writeIndent(out, indent + 1);
    out << "Body\n";
    if (body) {
        body->print(out, indent + 2);
    }
}

FunctionStmt::FunctionStmt(Token name, std::vector<Token> parameters, std::vector<StmtPtr> body)
    : name(std::move(name))
    , parameters(std::move(parameters))
    , body(std::move(body))
{
}

void FunctionStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Fun " << name.lexeme << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << parameters[i].lexeme;
    }
    out << ")\n";
    for (const auto& statement : body) {
        statement->print(out, indent + 1);
    }
}

ReturnStmt::ReturnStmt(Token keyword, ExprPtr value)
    : keyword(std::move(keyword))
    , value(std::move(value))
{
}

void ReturnStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Return ";
    writeExpr(out, value);
    out << '\n';
}

void Program::print(std::ostream& out) const
{
    out << "Program\n";
    for (const auto& statement : statements) {
        statement->print(out, 1);
    }
}
