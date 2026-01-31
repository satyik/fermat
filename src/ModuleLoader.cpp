#include "ModuleLoader.h"
#include "CodeGen.h"
#include "JIT.h"
#include "Lexer.h"
#include "Parser.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include <cstdio>
#include <filesystem>

using namespace llvm;
using namespace llvm::orc;

std::set<std::string> ImportedModules;

std::string getFileDirectory(const std::string &filepath) {
  std::filesystem::path p(filepath);
  return p.parent_path().string();
}

std::string resolvePath(const std::string &basePath,
                        const std::string &relativePath) {
  std::filesystem::path base(basePath);
  std::filesystem::path rel(relativePath);

  if (rel.is_absolute())
    return rel.string();

  std::filesystem::path resolved = base / rel;
  return std::filesystem::weakly_canonical(resolved).string();
}

bool loadModule(const std::string &filename) {
  // Resolve path relative to current file's directory
  std::string baseDir = getFileDirectory(CurrentFilePath);
  std::string fullPath = resolvePath(baseDir, filename);

  // Check if already imported (circular import prevention)
  if (ImportedModules.count(fullPath)) {
    return true;
  }
  ImportedModules.insert(fullPath);

  // Save current lexer state BEFORE opening new file
  LexerState savedState = saveLexerState();

  // Open the module file
  FILE *moduleFile = fopen(fullPath.c_str(), "r");
  if (!moduleFile) {
    fprintf(stderr, "Error: Cannot open module '%s'\n", fullPath.c_str());
    return false;
  }

  // Set up lexer for the new file
  setInputFile(moduleFile, fullPath);
  getNextToken(); // Prime the lexer

  // Parse the module - only process definitions and exports
  while (CurTok != tok_eof) {
    switch (CurTok) {
    case ';':
      getNextToken();
      break;
    case tok_export:
      getNextToken(); // eat 'export'
      if (CurTok == tok_def) {
        if (auto FnAST = ParseDefinition()) {
          if (auto *FnIR = FnAST->codegen()) {
            auto TSCtx =
                std::make_unique<ThreadSafeContext>(std::move(TheContext));
            ExitOnErr(TheJIT->addIRModule(
                ThreadSafeModule(std::move(TheModule), std::move(*TSCtx))));
            InitializeModuleAndPassManager();
          }
        }
      } else if (CurTok == tok_type || CurTok == tok_struct) {
        HandleStructDef();
      } else {
        fprintf(stderr,
                "Error: Expected 'def', 'type', or 'struct' after 'export'\n");
        getNextToken();
      }
      break;
    case tok_def:
      if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
          auto TSCtx =
              std::make_unique<ThreadSafeContext>(std::move(TheContext));
          ExitOnErr(TheJIT->addIRModule(
              ThreadSafeModule(std::move(TheModule), std::move(*TSCtx))));
          InitializeModuleAndPassManager();
        }
      }
      break;
    case tok_import:
      getNextToken(); // eat 'import'
      if (CurTok == tok_string) {
        std::string nestedModule = StringValue;
        getNextToken(); // eat string
        loadModule(nestedModule);
      }
      break;
    case tok_extern:
      if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
          FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        }
      } else {
        getNextToken();
      }
      break;
    case tok_static:
      if (auto GlobalAST = ParseStaticVar()) {
        GlobalAST->codegen();
      } else {
        getNextToken();
      }
      break;
    case tok_type:
    case tok_struct:
    case tok_abstract:
      HandleStructDef();
      break;
    default:
      getNextToken();
      break;
    }
  }

  // Close the module file
  fclose(moduleFile);

  // Restore original lexer state
  restoreLexerState(savedState);

  return true;
}
