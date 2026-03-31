#include "../include/pawn_moves.h"

namespace {

void append_pawn_move(std::vector<Move>& moves,
                      int from_row,
                      int from_col,
                      int to_row,
                      int to_col,
                      bool is_capture,
                      Color color) {
    const int promotion_row = (color == Color::White) ? 7 : 0;
    if (to_row == promotion_row) {
        moves.push_back(Move{from_row, from_col, to_row, to_col, is_capture, PieceType::Queen});
        moves.push_back(Move{from_row, from_col, to_row, to_col, is_capture, PieceType::Rook});
        moves.push_back(Move{from_row, from_col, to_row, to_col, is_capture, PieceType::Bishop});
        moves.push_back(Move{from_row, from_col, to_row, to_col, is_capture, PieceType::Knight});
        return;
    }

    moves.push_back(Move{from_row, from_col, to_row, to_col, is_capture});
}

}  // namespace

std::vector<Move> generate_pawn_moves(const Board& board, int row, int col, Color color) {
    std::vector<Move> moves;

    const int direction = (color == Color::White) ? 1 : -1;
    const int start_row = (color == Color::White) ? 1 : 6;

    const int one_step_row = row + direction;
    if (is_valid_square(one_step_row, col) && board.is_empty(one_step_row, col)) {
        append_pawn_move(moves, row, col, one_step_row, col, false, color);

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
            append_pawn_move(moves, row, col, one_step_row, target_col, true, color);
        }
    }

    return moves;
}
