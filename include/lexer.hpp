#pragma once

#include <string_view>

#include "token.hpp"

class Lexer
{
public:
    explicit Lexer(std::string_view source);

    Token next();

private:
    std::string_view m_source;
    std::size_t m_position = 0;

    bool at_end() const;

    char peek() const;

    char advance();

    Token make_token(TokenKind kind, std::size_t start) const;
};
