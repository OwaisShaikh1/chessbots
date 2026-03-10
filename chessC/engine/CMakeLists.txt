cmake_minimum_required(VERSION 3.16)
project(ChessBot VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Compiler flags ───────────────────────────────────────────────────────

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -O3
        -march=native
        -Wall
        -Wextra
        -Wpedantic
    )
elseif(MSVC)
    add_compile_options(/O2 /W4)
endif()

# ─── Include paths ────────────────────────────────────────────────────────

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ─── Sources ──────────────────────────────────────────────────────────────

set(ENGINE_SOURCES
    src/bitboard.cpp
    src/zobrist.cpp
    src/board.cpp
    src/attacks.cpp
    src/movegen.cpp
    src/legal_moves.cpp
    src/perft.cpp
    src/evaluation.cpp
    src/tt.cpp
    src/search.cpp
    src/uci.cpp
    src/main.cpp
)

# ─── Executable ───────────────────────────────────────────────────────────

add_executable(chessbot ${ENGINE_SOURCES})

# Link pthreads on Unix
if(UNIX)
    target_link_libraries(chessbot pthread)
endif()

# ─── Install ──────────────────────────────────────────────────────────────

install(TARGETS chessbot DESTINATION bin)
