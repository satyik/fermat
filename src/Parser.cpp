#include "Parser.h"
#include "BorrowCheck.h"
#include "CodeGen.h"
#include "Lexer.h"
#include "ModuleLoader.h"
#include <cstdio>

// Parser state
int CurTok;
std::map<int, int> BinopPrecedence;
std::string CurrentAnonName;

// Loop context for break/continue
std::vector<llvm::BasicBlock *> LoopEndBlocks;
std::vector<llvm::BasicBlock *> LoopCondBlocks;

// Struct type registry
std::map<std::string, StructDef> StructTypes;

int getNextToken() { return CurTok = gettok(); }

std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

// Forward declarations
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS);

/// Parse type annotation: int, float, string, bool, or struct name
TypeInfo ParseType() {
  switch (CurTok) {
  case tok_int:
    getNextToken();
    return TypeInfo(SpyType::Int);
  case tok_float:
    getNextToken();
    return TypeInfo(SpyType::Float);
  case tok_string:
    getNextToken();
    return TypeInfo(SpyType::String);
  case tok_bool:
    getNextToken();
    return TypeInfo(SpyType::Bool);
  case tok_identifier: {
    std::string typeName = IdentifierStr;
    getNextToken();
    return TypeInfo(typeName);
  }
  default:
    return TypeInfo(SpyType::Unknown);
  }
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  // All literals are float by default - use type annotations for int
  auto Result = std::make_unique<NumberExprAST>(NumVal, false);
  getNextToken();
  return std::move(Result);
}

static std::unique_ptr<ExprAST> ParseStringExpr() {
  auto Result = std::make_unique<StringExprAST>(StringValue);
  getNextToken();
  return std::move(Result);
}

static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat '('
  auto V = ParseExpression();
  if (!V)
    return nullptr;
  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ')'
  return V;
}

static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  TheBorrowChecker.checkUse(IdName);

  // Check for struct instantiation: Point{...}
  if (CurTok == '{') {
    getNextToken(); // eat '{'
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Fields;

    if (CurTok != '}') {
      while (true) {
        if (CurTok != tok_identifier)
          return LogError("expected field name in struct literal");
        std::string FieldName = IdentifierStr;
        getNextToken();

        if (CurTok != tok_colon)
          return LogError("expected ':' after field name");
        getNextToken();

        auto Val = ParseExpression();
        if (!Val)
          return nullptr;

        Fields.push_back({FieldName, std::move(Val)});

        if (CurTok == '}')
          break;
        if (CurTok != ',')
          return LogError("expected ',' or '}' in struct literal");
        getNextToken();
      }
    }
    getNextToken(); // eat '}'
    return std::make_unique<StructExprAST>(IdName, std::move(Fields));
  }

  // Check for assignment: name = expr
  if (CurTok == '=') {
    getNextToken(); // eat '='
    if (!TheBorrowChecker.checkAssign(IdName)) {
    }
    auto Value = ParseExpression();
    if (!Value)
      return nullptr;
    return std::make_unique<AssignExprAST>(IdName, std::move(Value));
  }

  // Check for member access: name.field
  if (CurTok == '.') {
    std::unique_ptr<ExprAST> Obj = std::make_unique<VariableExprAST>(IdName);
    while (CurTok == '.') {
      getNextToken(); // eat '.'
      if (CurTok != tok_identifier)
        return LogError("expected field name after '.'");
      std::string Member = IdentifierStr;
      getNextToken();
      Obj = std::make_unique<MemberExprAST>(std::move(Obj), Member);
    }
    return Obj;
  }

  if (CurTok != '(') // Simple variable ref
    return std::make_unique<VariableExprAST>(IdName);

  // Function call
  getNextToken(); // eat '('
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;
      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }
  getNextToken(); // eat ')'
  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// Parse let expression: let [mut] name[: type] = init [body]
std::unique_ptr<ExprAST> ParseLetExpr() {
  getNextToken(); // eat 'let'

  Mutability Mut = Mutability::Immutable;
  if (CurTok == tok_mut) {
    Mut = Mutability::Mutable;
    getNextToken();
  }

  if (CurTok != tok_identifier)
    return LogError("expected identifier after 'let'");

  std::string Name = IdentifierStr;
  getNextToken();

  // Optional type annotation
  TypeInfo Type(SpyType::Unknown);
  if (CurTok == tok_colon) {
    getNextToken(); // eat ':'
    Type = ParseType();
  }

  if (CurTok != '=')
    return LogError("expected '=' in let expression");
  getNextToken();

  auto Init = ParseExpression();
  if (!Init)
    return nullptr;

  TheBorrowChecker.declareVariable(Name, Mut == Mutability::Mutable);

  std::unique_ptr<ExprAST> Body = nullptr;
  if (CurTok == ';' || CurTok == tok_eof || CurTok == tok_def ||
      CurTok == tok_end || CurTok == tok_else) {
    return std::make_unique<LetExprAST>(Name, Mut, Type, std::move(Init),
                                        nullptr);
  }

  Body = ParseExpression();
  return std::make_unique<LetExprAST>(Name, Mut, Type, std::move(Init),
                                      std::move(Body));
}

std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextToken(); // eat 'if'

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then)
    return LogError("expected 'then' after if condition");
  getNextToken();

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  std::unique_ptr<ExprAST> Else = nullptr;
  if (CurTok == tok_else) {
    getNextToken();
    Else = ParseExpression();
    if (!Else)
      return nullptr;
  } else if (CurTok == tok_end) {
    getNextToken(); // eat end
  }

  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                     std::move(Else));
}

std::unique_ptr<ExprAST> ParseForExpr() {
  getNextToken(); // eat 'for'

  if (CurTok != tok_identifier)
    return LogError("expected identifier after 'for'");

  std::string IdName = IdentifierStr;
  getNextToken();

  if (CurTok != '=')
    return LogError("expected '=' after for loop variable");
  getNextToken();

  auto Start = ParseExpression();
  if (!Start)
    return nullptr;

  if (CurTok != ',')
    return LogError("expected ',' after for start value");
  getNextToken();

  auto End = ParseExpression();
  if (!End)
    return nullptr;

  std::unique_ptr<ExprAST> Step;
  if (CurTok == ',') {
    getNextToken();
    Step = ParseExpression();
    if (!Step)
      return nullptr;
  } else {
    Step = std::make_unique<NumberExprAST>(1.0);
  }

  if (CurTok != tok_do)
    return LogError("expected 'do' after for loop header");
  getNextToken();

  TheBorrowChecker.enterScope();
  TheBorrowChecker.declareVariable(IdName, true);

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  TheBorrowChecker.exitScope();

  if (CurTok != tok_end)
    return LogError("expected 'end' after for loop body");
  getNextToken();

  return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End),
                                      std::move(Step), std::move(Body));
}

std::unique_ptr<ExprAST> ParseWhileExpr() {
  getNextToken(); // eat 'while'

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_do)
    return LogError("expected 'do' after while condition");
  getNextToken();

  TheBorrowChecker.enterScope();

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  TheBorrowChecker.exitScope();

  if (CurTok != tok_end)
    return LogError("expected 'end' after while loop body");
  getNextToken();

  return std::make_unique<WhileExprAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<ExprAST> ParseBreakExpr() {
  getNextToken(); // eat 'break'
  return std::make_unique<BreakExprAST>();
}

std::unique_ptr<ExprAST> ParseContinueExpr() {
  getNextToken(); // eat 'continue'
  return std::make_unique<ContinueExprAST>();
}

/// Parse struct definition: type Name struct ... end
std::unique_ptr<StructDefAST> ParseStructDef() {
  bool IsAbstract = false;

  // Check for abstract keyword
  if (CurTok == tok_abstract) {
    IsAbstract = true;
    getNextToken(); // eat abstract
  }

  getNextToken(); // eat 'type'

  if (CurTok != tok_identifier)
    return nullptr;

  std::string Name = IdentifierStr;
  getNextToken();

  if (CurTok != tok_struct)
    return nullptr;
  getNextToken();

  std::vector<StructField> Fields;
  while (CurTok != tok_end && CurTok != tok_eof) {
    if (CurTok != tok_identifier)
      break;

    std::string FieldName = IdentifierStr;
    getNextToken();

    if (CurTok != tok_colon) {
      LogError("expected ':' after field name");
      break;
    }
    getNextToken();

    TypeInfo FieldType = ParseType();
    Fields.push_back({FieldName, FieldType});
  }

  if (CurTok == tok_end)
    getNextToken();

  return std::make_unique<StructDefAST>(Name, std::move(Fields), IsAbstract);
}

bool ParseImport() {
  getNextToken(); // eat 'import'
  if (CurTok != tok_string_lit) {
    LogError("expected string after 'import'");
    return false;
  }
  std::string filename = StringValue;
  getNextToken();
  return loadModule(filename);
}

std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case tok_string_lit:
    return ParseStringExpr();
  case tok_let:
    return ParseLetExpr();
  case tok_if:
    return ParseIfExpr();
  case tok_for:
    return ParseForExpr();
  case tok_while:
    return ParseWhileExpr();
  case tok_break:
    return ParseBreakExpr();
  case tok_continue:
    return ParseContinueExpr();
  case '(':
    return ParseParenExpr();
  }
}

static int GetTokPrecedence() {
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();
    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken();

    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }
    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;
  return ParseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");
  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<TypedArg> Args;
  getNextToken(); // eat '('

  while (CurTok == tok_identifier) {
    std::string ArgName = IdentifierStr;
    getNextToken();

    TypeInfo ArgType(SpyType::Float); // Default to float
    if (CurTok == tok_colon) {
      getNextToken();
      ArgType = ParseType();
    }

    Args.push_back({ArgName, ArgType});

    if (CurTok == ')')
      break;
    if (CurTok == ',')
      getNextToken();
  }

  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");
  getNextToken();

  // Parse return type
  TypeInfo RetType(SpyType::Float);
  if (CurTok == tok_arrow) {
    getNextToken();
    RetType = ParseType();
  }

  return std::make_unique<PrototypeAST>(FnName, std::move(Args), RetType);
}

std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken(); // eat 'def'
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  TheBorrowChecker.enterScope();

  for (const auto &Arg : Proto->getArgs()) {
    TheBorrowChecker.declareVariable(Arg.Name, false);
  }

  if (auto E = ParseExpression()) {
    TheBorrowChecker.exitScope();
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }

  TheBorrowChecker.exitScope();
  return nullptr;
}

std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken(); // eat 'extern'
  auto Proto = ParsePrototype();
  if (Proto)
    Proto->setIsExtern(true);
  return Proto;
}

std::unique_ptr<GlobalVarAST> ParseStaticVar() {
  getNextToken(); // eat 'static'
  if (CurTok != tok_identifier) {
    LogError("Expected identifier after static");
    return nullptr;
  }
  std::string Name = IdentifierStr;
  getNextToken();

  TypeInfo Type = TypeInfo(SpyType::Float);
  if (CurTok == tok_colon) {
    getNextToken();
    Type = ParseType();
  }

  std::unique_ptr<ExprAST> Init = nullptr;
  if (CurTok == '=') {
    getNextToken();
    Init = ParseExpression();
  } else {
    // Default init to 0
    Init = std::make_unique<NumberExprAST>(0.0);
  }

  return std::make_unique<GlobalVarAST>(Name, Type, std::move(Init));
}

std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    CurrentAnonName = "anon_expr_" + std::to_string(AnonExprCounter++);
    auto Proto = std::make_unique<PrototypeAST>(CurrentAnonName,
                                                std::vector<TypedArg>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}
