#include "codegen.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>


#include <cstdint>
#include <limits>
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
    if (type == "bool")
    {
        return m_builder.getInt1Ty();
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

    auto *entry = llvm::BasicBlock::Create(m_context, "entry", function);

    m_builder.SetInsertPoint(entry);

    std::size_t index = 0;

    for (auto &argument : function->args())
    {
        const auto &parameter = variant.parameters[index++];

        argument.setName(parameter->name);

        auto *storage = create_local(parameter->name, argument.getType());
        m_builder.CreateStore(&argument, storage);

        m_scope.emplace(parameter->name, storage);
    }

    auto *result = emit_block(*variant.body);

    if (!result)
    {
        throw std::runtime_error("Missing function result during code generation");
    }

    m_builder.CreateRet(result);
}

llvm::AllocaInst *CodeGenerator::create_local(const std::string &name, llvm::Type *type)
{
    auto *function = m_builder.GetInsertBlock()->getParent();
    auto &entry = function->getEntryBlock();

    llvm::IRBuilder<> entry_builder(&entry, entry.begin());

    return entry_builder.CreateAlloca(type, nullptr, name);
}

llvm::Value *CodeGenerator::emit_block(Block &block)
{
    auto saved_scope = m_scope;

    try
    {
        for (auto &statement : block.statements)
        {
            emit_statement(*statement);
        }

        llvm::Value *result = nullptr;

        if (block.expression)
        {
            result = emit_expression(*block.expression);
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

void CodeGenerator::emit_while(WhileStatement &statement)
{
    auto *function = m_builder.GetInsertBlock()->getParent();

    auto *condition_block = llvm::BasicBlock::Create(m_context, "while.condition", function);

    auto *body_block = llvm::BasicBlock::Create(m_context, "while.body", function);

    auto *exit_block = llvm::BasicBlock::Create(m_context, "while.exit", function);

    m_builder.CreateBr(condition_block);

    m_builder.SetInsertPoint(condition_block);

    auto *condition = emit_expression(*statement.condition);

    if (!condition->getType()->isIntegerTy(1))
    {
        throw std::runtime_error("Expected boolean while condition during code generation");
    }

    m_builder.CreateCondBr(condition, body_block, exit_block);

    m_builder.SetInsertPoint(body_block);

    emit_block(*statement.body);
    m_builder.CreateBr(condition_block);

    m_builder.SetInsertPoint(exit_block);
}

llvm::Value *CodeGenerator::emit_if(IfExpression &expression)
{
    auto *condition = emit_expression(*expression.condition);

    if (!condition->getType()->isIntegerTy(1))
    {
        throw std::runtime_error("Expected boolean condition during code generation");
    }

    auto *function = m_builder.GetInsertBlock()->getParent();

    auto *then_block = llvm::BasicBlock::Create(m_context, "if.then", function);

    auto *else_block = llvm::BasicBlock::Create(m_context, "if.else", function);

    auto *merge_block = llvm::BasicBlock::Create(m_context, "if.merge", function);

    m_builder.CreateCondBr(condition, then_block, else_block);

    // True branch.
    m_builder.SetInsertPoint(then_block);

    auto *then_value = emit_block(*expression.then_branch);
    auto *then_end = m_builder.GetInsertBlock();

    m_builder.CreateBr(merge_block);

    // False branch.
    m_builder.SetInsertPoint(else_block);

    auto *else_value = emit_block(*expression.else_branch);
    auto *else_end = m_builder.GetInsertBlock();

    m_builder.CreateBr(merge_block);

    if (!then_value || !else_value)
    {
        throw std::runtime_error("Missing if-expression value during code generation");
    }

    if (then_value->getType() != else_value->getType())
    {
        throw std::runtime_error("Mismatched branch types during code generation");
    }

    // Select the value supplied by the executed branch.
    m_builder.SetInsertPoint(merge_block);

    auto *result = m_builder.CreatePHI(then_value->getType(), 2, "iftmp");

    result->addIncoming(then_value, then_end);
    result->addIncoming(else_value, else_end);

    return result;
}

void CodeGenerator::emit_statement(AST &statement)
{
    if (auto *block = dynamic_cast<Block *>(&statement))
    {
        emit_block(*block);
        return;
    }
    if (auto *loop = dynamic_cast<WhileStatement *>(&statement))
    {
        emit_while(*loop);
        return;
    }
    if (auto *declaration = dynamic_cast<VariableDeclaration *>(&statement))
    {
        auto *value = emit_expression(*declaration->initializer);

        auto *storage = create_local(declaration->name, value->getType());

        m_builder.CreateStore(value, storage);
        m_scope.emplace(declaration->name, storage);
        return;
    }

    if (auto *assignment = dynamic_cast<Assignment *>(&statement))
    {
        auto *value = emit_expression(*assignment->value);

        m_builder.CreateStore(value, m_scope.at(assignment->name));

        return;
    }

    if (auto *expr = dynamic_cast<ExpressionStatement *>(&statement))
    {
        emit_expression(*expr->expression);
        return;
    }

    throw std::runtime_error("Unsupported statement in code generation");
}

llvm::Value *CodeGenerator::emit_expression(AST &expression)
{
    if (auto *conditional = dynamic_cast<IfExpression *>(&expression))
    {
        return emit_if(*conditional);
    }

    if (auto *literal = dynamic_cast<IntegerLiteral *>(&expression))
    {
        return m_builder.getInt32(static_cast<std::uint32_t>(literal->value));
    }
    if (auto *literal = dynamic_cast<BooleanLiteral *>(&expression))
    {
        return m_builder.getInt1(literal->value);
    }

    if (auto *identifier = dynamic_cast<Identifier *>(&expression))
    {
        const auto found = m_scope.find(identifier->name);

        if (found == m_scope.end())
        {
            throw std::runtime_error("Unresolved identifier: " + identifier->name);
        }

        return m_builder.CreateLoad(
            found->second->getAllocatedType(), found->second, identifier->name + ".value");
    }

    if (auto *binary = dynamic_cast<BinaryExpression *>(&expression))
    {
        auto *left = emit_expression(*binary->left);
        auto *right = emit_expression(*binary->right);

        switch (binary->op)
        {
            case TokenKind::Plus:
                return m_builder.CreateAdd(left, right, "addtmp");

            case TokenKind::Minus:
                return m_builder.CreateSub(left, right, "subtmp");

            case TokenKind::Star:
                return m_builder.CreateMul(left, right, "multmp");

            case TokenKind::Slash:
                return emit_division(left, right);

            case TokenKind::BangEqual:
                return m_builder.CreateICmpNE(left, right, "netmp");

            case TokenKind::EqualEqual:
                return m_builder.CreateICmpEQ(left, right, "eqtmp");

            case TokenKind::Less:
                return m_builder.CreateICmpSLT(left, right, "lttmp");

            case TokenKind::LessEqual:
                return m_builder.CreateICmpSLE(left, right, "letmp");

            case TokenKind::Greater:
                return m_builder.CreateICmpSGT(left, right, "gttmp");

            case TokenKind::GreaterEqual:
                return m_builder.CreateICmpSGE(left, right, "getmp");

            default:
                throw std::runtime_error("Unsupported binary operator");
        }
    }

    if (auto *unary = dynamic_cast<UnaryExpression *>(&expression))
    {
        auto *value = emit_expression(*unary->operand);

        switch (unary->op)
        {
            case TokenKind::Plus:
                return value;

            case TokenKind::Minus:
                return m_builder.CreateNeg(value, "negtmp");

            case TokenKind::Bang:
                return m_builder.CreateNot(value, "nottmp");

            default:
                throw std::runtime_error("Unsupported unary operator");
        }
    }

    if (auto *call = dynamic_cast<Call *>(&expression))
    {
        return emit_call(*call);
    }

    throw std::runtime_error("Unsupported expression in code generation");
}

llvm::Value *CodeGenerator::emit_division(llvm::Value *left, llvm::Value *right)
{
    auto *zero = m_builder.getInt32(0);
    auto *minus_one = m_builder.getInt32(static_cast<std::uint32_t>(-1));
    auto *minimum =
        m_builder.getInt32(static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::min()));

    auto *is_zero = m_builder.CreateICmpEQ(right, zero);

    auto *overflows = m_builder.CreateAnd(m_builder.CreateICmpEQ(left, minimum),
                                          m_builder.CreateICmpEQ(right, minus_one));

    auto *invalid = m_builder.CreateOr(is_zero, overflows);

    auto *function = m_builder.GetInsertBlock()->getParent();

    auto *failure = llvm::BasicBlock::Create(m_context, "division.fail", function);
    auto *success = llvm::BasicBlock::Create(m_context, "division.ok", function);

    m_builder.CreateCondBr(invalid, failure, success);

    m_builder.SetInsertPoint(failure);

    auto *trap = llvm::Intrinsic::getOrInsertDeclaration(m_module.get(), llvm::Intrinsic::trap);

    m_builder.CreateCall(trap);
    m_builder.CreateUnreachable();

    m_builder.SetInsertPoint(success);

    return m_builder.CreateSDiv(left, right, "divtmp");
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
