#include "../include/king_moves.h"

std::vector<Move> generate_king_moves(const Board& board, int row, int col, Color color) {
    std::vector<Move> moves;

    for (int row_offset = -1; row_offset <= 1; ++row_offset) {
        for (int col_offset = -1; col_offset <= 1; ++col_offset) {
            if (row_offset == 0 && col_offset == 0) {
                continue;
            }

            const int target_row = row + row_offset;
            const int target_col = col + col_offset;
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
    }

    return moves;
}
