cmake_minimum_required(VERSION 3.16)
project(ChessBot VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(ENGINE_SOURCES
    src/board.cpp
    src/pawn_moves.cpp
    src/knight_moves.cpp
    src/bishop_moves.cpp
    src/rook_moves.cpp
    src/queen_moves.cpp
    src/king_moves.cpp
    src/position_heuristics.cpp
    src/move_generator.cpp
    src/main.cpp
)

add_executable(chessbot ${ENGINE_SOURCES})
