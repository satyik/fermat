#ifndef CODEGEN_H
#define CODEGEN_H

#include "AST.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <map>
#include <memory>
#include <string>

using namespace llvm;
using namespace llvm::orc;

// Global LLVM state
extern std::unique_ptr<LLVMContext> TheContext;
extern std::unique_ptr<Module> TheModule;
extern std::unique_ptr<IRBuilder<>> Builder;
extern std::map<std::string, AllocaInst *> NamedValues;
extern std::map<std::string, TypeInfo> VariableTypes;

// JIT Engine
extern std::unique_ptr<LLJIT> TheJIT;
extern ExitOnError ExitOnErr;

// Function prototypes map
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

// Counter for unique anonymous expression names
extern unsigned AnonExprCounter;

// Struct type cache
extern std::map<std::string, llvm::StructType *> LLVMStructTypes;

// Helper functions
Value *LogErrorV(const char *Str);
Function *getFunction(std::string Name);
void InitializeModuleAndPassManager();
AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                   const std::string &VarName,
                                   Type *Ty = nullptr);
Type *GetLLVMType(const TypeInfo &type);

#endif // CODEGEN_H
