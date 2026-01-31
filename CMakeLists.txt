cmake_minimum_required(VERSION 3.10)
project(fermat)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LLVM configuration path
set(LLVM_DIR "/opt/homebrew/opt/llvm/lib/cmake/llvm")

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Source files
set(SOURCES
    src/main.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/CodeGen.cpp
    src/JIT.cpp
    src/BorrowCheck.cpp
    src/ModuleLoader.cpp
    src/Runtime.cpp
)

# Create executable
add_executable(fermat ${SOURCES})

# Link LLVM libraries
llvm_map_components_to_libnames(llvm_libs core support irreader native orcjit)
target_link_libraries(fermat ${llvm_libs})