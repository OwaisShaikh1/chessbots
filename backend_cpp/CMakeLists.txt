cmake_minimum_required(VERSION 3.16)
project(ChessBackendCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(Threads REQUIRED)

# Fetch dependencies
include(FetchContent)

# Crow (header-only HTTP framework)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.0+5
)
FetchContent_MakeAvailable(Crow)

# nlohmann/json for JSON handling
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Source files
set(SOURCES
    src/main.cpp
    src/chess/board.cpp
    src/chess/move.cpp
    src/chess/movegen.cpp
    src/evaluation/evaluation.cpp
    src/search/search.cpp
    src/bot/bot.cpp
)

set(HEADERS
    src/chess/board.hpp
    src/chess/move.hpp
    src/chess/movegen.hpp
    src/chess/types.hpp
    src/evaluation/evaluation.hpp
    src/evaluation/pst.hpp
    src/search/search.hpp
    src/bot/bot.hpp
)

add_executable(chess_backend ${SOURCES} ${HEADERS})

target_include_directories(chess_backend PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(chess_backend PRIVATE 
    Crow::Crow
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# Windows-specific settings
if(WIN32)
    target_link_libraries(chess_backend PRIVATE ws2_32 wsock32)
    target_compile_definitions(chess_backend PRIVATE _WIN32_WINNT=0x0601)
endif()

# Output directory
set_target_properties(chess_backend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
