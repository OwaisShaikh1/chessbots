#!/bin/bash
# Chess C++ Backend - Linux/macOS Build Script

echo "========================================"
echo "Chess C++ Backend - Build Script"
echo "========================================"

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS"
    EXTRA_FLAGS=""
else
    echo "Detected Linux"
    EXTRA_FLAGS="-pthread"
fi

# Build
echo ""
echo "Compiling..."
g++ -O2 -std=c++17 $EXTRA_FLAGS main.cpp chess.cpp -o chess_backend

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Build successful! Run: ./chess_backend"
    echo "========================================"
    chmod +x chess_backend
else
    echo "Build failed!"
    exit 1
fi
