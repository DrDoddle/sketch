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
        if (name == "string")
        {
            return Type::String;
        }
        if (name == "bool")
        {
            return Type::Bool;
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
    const auto actual = check_block(*variant.body);

    if (!actual)
    {
        throw std::runtime_error("Function body must end with a value expression");
    }

    if (*actual != expected)
    {
        throw std::runtime_error("Body type does not match declared return type");
    }
}

std::optional<Type> SemanticAnalyzer::check_block(Block &block)
{
    auto saved_scope = m_scope;

    try
    {
        for (auto &statement : block.statements)
        {
            check_statement(*statement);
        }

        std::optional<Type> result;

        if (block.expression)
        {
            result = check_expression(*block.expression);
        }

        m_scope.swap(saved_scope);
        return result;
    }
    catch (...)
    {
        m_scope.swap(saved_scope);
        throw;
    }
}

void SemanticAnalyzer::check_statement(AST &statement)
{
    if (auto *block = dynamic_cast<Block *>(&statement))
    {
        check_block(*block);
        return;
    }
    if (auto *loop = dynamic_cast<WhileStatement *>(&statement))
    {
        if (check_expression(*loop->condition) != Type::Bool)
        {
            throw std::runtime_error("A while condition must be a boolean");
        }

        check_block(*loop->body);
        return;
    }
    if (auto *declaration = dynamic_cast<VariableDeclaration *>(&statement))
    {
        if (m_scope.find(declaration->name) != m_scope.end())
        {
            throw std::runtime_error("Duplicate variable: " + declaration->name);
        }

        // Check before inserting the name: a := a cannot declare itself.
        const Type actual = check_expression(*declaration->initializer);

        if (declaration->declared_type)
        {
            const Type expected = resolve_type(*declaration->declared_type);

            if (actual != expected)
            {
                throw std::runtime_error("Initializer type mismatch for: " + declaration->name);
            }
        }

        m_scope.emplace(declaration->name, actual);
        return;
    }

    if (auto *assignment = dynamic_cast<Assignment *>(&statement))
    {
        const auto variable = m_scope.find(assignment->name);

        if (variable == m_scope.end())
        {
            throw std::runtime_error("Assignment to undeclared variable: " + assignment->name);
        }

        const Type expected = variable->second;
        const Type actual = check_expression(*assignment->value);

        if (expected != actual)
        {
            throw std::runtime_error("Assignment type mismatch for: " + assignment->name);
        }

        return;
    }

    if (auto *expr = dynamic_cast<ExpressionStatement *>(&statement))
    {
        check_expression(*expr->expression);
        return;
    }

    throw std::runtime_error("Unsupported statement");
}

Type SemanticAnalyzer::check_expression(AST &expression)
{
    if (dynamic_cast<StringLiteral *>(&expression))
    {
        return Type::String;
    }

    if (dynamic_cast<IntegerLiteral *>(&expression))
    {
        return Type::Int;
    }
    if (dynamic_cast<BooleanLiteral *>(&expression))
    {
        return Type::Bool;
    }

    if (auto *conditional = dynamic_cast<IfExpression *>(&expression))
    {
        if (check_expression(*conditional->condition) != Type::Bool)
        {
            throw std::runtime_error("An if condition must be a boolean");
        }

        const auto then_type = check_block(*conditional->then_branch);
        const auto else_type = check_block(*conditional->else_branch);

        if (!then_type || !else_type)
        {
            throw std::runtime_error("Both branches of an if expression must produce a value");
        }

        if (*then_type != *else_type)
        {
            throw std::runtime_error("Both if branches must produce the same type");
        }

        return *then_type;
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

        switch (binary->op)
        {
            case TokenKind::Plus:
            case TokenKind::Minus:
            case TokenKind::Star:
            case TokenKind::Slash:
            {
                if (left != Type::Int || right != Type::Int)
                {
                    throw std::runtime_error("Arithmetic requires two integers");
                }

                return Type::Int;
            }

            case TokenKind::Less:
            case TokenKind::LessEqual:
            case TokenKind::Greater:
            case TokenKind::GreaterEqual:
            {
                if (left != Type::Int || right != Type::Int)
                {
                    throw std::runtime_error("Ordered comparison requires two integers");
                }

                return Type::Bool;
            }

            case TokenKind::EqualEqual:
            case TokenKind::BangEqual:
            {
                if (left != right)
                {
                    throw std::runtime_error("Equality requires operands of the same type");
                }

                return Type::Bool;
            }

            default:
                throw std::runtime_error("Unsupported binary operator");
        }
    }

    if (auto *unary = dynamic_cast<UnaryExpression *>(&expression))
    {
        const Type operand = check_expression(*unary->operand);

        switch (unary->op)
        {
            case TokenKind::Bang:
            {
                if (operand != Type::Bool)
                {
                    throw std::runtime_error("'!' requires a boolean");
                }

                return Type::Bool;
            }

            case TokenKind::Plus:
            case TokenKind::Minus:
            {
                if (operand != Type::Int)
                {
                    throw std::runtime_error("Unary '+' and '-' require an integer");
                }

                return Type::Int;
            }

            default:
                throw std::runtime_error("Unsupported unary operator");
        }
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
