#ifndef JIT_H
#define JIT_H

// Main interpreter loop
void MainLoop();

// Handler functions
void HandleDefinition();
void HandleExtern();
void HandleTopLevelExpression();
void HandleExport();
void HandleImport();
void HandleStructDef();
void HandleStaticVar();

#endif // JIT_H
