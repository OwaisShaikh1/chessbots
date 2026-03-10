# Chess Backend C++

A high-performance C++ implementation of the chess backend API, compatible with the Flutter frontend.

## Features

- Full chess engine with alpha-beta search
- Piece-square tables for positional evaluation
- Move ordering with MVV-LVA
- Quiescence search
- Transposition table
- REST API compatible with the Python backend

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler (MSVC 2019+, GCC 9+, Clang 10+)
- Git (for fetching dependencies)

### Windows (Visual Studio)

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Windows (MinGW)

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Linux/macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Running

```bash
./bin/chess_backend
```

The server will start on `http://localhost:8000` by default.

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Health check |
| `/fen` | GET | Get current board FEN |
| `/move` | POST | Make a move (body: `{"uci": "e2e4"}`) |
| `/bot-move` | GET | Get and execute bot's best move |
| `/reset` | POST | Reset the game |
| `/parameters` | GET | Get evaluation parameters |
| `/parameters` | POST | Set evaluation parameters |

## Configuration

Environment variables:
- `PORT`: Server port (default: 8000)
- `SEARCH_DEPTH`: Default search depth (default: 4)

## Performance

The C++ backend is significantly faster than the Python version:
- ~10-50x faster move generation
- ~5-20x faster evaluation
- ~10-100x faster search at higher depths
