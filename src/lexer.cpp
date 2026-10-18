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

    if (ch == '"')
    {
        while (!at_end())
        {
            const char current = advance();

            if (current == '"')
            {
                return make_token(TokenKind::StringLiteral, start);
            }

            if (current == '\n' || current == '\r')
            {
                throw std::runtime_error("Unescaped newline in string at offset " +
                                         std::to_string(m_position - 1));
            }

            if (current == '\\')
            {
                if (at_end())
                {
                    break;
                }

                if (peek() == '\n' || peek() == '\r')
                {
                    throw std::runtime_error("Newline in string escape at offset " +
                                             std::to_string(m_position));
                }

                advance();
            }
        }

        throw std::runtime_error("Unterminated string literal at offset " + std::to_string(start));
    }

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

        if (word == "while")
        {
            return make_token(TokenKind::While, start);
        }

        if (word == "if")
        {
            return make_token(TokenKind::If, start);
        }

        if (word == "else")
        {
            return make_token(TokenKind::Else, start);
        }

        if (word == "void")
        {
            return make_token(TokenKind::Void, start);
        }

        if (word == "int")
        {
            return make_token(TokenKind::Int, start);
        }

        if (word == "string")
        {
            return make_token(TokenKind::String, start);
        }

        if (word == "bool")
        {
            return make_token(TokenKind::Bool, start);
        }

        if (word == "true")
        {
            return make_token(TokenKind::True, start);
        }

        if (word == "false")
        {
            return make_token(TokenKind::False, start);
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
        {
            if (!at_end() && peek() == '=')
            {
                advance();
                return make_token(TokenKind::ColonEqual, start);
            }

            return make_token(TokenKind::Colon, start);
        }

        case '=':
        {
            if (!at_end() && peek() == '=')
            {
                advance();
                return make_token(TokenKind::EqualEqual, start);
            }

            return make_token(TokenKind::Equal, start);
        }

        case '<':
        {
            if (!at_end() && peek() == '=')
            {
                advance();
                return make_token(TokenKind::LessEqual, start);
            }

            return make_token(TokenKind::Less, start);
        }

        case '>':
        {
            if (!at_end() && peek() == '=')
            {
                advance();
                return make_token(TokenKind::GreaterEqual, start);
            }

            return make_token(TokenKind::Greater, start);
        }
        case '!':
        {
            if (!at_end() && peek() == '=')
            {
                advance();
                return make_token(TokenKind::BangEqual, start);
            }

            return make_token(TokenKind::Bang, start);
        }
        case ';':
            return make_token(TokenKind::Semicolon, start);
        case ',':
            return make_token(TokenKind::Comma, start);
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
