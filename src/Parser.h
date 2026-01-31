#ifndef PARSER_H
#define PARSER_H

#include "AST.h"
#include <map>
#include <memory>
#include <vector>

// Current token being parsed
extern int CurTok;
extern std::map<int, int> BinopPrecedence;

// Global to store current anon name for lookup
extern std::string CurrentAnonName;

// Loop context for break/continue
extern std::vector<llvm::BasicBlock *> LoopEndBlocks;
extern std::vector<llvm::BasicBlock *> LoopCondBlocks;

// Get the next token
int getNextToken();

// Parsing functions
std::unique_ptr<ExprAST> ParseExpression();
std::unique_ptr<ExprAST> ParsePrimary();
std::unique_ptr<PrototypeAST> ParsePrototype();
std::unique_ptr<FunctionAST> ParseDefinition();
std::unique_ptr<PrototypeAST> ParseExtern();
std::unique_ptr<FunctionAST> ParseTopLevelExpr();
std::unique_ptr<GlobalVarAST>
ParseStaticVar(); // New parser for static variables

// Control flow parsers
std::unique_ptr<ExprAST> ParseLetExpr();
std::unique_ptr<ExprAST> ParseIfExpr();
std::unique_ptr<ExprAST> ParseForExpr();
std::unique_ptr<ExprAST> ParseWhileExpr();
std::unique_ptr<ExprAST> ParseBreakExpr();
std::unique_ptr<ExprAST> ParseContinueExpr();

// Type parsers
TypeInfo ParseType();
std::unique_ptr<StructDefAST> ParseStructDef();

// Module system parser
bool ParseImport();
bool loadModule(const std::string &filename);

// Error handling
std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);

#endif // PARSER_H
