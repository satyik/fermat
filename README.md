# Fermat

Fermat is a simple JIT-compiled programming language built with LLVM.

## Prerequisites

To build and run SpyLang, you need the following tools installed on your system:

- **C++ Compiler**: Clang (recommended) or GCC supporting C++17.
- **CMake**: Version 3.10 or higher.
- **LLVM**: Version 10 or higher (development libraries).

### macOS (Homebrew)
```bash
brew install llvm cmake
```

### Ubuntu/Debian
```bash
sudo apt-get install llvm-dev clang cmake build-essential
```

## Building

1.  Clone the repository or copy the project files to your machine.
2.  Navigate to the project directory:
    ```bash
    cd fermat
    ```
3.  Create a build directory and run CMake:
    ```bash
    mkdir build
    cd build
    # For macOS with Homebrew LLVM:
    cmake .. -DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm
    
    # For standard Linux installations:
    cmake ..
    ```
4.  Build the project:
    ```bash
    make
    ```

## Usage

Once built, you can run SpyLang programs using the `spy` executable in the `build` directory.

### Running a File
```bash
./build/fermat path/to/your_script.frmt
```

### Interactive Mode (REPL)
Run `fermat` without arguments to enter the generic REPL (basic expression evaluation):
```bash
./build/fermat
```

## Language Features
See [REFERENCE.md](REFERENCE.md) for details on syntax, keywords, and examples.
