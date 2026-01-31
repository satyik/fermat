#ifndef LEXER_H
#define LEXER_H

#include <cstdio>
#include <string>

//===----------------------------------------------------------------------===//
// Token Types
//===----------------------------------------------------------------------===//

enum Token {
  tok_eof = -1,

  // Keywords
  tok_def = -2,
  tok_extern = -3,
  tok_let = -4,
  tok_mut = -5,
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,
  tok_for = -9,
  tok_in = -10,
  tok_while = -11,
  tok_do = -12,
  tok_end = -13,
  tok_import = -14,
  tok_export = -15,

  // Loop control
  tok_break = -16,
  tok_continue = -17,

  // Type keywords
  tok_type = -18,
  tok_struct = -19,
  tok_int = -25,
  tok_float = -26,
  tok_string = -27,
  tok_bool = -28,

  // New keywords
  tok_static = -29,
  tok_abstract = -30,

  // Primary tokens
  tok_identifier = -20,
  tok_number = -21,
  tok_string_lit = -22,

  // Operators
  tok_arrow = -23, // ->
  tok_ne = -31,
  tok_eq = -32,
  tok_colon = -24, // :
};

// Global state for the lexer
extern std::string IdentifierStr;
extern std::string StringValue;
extern double NumVal;
extern FILE *InputFile;
extern std::string CurrentFilePath;

// Lexer state structure for save/restore
struct LexerState {
  FILE *File;
  std::string FilePath;
  int LastChar;
  int CurToken;
  std::string IdentStr;
  std::string StrVal;
  double NumValue;
};

int gettok();
void setInputFile(FILE *file, const std::string &path);
LexerState saveLexerState();
void restoreLexerState(const LexerState &state);

#endif // LEXER_H
