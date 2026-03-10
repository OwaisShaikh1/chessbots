# Chess C++ Backend (Standalone)

A complete, self-contained C++ chess engine and HTTP server with **zero external dependencies**. This is a drop-in replacement for the Python backend.

## Features

- ✅ Full chess engine with legal move generation
- ✅ Alpha-beta search with quiescence search
- ✅ Transposition table for caching
- ✅ Late Move Reductions (LMR)
- ✅ Principal Variation Search (PVS)  
- ✅ Piece-square tables (PST) evaluation
- ✅ Built-in HTTP server (no external libraries)
- ✅ CORS support for browser/Flutter access
- ✅ Same API as Python backend

## Quick Build (Windows)

### Option 1: Visual Studio (Recommended)
1. Open "Developer Command Prompt for VS 2022"
2. Navigate to this folder
3. Run:
```batch
build.bat
```
Or manually:
```batch
cl /EHsc /O2 /std:c++17 main.cpp chess.cpp /Fe:chess_backend.exe /link ws2_32.lib
```

### Option 2: MinGW-w64
```batch
g++ -O2 -std=c++17 main.cpp chess.cpp -o chess_backend.exe -lws2_32 -lpthread
```

## Quick Build (Linux/macOS)

```bash
chmod +x build.sh
./build.sh
```
Or manually:
```bash
g++ -O2 -std=c++17 -pthread main.cpp chess.cpp -o chess_backend
```

## Run

```bash
./chess_backend      # Linux/macOS
chess_backend.exe    # Windows
```

The server starts on **http://localhost:8000**

## API Endpoints

All endpoints are compatible with the Python backend:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Health check |
| `/fen` | GET | Get current FEN position |
| `/fen` | POST | Set position from FEN |
| `/move` | POST | Make a move (JSON: `{"move": "e2e4"}`) |
| `/bot-move` | GET | Get and play bot's best move |
| `/reset` | POST | Reset to starting position |
| `/parameters` | GET | Get search parameters |
| `/parameters` | POST | Set search depth |
| `/metrics` | GET | Server statistics |
| `/analytics` | GET | Engine analytics |

## Usage Example

```bash
# Reset the game
curl -X POST http://localhost:8000/reset

# Make a move
curl -X POST http://localhost:8000/move -H "Content-Type: application/json" -d '{"move":"e2e4"}'

# Get bot's response
curl http://localhost:8000/bot-move

# Get current position
curl http://localhost:8000/fen

# Set search depth
curl -X POST http://localhost:8000/parameters -H "Content-Type: application/json" -d '{"search_depth":5}'
```

## Switching from Python to C++

1. Stop your Python backend (Ctrl+C)
2. Build and run the C++ backend
3. Your Flutter frontend will connect automatically (same port 8000)

## File Structure

```
backend_cpp_standalone/
├── chess.hpp      # Chess engine header (types, classes, declarations)
├── chess.cpp      # Chess engine implementation
├── main.cpp       # HTTP server with all API endpoints
├── build.bat      # Windows build script
├── build.sh       # Linux/macOS build script
└── README.md      # This file
```

## Performance

The C++ backend is significantly faster than Python:
- Depth 4: ~0.5-1 second (vs ~5-6 seconds in Python)
- Depth 5: ~2-4 seconds
- Depth 6: ~10-20 seconds

## Customization

### Change Search Depth

Via API:
```bash
curl -X POST http://localhost:8000/parameters -d '{"search_depth":5}'
```

In code (main.cpp):
```cpp
int search_depth = 5;  // Change default depth
```

### Change Port

In main.cpp, change the port number:
```cpp
server.listen("0.0.0.0", 8080);  // Use port 8080 instead
```
