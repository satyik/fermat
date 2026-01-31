#!/bin/bash
# Fermat Installer

set -e

echo "Detecting OS..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "  OS: macOS"
    # Try to find LLVM via Homebrew or common paths
    if [ -d "/opt/homebrew/opt/llvm" ]; then
        echo "  LLVM found at /opt/homebrew/opt/llvm"
        export LLVM_DIR="/opt/homebrew/opt/llvm/lib/cmake/llvm"
    elif [ -d "/usr/local/opt/llvm" ]; then
        echo "  LLVM found at /usr/local/opt/llvm"
        export LLVM_DIR="/usr/local/opt/llvm/lib/cmake/llvm"
    else
        echo "  WARNING: LLVM not found in standard Homebrew paths."
        echo "  Please ensure LLVM is installed (brew install llvm)."
    fi
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "  OS: Linux"
    # Basic check - users might need to set LLVM_DIR manually
    echo "  Assuming LLVM is installed via package manager."
else
    echo "  Unknown OS: $OSTYPE"
fi

echo "Building Fermat..."
rm -rf build 2>/dev/null
mkdir -p build
cd build

if command -v cmake >/dev/null 2>&1; then
    cmake ..
    make
    echo ""
    echo "Build Complete!"
    echo "You can now use Fermat:"
    echo "  ./build/fermat test.frmt"
} else
    echo "Error: cmake not found."
    exit 1
fi
