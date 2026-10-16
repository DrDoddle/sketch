#include "codegen.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <vector>

CodeGenerator::CodeGenerator()
    : m_module(std::make_unique<llvm::Module>("doodle", m_context)), m_builder(m_context)
{
}

void CodeGenerator::generate(Program &program)
{
    // Reset state so this object can generate another module.
    m_builder.ClearInsertionPoint();
    m_functions.clear();
    m_scope.clear();
    m_module = std::make_unique<llvm::Module>("doodle", m_context);

    for (auto &node : program.function_groups)
    {
        auto *group = dynamic_cast<FunctionGroup *>(node.get());

        if (!group)
        {
            throw std::runtime_error("Expected function group");
        }

        declare_group(*group);
    }

    for (auto &node : program.function_groups)
    {
        auto &group = dynamic_cast<FunctionGroup &>(*node);

        for (auto &variant : group.variants)
        {
            emit_variant(*variant);
        }
    }

    if (llvm::verifyModule(*m_module, &llvm::errs()))
    {
        throw std::runtime_error("Generated invalid LLVM IR");
    }
}

void CodeGenerator::print() const
{
    m_module->print(llvm::outs(), nullptr);
}

void CodeGenerator::write_ir(const std::string &path) const
{
    std::error_code error;
    llvm::raw_fd_ostream output(path, error, llvm::sys::fs::OF_None);

    if (error)
    {
        throw std::runtime_error("Could not open IR output file: " + path + ": " + error.message());
    }

    m_module->print(output, nullptr);
    output.close();

    if (output.has_error())
    {
        output.clear_error();

        throw std::runtime_error("Failed to write IR output file: " + path);
    }
}

llvm::Type *CodeGenerator::lower_type(const std::string &type)
{
    if (type == "int")
    {
        return m_builder.getInt32Ty();
    }

    throw std::runtime_error("Unsupported type: " + type);
}

void CodeGenerator::declare_group(FunctionGroup &group)
{
    if (group.name == "main")
    {
        if (group.variants.size() != 1 || !group.variants.front()->parameters.empty() ||
            group.variants.front()->return_type != "int")
        {
            throw std::runtime_error("main must have exactly one variant: >(): int");
        }
    }

    for (std::size_t i = 0; i < group.variants.size(); ++i)
    {
        auto &variant = *group.variants[i];

        std::vector<llvm::Type *> parameter_types;

        for (const auto &parameter : variant.parameters)
        {
            parameter_types.push_back(lower_type(parameter->param_type));
        }

        auto *signature =
            llvm::FunctionType::get(lower_type(variant.return_type), parameter_types, false);

        const std::string symbol =
            group.name == "main" ? "main" : "doodle." + group.name + "." + std::to_string(i);

        auto *function =
            llvm::Function::Create(signature, llvm::Function::ExternalLinkage, symbol, *m_module);

        m_functions.emplace(&variant, function);
    }
}

void CodeGenerator::emit_variant(FunctionVariant &variant)
{
    auto *function = m_functions.at(&variant);

    m_scope.clear();

    std::size_t index = 0;

    for (auto &argument : function->args())
    {
        const auto &parameter = variant.parameters[index++];

        argument.setName(parameter->name);
        m_scope.emplace(parameter->name, &argument);
    }

    auto *entry = llvm::BasicBlock::Create(m_context, "entry", function);

    m_builder.SetInsertPoint(entry);

    auto *result = emit_expression(*variant.body->expression);
    m_builder.CreateRet(result);
}

llvm::Value *CodeGenerator::emit_expression(AST &expression)
{
    if (auto *literal = dynamic_cast<IntegerLiteral *>(&expression))
    {
        return m_builder.getInt32(static_cast<std::uint32_t>(literal->value));
    }

    if (auto *identifier = dynamic_cast<Identifier *>(&expression))
    {
        const auto found = m_scope.find(identifier->name);

        if (found == m_scope.end())
        {
            throw std::runtime_error("Unresolved identifier: " + identifier->name);
        }

        return found->second;
    }

    if (auto *addition = dynamic_cast<AddExpression *>(&expression))
    {
        auto *left = emit_expression(*addition->left);
        auto *right = emit_expression(*addition->right);

        return m_builder.CreateAdd(left, right, "addtmp");
    }

    if (auto *call = dynamic_cast<Call *>(&expression))
    {
        return emit_call(*call);
    }

    throw std::runtime_error("Unsupported expression in code generation");
}

llvm::Value *CodeGenerator::emit_call(Call &call)
{
    if (!call.resolved_variant)
    {
        throw std::runtime_error("Call was not semantically resolved: " + call.name);
    }

    auto &variant = *call.resolved_variant;
    auto *function = m_functions.at(&variant);

    std::unordered_map<std::string, llvm::Value *> values;

    for (const auto &argument : call.arguments)
    {
        auto *value = emit_expression(*argument->expression);
        values.emplace(argument->name, value);
    }

    std::vector<llvm::Value *> ordered_arguments;

    for (const auto &parameter : variant.parameters)
    {
        ordered_arguments.push_back(values.at(parameter->name));
    }

    return m_builder.CreateCall(function, ordered_arguments, "calltmp");
}
