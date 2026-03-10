#!/bin/bash
echo "Building Chess C++ Backend..."

mkdir -p build
cd build

echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "CMake configuration failed."
    exit 1
fi

echo "Building..."
cmake --build . -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed."
    exit 1
fi

echo ""
echo "Build successful!"
echo "Executable located at: build/bin/chess_backend"
echo ""
echo "To run: ./bin/chess_backend [port]"
echo "Default port: 8000"
