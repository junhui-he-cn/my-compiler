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

    if (annotation.kind == TypeAnnotation::Kind::Qualified) {
        out << annotation.qualifier.lexeme << '.' << annotation.token.lexeme;
        return;
    }

    if (annotation.kind == TypeAnnotation::Kind::Array) {
        out << '[';
        writeTypeAnnotation(out, *annotation.elementType);
        out << ']';
        return;
    }

    if (annotation.kind == TypeAnnotation::Kind::Nullable) {
        writeTypeAnnotation(out, *annotation.innerType);
        out << '?';
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
    if (const auto* module = dynamic_cast<const ModuleStmt*>(&stmt)) {
        out << "(module " << module->moduleId;
        for (const auto& child : module->statements) {
            out << ' ';
            writeInlineStmt(out, *child);
        }
        out << ')';
        return;
    }

    if (const auto* import = dynamic_cast<const ImportStmt*>(&stmt)) {
        out << "(import " << import->path.lexeme;
        if (import->alias) {
            out << " as " << import->alias->lexeme;
        }
        out << ')';
        return;
    }

    if (const auto* exportStmt = dynamic_cast<const ExportStmt*>(&stmt)) {
        out << "(export";
        for (const auto& name : exportStmt->names) {
            out << ' ' << name.lexeme;
        }
        out << ')';
        return;
    }

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

    if (const auto* forStmt = dynamic_cast<const ForStmt*>(&stmt)) {
        out << "(for ";
        if (forStmt->initializer) {
            writeInlineStmt(out, *forStmt->initializer);
        } else {
            out << "nil";
        }
        out << ' ';
        writeExpr(out, forStmt->condition);
        out << ' ';
        writeExpr(out, forStmt->increment);
        out << ' ';
        writeInlineStmt(out, *forStmt->body);
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

    if (const auto* implStmt = dynamic_cast<const ImplStmt*>(&stmt)) {
        out << "(impl " << implStmt->typeName.lexeme;
        for (const MethodDecl& method : implStmt->methods) {
            out << " (method " << method.name.lexeme;
            writeParameterList(out, method.parameters);
            writeReturnAnnotation(out, method.returnTypeName);
            for (const auto& child : method.body) {
                out << ' ';
                writeInlineStmt(out, *child);
            }
            out << ')';
        }
        out << ')';
        return;
    }

    if (const auto* structDecl = dynamic_cast<const StructDeclStmt*>(&stmt)) {
        out << "(struct " << structDecl->name.lexeme;
        for (const StructFieldDecl& field : structDecl->fields) {
            out << ' ' << field.name.lexeme << ": ";
            writeTypeAnnotation(out, field.typeName);
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

TypeAnnotation TypeAnnotation::qualified(Token qualifier, Token name)
{
    TypeAnnotation result;
    result.kind = Kind::Qualified;
    result.qualifier = std::move(qualifier);
    result.token = std::move(name);
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

TypeAnnotation TypeAnnotation::array(Token token, TypeAnnotation elementType)
{
    TypeAnnotation result;
    result.kind = Kind::Array;
    result.token = std::move(token);
    result.elementType = std::make_shared<TypeAnnotation>(std::move(elementType));
    return result;
}

TypeAnnotation TypeAnnotation::nullable(Token token, TypeAnnotation innerType)
{
    TypeAnnotation result;
    result.kind = Kind::Nullable;
    result.token = std::move(token);
    result.innerType = std::make_shared<TypeAnnotation>(std::move(innerType));
    return result;
}

MethodDecl::MethodDecl(Token name, std::vector<Parameter> parameters, std::optional<TypeAnnotation> returnTypeName, std::vector<StmtPtr> body)
    : name(std::move(name))
    , parameters(std::move(parameters))
    , returnTypeName(std::move(returnTypeName))
    , body(std::move(body))
{
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

CompoundAssignExpr::CompoundAssignExpr(Token name, Token op, ExprPtr value)
    : name(std::move(name))
    , op(std::move(op))
    , value(std::move(value))
{
}

void CompoundAssignExpr::print(std::ostream& out) const
{
    out << '(' << op.lexeme << ' ' << name.lexeme << ' ';
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

MemberCallExpr::MemberCallExpr(ExprPtr receiver, Token name, Token paren, std::vector<ExprPtr> arguments)
    : receiver(std::move(receiver))
    , name(std::move(name))
    , paren(std::move(paren))
    , arguments(std::move(arguments))
{
}

void MemberCallExpr::print(std::ostream& out) const
{
    out << "(member-call ";
    writeExpr(out, receiver);
    out << ' ' << name.lexeme;
    for (const auto& argument : arguments) {
        out << ' ';
        writeExpr(out, argument);
    }
    out << ')';
}

ArrayExpr::ArrayExpr(Token bracket, std::vector<ExprPtr> elements)
    : bracket(std::move(bracket))
    , elements(std::move(elements))
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

StructConstructExpr::StructConstructExpr(std::optional<Token> qualifier, Token name, std::vector<StructField> fields)
    : qualifier(std::move(qualifier))
    , name(std::move(name))
    , fields(std::move(fields))
{
}

void StructConstructExpr::print(std::ostream& out) const
{
    out << "(construct ";
    if (qualifier) {
        out << qualifier->lexeme << '.';
    }
    out << name.lexeme;
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

StructDeclStmt::StructDeclStmt(Token name, std::vector<StructFieldDecl> fields)
    : name(std::move(name))
    , fields(std::move(fields))
{
}

void StructDeclStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Struct " << name.lexeme << " {";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << fields[i].name.lexeme << ": ";
        writeTypeAnnotation(out, fields[i].typeName);
    }
    out << "}\n";
}

ImplStmt::ImplStmt(Token typeName, std::vector<MethodDecl> methods)
    : typeName(std::move(typeName))
    , methods(std::move(methods))
{
}

void ImplStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Impl " << typeName.lexeme << '\n';
    for (const MethodDecl& method : methods) {
        writeIndent(out, indent + 1);
        out << "Method " << method.name.lexeme;
        writeParameterList(out, method.parameters);
        writeReturnAnnotation(out, method.returnTypeName);
        out << '\n';
        for (const auto& statement : method.body) {
            statement->print(out, indent + 2);
        }
    }
}

ImportStmt::ImportStmt(Token keyword, Token path, std::optional<Token> alias)
    : keyword(std::move(keyword))
    , path(std::move(path))
    , alias(std::move(alias))
{
}

void ImportStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Import " << path.lexeme;
    if (alias) {
        out << " as " << alias->lexeme;
    }
    out << "\n";
}

ExportStmt::ExportStmt(Token keyword, std::vector<Token> names)
    : keyword(std::move(keyword))
    , names(std::move(names))
{
}

void ExportStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Export";
    for (const auto& name : names) {
        out << ' ' << name.lexeme;
    }
    out << "\n";
}

ModuleStmt::ModuleStmt(std::size_t moduleId, std::string path, std::string source, std::vector<StmtPtr> statements, bool isEntry)
    : moduleId(moduleId)
    , path(std::move(path))
    , source(std::move(source))
    , statements(std::move(statements))
    , isEntry(isEntry)
{
}

void ModuleStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Module " << moduleId << (isEntry ? " entry" : "") << "\n";
    for (const auto& statement : statements) {
        statement->print(out, indent + 1);
    }
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

ForStmt::ForStmt(Token keyword, StmtPtr initializer, ExprPtr condition, ExprPtr increment, StmtPtr body)
    : keyword(std::move(keyword))
    , initializer(std::move(initializer))
    , condition(std::move(condition))
    , increment(std::move(increment))
    , body(std::move(body))
{
}

void ForStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "For\n";

    writeIndent(out, indent + 1);
    out << "Initializer\n";
    if (initializer) {
        initializer->print(out, indent + 2);
    } else {
        writeIndent(out, indent + 2);
        out << "nil\n";
    }

    writeIndent(out, indent + 1);
    out << "Condition ";
    writeExpr(out, condition);
    out << "\n";

    writeIndent(out, indent + 1);
    out << "Increment ";
    writeExpr(out, increment);
    out << "\n";

    writeIndent(out, indent + 1);
    out << "Body\n";
    if (body) {
        body->print(out, indent + 2);
    }
}

ForInStmt::ForInStmt(Token keyword, Token variable, ExprPtr iterable, StmtPtr body)
    : keyword(std::move(keyword))
    , variable(std::move(variable))
    , iterable(std::move(iterable))
    , body(std::move(body))
{
}

void ForInStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "ForIn " << variable.lexeme << " in ";
    writeExpr(out, iterable);
    out << "\n";
    if (body) {
        body->print(out, indent + 1);
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
