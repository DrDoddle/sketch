#include "string_literal.hpp"
#include "token.hpp"

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
    [[noreturn]] void string_error(const std::string &message, std::size_t offset)
    {
        throw std::runtime_error("String error: " + message + " at offset " +
                                 std::to_string(offset));
    }

    void validate_utf8(std::string_view bytes, const std::vector<std::size_t> &origins)
    {
        for (std::size_t i = 0; i < bytes.size();)
        {
            const auto first = static_cast<unsigned char>(bytes[i]);

            if (first < 0x80)
            {
                ++i;
                continue;
            }

            std::size_t width;
            std::uint32_t scalar;
            std::uint32_t minimum;

            if (first >= 0xC2 && first <= 0xDF)
            {
                width = 2;
                scalar = first & 0x1FU;
                minimum = 0x80;
            }
            else if (first >= 0xE0 && first <= 0xEF)
            {
                width = 3;
                scalar = first & 0x0FU;
                minimum = 0x800;
            }
            else if (first >= 0xF0 && first <= 0xF4)
            {
                width = 4;
                scalar = first & 0x07U;
                minimum = 0x10000;
            }
            else
            {
                string_error("Invalid UTF-8 leading byte", origins[i]);
            }

            if (bytes.size() - i < width)
            {
                string_error("Truncated UTF-8 sequence", origins[i]);
            }

            for (std::size_t j = 1; j < width; ++j)
            {
                const auto byte = static_cast<unsigned char>(bytes[i + j]);

                if ((byte & 0xC0U) != 0x80U)
                {
                    string_error("Invalid UTF-8 continuation byte", origins[i + j]);
                }

                scalar = (scalar << 6U) | (byte & 0x3FU);
            }

            if (scalar < minimum || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
            {
                string_error("Invalid UTF-8 scalar encoding", origins[i]);
            }

            i += width;
        }
    }
} // namespace

std::string decode_string_literal(const Token &token)
{
    const auto text = token.text;

    if (text.size() < 2 || text.front() != '"' || text.back() != '"')
    {
        string_error("Expected a quoted string", token.span.offset);
    }

    std::string decoded;
    std::vector<std::size_t> origins;
    decoded.reserve(text.size() - 2);
    origins.reserve(text.size() - 2);

    const std::size_t end = text.size() - 1;

    for (std::size_t i = 1; i < end; ++i)
    {
        const std::size_t origin = token.span.offset + i;
        char ch = text[i];

        if (ch == '\n' || ch == '\r')
        {
            string_error("Unescaped newline in string literal", origin);
        }

        if (ch == '\\')
        {
            if (i + 1 >= end)
            {
                string_error("Unfinished escape sequence", origin);
            }

            switch (text[++i])
            {
                case '"':
                    ch = '"';
                    break;
                case '\\':
                    ch = '\\';
                    break;
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                case 't':
                    ch = '\t';
                    break;
                case '0':
                    ch = '\0';
                    break;
                default:
                    string_error("Unknown escape sequence", origin);
            }
        }

        decoded.push_back(ch);
        origins.push_back(origin);
    }

    validate_utf8(decoded, origins);
    return decoded;
}
