#ifndef MODULELOADER_H
#define MODULELOADER_H

#include <set>
#include <string>

/// Load and parse an imported module file
/// Returns true if successful
bool loadModule(const std::string &filename);

/// Get the directory of the current file being parsed
std::string getFileDirectory(const std::string &filepath);

/// Resolve a relative path against a base directory
std::string resolvePath(const std::string &basePath,
                        const std::string &relativePath);

/// Track which modules have been imported to prevent circular imports
extern std::set<std::string> ImportedModules;

#endif // MODULELOADER_H
