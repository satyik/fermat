#ifndef BORROWCHECK_H
#define BORROWCHECK_H

#include <map>
#include <string>
#include <vector>

/// Tracks the state of a variable for borrow checking
struct VariableState {
  bool IsMutable = false;    // Was declared with 'mut'
  bool IsMoved = false;      // Has ownership been transferred?
  bool IsInitialized = true; // Has a value?
  int ImmutableBorrows = 0;  // Count of &x borrows
  int MutableBorrows = 0;    // Count of &mut x borrows (max 1)
  int ScopeLevel = 0;        // Scope where declared
  int Line = 0;              // Line where declared (for errors)
};

/// Compile-time borrow checker
class BorrowChecker {
  std::map<std::string, VariableState> Variables;
  int CurrentScope = 0;
  std::vector<std::string> Errors;
  int CurrentLine = 1;

public:
  /// Enter a new scope (function body, block, etc.)
  void enterScope() { CurrentScope++; }

  /// Exit scope - releases all variables declared in this scope
  void exitScope();

  /// Set current line for error reporting
  void setLine(int Line) { CurrentLine = Line; }

  /// Declare a new variable
  void declareVariable(const std::string &Name, bool IsMutable);

  /// Check if variable can be used (not moved)
  bool checkUse(const std::string &Name);

  /// Check if variable can be assigned to (must be mutable)
  bool checkAssign(const std::string &Name);

  /// Transfer ownership (move)
  void moveVariable(const std::string &Name);

  /// Borrow a variable immutably
  bool borrowImmutable(const std::string &Name);

  /// Borrow a variable mutably
  bool borrowMutable(const std::string &Name);

  /// Release a borrow
  void releaseBorrow(const std::string &Name, bool Mutable);

  /// Check if a variable exists
  bool exists(const std::string &Name) const;

  /// Check if variable is mutable
  bool isMutable(const std::string &Name) const;

  /// Get all errors
  const std::vector<std::string> &getErrors() const { return Errors; }

  /// Check if there are any errors
  bool hasErrors() const { return !Errors.empty(); }

  /// Clear all errors
  void clearErrors() { Errors.clear(); }

  /// Report an error
  void reportError(const std::string &Msg);
};

/// Global borrow checker instance
extern BorrowChecker TheBorrowChecker;

#endif // BORROWCHECK_H
