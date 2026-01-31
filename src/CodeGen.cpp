#include "CodeGen.h"
#include "Lexer.h"
#include "Parser.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <cstdio>

// Global LLVM state definitions
std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;
std::map<std::string, AllocaInst *> NamedValues;
std::map<std::string, TypeInfo> VariableTypes;

// JIT Engine
std::unique_ptr<LLJIT> TheJIT;
ExitOnError ExitOnErr;

// Function prototypes map
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

// Counter for unique anonymous expression names
unsigned AnonExprCounter = 0;

// Struct type cache
std::map<std::string, llvm::StructType *> LLVMStructTypes;

Value *LogErrorV(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

void InitializeModuleAndPassManager() {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("SpyLang JIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                   const std::string &VarName, Type *Ty) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                   TheFunction->getEntryBlock().begin());
  if (!Ty)
    Ty = Type::getDoubleTy(*TheContext);
  return TmpB.CreateAlloca(Ty, nullptr, VarName);
}

Type *GetLLVMType(const TypeInfo &type) {
  switch (type.BaseType) {
  case SpyType::Int:
    return Type::getInt64Ty(*TheContext);
  case SpyType::Float:
    return Type::getDoubleTy(*TheContext);
  case SpyType::Bool:
    return Type::getInt1Ty(*TheContext);
  case SpyType::String:
    return PointerType::get(Type::getInt8Ty(*TheContext), 0);
  case SpyType::Struct: {
    auto it = LLVMStructTypes.find(type.StructName);
    if (it != LLVMStructTypes.end())
      return it->second;
    return nullptr;
  }
  default:
    return Type::getDoubleTy(*TheContext);
  }
}

Function *getFunction(std::string Name) {
  if (auto *F = TheModule->getFunction(Name))
    return F;

  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  return nullptr;
}

//===----------------------------------------------------------------------===//
// Expression Code Generation
//===----------------------------------------------------------------------===//

Value *NumberExprAST::codegen() {
  if (IsInt)
    return ConstantInt::get(Type::getInt64Ty(*TheContext), (int64_t)Val);
  return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *StringExprAST::codegen() {
  return Builder->CreateGlobalString(Val, "str");
}

Value *VariableExprAST::codegen() {
  Value *V = NamedValues[getName()];
  if (!V) {
    // Check for global variable
    V = TheModule->getNamedGlobal(getName());
  }

  if (!V)
    return LogErrorV("Unknown variable name");

  // Handle both AllocaInst (local) and GlobalVariable (static)
  Type *Ty = nullptr;
  if (auto *Alloca = dyn_cast<AllocaInst>(V)) {
    Ty = Alloca->getAllocatedType();
  } else if (auto *GV = dyn_cast<GlobalVariable>(V)) {
    Ty = GV->getValueType();
  } else {
    // Fallback / Error?
    Ty = Type::getDoubleTy(*TheContext);
  }

  return Builder->CreateLoad(Ty, V, getName().c_str());
}

Value *UnaryExprAST::codegen() {
  Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;

  if (Opcode == '-')
    return Builder->CreateFNeg(OperandV, "negtmp");

  return LogErrorV("Unknown unary operator");
}

Value *BinaryExprAST::codegen() {
  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  // Handle integer vs float operations
  bool isIntOp = L->getType()->isIntegerTy() && R->getType()->isIntegerTy();

  switch (Op) {
  case '+':
    return isIntOp ? Builder->CreateAdd(L, R, "addtmp")
                   : Builder->CreateFAdd(L, R, "addtmp");
  case '-':
    return isIntOp ? Builder->CreateSub(L, R, "subtmp")
                   : Builder->CreateFSub(L, R, "subtmp");
  case '*':
    return isIntOp ? Builder->CreateMul(L, R, "multmp")
                   : Builder->CreateFMul(L, R, "multmp");
  case '/':
    return isIntOp ? Builder->CreateSDiv(L, R, "divtmp")
                   : Builder->CreateFDiv(L, R, "divtmp");
  case '<':
    if (isIntOp) {
      L = Builder->CreateICmpSLT(L, R, "cmptmp");
      return Builder->CreateZExt(L, Type::getInt64Ty(*TheContext), "booltmp");
    }
    L = Builder->CreateFCmpULT(L, R, "cmptmp");
    return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
  case '>':
    if (isIntOp) {
      L = Builder->CreateICmpSGT(L, R, "cmptmp");
      return Builder->CreateZExt(L, Type::getInt64Ty(*TheContext), "booltmp");
    }
    L = Builder->CreateFCmpUGT(L, R, "cmptmp");
    return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
  case tok_eq:
    if (isIntOp) {
      L = Builder->CreateICmpEQ(L, R, "cmptmp");
      return Builder->CreateZExt(L, Type::getInt64Ty(*TheContext), "booltmp");
    }
    L = Builder->CreateFCmpOEQ(L, R, "cmptmp");
    return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
  case tok_ne:
    if (isIntOp) {
      L = Builder->CreateICmpNE(L, R, "cmptmp");
      return Builder->CreateZExt(L, Type::getInt64Ty(*TheContext), "booltmp");
    }
    L = Builder->CreateFCmpONE(L, R, "cmptmp");
    return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
  case ';':
    return R; // Return RHS (sequence operator)
  default:
    return LogErrorV("invalid binary operator");
  }
}

Value *CallExprAST::codegen() {
  std::string MangledCallee = getMangledName();
  Function *CalleeF = getFunction(MangledCallee);
  if (!CalleeF) {
    // Fallback: try looking up original name (e.g. for externs)
    CalleeF = getFunction(Callee);
  }

  if (!CalleeF)
    return LogErrorV("Unknown function referenced");

  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back())
      return nullptr;
  }

  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Value *LetExprAST::codegen() {
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  Value *InitVal = Init->codegen();
  if (!InitVal)
    return nullptr;

  Type *VarType = InitVal->getType();
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Name, VarType);
  Builder->CreateStore(InitVal, Alloca);
  NamedValues[Name] = Alloca;

  Value *BodyVal = nullptr;
  if (Body) {
    BodyVal = Body->codegen();
    if (!BodyVal)
      return nullptr;
  } else {
    BodyVal = InitVal;
  }

  return BodyVal;
}

Value *AssignExprAST::codegen() {
  AllocaInst *Variable = NamedValues[Name];
  if (!Variable)
    return LogErrorV("Unknown variable name for assignment");

  Value *Val = Value_->codegen();
  if (!Val)
    return nullptr;

  Builder->CreateStore(Val, Variable);
  return Val;
}

Value *IfExprAST::codegen() {
  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  // Convert to bool
  if (CondV->getType()->isDoubleTy()) {
    CondV = Builder->CreateFCmpONE(
        CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");
  } else if (CondV->getType()->isIntegerTy(64)) {
    CondV = Builder->CreateICmpNE(
        CondV, ConstantInt::get(Type::getInt64Ty(*TheContext), 0), "ifcond");
  }

  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  Builder->SetInsertPoint(ThenBB);
  Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;
  Builder->CreateBr(MergeBB);
  ThenBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);

  Value *ElseV = nullptr;
  if (Else) {
    ElseV = Else->codegen();
    if (!ElseV)
      return nullptr;
  } else {
    ElseV = ConstantFP::get(*TheContext, APFloat(0.0));
  }

  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);

  PHINode *PN = Builder->CreatePHI(ThenV->getType(), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

Value *ForExprAST::codegen() {
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

  Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  Builder->CreateStore(StartVal, Alloca);

  BasicBlock *CondBB = BasicBlock::Create(*TheContext, "forcond", TheFunction);
  BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "forloop");
  BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterfor");

  // Push loop context for break/continue
  LoopCondBlocks.push_back(CondBB);
  LoopEndBlocks.push_back(AfterBB);

  Builder->CreateBr(CondBB);
  Builder->SetInsertPoint(CondBB);

  Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  Value *CurVar = Builder->CreateLoad(Type::getDoubleTy(*TheContext), Alloca);
  Value *Cmp = Builder->CreateFCmpOLT(CurVar, EndCond, "forcond");

  Builder->CreateCondBr(Cmp, LoopBB, AfterBB);

  TheFunction->insert(TheFunction->end(), LoopBB);
  Builder->SetInsertPoint(LoopBB);

  AllocaInst *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Alloca;

  if (!Body->codegen())
    return nullptr;

  Value *StepVal =
      Step ? Step->codegen() : ConstantFP::get(*TheContext, APFloat(1.0));
  if (!StepVal)
    return nullptr;

  CurVar = Builder->CreateLoad(Type::getDoubleTy(*TheContext), Alloca);
  Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
  Builder->CreateStore(NextVar, Alloca);

  Builder->CreateBr(CondBB);

  TheFunction->insert(TheFunction->end(), AfterBB);
  Builder->SetInsertPoint(AfterBB);

  // Pop loop context
  LoopCondBlocks.pop_back();
  LoopEndBlocks.pop_back();

  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  return ConstantFP::get(*TheContext, APFloat(0.0));
}

Value *WhileExprAST::codegen() {
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  BasicBlock *CondBB =
      BasicBlock::Create(*TheContext, "whilecond", TheFunction);
  BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "whilebody");
  BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterwhile");

  // Push loop context for break/continue
  LoopCondBlocks.push_back(CondBB);
  LoopEndBlocks.push_back(AfterBB);

  Builder->CreateBr(CondBB);
  Builder->SetInsertPoint(CondBB);

  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  if (CondV->getType()->isDoubleTy()) {
    CondV = Builder->CreateFCmpONE(
        CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "whilecond");
  }

  Builder->CreateCondBr(CondV, LoopBB, AfterBB);

  TheFunction->insert(TheFunction->end(), LoopBB);
  Builder->SetInsertPoint(LoopBB);

  if (!Body->codegen())
    return nullptr;

  Builder->CreateBr(CondBB);

  TheFunction->insert(TheFunction->end(), AfterBB);
  Builder->SetInsertPoint(AfterBB);

  // Pop loop context
  LoopCondBlocks.pop_back();
  LoopEndBlocks.pop_back();

  return ConstantFP::get(*TheContext, APFloat(0.0));
}

Value *BreakExprAST::codegen() {
  if (LoopEndBlocks.empty())
    return LogErrorV("break used outside of loop");

  Builder->CreateBr(LoopEndBlocks.back());

  // Create unreachable block for code after break
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  BasicBlock *UnreachableBB =
      BasicBlock::Create(*TheContext, "afterbreak", TheFunction);
  Builder->SetInsertPoint(UnreachableBB);

  return ConstantFP::get(*TheContext, APFloat(0.0));
}

Value *ContinueExprAST::codegen() {
  if (LoopCondBlocks.empty())
    return LogErrorV("continue used outside of loop");

  Builder->CreateBr(LoopCondBlocks.back());

  // Create unreachable block for code after continue
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  BasicBlock *UnreachableBB =
      BasicBlock::Create(*TheContext, "aftercontinue", TheFunction);
  Builder->SetInsertPoint(UnreachableBB);

  return ConstantFP::get(*TheContext, APFloat(0.0));
}

Value *StructExprAST::codegen() {
  auto it = StructTypes.find(StructName);
  if (it == StructTypes.end())
    return LogErrorV("Unknown struct type");

  auto llvmIt = LLVMStructTypes.find(StructName);
  if (llvmIt == LLVMStructTypes.end())
    return LogErrorV("LLVM struct type not found");

  llvm::StructType *StructTy = llvmIt->second;

  // Allocate struct
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, "struct", StructTy);

  // Initialize fields
  const StructDef &Def = it->second;
  for (size_t i = 0; i < Fields.size(); ++i) {
    const auto &Field = Fields[i];

    // Find field index
    int FieldIdx = -1;
    for (size_t j = 0; j < Def.Fields.size(); ++j) {
      if (Def.Fields[j].Name == Field.first) {
        FieldIdx = j;
        break;
      }
    }

    if (FieldIdx < 0) {
      fprintf(stderr, "Unknown field: %s\n", Field.first.c_str());
      continue;
    }

    Value *FieldVal = Field.second->codegen();
    if (!FieldVal)
      return nullptr;

    Value *FieldPtr = Builder->CreateStructGEP(StructTy, Alloca, FieldIdx);
    Builder->CreateStore(FieldVal, FieldPtr);
  }

  return Builder->CreateLoad(StructTy, Alloca);
}

Value *MemberExprAST::codegen() {
  Value *ObjVal = Object->codegen();
  if (!ObjVal)
    return nullptr;

  Type *ObjType = ObjVal->getType();

  // Handle Struct Value (Reg) - VariableExpr returns loaded value
  if (ObjType->isStructTy()) {
    llvm::StructType *ST = cast<llvm::StructType>(ObjType);
    std::string StructName = ST->getName().str();

    // Look up field index
    auto it = StructTypes.find(StructName);
    if (it == StructTypes.end())
      return LogErrorV("Unknown struct type for member access");

    int FieldIdx = -1;
    const auto &Fields = it->second.Fields;
    for (size_t i = 0; i < Fields.size(); ++i) {
      if (Fields[i].Name == Member) {
        FieldIdx = i;
        break;
      }
    }

    if (FieldIdx == -1)
      return LogErrorV("Unknown field name");

    return Builder->CreateExtractValue(ObjVal, FieldIdx, "membertmp");
  }

  return LogErrorV("Attempted member access on non-struct type");
}

//===----------------------------------------------------------------------===//
// Function Code Generation
//===----------------------------------------------------------------------===//

Function *PrototypeAST::codegen() {
  // For now, all function parameters and return types are double
  // Type annotations are parsed but type checking is deferred to future
  std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*TheContext));

  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);
  Function *F = Function::Create(FT, Function::ExternalLinkage, getName(),
                                 TheModule.get());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++].Name);

  return F;
}

Function *FunctionAST::codegen() {
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  unsigned Idx = 0;
  for (auto &Arg : TheFunction->args()) {
    AllocaInst *Alloca = CreateEntryBlockAlloca(
        TheFunction, std::string(Arg.getName()), Arg.getType());
    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = Alloca;
    ++Idx;
  }

  if (Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    verifyFunction(*TheFunction);
    return TheFunction;
  }

  TheFunction->eraseFromParent();
  return nullptr;
}

void StructDefAST::codegen() {
  std::vector<Type *> FieldTypes;
  for (const auto &Field : Fields) {
    FieldTypes.push_back(GetLLVMType(Field.Type));
  }

  llvm::StructType *StructTy =
      llvm::StructType::create(*TheContext, FieldTypes, Name);
  LLVMStructTypes[Name] = StructTy;

  // Register in global struct registry
  StructTypes[Name] = {Name, Fields};
}

void GlobalVarAST::codegen() {
  TheModule->getOrInsertGlobal(Name, GetLLVMType(Type));
  GlobalVariable *GVar = TheModule->getNamedGlobal(Name);
  GVar->setLinkage(GlobalValue::ExternalLinkage);

  // TODO: Add initializer support (requires constant expr or init function)
  // For now, zero initialize
  GVar->setInitializer(Constant::getNullValue(GetLLVMType(Type)));
}
