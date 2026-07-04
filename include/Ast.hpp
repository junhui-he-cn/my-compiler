#pragma once

#include "Token.hpp"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

// Base class for all expression nodes. Each node knows how to print itself
// in a compact prefix representation used by this demo front-end.
struct Expr {
    virtual ~Expr() = default;
    virtual void print(std::ostream& out) const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

struct LiteralExpr final : Expr {
    explicit LiteralExpr(std::string value);
    void print(std::ostream& out) const override;

    std::string value;
};

struct VariableExpr final : Expr {
    explicit VariableExpr(Token name);
    void print(std::ostream& out) const override;

    Token name;
};

struct AssignExpr final : Expr {
    AssignExpr(Token name, ExprPtr value);
    void print(std::ostream& out) const override;

    Token name;
    ExprPtr value;
};

struct IndexAssignExpr final : Expr {
    IndexAssignExpr(ExprPtr collection, Token bracket, ExprPtr index, ExprPtr value);
    void print(std::ostream& out) const override;

    ExprPtr collection;
    Token bracket;
    ExprPtr index;
    ExprPtr value;
};

struct UnaryExpr final : Expr {
    UnaryExpr(Token op, ExprPtr right);
    void print(std::ostream& out) const override;

    Token op;
    ExprPtr right;
};

struct BinaryExpr final : Expr {
    BinaryExpr(ExprPtr left, Token op, ExprPtr right);
    void print(std::ostream& out) const override;

    ExprPtr left;
    Token op;
    ExprPtr right;
};

struct LogicalExpr final : Expr {
    LogicalExpr(ExprPtr left, Token op, ExprPtr right);
    void print(std::ostream& out) const override;

    ExprPtr left;
    Token op;
    ExprPtr right;
};

struct GroupingExpr final : Expr {
    explicit GroupingExpr(ExprPtr expression);
    void print(std::ostream& out) const override;

    ExprPtr expression;
};

struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, Token paren, std::vector<ExprPtr> arguments);
    void print(std::ostream& out) const override;

    ExprPtr callee;
    Token paren;
    std::vector<ExprPtr> arguments;
};

struct ArrayExpr final : Expr {
    explicit ArrayExpr(std::vector<ExprPtr> elements);
    void print(std::ostream& out) const override;

    std::vector<ExprPtr> elements;
};

struct IndexExpr final : Expr {
    IndexExpr(ExprPtr collection, Token bracket, ExprPtr index);
    void print(std::ostream& out) const override;

    ExprPtr collection;
    Token bracket;
    ExprPtr index;
};

struct FunctionExpr final : Expr {
    FunctionExpr(Token keyword, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out) const override;

    Token keyword;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};

struct Stmt {
    virtual ~Stmt() = default;
    virtual void print(std::ostream& out, int indent) const = 0;
};

struct LetStmt final : Stmt {
    LetStmt(Token name, std::optional<Token> typeName, ExprPtr initializer);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::optional<Token> typeName;
    ExprPtr initializer;
};

struct PrintStmt final : Stmt {
    explicit PrintStmt(ExprPtr expression);
    void print(std::ostream& out, int indent) const override;

    ExprPtr expression;
};

struct ExpressionStmt final : Stmt {
    explicit ExpressionStmt(ExprPtr expression);
    void print(std::ostream& out, int indent) const override;

    ExprPtr expression;
};

struct BlockStmt final : Stmt {
    explicit BlockStmt(std::vector<StmtPtr> statements);
    void print(std::ostream& out, int indent) const override;

    std::vector<StmtPtr> statements;
};

struct IfStmt final : Stmt {
    IfStmt(ExprPtr condition, StmtPtr thenBranch, StmtPtr elseBranch);
    void print(std::ostream& out, int indent) const override;

    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
};

struct WhileStmt final : Stmt {
    WhileStmt(ExprPtr condition, StmtPtr body);
    void print(std::ostream& out, int indent) const override;

    ExprPtr condition;
    StmtPtr body;
};

struct BreakStmt final : Stmt {
    explicit BreakStmt(Token keyword);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
};

struct ContinueStmt final : Stmt {
    explicit ContinueStmt(Token keyword);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
};

struct FunctionStmt final : Stmt {
    FunctionStmt(Token name, std::vector<Token> parameters, std::vector<StmtPtr> body);
    void print(std::ostream& out, int indent) const override;

    Token name;
    std::vector<Token> parameters;
    std::vector<StmtPtr> body;
};

struct ReturnStmt final : Stmt {
    ReturnStmt(Token keyword, ExprPtr value);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
    ExprPtr value;
};

struct Program {
    std::vector<StmtPtr> statements;

    // Emit a readable tree view of the parsed program.
    void print(std::ostream& out) const;
};
