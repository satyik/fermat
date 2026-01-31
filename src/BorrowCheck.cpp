#include "BorrowCheck.h"
#include <sstream>

// Global borrow checker instance
BorrowChecker TheBorrowChecker;

void BorrowChecker::exitScope() {
  // Remove all variables declared in the current scope
  auto it = Variables.begin();
  while (it != Variables.end()) {
    if (it->second.ScopeLevel == CurrentScope) {
      it = Variables.erase(it);
    } else {
      ++it;
    }
  }
  CurrentScope--;
}

void BorrowChecker::declareVariable(const std::string &Name, bool IsMutable) {
  // Check if variable already exists in current scope
  auto it = Variables.find(Name);
  if (it != Variables.end() && it->second.ScopeLevel == CurrentScope) {
    reportError("Variable '" + Name + "' already declared in this scope");
    return;
  }

  VariableState State;
  State.IsMutable = IsMutable;
  State.ScopeLevel = CurrentScope;
  State.Line = CurrentLine;
  Variables[Name] = State;
}

bool BorrowChecker::checkUse(const std::string &Name) {
  auto it = Variables.find(Name);
  if (it == Variables.end()) {
    // Not tracked - might be a function parameter, allow it
    return true;
  }

  if (it->second.IsMoved) {
    reportError("Cannot use '" + Name + "': value has been moved");
    return false;
  }

  return true;
}

bool BorrowChecker::checkAssign(const std::string &Name) {
  auto it = Variables.find(Name);
  if (it == Variables.end()) {
    reportError("Cannot assign to undeclared variable '" + Name + "'");
    return false;
  }

  if (!it->second.IsMutable) {
    reportError("Cannot assign to immutable variable '" + Name +
                "'. Consider using 'let mut " + Name + "'");
    return false;
  }

  if (it->second.ImmutableBorrows > 0) {
    reportError("Cannot assign to '" + Name +
                "' while it is borrowed immutably");
    return false;
  }

  if (it->second.MutableBorrows > 0) {
    reportError("Cannot assign to '" + Name + "' while it is borrowed mutably");
    return false;
  }

  return true;
}

void BorrowChecker::moveVariable(const std::string &Name) {
  auto it = Variables.find(Name);
  if (it != Variables.end()) {
    it->second.IsMoved = true;
  }
}

bool BorrowChecker::borrowImmutable(const std::string &Name) {
  auto it = Variables.find(Name);
  if (it == Variables.end()) {
    return true; // Not tracked
  }

  if (it->second.IsMoved) {
    reportError("Cannot borrow '" + Name + "': value has been moved");
    return false;
  }

  if (it->second.MutableBorrows > 0) {
    reportError("Cannot borrow '" + Name +
                "' as immutable: already borrowed as mutable");
    return false;
  }

  it->second.ImmutableBorrows++;
  return true;
}

bool BorrowChecker::borrowMutable(const std::string &Name) {
  auto it = Variables.find(Name);
  if (it == Variables.end()) {
    return true; // Not tracked
  }

  if (it->second.IsMoved) {
    reportError("Cannot borrow '" + Name + "': value has been moved");
    return false;
  }

  if (!it->second.IsMutable) {
    reportError("Cannot borrow '" + Name +
                "' as mutable: variable is not mutable");
    return false;
  }

  if (it->second.ImmutableBorrows > 0) {
    reportError("Cannot borrow '" + Name +
                "' as mutable: already borrowed as immutable");
    return false;
  }

  if (it->second.MutableBorrows > 0) {
    reportError("Cannot borrow '" + Name +
                "' as mutable more than once at a time");
    return false;
  }

  it->second.MutableBorrows++;
  return true;
}

void BorrowChecker::releaseBorrow(const std::string &Name, bool Mutable) {
  auto it = Variables.find(Name);
  if (it != Variables.end()) {
    if (Mutable) {
      it->second.MutableBorrows--;
    } else {
      it->second.ImmutableBorrows--;
    }
  }
}

bool BorrowChecker::exists(const std::string &Name) const {
  return Variables.find(Name) != Variables.end();
}

bool BorrowChecker::isMutable(const std::string &Name) const {
  auto it = Variables.find(Name);
  if (it != Variables.end()) {
    return it->second.IsMutable;
  }
  return false;
}

void BorrowChecker::reportError(const std::string &Msg) {
  std::ostringstream oss;
  oss << "error: " << Msg;
  Errors.push_back(oss.str());
}
