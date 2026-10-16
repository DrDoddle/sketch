#pragma once

#include <parser.hpp>

enum class Type
{
    Int
};

#include <unordered_map>

class SemanticAnalyzer
{
public:
    void analyze(Program &program);

private:
    std::unordered_map<std::string, FunctionGroup *> m_functions;
    std::unordered_map<std::string, Type> m_scope;

    void collect_functions(Program &program);

    void check_function_group(FunctionGroup &group);

    void check_function_variant(FunctionVariant &variant);

    Type check_expression(AST &expression);

    Type check_call(Call &call);
};
