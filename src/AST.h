#ifndef AST_H
#define AST_H

#include "llvm/IR/Value.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

//===----------------------------------------------------------------------===//
// Type System
//===----------------------------------------------------------------------===//

enum class SpyType { Unknown, Int, Float, String, Bool, Struct, Void };

// Type information for variables and functions
struct TypeInfo {
  SpyType BaseType = SpyType::Unknown;
  std::string StructName; // For struct types

  TypeInfo() = default;
  TypeInfo(SpyType t) : BaseType(t) {}
  TypeInfo(const std::string &structName)
      : BaseType(SpyType::Struct), StructName(structName) {}

  bool operator==(const TypeInfo &other) const {
    return BaseType == other.BaseType && StructName == other.StructName;
  }

  std::string toString() const {
    switch (BaseType) {
    case SpyType::Int:
      return "int";
    case SpyType::Float:
      return "float";
    case SpyType::String:
      return "string";
    case SpyType::Bool:
      return "bool";
    case SpyType::Void:
      return "void";
    case SpyType::Struct:
      return StructName;
    default:
      return "unknown";
    }
  }
};

// Struct field definition
struct StructField {
  std::string Name;
  TypeInfo Type;
};

// Struct type definition
struct StructDef {
  std::string Name;
  std::vector<StructField> Fields;
};

// Global struct registry
extern std::map<std::string, StructDef> StructTypes;

//===----------------------------------------------------------------------===//
// Ownership Types
//===----------------------------------------------------------------------===//

enum class Mutability { Immutable, Mutable };

enum class Ownership { Owned, Borrowed, BorrowedMut };

//===----------------------------------------------------------------------===//
// Expression AST Nodes
//===----------------------------------------------------------------------===//

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual Value *codegen() = 0;
  virtual TypeInfo getType() const { return TypeInfo(SpyType::Float); }
};

class NumberExprAST : public ExprAST {
  double Val;
  bool IsInt;

public:
  NumberExprAST(double Val, bool isInt = false) : Val(Val), IsInt(isInt) {}
  Value *codegen() override;
  double getValue() const { return Val; }
  TypeInfo getType() const override {
    return TypeInfo(IsInt ? SpyType::Int : SpyType::Float);
  }
};

class StringExprAST : public ExprAST {
  std::string Val;

public:
  StringExprAST(const std::string &Val) : Val(Val) {}
  Value *codegen() override;
  const std::string &getValue() const { return Val; }
  TypeInfo getType() const override { return TypeInfo(SpyType::String); }
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
  Value *codegen() override;
  const std::string &getName() const { return Name; }
};

class UnaryExprAST : public ExprAST {
  int Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(int Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}
  Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  int Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  Value *codegen() override;
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}
  Value *codegen() override;
  // Construct mangled name for lookups
  std::string getMangledName() const {
    return Callee + "$" + std::to_string(Args.size());
  }
};

class LetExprAST : public ExprAST {
  std::string Name;
  Mutability Mut;
  TypeInfo DeclaredType;
  std::unique_ptr<ExprAST> Init;
  std::unique_ptr<ExprAST> Body;

public:
  LetExprAST(const std::string &Name, Mutability Mut, TypeInfo Type,
             std::unique_ptr<ExprAST> Init, std::unique_ptr<ExprAST> Body)
      : Name(Name), Mut(Mut), DeclaredType(Type), Init(std::move(Init)),
        Body(std::move(Body)) {}
  Value *codegen() override;
  const std::string &getName() const { return Name; }
  bool isMutable() const { return Mut == Mutability::Mutable; }
};

class AssignExprAST : public ExprAST {
  std::string Name;
  std::unique_ptr<ExprAST> Value_;

public:
  AssignExprAST(const std::string &Name, std::unique_ptr<ExprAST> Value)
      : Name(Name), Value_(std::move(Value)) {}
  Value *codegen() override;
  const std::string &getName() const { return Name; }
};

class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
  Value *codegen() override;
};

class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
             std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
             std::unique_ptr<ExprAST> Body)
      : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
        Step(std::move(Step)), Body(std::move(Body)) {}
  Value *codegen() override;
};

class WhileExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Body;

public:
  WhileExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Body)
      : Cond(std::move(Cond)), Body(std::move(Body)) {}
  Value *codegen() override;
};

class BreakExprAST : public ExprAST {
public:
  BreakExprAST() = default;
  Value *codegen() override;
};

class ContinueExprAST : public ExprAST {
public:
  ContinueExprAST() = default;
  Value *codegen() override;
};

// Struct instantiation: Point{x: 1.0, y: 2.0}
class StructExprAST : public ExprAST {
  std::string StructName;
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Fields;

public:
  StructExprAST(
      const std::string &Name,
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Fields)
      : StructName(Name), Fields(std::move(Fields)) {}
  Value *codegen() override;
  TypeInfo getType() const override { return TypeInfo(StructName); }
};

// Field access: obj.field
class MemberExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Object;
  std::string Member;

public:
  MemberExprAST(std::unique_ptr<ExprAST> Object, const std::string &Member)
      : Object(std::move(Object)), Member(Member) {}
  Value *codegen() override;
};

//===----------------------------------------------------------------------===//
// Function AST Nodes
//===----------------------------------------------------------------------===//

struct TypedArg {
  std::string Name;
  TypeInfo Type;
};

class PrototypeAST {
  std::string Name;
  std::vector<TypedArg> Args;
  TypeInfo ReturnType;

  std::string MangledName;
  bool IsExtern = false;

public:
  PrototypeAST(const std::string &Name, std::vector<TypedArg> Args,
               TypeInfo RetType = TypeInfo(SpyType::Float))
      : Name(Name), Args(std::move(Args)), ReturnType(RetType) {
    // Basic arity-based mangling: name$arity
    // e.g., add(x) -> add$1, add(x, y) -> add$2
    MangledName = Name + "$" + std::to_string(this->Args.size());
  }

  void setIsExtern(bool isExtern) { IsExtern = isExtern; }

  const std::string &getName() const {
    return IsExtern ? Name : MangledName;
  } // Use mangled name as the primary name
  const std::string &getOriginalName() const { return Name; }
  const std::vector<TypedArg> &getArgs() const { return Args; }
  const TypeInfo &getReturnType() const { return ReturnType; }
  Function *codegen();
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}
  Function *codegen();
};

// Struct type definition AST
// Static global variable AST
class GlobalVarAST {
  std::string Name;
  TypeInfo Type;
  std::unique_ptr<ExprAST> Init;

public:
  GlobalVarAST(const std::string &Name, TypeInfo Type,
               std::unique_ptr<ExprAST> Init)
      : Name(Name), Type(Type), Init(std::move(Init)) {}
  void codegen();
  const std::string &getName() const { return Name; }
};

// Struct type definition AST
class StructDefAST {
  std::string Name;
  std::vector<StructField> Fields;
  bool IsAbstract;

public:
  StructDefAST(const std::string &Name, std::vector<StructField> Fields,
               bool IsAbstract = false)
      : Name(Name), Fields(std::move(Fields)), IsAbstract(IsAbstract) {}
  void codegen();
  const std::string &getName() const { return Name; }
  bool isAbstract() const { return IsAbstract; }
};

#endif // AST_H
