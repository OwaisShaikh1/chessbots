#include "../include/knight_moves.h"

std::vector<Move> generate_knight_moves(const Board& board, int row, int col, Color color) {
    std::vector<Move> moves;

    const int offsets[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };

    for (const auto& offset : offsets) {
        const int target_row = row + offset[0];
        const int target_col = col + offset[1];
        if (!is_valid_square(target_row, target_col)) {
            continue;
        }
        if (board.has_friendly_piece(target_row, target_col, color)) {
            continue;
        }

        moves.push_back(Move{
            row,
            col,
            target_row,
            target_col,
            board.has_enemy_piece(target_row, target_col, color)
        });
    }

    return moves;
}
