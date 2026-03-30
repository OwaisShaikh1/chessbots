#include "../include/move_generator.h"

#include "../include/pawn_moves.h"
#include "../include/knight_moves.h"
#include "../include/bishop_moves.h"
#include "../include/rook_moves.h"
#include "../include/queen_moves.h"
#include "../include/king_moves.h"
#include "../include/position_heuristics.h"

#include <chrono>
#include <limits>

namespace MoveGenerator {

namespace {

struct SearchControl {
    bool use_time_limit = false;
    std::chrono::steady_clock::time_point deadline;
};

bool same_move(const Move& a, const Move& b) {
    return a.from_row == b.from_row
        && a.from_col == b.from_col
        && a.to_row == b.to_row
        && a.to_col == b.to_col
        && a.is_capture == b.is_capture;
}

void move_to_front(std::vector<Move>& moves, const Move& target) {
    for (size_t i = 0; i < moves.size(); ++i) {
        if (same_move(moves[i], target)) {
            if (i != 0) {
                std::swap(moves[0], moves[i]);
            }
            return;
        }
    }
}

Color other_side(Color c) {
    return (c == Color::White) ? Color::Black : Color::White;
}

bool time_is_up(const SearchControl& control) {
    return control.use_time_limit && std::chrono::steady_clock::now() >= control.deadline;
}

int minimax(const Board& board,
            Color side_to_move,
            Color root_side,
            int depth,
            const SearchControl& control,
            bool& completed) {
    if (time_is_up(control)) {
        completed = false;
        return 0;
    }

    if (depth <= 0) {
        completed = true;
        return PositionHeuristics::evaluate_board(board, root_side);
    }

    const std::vector<Move> moves = generate_all(board, side_to_move);
    if (moves.empty()) {
        completed = true;
        return PositionHeuristics::evaluate_board(board, root_side);
    }

    if (side_to_move == root_side) {
        int best = std::numeric_limits<int>::min();
        for (const Move& move : moves) {
            if (time_is_up(control)) {
                completed = false;
                return 0;
            }

            bool child_completed = true;
            const Board next = board.simulate_move(move);
            const int score = minimax(next, other_side(side_to_move), root_side, depth - 1, control, child_completed);
            if (!child_completed) {
                completed = false;
                return 0;
            }
            if (score > best) best = score;
        }
        completed = true;
        return best;
    }

    int best = std::numeric_limits<int>::max();
    for (const Move& move : moves) {
        if (time_is_up(control)) {
            completed = false;
            return 0;
        }

        bool child_completed = true;
        const Board next = board.simulate_move(move);
        const int score = minimax(next, other_side(side_to_move), root_side, depth - 1, control, child_completed);
        if (!child_completed) {
            completed = false;
            return 0;
        }
        if (score < best) best = score;
    }
    completed = true;
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
    return choose_best_move(board, side, depth, 0);
}

ScoredMove choose_best_move(const Board& board, Color side, int depth, int movetime_ms) {
    const std::vector<Move> legal_moves = generate_all(board, side);
    if (legal_moves.empty()) {
        return ScoredMove{};
    }

    std::vector<Move> root_moves = legal_moves;

    const int max_depth = (depth < 1) ? 1 : depth;

    SearchControl control;
    if (movetime_ms > 0) {
        control.use_time_limit = true;
        control.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(movetime_ms);
    }

    ScoredMove best_overall{};
    best_overall.move = root_moves.front();
    best_overall.score = std::numeric_limits<int>::min();
    best_overall.valid = true;

    // Iterative deepening: only start depth N+1 after depth N completes.
    for (int current_depth = 1; current_depth <= max_depth; ++current_depth) {
        if (time_is_up(control)) {
            break;
        }

        if (best_overall.valid) {
            move_to_front(root_moves, best_overall.move);
        }

        ScoredMove depth_best{};
        depth_best.move = root_moves.front();
        depth_best.score = std::numeric_limits<int>::min();
        depth_best.valid = false;

        bool depth_completed = true;
        for (const Move& move : root_moves) {
            if (time_is_up(control)) {
                depth_completed = false;
                break;
            }

            const Board next = board.simulate_move(move);
            bool move_completed = true;
            const int score = minimax(next, other_side(side), side, current_depth - 1, control, move_completed);

            if (!move_completed) {
                depth_completed = false;
                break;
            }

            if (!depth_best.valid || score > depth_best.score) {
                depth_best.move = move;
                depth_best.score = score;
                depth_best.valid = true;
            }
        }

        // Use completed depth result when available.
        if (depth_completed && depth_best.valid) {
            best_overall = depth_best;
            move_to_front(root_moves, depth_best.move);
            continue;
        }

        // Time-forced stop: keep previous completed depth, unless current depth
        // already found a better fully evaluated move.
        if (depth_best.valid && (!best_overall.valid || depth_best.score > best_overall.score)) {
            best_overall = depth_best;
        }
        break;
    }

    return best_overall;
}

}  // namespace MoveGenerator
