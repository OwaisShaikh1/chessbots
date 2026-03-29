#include "../include/bishop_moves.h"

std::vector<Move> generate_bishop_moves(const Board& board, int row, int col, Color color) {
    std::vector<Move> moves;

    const int directions[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (const auto& direction : directions) {
        int target_row = row + direction[0];
        int target_col = col + direction[1];

        while (is_valid_square(target_row, target_col)) {
            if (board.has_friendly_piece(target_row, target_col, color)) {
                break;
            }

            const bool is_capture = board.has_enemy_piece(target_row, target_col, color);
            moves.push_back(Move{row, col, target_row, target_col, is_capture});
            if (is_capture) {
                break;
            }

            target_row += direction[0];
            target_col += direction[1];
        }
    }

    return moves;
}
