#include "../include/move_generator.h"

#include "../include/pawn_moves.h"
#include "../include/knight_moves.h"
#include "../include/bishop_moves.h"
#include "../include/rook_moves.h"
#include "../include/queen_moves.h"
#include "../include/king_moves.h"
#include "../include/position_heuristics.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>

namespace MoveGenerator {

namespace {

struct SearchControl {
    bool use_time_limit = false;
    std::chrono::steady_clock::time_point deadline;
};

constexpr double PARTIAL_DEPTH_TRUST_RATIO = 0.30;
constexpr int MATE_SCORE = 100000;

int draw_score_from_eval(const Board& board, Color root_side) {
    (void)board;
    (void)root_side;
    return 0;
}

int piece_value(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return 100;
        case PieceType::Knight: return 320;
        case PieceType::Bishop: return 330;
        case PieceType::Rook: return 500;
        case PieceType::Queen: return 900;
        case PieceType::King: return 20000;
        case PieceType::None: return 0;
    }
    return 0;
}

int move_order_score(const Board& board, const Move& move) {
    int score = 0;

    if (move.promotion != PieceType::None) {
        score += 20000 + piece_value(move.promotion);
    }

    if (move.is_capture) {
        const Piece victim = board.at(move.to_row, move.to_col);
        const Piece attacker = board.at(move.from_row, move.from_col);
        score += 10000 + 10 * piece_value(victim.type) - piece_value(attacker.type);
    }

    return score;
}

void order_moves(const Board& board, std::vector<Move>& moves) {
    std::stable_sort(moves.begin(), moves.end(), [&board](const Move& a, const Move& b) {
        return move_order_score(board, a) > move_order_score(board, b);
    });
}

bool same_move(const Move& a, const Move& b) {
    return a.from_row == b.from_row
        && a.from_col == b.from_col
        && a.to_row == b.to_row
        && a.to_col == b.to_col
    && a.is_capture == b.is_capture
    && a.promotion == b.promotion;
}

bool is_square_attacked_by(const Board& board, int target_row, int target_col, Color attacker) {
    const int pawn_row = (attacker == Color::White) ? (target_row - 1) : (target_row + 1);
    for (int dc : {-1, 1}) {
        const int pawn_col = target_col + dc;
        if (!is_valid_square(pawn_row, pawn_col)) {
            continue;
        }
        const Piece p = board.at(pawn_row, pawn_col);
        if (p.color == attacker && p.type == PieceType::Pawn) {
            return true;
        }
    }

    const int knight_offsets[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
    for (const auto& off : knight_offsets) {
        const int row = target_row + off[0];
        const int col = target_col + off[1];
        if (!is_valid_square(row, col)) {
            continue;
        }
        const Piece p = board.at(row, col);
        if (p.color == attacker && p.type == PieceType::Knight) {
            return true;
        }
    }

    const int diag_dirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (const auto& d : diag_dirs) {
        int row = target_row + d[0];
        int col = target_col + d[1];
        while (is_valid_square(row, col)) {
            const Piece p = board.at(row, col);
            if (p.type != PieceType::None) {
                if (p.color == attacker && (p.type == PieceType::Bishop || p.type == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            row += d[0];
            col += d[1];
        }
    }

    const int ortho_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : ortho_dirs) {
        int row = target_row + d[0];
        int col = target_col + d[1];
        while (is_valid_square(row, col)) {
            const Piece p = board.at(row, col);
            if (p.type != PieceType::None) {
                if (p.color == attacker && (p.type == PieceType::Rook || p.type == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            row += d[0];
            col += d[1];
        }
    }

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) {
                continue;
            }
            const int row = target_row + dr;
            const int col = target_col + dc;
            if (!is_valid_square(row, col)) {
                continue;
            }
            const Piece p = board.at(row, col);
            if (p.color == attacker && p.type == PieceType::King) {
                return true;
            }
        }
    }

    return false;
}

bool is_in_check(const Board& board, Color side) {
    int king_row = -1;
    int king_col = -1;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const Piece p = board.at(row, col);
            if (p.color == side && p.type == PieceType::King) {
                king_row = row;
                king_col = col;
                break;
            }
        }
        if (king_row != -1) {
            break;
        }
    }

    if (king_row == -1) {
        return true;
    }

    const Color attacker = (side == Color::White) ? Color::Black : Color::White;
    return is_square_attacked_by(board, king_row, king_col, attacker);
}

std::vector<Move> generate_pseudo_all(const Board& board, Color side) {
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

bool is_threefold_repetition(const std::vector<std::uint64_t>& history, std::uint64_t key) {
    int count = 0;
    for (std::uint64_t seen : history) {
        if (seen == key) {
            count++;
            if (count >= 3) {
                return true;
            }
        }
    }
    return false;
}

std::uint64_t board_key(const Board& board, Color side_to_move) {
    // Lightweight FNV-1a hash over board occupancy + side to move.
    std::uint64_t hash = 1469598103934665603ULL;
    constexpr std::uint64_t fnv_prime = 1099511628211ULL;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const Piece p = board.at(row, col);
            const std::uint64_t piece_value =
                (static_cast<std::uint64_t>(static_cast<int>(p.type)) << 1ULL)
                ^ static_cast<std::uint64_t>(static_cast<int>(p.color));
            const std::uint64_t square_mix = static_cast<std::uint64_t>(row * 8 + col + 1);
            hash ^= (piece_value + 1ULL) * (square_mix + 17ULL);
            hash *= fnv_prime;
        }
    }

    hash ^= (side_to_move == Color::White) ? 0xA5A5A5A5A5A5A5A5ULL : 0x5A5A5A5A5A5A5A5AULL;
    hash *= fnv_prime;
    return hash;
}

int minimax(const Board& board,
            Color side_to_move,
            Color root_side,
            int depth,
            int ply_from_root,
            int alpha,
            int beta,
            const SearchControl& control,
            bool& completed,
            const std::vector<std::uint64_t>& game_history_keys) {
    if (time_is_up(control)) {
        completed = false;
        return 0;
    }

    const std::uint64_t current_key = board_key(board, side_to_move);
    if (is_threefold_repetition(game_history_keys, current_key)) {
        completed = true;
        return draw_score_from_eval(board, root_side);
    }

    if (depth <= 0) {
        completed = true;
        return PositionHeuristics::evaluate_board(board, root_side);
    }

    std::vector<Move> moves = generate_all(board, side_to_move);
    if (moves.empty()) {
        completed = true;
        if (is_in_check(board, side_to_move)) {
            return (side_to_move == root_side)
                ? (-MATE_SCORE + ply_from_root)
                : (MATE_SCORE - ply_from_root);
        }
        return draw_score_from_eval(board, root_side);
    }
    order_moves(board, moves);

    if (side_to_move == root_side) {
        int best = std::numeric_limits<int>::min();
        for (const Move& move : moves) {
            if (time_is_up(control)) {
                completed = false;
                return 0;
            }

            bool child_completed = true;
            const Board next = board.simulate_move(move);
            const int score = minimax(next,
                                      other_side(side_to_move),
                                      root_side,
                                      depth - 1,
                                      ply_from_root + 1,
                                      alpha,
                                      beta,
                                      control,
                                      child_completed,
                                      game_history_keys);
            if (!child_completed) {
                completed = false;
                return 0;
            }
            if (score > best) best = score;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;
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
        const int score = minimax(next,
                                  other_side(side_to_move),
                                  root_side,
                                  depth - 1,
                                  ply_from_root + 1,
                                  alpha,
                                  beta,
                                  control,
                                  child_completed,
                                  game_history_keys);
        if (!child_completed) {
            completed = false;
            return 0;
        }
        if (score < best) best = score;
        if (best < beta) beta = best;
        if (alpha >= beta) break;
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
    const std::vector<Move> pseudo_moves = generate_pseudo_all(board, side);
    std::vector<Move> legal_moves;
    legal_moves.reserve(pseudo_moves.size());

    for (const Move& move : pseudo_moves) {
        const Board next = board.simulate_move(move);
        if (!is_in_check(next, side)) {
            legal_moves.push_back(move);
        }
    }

    return legal_moves;
}

std::uint64_t position_key(const Board& board, Color side_to_move) {
    return board_key(board, side_to_move);
}

ScoredMove choose_best_move(const Board& board, Color side) {
    return choose_best_move(board, side, 1);
}

ScoredMove choose_best_move(const Board& board, Color side, int depth) {
    return choose_best_move(board, side, depth, 0);
}

ScoredMove choose_best_move(const Board& board, Color side, int depth, int movetime_ms) {
    std::vector<std::uint64_t> prior_keys;
    prior_keys.push_back(board_key(board, side));
    return choose_best_move(board, side, depth, movetime_ms, prior_keys);
}

ScoredMove choose_best_move(const Board& board,
                           Color side,
                           int depth,
                           int movetime_ms,
                           const std::vector<std::uint64_t>& prior_position_keys) {
    const std::vector<Move> legal_moves = generate_all(board, side);
    if (legal_moves.empty()) {
        return ScoredMove{};
    }

    std::vector<std::uint64_t> game_history_keys = prior_position_keys;
    if (game_history_keys.empty()) {
        game_history_keys.push_back(board_key(board, side));
    }

    std::vector<Move> root_moves = legal_moves;
    order_moves(board, root_moves);

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
        int completed_root_moves = 0;
        for (const Move& move : root_moves) {
            if (time_is_up(control)) {
                depth_completed = false;
                break;
            }

            const Board next = board.simulate_move(move);
            bool move_completed = true;
            const int score = minimax(next,
                                      other_side(side),
                                      side,
                                      current_depth - 1,
                                      1,
                                      std::numeric_limits<int>::min(),
                                      std::numeric_limits<int>::max(),
                                      control,
                                      move_completed,
                                      game_history_keys);

            if (!move_completed) {
                depth_completed = false;
                break;
            }

            completed_root_moves++;

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
        // already has enough root coverage to trust this deeper result.
        const double explored_ratio = static_cast<double>(completed_root_moves)
                                    / static_cast<double>(root_moves.size());
        const bool trust_current_depth = explored_ratio >= PARTIAL_DEPTH_TRUST_RATIO;
        if (depth_best.valid && (trust_current_depth
            || !best_overall.valid
            || depth_best.score > best_overall.score)) {
            best_overall = depth_best;
        }
        break;
    }

    return best_overall;
}

}  // namespace MoveGenerator
