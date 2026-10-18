#include "parser.hpp"
#include "string_literal.hpp"
#include <stdexcept>
#include <utility>

Program::Program(std::vector<std::unique_ptr<AST>> groups) : function_groups(std::move(groups))
{
}

Parameter::Parameter(std::string n, std::string t) : name(std::move(n)), param_type(std::move(t))
{
}

Block::Block(std::vector<std::unique_ptr<AST>> stmts, std::unique_ptr<AST> expr)
    : statements(std::move(stmts)), expression(std::move(expr))
{
}

WhileStatement::WhileStatement(std::unique_ptr<AST> cond, std::unique_ptr<Block> block)
    : condition(std::move(cond)), body(std::move(block))
{
}

IfExpression::IfExpression(std::unique_ptr<AST> cond, std::unique_ptr<Block> then_block,
                           std::unique_ptr<Block> else_block)
    : condition(std::move(cond)), then_branch(std::move(then_block)),
      else_branch(std::move(else_block))
{
}

VariableDeclaration::VariableDeclaration(std::string n, std::optional<std::string> type,
                                         std::unique_ptr<AST> init)
    : name(std::move(n)), declared_type(std::move(type)), initializer(std::move(init))
{
}

Assignment::Assignment(std::string n, std::unique_ptr<AST> expr)
    : name(std::move(n)), value(std::move(expr))
{
}

ExpressionStatement::ExpressionStatement(std::unique_ptr<AST> expr) : expression(std::move(expr))
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

BooleanLiteral::BooleanLiteral(bool val) : value(val)
{
}

StringLiteral::StringLiteral(std::string bytes) : value(std::move(bytes))
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

BinaryExpression::BinaryExpression(TokenKind operation, std::unique_ptr<AST> l,
                                   std::unique_ptr<AST> r)
    : op(operation), left(std::move(l)), right(std::move(r))
{
}

UnaryExpression::UnaryExpression(TokenKind operation, std::unique_ptr<AST> expr)
    : op(operation), operand(std::move(expr))
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

    std::string return_type = parse_type();

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
    std::string type = parse_type();
    return std::make_unique<Parameter>(name, type);
}

std::string Parser::parse_type()
{
    if (m_current_token.kind != TokenKind::Int && m_current_token.kind != TokenKind::Bool &&
        m_current_token.kind != TokenKind::String)
    {
        error("Expected a type: int, bool, or string");
    }
    std::string name(m_current_token.text);
    advance();

    return name;
}

std::unique_ptr<Block> Parser::parse_block()
{
    eat(TokenKind::LeftBrace);

    std::vector<std::unique_ptr<AST>> statements;

    while (m_current_token.kind != TokenKind::RightBrace)
    {
        if (m_current_token.kind == TokenKind::End)
        {
            error("Expected '}' before end of file");
        }

        if (m_current_token.kind == TokenKind::LeftBrace)
        {
            statements.push_back(parse_block());
            continue;
        }

        if (m_current_token.kind == TokenKind::While)
        {
            statements.push_back(parse_while());
            continue;
        }

        if (m_current_token.kind == TokenKind::Identifier)
        {
            const TokenKind next = peek_kind();

            if (next == TokenKind::Colon || next == TokenKind::ColonEqual)
            {
                statements.push_back(parse_variable_declaration());
                continue;
            }

            if (next == TokenKind::Equal)
            {
                statements.push_back(parse_assignment());
                continue;
            }
        }

        auto expression = parse_expression();

        if (m_current_token.kind == TokenKind::Semicolon)
        {
            eat(TokenKind::Semicolon);

            statements.push_back(std::make_unique<ExpressionStatement>(std::move(expression)));

            continue;
        }

        // An expression without a semicolon must finish the block.
        eat(TokenKind::RightBrace);

        return std::make_unique<Block>(std::move(statements), std::move(expression));
    }

    eat(TokenKind::RightBrace);

    return std::make_unique<Block>(std::move(statements), nullptr);
}

std::unique_ptr<WhileStatement> Parser::parse_while()
{
    eat(TokenKind::While);

    auto condition = parse_expression();
    auto body = parse_block();

    return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
}

std::unique_ptr<IfExpression> Parser::parse_if()
{
    eat(TokenKind::If);

    auto condition = parse_expression();
    auto then_branch = parse_block();

    if (m_current_token.kind != TokenKind::Else)
    {
        error("An if expression requires an else branch");
    }

    eat(TokenKind::Else);
    auto else_branch = parse_block();

    return std::make_unique<IfExpression>(
        std::move(condition), std::move(then_branch), std::move(else_branch));
}

std::unique_ptr<VariableDeclaration> Parser::parse_variable_declaration()
{
    std::string name(m_current_token.text);
    eat(TokenKind::Identifier);

    std::optional<std::string> type;

    if (m_current_token.kind == TokenKind::Colon)
    {
        eat(TokenKind::Colon);
        type = parse_type();
    }

    eat(TokenKind::ColonEqual);

    auto initializer = parse_expression();

    eat(TokenKind::Semicolon);

    return std::make_unique<VariableDeclaration>(
        std::move(name), std::move(type), std::move(initializer));
}

std::unique_ptr<Assignment> Parser::parse_assignment()
{
    std::string name(m_current_token.text);
    eat(TokenKind::Identifier);
    eat(TokenKind::Equal);

    auto value = parse_expression();

    eat(TokenKind::Semicolon);

    return std::make_unique<Assignment>(std::move(name), std::move(value));
}

std::unique_ptr<AST> Parser::parse_expression()
{
    return parse_equality();
}

std::unique_ptr<AST> Parser::parse_equality()
{
    auto node = parse_comparison();

    while (m_current_token.kind == TokenKind::EqualEqual ||
           m_current_token.kind == TokenKind::BangEqual)
    {
        const TokenKind op = m_current_token.kind;
        advance();

        auto right = parse_comparison();

        node = std::make_unique<BinaryExpression>(op, std::move(node), std::move(right));
    }

    return node;
}

std::unique_ptr<AST> Parser::parse_comparison()
{
    auto node = parse_addition();

    while (m_current_token.kind == TokenKind::Less ||
           m_current_token.kind == TokenKind::LessEqual ||
           m_current_token.kind == TokenKind::Greater ||
           m_current_token.kind == TokenKind::GreaterEqual)
    {
        const TokenKind op = m_current_token.kind;
        advance();

        auto right = parse_addition();

        node = std::make_unique<BinaryExpression>(op, std::move(node), std::move(right));
    }

    return node;
}

std::unique_ptr<AST> Parser::parse_addition()
{
    auto node = parse_multiplication();

    while (m_current_token.kind == TokenKind::Plus || m_current_token.kind == TokenKind::Minus)
    {
        const TokenKind op = m_current_token.kind;
        advance();

        auto right = parse_multiplication();

        node = std::make_unique<BinaryExpression>(op, std::move(node), std::move(right));
    }

    return node;
}

std::unique_ptr<AST> Parser::parse_multiplication()
{
    auto node = parse_unary();

    while (m_current_token.kind == TokenKind::Star || m_current_token.kind == TokenKind::Slash)
    {
        const TokenKind op = m_current_token.kind;
        advance();

        auto right = parse_unary();

        node = std::make_unique<BinaryExpression>(op, std::move(node), std::move(right));
    }

    return node;
}

std::unique_ptr<AST> Parser::parse_unary()
{
    if (m_current_token.kind == TokenKind::Plus || m_current_token.kind == TokenKind::Minus ||
        m_current_token.kind == TokenKind::Bang)
    {
        const TokenKind op = m_current_token.kind;
        advance();

        return std::make_unique<UnaryExpression>(op, parse_unary());
    }

    return parse_primary();
}

std::unique_ptr<AST> Parser::parse_primary()
{
    if (m_current_token.kind == TokenKind::StringLiteral)
    {
        auto value = decode_string_literal(m_current_token);
        advance();
        return std::make_unique<StringLiteral>(std::move(value));
    }

    if (m_current_token.kind == TokenKind::If)
    {
        return parse_if();
    }

    if (m_current_token.kind == TokenKind::True || m_current_token.kind == TokenKind::False)
    {
        const bool value = m_current_token.kind == TokenKind::True;
        advance();

        return std::make_unique<BooleanLiteral>(value);
    }
    else if (m_current_token.kind == TokenKind::IntegerLiteral)
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
