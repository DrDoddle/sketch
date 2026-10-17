#pragma once

#include "parser.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <memory>
#include <string>
#include <unordered_map>

class CodeGenerator
{
public:
    CodeGenerator();

    void generate(Program &program);

    void print() const;

    void write_ir(const std::string &path) const;

private:
    llvm::LLVMContext m_context;
    std::unique_ptr<llvm::Module> m_module;
    llvm::IRBuilder<> m_builder;

    std::unordered_map<FunctionVariant *, llvm::Function *> m_functions;
    std::unordered_map<std::string, llvm::AllocaInst *> m_scope;

    llvm::Type *lower_type(const std::string &type);

    void declare_group(FunctionGroup &group);

    void emit_variant(FunctionVariant &variant);

    llvm::AllocaInst *create_local(const std::string &name, llvm::Type *type);

    void emit_statement(AST &statement);

    llvm::Value *emit_block(Block &block);

    llvm::Value *emit_if(IfExpression &expression);

    llvm::Value *emit_expression(AST &expression);

    llvm::Value *emit_division(llvm::Value *left, llvm::Value *right);

    llvm::Value *emit_call(Call &call);
};
