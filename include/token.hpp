#include <cstddef>
#include <string_view>

enum class TokenKind {
    End,
    Identifier,
    IntegerLiteral,

    Fn,
    Int,

    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,

    Colon,
    Semicolon,
    Comma,
    Greater,
    Plus
};

struct SourceSpan {
    std::size_t offset;
    std::size_t length;
};

struct Token {
    TokenKind kind;
    std::string_view text;
    SourceSpan span;
};
