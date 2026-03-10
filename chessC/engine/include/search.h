#pragma once

#include "board.h"
#include "movegen.h"
#include "tt.h"
#include <atomic>
#include <chrono>
#include <cstring>

// ─── Search limits ────────────────────────────────────────────────────────

struct SearchLimits {
    int   depth       = 64;
    int   movetime_ms = 0;   // 0 = no time limit
    bool  infinite    = false;
};

// ─── Search info (output) ─────────────────────────────────────────────────

struct SearchInfo {
    int    depth      = 0;
    int    score      = 0;
    Move   best_move;
    uint64_t nodes    = 0;
    double elapsed_ms = 0.0;
};

// ─── Searcher ─────────────────────────────────────────────────────────────

class Searcher {
public:
    static constexpr int MAX_PLY = 128;
    static constexpr int MAX_KILLERS = 2;

    Searcher();

    // Start a search — populates info.best_move
    void search_position(Board& b, const SearchLimits& limits, SearchInfo& info);

    void stop() { stopped_ = true; }
    bool is_stopped() const { return stopped_; }

    // Move ordering state (public so move ordering helpers can access)
    Move  killers[MAX_PLY][MAX_KILLERS];
    int   history[COLOR_NB][SQUARE_NB][SQUARE_NB]; // history[side][from][to]

private:
    std::atomic<bool> stopped_;
    uint64_t nodes_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    int time_limit_ms_;

    void clear_heuristics();
    bool time_up() const;

    int iterative_deepening(Board& b, int max_depth, SearchInfo& info);
    int alpha_beta(Board& b, int alpha, int beta, int depth, int ply);
    int quiescence(Board& b, int alpha, int beta, int ply);

    void order_moves(const Board& b, MoveList& list, int ply, Move tt_move);
    int  move_score(const Board& b, Move m, int ply, Move tt_move) const;
};

// Global searcher instance
extern Searcher ENGINE;
