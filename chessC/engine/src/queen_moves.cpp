#include "../include/queen_moves.h"
#include "../include/bishop_moves.h"
#include "../include/rook_moves.h"

std::vector<Move> generate_queen_moves(const Board& board, int row, int col, Color color) {
    std::vector<Move> moves = generate_bishop_moves(board, row, col, color);
    std::vector<Move> rook_moves = generate_rook_moves(board, row, col, color);
    moves.insert(moves.end(), rook_moves.begin(), rook_moves.end());
    return moves;
}
