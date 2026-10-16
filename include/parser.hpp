#pragma once

#include "lexer.hpp"
#include <memory>
#include <string>
#include <vector>

struct AST
{
    virtual ~AST() = default;
};

struct Program : public AST
{
    std::vector<std::unique_ptr<AST>> function_groups;

    explicit Program(std::vector<std::unique_ptr<AST>> groups);
};

struct Parameter : public AST
{
    std::string name;
    std::string param_type;

    Parameter(std::string n, std::string t);
};

struct Block : public AST
{
    std::unique_ptr<AST> expression;

    explicit Block(std::unique_ptr<AST> expr);
};

struct FunctionVariant : public AST
{
    std::vector<std::unique_ptr<Parameter>> parameters;
    std::string return_type;
    std::unique_ptr<Block> body;

    FunctionVariant(std::vector<std::unique_ptr<Parameter>> params, std::string ret_type,
                    std::unique_ptr<Block> b);
};

struct FunctionGroup : public AST
{
    std::string name;
    std::vector<std::unique_ptr<FunctionVariant>> variants;

    FunctionGroup(std::string n, std::vector<std::unique_ptr<FunctionVariant>> vars);
};

struct IntegerLiteral : public AST
{
    int value;

    explicit IntegerLiteral(int val);
};

struct Identifier : public AST
{
    std::string name;

    explicit Identifier(std::string n);
};

struct Argument : public AST
{
    std::string name;
    std::unique_ptr<AST> expression;

    Argument(std::string n, std::unique_ptr<AST> expr);
};

struct Call : public AST
{
    std::string name;
    std::vector<std::unique_ptr<Argument>> arguments;

    FunctionVariant *resolved_variant = nullptr;

    Call(std::string n, std::vector<std::unique_ptr<Argument>> args);
};

struct AddExpression : public AST
{
    std::unique_ptr<AST> left;
    std::unique_ptr<AST> right;

    AddExpression(std::unique_ptr<AST> l, std::unique_ptr<AST> r);
};

class Parser
{
public:
    explicit Parser(Lexer &lexer);

    std::unique_ptr<Program> parse();

private:
    Lexer &m_lexer;
    Token m_current_token;
    Token m_next_token;

    void advance();

    TokenKind peek_kind() const;

    [[noreturn]] void error(const std::string &message);

    void eat(TokenKind kind);

    std::unique_ptr<FunctionGroup> parse_function_group();

    std::unique_ptr<FunctionVariant> parse_function_variant();

    std::vector<std::unique_ptr<Parameter>> parse_parameters();

    std::unique_ptr<Parameter> parse_parameter();

    std::unique_ptr<Block> parse_block();

    std::unique_ptr<AST> parse_expression();

    std::unique_ptr<AST> parse_addition();

    std::unique_ptr<AST> parse_primary();

    std::unique_ptr<Call> parse_call();

    std::vector<std::unique_ptr<Argument>> parse_arguments();

    std::unique_ptr<Argument> parse_argument();
};
