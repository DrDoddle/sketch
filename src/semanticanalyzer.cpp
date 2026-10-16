#include "semanticanalyzer.hpp"

#include <stdexcept>
#include <unordered_set>

namespace
{
    Type resolve_type(const std::string &name)
    {
        if (name == "int")
        {
            return Type::Int;
        }

        throw std::runtime_error("Unknown type: " + name);
    }
} // namespace

void SemanticAnalyzer::analyze(Program &program)
{
    m_functions.clear();
    m_scope.clear();

    collect_functions(program);

    for (auto &node : program.function_groups)
    {
        auto *group = dynamic_cast<FunctionGroup *>(node.get());

        if (!group)
        {
            throw std::runtime_error("Expected a function group");
        }

        check_function_group(*group);
    }
}

void SemanticAnalyzer::collect_functions(Program &program)
{
    for (auto &node : program.function_groups)
    {
        auto *group = dynamic_cast<FunctionGroup *>(node.get());

        if (!group)
        {
            throw std::runtime_error("Expected a function group");
        }

        const auto result = m_functions.emplace(group->name, group);

        if (!result.second)
        {
            throw std::runtime_error("Duplicate function group: " + group->name);
        }
    }
}

void SemanticAnalyzer::check_function_group(FunctionGroup &group)
{
    for (auto &variant : group.variants)
    {
        check_function_variant(*variant);
    }
}

void SemanticAnalyzer::check_function_variant(FunctionVariant &variant)
{
    m_scope.clear();

    for (const auto &parameter : variant.parameters)
    {
        const Type type = resolve_type(parameter->param_type);
        const auto result = m_scope.emplace(parameter->name, type);

        if (!result.second)
        {
            throw std::runtime_error("Duplicate parameter: " + parameter->name);
        }
    }

    const Type expected = resolve_type(variant.return_type);
    const Type actual = check_expression(*variant.body->expression);

    if (actual != expected)
    {
        throw std::runtime_error("Body type does not match declared return type");
    }
}

Type SemanticAnalyzer::check_expression(AST &expression)
{
    if (dynamic_cast<IntegerLiteral *>(&expression))
    {
        return Type::Int;
    }

    if (auto *identifier = dynamic_cast<Identifier *>(&expression))
    {
        const auto it = m_scope.find(identifier->name);

        if (it == m_scope.end())
        {
            throw std::runtime_error("Unknown identifier: " + identifier->name);
        }

        return it->second;
    }

    if (auto *binary = dynamic_cast<BinaryExpression *>(&expression))
    {
        const Type left = check_expression(*binary->left);
        const Type right = check_expression(*binary->right);

        if (left != Type::Int || right != Type::Int)
        {
            throw std::runtime_error("Arithmetic requires two integers");
        }

        return Type::Int;
    }

    if (auto *unary = dynamic_cast<UnaryExpression *>(&expression))
    {
        if (check_expression(*unary->operand) != Type::Int)
        {
            throw std::runtime_error("Unary arithmetic requires an integer");
        }

        return Type::Int;
    }

    if (auto *call = dynamic_cast<Call *>(&expression))
    {
        return check_call(*call);
    }

    throw std::runtime_error("Unsupported expression node");
}

Type SemanticAnalyzer::check_call(Call &call)
{
    const auto function = m_functions.find(call.name);

    if (function == m_functions.end())
    {
        throw std::runtime_error("Unknown function: " + call.name);
    }

    // Detect repeated labels before checking argument expressions.
    std::unordered_set<std::string> labels;

    for (const auto &argument : call.arguments)
    {
        if (!labels.insert(argument->name).second)
        {
            throw std::runtime_error("Duplicate argument: " + argument->name);
        }
    }

    std::unordered_map<std::string, Type> arguments;

    for (const auto &argument : call.arguments)
    {
        arguments.emplace(argument->name, check_expression(*argument->expression));
    }

    FunctionVariant *selected = nullptr;

    for (const auto &variant : function->second->variants)
    {
        if (variant->parameters.size() != arguments.size())
        {
            continue;
        }

        bool matches = true;

        for (const auto &parameter : variant->parameters)
        {
            const auto argument = arguments.find(parameter->name);

            if (argument == arguments.end() ||
                argument->second != resolve_type(parameter->param_type))
            {
                matches = false;
                break;
            }
        }

        if (!matches)
        {
            continue;
        }

        if (selected)
        {
            throw std::runtime_error("Ambiguous call to: " + call.name);
        }

        selected = variant.get();
    }

    if (!selected)
    {
        throw std::runtime_error("No matching variant for call to: " + call.name);
    }
    call.resolved_variant = selected;
    return resolve_type(selected->return_type);
}
