#include "lexer.hpp"

#include <cctype>
#include <stdexcept>
#include <string>

Lexer::Lexer(std::string_view source) : m_source(source)
{
}

Token Lexer::next()
{
    // Skip whitespace and line comments.
    for (;;)
    {
        while (!at_end() && std::isspace(static_cast<unsigned char>(peek())))
        {
            advance();
        }

        if (m_position + 1 < m_source.size() && m_source[m_position] == '/' &&
            m_source[m_position + 1] == '/')
        {
            while (!at_end() && peek() != '\n')
            {
                advance();
            }

            continue;
        }

        break;
    }

    const std::size_t start = m_position;

    if (at_end())
    {
        return make_token(TokenKind::End, start);
    }

    const char ch = advance();

    // Identifiers and keywords.
    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_')
    {
        while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
        {
            advance();
        }

        const std::string_view word = m_source.substr(start, m_position - start);

        if (word == "fn")
        {
            return make_token(TokenKind::Fn, start);
        }

        if (word == "int")
        {
            return make_token(TokenKind::Int, start);
        }

        return make_token(TokenKind::Identifier, start);
    }

    // Integer literals.
    if (std::isdigit(static_cast<unsigned char>(ch)))
    {
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek())))
        {
            advance();
        }

        return make_token(TokenKind::IntegerLiteral, start);
    }

    // Punctuation and operators.
    switch (ch)
    {
        case '(':
            return make_token(TokenKind::LeftParen, start);
        case ')':
            return make_token(TokenKind::RightParen, start);
        case '{':
            return make_token(TokenKind::LeftBrace, start);
        case '}':
            return make_token(TokenKind::RightBrace, start);
        case ':':
            return make_token(TokenKind::Colon, start);
        case ';':
            return make_token(TokenKind::Semicolon, start);
        case ',':
            return make_token(TokenKind::Comma, start);
        case '>':
            return make_token(TokenKind::Greater, start);
        case '+':
            return make_token(TokenKind::Plus, start);
        case '-':
            return make_token(TokenKind::Minus, start);
        case '*':
            return make_token(TokenKind::Star, start);
        case '/':
            return make_token(TokenKind::Slash, start);

        default:
            throw std::runtime_error("Unexpected character '" + std::string(1, ch) +
                                     "' at offset " + std::to_string(start));
    }
}

bool Lexer::at_end() const
{
    return m_position >= m_source.size();
}

char Lexer::peek() const
{
    return at_end() ? '\0' : m_source[m_position];
}

char Lexer::advance()
{
    return m_source[m_position++];
}

Token Lexer::make_token(TokenKind kind, std::size_t start) const
{
    const std::size_t length = m_position - start;

    return Token{kind, m_source.substr(start, length), SourceSpan{start, length}};
}
