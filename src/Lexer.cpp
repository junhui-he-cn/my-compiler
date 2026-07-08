#include "Diagnostic.hpp"
#include "Lexer.hpp"

#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace {

bool isAlpha(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isAlphaNumeric(char c)
{
    return isAlpha(c) || std::isdigit(static_cast<unsigned char>(c));
}

} // namespace

Lexer::Lexer(std::string source)
    : source_(std::move(source))
{
}

std::vector<Token> Lexer::scanTokens()
{
    while (!isAtEnd()) {
        start_ = current_;
        tokenColumn_ = column_;
        scanToken();
    }

    tokens_.push_back(Token{TokenType::EndOfFile, "", line_, column_});
    return tokens_;
}

bool Lexer::isAtEnd() const
{
    return current_ >= source_.size();
}

char Lexer::advance()
{
    char c = source_[current_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match(char expected)
{
    if (isAtEnd() || source_[current_] != expected) {
        return false;
    }
    advance();
    return true;
}

char Lexer::peek() const
{
    if (isAtEnd()) {
        return '\0';
    }
    return source_[current_];
}

char Lexer::peekNext() const
{
    if (current_ + 1 >= source_.size()) {
        return '\0';
    }
    return source_[current_ + 1];
}

void Lexer::scanToken()
{
    const char c = advance();
    switch (c) {
    case '(':
        addToken(TokenType::LeftParen);
        break;
    case ')':
        addToken(TokenType::RightParen);
        break;
    case '[':
        addToken(TokenType::LeftBracket);
        break;
    case ']':
        addToken(TokenType::RightBracket);
        break;
    case '{':
        addToken(TokenType::LeftBrace);
        break;
    case '}':
        addToken(TokenType::RightBrace);
        break;
    case ':':
        addToken(TokenType::Colon);
        break;
    case ';':
        addToken(TokenType::Semicolon);
        break;
    case ',':
        addToken(TokenType::Comma);
        break;
    case '.':
        addToken(TokenType::Dot);
        break;
    case '+':
        addToken(TokenType::Plus);
        break;
    case '-':
        addToken(TokenType::Minus);
        break;
    case '*':
        addToken(TokenType::Star);
        break;
    case '/':
        if (match('/')) {
            // Line comments are skipped entirely; the following newline is
            // consumed by the normal whitespace branch on the next iteration.
            while (peek() != '\n' && !isAtEnd()) {
                advance();
            }
        } else {
            addToken(TokenType::Slash);
        }
        break;
    case '!':
        addToken(match('=') ? TokenType::BangEqual : TokenType::Bang);
        break;
    case '=':
        addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);
        break;
    case '<':
        addToken(match('=') ? TokenType::LessEqual : TokenType::Less);
        break;
    case '>':
        addToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater);
        break;
    case '&':
        if (match('&')) {
            addToken(TokenType::AmpersandAmpersand);
        } else {
            throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
                "unexpected character `&`");
        }
        break;
    case '|':
        if (match('|')) {
            addToken(TokenType::PipePipe);
        } else {
            throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
                "unexpected character `|`");
        }
        break;
    case '"':
        stringLiteral();
        break;
    case ' ':
    case '\r':
    case '\t':
    case '\n':
        break;
    default:
        if (std::isdigit(static_cast<unsigned char>(c))) {
            numberLiteral();
        } else if (isAlpha(c)) {
            identifier();
        } else {
            throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
                "unexpected character `" + std::string(1, c) + "`");
        }
        break;
    }
}

void Lexer::addToken(TokenType type)
{
    tokens_.push_back(Token{
        type,
        source_.substr(start_, current_ - start_),
        line_,
        tokenColumn_,
    });
}

void Lexer::stringLiteral()
{
    while (peek() != '"' && !isAtEnd()) {
        advance();
    }

    if (isAtEnd()) {
        throw DiagnosticError(DiagnosticKind::Lex, SourceLocation{line_, tokenColumn_},
            "unterminated string");
    }

    advance();
    addToken(TokenType::String);
}

void Lexer::numberLiteral()
{
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    // Accept a fractional part only when at least one digit follows the dot,
    // so `1.` can be diagnosed by the parser as a separate token sequence.
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    addToken(TokenType::Number);
}

void Lexer::identifier()
{
    // Keyword lookup happens after the full identifier has been consumed,
    // which lets names such as `printable` remain identifiers.
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"break", TokenType::Break},
        {"continue", TokenType::Continue},
        {"if", TokenType::If},
        {"import", TokenType::Import},
        {"as", TokenType::As},
        {"export", TokenType::Export},
        {"else", TokenType::Else},
        {"fun", TokenType::Fun},
        {"let", TokenType::Let},
        {"print", TokenType::Print},
        {"return", TokenType::Return},
        {"struct", TokenType::Struct},
        {"while", TokenType::While},
        {"true", TokenType::True},
        {"false", TokenType::False},
        {"nil", TokenType::Nil},
    };

    while (isAlphaNumeric(peek())) {
        advance();
    }

    const std::string text = source_.substr(start_, current_ - start_);
    const auto found = keywords.find(text);
    addToken(found == keywords.end() ? TokenType::Identifier : found->second);
}

std::string tokenTypeName(TokenType type)
{
    switch (type) {
    case TokenType::LeftParen:
        return "LeftParen";
    case TokenType::RightParen:
        return "RightParen";
    case TokenType::LeftBracket:
        return "LeftBracket";
    case TokenType::RightBracket:
        return "RightBracket";
    case TokenType::LeftBrace:
        return "LeftBrace";
    case TokenType::RightBrace:
        return "RightBrace";
    case TokenType::Colon:
        return "Colon";
    case TokenType::Semicolon:
        return "Semicolon";
    case TokenType::Comma:
        return "Comma";
    case TokenType::Dot:
        return "Dot";
    case TokenType::Plus:
        return "Plus";
    case TokenType::Minus:
        return "Minus";
    case TokenType::Star:
        return "Star";
    case TokenType::Slash:
        return "Slash";
    case TokenType::Bang:
        return "Bang";
    case TokenType::BangEqual:
        return "BangEqual";
    case TokenType::Equal:
        return "Equal";
    case TokenType::EqualEqual:
        return "EqualEqual";
    case TokenType::Less:
        return "Less";
    case TokenType::LessEqual:
        return "LessEqual";
    case TokenType::Greater:
        return "Greater";
    case TokenType::GreaterEqual:
        return "GreaterEqual";
    case TokenType::AmpersandAmpersand:
        return "AmpersandAmpersand";
    case TokenType::PipePipe:
        return "PipePipe";
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::Number:
        return "Number";
    case TokenType::String:
        return "String";
    case TokenType::Break:
        return "Break";
    case TokenType::Continue:
        return "Continue";
    case TokenType::If:
        return "If";
    case TokenType::Import:
        return "Import";
    case TokenType::As:
        return "As";
    case TokenType::Export:
        return "Export";
    case TokenType::Else:
        return "Else";
    case TokenType::Fun:
        return "Fun";
    case TokenType::Let:
        return "Let";
    case TokenType::Print:
        return "Print";
    case TokenType::Return:
        return "Return";
    case TokenType::Struct:
        return "Struct";
    case TokenType::While:
        return "While";
    case TokenType::True:
        return "True";
    case TokenType::False:
        return "False";
    case TokenType::Nil:
        return "Nil";
    case TokenType::EndOfFile:
        return "EndOfFile";
    }

    return "Unknown";
}

std::ostream& operator<<(std::ostream& out, const Token& token)
{
    out << token.line << ':' << token.column << " "
        << tokenTypeName(token.type);
    if (!token.lexeme.empty()) {
        out << " `" << token.lexeme << "`";
    }
    return out;
}
