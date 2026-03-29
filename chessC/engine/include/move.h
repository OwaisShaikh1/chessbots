#pragma once

#include <string>

struct Move {
    int from_row = 0;
    int from_col = 0;
    int to_row = 0;
    int to_col = 0;
    bool is_capture = false;
};

inline std::string to_algebraic(int row, int col) {
    const char file = static_cast<char>('a' + col);
    const char rank = static_cast<char>('1' + row);
    return std::string{file, rank};
}
