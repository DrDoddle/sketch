#include <cstddef>
#include <string_view>

enum class TokenKind
{
    End,
    Identifier,
    IntegerLiteral,

    Fn,
    Int,
    Bool,

    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,

    Colon,
    Semicolon,
    Comma,

    ColonEqual,
    Equal,


    EqualEqual,
    Greater,
    Less,
    LessEqual,
    GreaterEqual,
    Bang,
    BangEqual,

    Plus,
    Minus,
    Star,
    Slash,

    True,
    False,

    If,
    Else,
    While
};

struct SourceSpan
{
    std::size_t offset;
    std::size_t length;
};

struct Token
{
    TokenKind kind;
    std::string_view text;
    SourceSpan span;
};
