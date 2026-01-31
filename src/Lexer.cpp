#include "Lexer.h"
#include "Parser.h"
#include <cctype>
#include <cstdlib>

// Global lexer state
std::string IdentifierStr;
std::string StringValue;
double NumVal;
FILE *InputFile = nullptr;
std::string CurrentFilePath;

static int LastChar = ' ';

void setInputFile(FILE *file, const std::string &path) {
  InputFile = file;
  CurrentFilePath = path;
  LastChar = ' ';
}

LexerState saveLexerState() {
  LexerState state;
  state.File = InputFile;
  state.FilePath = CurrentFilePath;
  state.LastChar = LastChar;
  state.CurToken = CurTok;
  state.IdentStr = IdentifierStr;
  state.StrVal = StringValue;
  state.NumValue = NumVal;
  return state;
}

void restoreLexerState(const LexerState &state) {
  InputFile = state.File;
  CurrentFilePath = state.FilePath;
  LastChar = state.LastChar;
  CurTok = state.CurToken;
  IdentifierStr = state.IdentStr;
  StringValue = state.StrVal;
  NumVal = state.NumValue;
}

int gettok() {
  while (isspace(LastChar))
    LastChar = fgetc(InputFile);

  // Identifier and keywords
  if (isalpha(LastChar) || LastChar == '_') {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = fgetc(InputFile))) || LastChar == '_')
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    if (IdentifierStr == "let")
      return tok_let;
    if (IdentifierStr == "mut")
      return tok_mut;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "then")
      return tok_then;
    if (IdentifierStr == "else")
      return tok_else;
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "in")
      return tok_in;
    if (IdentifierStr == "while")
      return tok_while;
    if (IdentifierStr == "do")
      return tok_do;
    if (IdentifierStr == "end")
      return tok_end;
    if (IdentifierStr == "import")
      return tok_import;
    if (IdentifierStr == "export")
      return tok_export;
    if (IdentifierStr == "break")
      return tok_break;
    if (IdentifierStr == "continue")
      return tok_continue;
    if (IdentifierStr == "type")
      return tok_type;
    if (IdentifierStr == "struct")
      return tok_struct;
    if (IdentifierStr == "int")
      return tok_int;
    if (IdentifierStr == "float")
      return tok_float;
    if (IdentifierStr == "string")
      return tok_string;
    if (IdentifierStr == "bool")
      return tok_bool;
    if (IdentifierStr == "static")
      return tok_static;
    if (IdentifierStr == "abstract")
      return tok_abstract;
    return tok_identifier;
  }

  // String literal: "..."
  if (LastChar == '"') {
    StringValue = "";
    while ((LastChar = fgetc(InputFile)) != '"' && LastChar != EOF) {
      if (LastChar == '\\') {
        LastChar = fgetc(InputFile);
        switch (LastChar) {
        case 'n':
          StringValue += '\n';
          break;
        case 't':
          StringValue += '\t';
          break;
        case '\\':
          StringValue += '\\';
          break;
        case '"':
          StringValue += '"';
          break;
        default:
          StringValue += LastChar;
          break;
        }
      } else {
        StringValue += LastChar;
      }
    }
    LastChar = fgetc(InputFile);
    return tok_string_lit;
  }

  // Number
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = fgetc(InputFile);
    } while (isdigit(LastChar) || LastChar == '.');
    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  // Arrow: ->
  if (LastChar == '-') {
    int NextChar = fgetc(InputFile);
    if (NextChar == '>') {
      LastChar = fgetc(InputFile);
      return tok_arrow;
    }
    // Not arrow, just minus
    ungetc(NextChar, InputFile);
    int ThisChar = LastChar;
    LastChar = fgetc(InputFile);
    return ThisChar;
  }

  // Colon: :
  if (LastChar == ':') {
    LastChar = fgetc(InputFile);
    return tok_colon;
  }

  // Equal: == or =
  if (LastChar == '=') {
    int NextChar = fgetc(InputFile);
    if (NextChar == '=') {
      LastChar = fgetc(InputFile);
      return tok_eq;
    }
    ungetc(NextChar, InputFile);
    int ThisChar = LastChar;
    LastChar = fgetc(InputFile);
    return ThisChar;
  }

  if (LastChar == ':') {
    LastChar = fgetc(InputFile);
    return tok_colon;
  }

  // Not Equal: !=
  if (LastChar == '!') {
    int NextChar = fgetc(InputFile);
    if (NextChar == '=') {
      LastChar = fgetc(InputFile);
      return tok_ne;
    }
    ungetc(NextChar, InputFile);
    int ThisChar = LastChar;
    LastChar = fgetc(InputFile);
    return ThisChar;
  }

  // Comment: # until end of line
  if (LastChar == '#') {
    do
      LastChar = fgetc(InputFile);
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = fgetc(InputFile);
  return ThisChar;
}
