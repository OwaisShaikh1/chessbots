#include "../include/search.h"
#include "../include/evaluation.h"
#include "../include/legal_moves.h"
#include "../include/attacks.h"
#include <algorithm>
#include <iostream>
#include <cstring>

Searcher ENGINE;

// ─── MVV-LVA table ────────────────────────────────────────────────────────
//   [attacker][victim] — higher = better capture to try first

static const int MVV_LVA[PIECE_TYPE_NB][PIECE_TYPE_NB] = {
    // victim: none  pawn knight bishop rook queen king
    {0, 0,   0,   0,   0,   0,   0},   // attacker: none
    {0, 105, 205, 305, 405, 505, 605}, // pawn
    {0, 104, 204, 304, 404, 504, 604}, // knight
    {0, 103, 203, 303, 403, 503, 603}, // bishop
    {0, 102, 202, 302, 402, 502, 602}, // rook
    {0, 101, 201, 301, 401, 501, 601}, // queen
    {0, 100, 200, 300, 400, 500, 600}, // king
};

// ─── Constructor ──────────────────────────────────────────────────────────

Searcher::Searcher() : stopped_(false), nodes_(0), time_limit_ms_(0) {
    clear_heuristics();
}

void Searcher::clear_heuristics() {
    std::memset(killers, 0, sizeof(killers));
    std::memset(history, 0, sizeof(history));
}

// ─── Time management ──────────────────────────────────────────────────────

bool Searcher::time_up() const {
    if (time_limit_ms_ <= 0) return false;
    auto now = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(now - start_time_).count();
    return elapsed >= time_limit_ms_;
}

// ─── Move ordering ────────────────────────────────────────────────────────

int Searcher::move_score(const Board& b, Move m, int ply, Move tt_move) const {
    // 1. TT move gets highest priority
    if (m == tt_move) return 2000000;

    // 2. Captures - MVV-LVA
    if (m.is_capture() || m.flag() == EN_PASSANT) {
        PieceType attacker = type_of(b.piece_at(m.from()));
        PieceType victim;
        if (m.flag() == EN_PASSANT) {
            victim = PAWN;
        } else {
            Piece cap_pc = b.piece_at(m.to());
            victim = (cap_pc == NO_PIECE) ? NO_PIECE_TYPE : type_of(cap_pc);
        }
        return 1000000 + MVV_LVA[attacker][victim];
    }

    // 3. Promotions
    if (m.is_promotion()) return 900000 + PIECE_VALUE[m.promo_piece()];

    // 4. Killer moves
    if (ply < MAX_PLY) {
        if (m == killers[ply][0]) return 800000;
        if (m == killers[ply][1]) return 700000;
    }

    // 5. History heuristic
    if (ply < MAX_PLY) {
        int h = history[b.side_to_move][m.from()][m.to()];
        return std::min(h, 599999);
    }

    return 0;
}

void Searcher::order_moves(const Board& b, MoveList& list, int ply, Move tt_move) {
    // Score all moves, then insertion-sort (good enough for small lists)
    int scores[256];
    for (int i = 0; i < list.count; i++)
        scores[i] = move_score(b, list.moves[i], ply, tt_move);

    // Simple selection sort
    for (int i = 0; i < list.count - 1; i++) {
        int best_idx = i;
        for (int j = i + 1; j < list.count; j++)
            if (scores[j] > scores[best_idx]) best_idx = j;
        if (best_idx != i) {
            std::swap(list.moves[i], list.moves[best_idx]);
            std::swap(scores[i],     scores[best_idx]);
        }
    }
}

// ─── Quiescence search ────────────────────────────────────────────────────

int Searcher::quiescence(Board& b, int alpha, int beta, int ply) {
    if (stopped_ || time_up()) { stopped_ = true; return 0; }
    nodes_++;

    int stand_pat = Eval::evaluate(b);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    if (ply >= MAX_PLY) return alpha;

    MoveList captures;
    MoveGen::generate_captures(b, captures);

    // Filter to legal captures only (fast path)
    Color us = b.side_to_move;
    for (int i = 0; i < captures.count; i++) {
        Move m = captures.moves[i];
        b.make_move(m);
        Bitboard king_bb = b.pieces(us, KING);
        bool illegal = king_bb && is_square_attacked(b, lsb(king_bb), ~us);
        b.undo_move(m);
        if (illegal) { captures.moves[i] = captures.moves[--captures.count]; i--; }
    }

    order_moves(b, captures, ply, NULL_MOVE);

    for (int i = 0; i < captures.count; i++) {
        b.make_move(captures.moves[i]);
        int score = -quiescence(b, -beta, -alpha, ply + 1);
        b.undo_move(captures.moves[i]);

        if (stopped_) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

// ─── Alpha-beta ───────────────────────────────────────────────────────────

int Searcher::alpha_beta(Board& b, int alpha, int beta, int depth, int ply) {
    if (stopped_ || time_up()) { stopped_ = true; return 0; }

    // Mate distance pruning
    alpha = std::max(alpha, mated_in(ply));
    beta  = std::min(beta,  mate_in(ply));
    if (alpha >= beta) return alpha;

    // TT probe
    TTEntry* tte = TT.probe(b.zobrist_hash);
    Move tt_move = NULL_MOVE;
    if (tte) {
        tt_move = tte->best_move;
        if (tte->depth >= depth) {
            int tt_score = tte->score;
            if      (tte->flag == TT_EXACT) return tt_score;
            else if (tte->flag == TT_ALPHA && tt_score <= alpha) return alpha;
            else if (tte->flag == TT_BETA  && tt_score >= beta)  return beta;
        }
    }

    if (depth <= 0) return quiescence(b, alpha, beta, ply);

    nodes_++;

    MoveList legal;
    generate_legal_moves(b, legal);

    if (legal.count == 0) {
        if (in_check(b, b.side_to_move)) return mated_in(ply);
        return DRAW_SCORE; // stalemate
    }

    // Fifty-move rule / threefold (simplified: just 50-move)
    if (b.halfmove_clock >= 100) return DRAW_SCORE;

    order_moves(b, legal, ply, tt_move);

    int orig_alpha = alpha;
    Move best_move = legal.moves[0];
    int  best_score = -SCORE_INFINITE;

    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        b.make_move(m);
        int score = -alpha_beta(b, -beta, -alpha, depth - 1, ply + 1);
        b.undo_move(m);

        if (stopped_) return 0;

        if (score > best_score) {
            best_score = score;
            best_move  = m;
        }
        if (score > alpha) {
            alpha = score;
            if (alpha >= beta) {
                // Beta cutoff — store killer and history
                if (!m.is_capture() && ply < MAX_PLY) {
                    killers[ply][1] = killers[ply][0];
                    killers[ply][0] = m;
                    history[b.side_to_move][m.from()][m.to()] += depth * depth;
                }
                TT.store(b.zobrist_hash, depth, beta, TT_BETA, best_move);
                return beta;
            }
        }
    }

    TTFlag flag = (best_score > orig_alpha) ? TT_EXACT : TT_ALPHA;
    TT.store(b.zobrist_hash, depth, best_score, flag, best_move);
    return best_score;
}

// ─── Iterative deepening ──────────────────────────────────────────────────

int Searcher::iterative_deepening(Board& b, int max_depth, SearchInfo& info) {
    int best_score = 0;
    Move best_move = NULL_MOVE;

    for (int depth = 1; depth <= max_depth; depth++) {
        int score = alpha_beta(b, -SCORE_INFINITE, SCORE_INFINITE, depth, 0);

        if (stopped_) break;

        best_score = score;

        // Retrieve best move from TT
        TTEntry* tte = TT.probe(b.zobrist_hash);
        if (tte && !tte->best_move.is_null()) best_move = tte->best_move;

        info.depth  = depth;
        info.score  = best_score;
        info.best_move = best_move;
        info.nodes  = nodes_;

        auto now = std::chrono::high_resolution_clock::now();
        info.elapsed_ms = std::chrono::duration<double, std::milli>(now - start_time_).count();

        // UCI info output
        int nps = (info.elapsed_ms > 0) ? int(nodes_ / (info.elapsed_ms / 1000.0)) : 0;
        std::cout << "info depth " << depth
                  << " score cp " << best_score
                  << " nodes " << nodes_
                  << " nps " << nps
                  << " time " << int(info.elapsed_ms);
        if (!best_move.is_null()) {
            std::cout << " pv "
                      << char('a' + file_of(best_move.from()))
                      << char('1' + rank_of(best_move.from()))
                      << char('a' + file_of(best_move.to()))
                      << char('1' + rank_of(best_move.to()));
            if (best_move.is_promotion()) {
                static const char pc[] = "  nbrq";
                std::cout << pc[best_move.promo_piece()];
            }
        }
        std::cout << "\n";
        std::cout.flush();

        if (time_up()) break;
    }

    return best_score;
}

// ─── search_position ──────────────────────────────────────────────────────

void Searcher::search_position(Board& b, const SearchLimits& limits, SearchInfo& info) {
    stopped_       = false;
    nodes_         = 0;
    time_limit_ms_ = limits.movetime_ms;
    start_time_    = std::chrono::high_resolution_clock::now();

    clear_heuristics();

    iterative_deepening(b, limits.depth, info);
}
