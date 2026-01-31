//===----------------------------------------------------------------------===//
// Fermat - A simple JIT-compiled programming language
//===----------------------------------------------------------------------===//

#include "CodeGen.h"
#include "JIT.h"
#include "Lexer.h"
#include "Parser.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdio>
#include <string>

using namespace llvm;

int main(int argc, char *argv[]) {
  // 1. Initialize LLVM native target for JIT
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  // 2. Setup operator precedences
  BinopPrecedence['<'] = 10;
  BinopPrecedence['>'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;
  BinopPrecedence[';'] = 1;
  BinopPrecedence[tok_eq] = 10;
  BinopPrecedence[tok_ne] = 10;

  // 3. Handle input source (file vs terminal)
  std::string filepath = ".";
  if (argc > 1) {
    filepath = argv[1];
    FILE *file = fopen(filepath.c_str(), "r");
    if (!file) {
      fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
      return 1;
    }
    setInputFile(file, filepath);
  } else {
    setInputFile(stdin, ".");
  }

  // 4. Create the JIT engine
  auto JITExpected = LLJITBuilder().create();
  if (!JITExpected) {
    errs() << "Failed to create JIT: " << toString(JITExpected.takeError())
           << "\n";
    return 1;
  }
  TheJIT = std::move(*JITExpected);

  // Register host process symbols for Runtime.cpp
  const char dl_pre[2] = {0, 0}; // Empty prefix for dlsym
  TheJIT->getMainJITDylib().addGenerator(
      cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(dl_pre[0])));

  // 5. Initialize fresh module
  InitializeModuleAndPassManager();

  // 6. Prime the first token
  getNextToken();

  // 7. Run the interpreter loop
  MainLoop();

  // 8. Cleanup - must reset JIT before static globals are destroyed
  TheModule.reset();
  TheContext.reset();
  TheJIT.reset();

  if (InputFile != stdin)
    fclose(InputFile);

  return 0;
}