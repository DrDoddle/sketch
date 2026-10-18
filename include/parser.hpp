#pragma once

#include "lexer.hpp"
#include <memory>
#include <optional>
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
    std::vector<std::unique_ptr<AST>> statements;
    std::unique_ptr<AST> expression;

    Block(std::vector<std::unique_ptr<AST>> stmts, std::unique_ptr<AST> expr);
};

struct WhileStatement : public AST
{
    std::unique_ptr<AST> condition;
    std::unique_ptr<Block> body;

    WhileStatement(std::unique_ptr<AST> cond, std::unique_ptr<Block> block);
};

struct IfExpression : public AST
{
    std::unique_ptr<AST> condition;
    std::unique_ptr<Block> then_branch;
    std::unique_ptr<Block> else_branch;

    IfExpression(std::unique_ptr<AST> cond, std::unique_ptr<Block> then_block,
                 std::unique_ptr<Block> else_block);
};

struct VariableDeclaration : public AST
{
    std::string name;
    std::optional<std::string> declared_type;
    std::unique_ptr<AST> initializer;

    VariableDeclaration(std::string n, std::optional<std::string> type, std::unique_ptr<AST> init);
};

struct Assignment : public AST
{
    std::string name;
    std::unique_ptr<AST> value;

    Assignment(std::string n, std::unique_ptr<AST> expr);
};

struct ExpressionStatement : public AST
{
    std::unique_ptr<AST> expression;

    explicit ExpressionStatement(std::unique_ptr<AST> expr);
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

struct BooleanLiteral : public AST
{
    bool value;

    explicit BooleanLiteral(bool val);
};

struct StringLiteral : public AST
{
    std::string value;

    explicit StringLiteral(std::string bytes);
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

struct BinaryExpression : public AST
{
    TokenKind op;
    std::unique_ptr<AST> left;
    std::unique_ptr<AST> right;

    BinaryExpression(TokenKind operation, std::unique_ptr<AST> l, std::unique_ptr<AST> r);
};

struct UnaryExpression : public AST
{
    TokenKind op;
    std::unique_ptr<AST> operand;

    UnaryExpression(TokenKind operation, std::unique_ptr<AST> expr);
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

    std::string parse_type(bool allow_void = false);

    std::unique_ptr<Block> parse_block();

    std::unique_ptr<WhileStatement> parse_while();

    std::unique_ptr<IfExpression> parse_if();

    std::unique_ptr<VariableDeclaration> parse_variable_declaration();

    std::unique_ptr<Assignment> parse_assignment();

    std::unique_ptr<AST> parse_equality();

    std::unique_ptr<AST> parse_comparison();

    std::unique_ptr<AST> parse_expression();

    std::unique_ptr<AST> parse_addition();

    std::unique_ptr<AST> parse_multiplication();

    std::unique_ptr<AST> parse_unary();

    std::unique_ptr<AST> parse_primary();

    std::unique_ptr<Call> parse_call();

    std::vector<std::unique_ptr<Argument>> parse_arguments();

    std::unique_ptr<Argument> parse_argument();
};
