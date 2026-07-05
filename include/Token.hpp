#pragma once

#include <ostream>
#include <string>

// TokenType describes every syntactic atom the lexer can emit.
enum class TokenType {
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Colon,
    Semicolon,
    Comma,
    Dot,

    Plus,
    Minus,
    Star,
    Slash,
    Bang,
    BangEqual,
    Equal,
    EqualEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    AmpersandAmpersand,
    PipePipe,

    Identifier,
    Number,
    String,

    Break,
    Continue,
    If,
    Else,
    Fun,
    Let,
    Print,
    Return,
    While,
    True,
    False,
    Nil,

    EndOfFile,
};

struct Token {
    TokenType type;
    std::string lexeme;
    // 1-based source location for diagnostics.
    int line;
    int column;
};

std::string tokenTypeName(TokenType type);
std::ostream& operator<<(std::ostream& out, const Token& token);
