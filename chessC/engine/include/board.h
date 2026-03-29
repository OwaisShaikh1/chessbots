#pragma once

#include <array>
#include <string>
#include "types.h"
#include "move.h"

class Board {
public:
    Board();

    const Piece& at(int row, int col) const;
    bool is_empty(int row, int col) const;
    bool has_enemy_piece(int row, int col, Color my_color) const;
    bool has_friendly_piece(int row, int col, Color my_color) const;
    void apply_move(const Move& move);
    Board simulate_move(const Move& move) const;
    void load_from_fen(const std::string& fen);

private:
    std::array<std::array<Piece, 8>, 8> squares_;

    void clear();
    void setup_start_position();
};
