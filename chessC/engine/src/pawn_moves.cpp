#include "../include/pawn_moves.h"

std::vector<Move> generate_pawn_moves(const Board& board, int row, int col, Color color) {
    std::vector<Move> moves;

    const int direction = (color == Color::White) ? 1 : -1;
    const int start_row = (color == Color::White) ? 1 : 6;

    const int one_step_row = row + direction;
    if (is_valid_square(one_step_row, col) && board.is_empty(one_step_row, col)) {
        moves.push_back(Move{row, col, one_step_row, col, false});

        const int two_step_row = row + (2 * direction);
        if (row == start_row && board.is_empty(two_step_row, col)) {
            moves.push_back(Move{row, col, two_step_row, col, false});
        }
    }

    const int capture_cols[2] = {col - 1, col + 1};
    for (const int target_col : capture_cols) {
        if (!is_valid_square(one_step_row, target_col)) {
            continue;
        }
        if (board.has_enemy_piece(one_step_row, target_col, color)) {
            moves.push_back(Move{row, col, one_step_row, target_col, true});
        }
    }

    return moves;
}
