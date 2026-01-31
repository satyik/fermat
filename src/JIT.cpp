#include "JIT.h"
#include "BorrowCheck.h"
#include "CodeGen.h"
#include "Lexer.h"
#include "Parser.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include <cstdio>
#include <unistd.h>

using namespace llvm;
using namespace llvm::orc;

static bool checkBorrowErrors() {
  if (TheBorrowChecker.hasErrors()) {
    for (const auto &Err : TheBorrowChecker.getErrors()) {
      fprintf(stderr, "%s\n", Err.c_str());
    }
    TheBorrowChecker.clearErrors();
    return true;
  }
  return false;
}

void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (checkBorrowErrors())
      return;

    if (auto *FnIR = FnAST->codegen()) {
      if (isatty(fileno(InputFile)))
        fprintf(stderr, "Parsed function definition.\n");

      auto TSCtx = std::make_unique<ThreadSafeContext>(std::move(TheContext));
      ExitOnErr(TheJIT->addIRModule(
          ThreadSafeModule(std::move(TheModule), std::move(*TSCtx))));
      InitializeModuleAndPassManager();
    }
  } else {
    getNextToken();
  }
}

void HandleExport() {
  fprintf(stderr, "DEBUG: Parsing Export\n");
  getNextToken(); // eat 'export'
  if (CurTok == tok_def) {
    HandleDefinition();
  } else if (CurTok == tok_type || CurTok == tok_struct) {
    HandleStructDef();
  } else {
    fprintf(stderr, "Error: Expected 'def' or 'type' after 'export'\n");
    getNextToken();
  }
}

void HandleImport() {
  if (!ParseImport()) {
  }
}

void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      if (isatty(fileno(InputFile)))
        fprintf(stderr, "Parsed an extern\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    getNextToken();
  }
}

// Helper to handle static variables in JIT
void HandleStaticVar() {
  if (auto GlobalAST = ParseStaticVar()) {
    GlobalAST->codegen();
    if (isatty(fileno(InputFile)))
      fprintf(stderr, "Parsed static variable.\n");
  } else {
    getNextToken();
  }
}

void HandleStructDef() {
  if (auto StructAST = ParseStructDef()) {
    StructAST->codegen();
    if (isatty(fileno(InputFile))) {
      if (StructAST->isAbstract())
        fprintf(stderr, "Parsed abstract struct definition.\n");
      else
        fprintf(stderr, "Parsed struct definition.\n");
    }
  } else {
    getNextToken();
  }
}

void HandleTopLevelExpression() {
  if (auto FnAST = ParseTopLevelExpr()) {
    if (checkBorrowErrors())
      return;

    if (FnAST->codegen()) {
      auto TSCtx = std::make_unique<ThreadSafeContext>(std::move(TheContext));
      auto TSM = ThreadSafeModule(std::move(TheModule), std::move(*TSCtx));
      ExitOnErr(TheJIT->addIRModule(std::move(TSM)));
      InitializeModuleAndPassManager();

      auto ExprSymbol = ExitOnErr(TheJIT->lookup(CurrentAnonName + "$0"));
      auto *FP = ExprSymbol.toPtr<double (*)()>();
      double val = FP();
      if (isatty(fileno(InputFile)))
        fprintf(stdout, "%.10g\n", val);
    }
  } else {
    getNextToken();
  }
}

void MainLoop() {
  while (true) {
    if (isatty(fileno(InputFile)))
      fprintf(stderr, "ready> ");

    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_export:
      HandleExport();
      break;
    case tok_import:
      HandleImport();
      break;
    case tok_extern:
      HandleExtern();
      break;
    case tok_type:
      HandleStructDef();
      break;
    case tok_static:
      HandleStaticVar();
      break;
    case tok_abstract:
      HandleStructDef();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}
