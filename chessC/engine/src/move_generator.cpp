#include "../include/move_generator.h"

#include "../include/pawn_moves.h"
#include "../include/knight_moves.h"
#include "../include/bishop_moves.h"
#include "../include/rook_moves.h"
#include "../include/queen_moves.h"
#include "../include/king_moves.h"
#include "../include/position_heuristics.h"

#include <limits>

namespace MoveGenerator {

namespace {

Color other_side(Color c) {
    return (c == Color::White) ? Color::Black : Color::White;
}

int minimax(const Board& board, Color side_to_move, Color root_side, int depth) {
    if (depth <= 0) {
        return PositionHeuristics::evaluate_board(board, root_side);
    }

    const std::vector<Move> moves = generate_all(board, side_to_move);
    if (moves.empty()) {
        return PositionHeuristics::evaluate_board(board, root_side);
    }

    if (side_to_move == root_side) {
        int best = std::numeric_limits<int>::min();
        for (const Move& move : moves) {
            const Board next = board.simulate_move(move);
            const int score = minimax(next, other_side(side_to_move), root_side, depth - 1);
            if (score > best) best = score;
        }
        return best;
    }

    int best = std::numeric_limits<int>::max();
    for (const Move& move : moves) {
        const Board next = board.simulate_move(move);
        const int score = minimax(next, other_side(side_to_move), root_side, depth - 1);
        if (score < best) best = score;
    }
    return best;
}

} // namespace

std::vector<Move> generate_for_piece(const Board& board, int row, int col) {
    const Piece piece = board.at(row, col);
    if (piece.type == PieceType::None || piece.color == Color::None) {
        return {};
    }

    switch (piece.type) {
        case PieceType::Pawn:
            return generate_pawn_moves(board, row, col, piece.color);
        case PieceType::Knight:
            return generate_knight_moves(board, row, col, piece.color);
        case PieceType::Bishop:
            return generate_bishop_moves(board, row, col, piece.color);
        case PieceType::Rook:
            return generate_rook_moves(board, row, col, piece.color);
        case PieceType::Queen:
            return generate_queen_moves(board, row, col, piece.color);
        case PieceType::King:
            return generate_king_moves(board, row, col, piece.color);
        case PieceType::None:
            return {};
    }

    return {};
}

std::vector<Move> generate_all(const Board& board, Color side) {
    std::vector<Move> all_moves;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const Piece piece = board.at(row, col);
            if (piece.type == PieceType::None || piece.color != side) {
                continue;
            }

            std::vector<Move> piece_moves = generate_for_piece(board, row, col);
            all_moves.insert(all_moves.end(), piece_moves.begin(), piece_moves.end());
        }
    }

    return all_moves;
}

ScoredMove choose_best_move(const Board& board, Color side) {
    return choose_best_move(board, side, 1);
}

ScoredMove choose_best_move(const Board& board, Color side, int depth) {
    const std::vector<Move> moves = generate_all(board, side);
    if (moves.empty()) {
        return ScoredMove{};
    }

    const int search_depth = (depth < 1) ? 1 : depth;

    ScoredMove best{};
    best.move = moves.front();
    best.score = std::numeric_limits<int>::min();
    best.valid = true;

    for (const Move& move : moves) {
        const Board next = board.simulate_move(move);
        const int score = minimax(next, other_side(side), side, search_depth - 1);
        if (score > best.score) {
            best.move = move;
            best.score = score;
        }
    }

    return best;
}

}  // namespace MoveGenerator
