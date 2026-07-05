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

void writeTypeAnnotation(std::ostream& out, const TypeAnnotation& annotation)
{
    if (annotation.kind == TypeAnnotation::Kind::Simple) {
        out << annotation.token.lexeme;
        return;
    }

    out << "fun(";
    for (std::size_t i = 0; i < annotation.parameterTypes.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        writeTypeAnnotation(out, annotation.parameterTypes[i]);
    }
    out << "): ";
    writeTypeAnnotation(out, *annotation.returnType);
}

void writeOptionalTypeAnnotation(std::ostream& out, const std::optional<TypeAnnotation>& annotation)
{
    if (annotation) {
        out << ": ";
        writeTypeAnnotation(out, *annotation);
    }
}

void writeParameterList(std::ostream& out, const std::vector<Parameter>& parameters)
{
    out << '(';
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << parameters[i].name.lexeme;
        writeOptionalTypeAnnotation(out, parameters[i].typeName);
    }
    out << ')';
}

void writeReturnAnnotation(std::ostream& out, const std::optional<TypeAnnotation>& returnTypeName)
{
    writeOptionalTypeAnnotation(out, returnTypeName);
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
        writeOptionalTypeAnnotation(out, letStmt->typeName);
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

    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        out << "(break)";
        return;
    }

    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        out << "(continue)";
        return;
    }

    if (const auto* functionStmt = dynamic_cast<const FunctionStmt*>(&stmt)) {
        out << "(fun " << functionStmt->name.lexeme << ' ';
        writeParameterList(out, functionStmt->parameters);
        writeReturnAnnotation(out, functionStmt->returnTypeName);
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

TypeAnnotation TypeAnnotation::simple(Token token)
{
    TypeAnnotation result;
    result.kind = Kind::Simple;
    result.token = std::move(token);
    return result;
}

TypeAnnotation TypeAnnotation::function(Token token, std::vector<TypeAnnotation> parameterTypes, TypeAnnotation returnType)
{
    TypeAnnotation result;
    result.kind = Kind::Function;
    result.token = std::move(token);
    result.parameterTypes = std::move(parameterTypes);
    result.returnType = std::make_shared<TypeAnnotation>(std::move(returnType));
    return result;
}

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

StructExpr::StructExpr(std::vector<StructField> fields)
    : fields(std::move(fields))
{
}

void StructExpr::print(std::ostream& out) const
{
    out << "(struct";
    for (const StructField& field : fields) {
        out << ' ' << field.name.lexeme << ": ";
        writeExpr(out, field.value);
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

FieldAccessExpr::FieldAccessExpr(ExprPtr object, Token name)
    : object(std::move(object))
    , name(std::move(name))
{
}

void FieldAccessExpr::print(std::ostream& out) const
{
    out << "(field ";
    writeExpr(out, object);
    out << ' ' << name.lexeme << ')';
}

FieldAssignExpr::FieldAssignExpr(ExprPtr object, Token name, ExprPtr value)
    : object(std::move(object))
    , name(std::move(name))
    , value(std::move(value))
{
}

void FieldAssignExpr::print(std::ostream& out) const
{
    out << "(= (field ";
    writeExpr(out, object);
    out << ' ' << name.lexeme << ") ";
    writeExpr(out, value);
    out << ')';
}

FunctionExpr::FunctionExpr(Token keyword, std::vector<Parameter> parameters, std::optional<TypeAnnotation> returnTypeName, std::vector<StmtPtr> body)
    : keyword(std::move(keyword))
    , parameters(std::move(parameters))
    , returnTypeName(std::move(returnTypeName))
    , body(std::move(body))
{
}

void FunctionExpr::print(std::ostream& out) const
{
    out << "(fun ";
    writeParameterList(out, parameters);
    writeReturnAnnotation(out, returnTypeName);
    for (const auto& statement : body) {
        out << ' ';
        writeInlineStmt(out, *statement);
    }
    out << ')';
}

LetStmt::LetStmt(Token name, std::optional<TypeAnnotation> typeName, ExprPtr initializer)
    : name(std::move(name))
    , typeName(std::move(typeName))
    , initializer(std::move(initializer))
{
}

void LetStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Let " << name.lexeme;
    writeOptionalTypeAnnotation(out, typeName);
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

BreakStmt::BreakStmt(Token keyword)
    : keyword(std::move(keyword))
{
}

void BreakStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Break\n";
}

ContinueStmt::ContinueStmt(Token keyword)
    : keyword(std::move(keyword))
{
}

void ContinueStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Continue\n";
}

FunctionStmt::FunctionStmt(Token name, std::vector<Parameter> parameters, std::optional<TypeAnnotation> returnTypeName, std::vector<StmtPtr> body)
    : name(std::move(name))
    , parameters(std::move(parameters))
    , returnTypeName(std::move(returnTypeName))
    , body(std::move(body))
{
}

void FunctionStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Fun " << name.lexeme;
    writeParameterList(out, parameters);
    writeReturnAnnotation(out, returnTypeName);
    out << '\n';
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
