#include "parser.hpp"
#include <stdexcept>
#include <utility>

Program::Program(std::vector<std::unique_ptr<AST>> groups) : function_groups(std::move(groups))
{
}

Parameter::Parameter(std::string n, std::string t) : name(std::move(n)), param_type(std::move(t))
{
}

Block::Block(std::unique_ptr<AST> expr) : expression(std::move(expr))
{
}

FunctionVariant::FunctionVariant(std::vector<std::unique_ptr<Parameter>> params,
                                 std::string ret_type, std::unique_ptr<Block> b)
    : parameters(std::move(params)), return_type(std::move(ret_type)), body(std::move(b))
{
}

FunctionGroup::FunctionGroup(std::string n, std::vector<std::unique_ptr<FunctionVariant>> vars)
    : name(std::move(n)), variants(std::move(vars))
{
}

IntegerLiteral::IntegerLiteral(int val) : value(val)
{
}

Identifier::Identifier(std::string n) : name(std::move(n))
{
}

Argument::Argument(std::string n, std::unique_ptr<AST> expr)
    : name(std::move(n)), expression(std::move(expr))
{
}

Call::Call(std::string n, std::vector<std::unique_ptr<Argument>> args)
    : name(std::move(n)), arguments(std::move(args))
{
}

AddExpression::AddExpression(std::unique_ptr<AST> l, std::unique_ptr<AST> r)
    : left(std::move(l)), right(std::move(r))
{
}

Parser::Parser(Lexer &lexer) : m_lexer(lexer)
{
    m_current_token = m_lexer.next();
    m_next_token = m_lexer.next();
}

std::unique_ptr<Program> Parser::parse()
{
    std::vector<std::unique_ptr<AST>> function_groups;
    while (m_current_token.kind != TokenKind::End)
    {
        function_groups.push_back(parse_function_group());
    }
    return std::make_unique<Program>(std::move(function_groups));
}

void Parser::advance()
{
    m_current_token = m_next_token;
    m_next_token = m_lexer.next();
}

TokenKind Parser::peek_kind() const
{
    return m_next_token.kind;
}

[[noreturn]] void Parser::error(const std::string &message)
{
    throw std::runtime_error("Parser Error: " + message + " at offset " +
                             std::to_string(m_current_token.span.offset));
}

void Parser::eat(TokenKind kind)
{
    if (m_current_token.kind != kind)
    {
        error("Expected token kind " + std::to_string(static_cast<int>(kind)) + ", got '" +
              std::string(m_current_token.text) + "'");
    }

    advance();
}

std::unique_ptr<FunctionGroup> Parser::parse_function_group()
{
    eat(TokenKind::Fn);
    std::string name(m_current_token.text);
    eat(TokenKind::Identifier);
    eat(TokenKind::LeftBrace);

    std::vector<std::unique_ptr<FunctionVariant>> variants;
    variants.push_back(parse_function_variant());

    while (m_current_token.kind == TokenKind::Greater)
    {
        variants.push_back(parse_function_variant());
    }

    eat(TokenKind::RightBrace);
    return std::make_unique<FunctionGroup>(name, std::move(variants));
}

std::unique_ptr<FunctionVariant> Parser::parse_function_variant()
{
    eat(TokenKind::Greater);
    eat(TokenKind::LeftParen);

    std::vector<std::unique_ptr<Parameter>> parameters;
    if (m_current_token.kind != TokenKind::RightParen)
    {
        parameters = parse_parameters();
    }

    eat(TokenKind::RightParen);
    eat(TokenKind::Colon);

    std::string return_type(m_current_token.text);
    eat(TokenKind::Int);

    auto block = parse_block();
    eat(TokenKind::Semicolon);

    return std::make_unique<FunctionVariant>(std::move(parameters), return_type, std::move(block));
}

std::vector<std::unique_ptr<Parameter>> Parser::parse_parameters()
{
    std::vector<std::unique_ptr<Parameter>> params;
    params.push_back(parse_parameter());

    while (m_current_token.kind == TokenKind::Comma)
    {
        eat(TokenKind::Comma);
        params.push_back(parse_parameter());
    }
    return params;
}

std::unique_ptr<Parameter> Parser::parse_parameter()
{
    std::string name(m_current_token.text);
    eat(TokenKind::Identifier);
    eat(TokenKind::Colon);
    std::string type(m_current_token.text);
    eat(TokenKind::Int);
    return std::make_unique<Parameter>(name, type);
}

std::unique_ptr<Block> Parser::parse_block()
{
    eat(TokenKind::LeftBrace);
    auto expr = parse_expression();
    eat(TokenKind::RightBrace);
    return std::make_unique<Block>(std::move(expr));
}

std::unique_ptr<AST> Parser::parse_expression()
{
    return parse_addition();
}

std::unique_ptr<AST> Parser::parse_addition()
{
    auto node = parse_primary();
    while (m_current_token.kind == TokenKind::Plus)
    {
        eat(TokenKind::Plus);
        auto right = parse_primary();
        node = std::make_unique<AddExpression>(std::move(node), std::move(right));
    }
    return node;
}

std::unique_ptr<AST> Parser::parse_primary()
{
    if (m_current_token.kind == TokenKind::IntegerLiteral)
    {
        int val = std::stoi(std::string(m_current_token.text));
        eat(TokenKind::IntegerLiteral);
        return std::make_unique<IntegerLiteral>(val);
    }
    else if (m_current_token.kind == TokenKind::Identifier)
    {
        // Disambiguate between a variable identifier and a function call
        if (peek_kind() == TokenKind::LeftParen)
        {
            return parse_call();
        }
        else
        {
            std::string name(m_current_token.text);
            eat(TokenKind::Identifier);
            return std::make_unique<Identifier>(name);
        }
    }
    else if (m_current_token.kind == TokenKind::LeftParen)
    {
        eat(TokenKind::LeftParen);
        auto expr = parse_expression();
        eat(TokenKind::RightParen);
        return expr;
    }
    else
    {
        error("Unexpected primary token.");
        return nullptr;
    }
}

std::unique_ptr<Call> Parser::parse_call()
{
    std::string name(m_current_token.text);
    eat(TokenKind::Identifier);
    eat(TokenKind::LeftParen);

    std::vector<std::unique_ptr<Argument>> arguments;
    if (m_current_token.kind != TokenKind::RightParen)
    {
        arguments = parse_arguments();
    }

    eat(TokenKind::RightParen);
    return std::make_unique<Call>(name, std::move(arguments));
}

std::vector<std::unique_ptr<Argument>> Parser::parse_arguments()
{
    std::vector<std::unique_ptr<Argument>> args;
    args.push_back(parse_argument());

    while (m_current_token.kind == TokenKind::Comma)
    {
        eat(TokenKind::Comma);
        args.push_back(parse_argument());
    }
    return args;
}

std::unique_ptr<Argument> Parser::parse_argument()
{
    std::string name(m_current_token.text);
    eat(TokenKind::Identifier);
    eat(TokenKind::Colon);
    auto expr = parse_expression();
    return std::make_unique<Argument>(name, std::move(expr));
}