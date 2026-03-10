#pragma once

#include "../chess/board.hpp"
#include "../evaluation/evaluation.hpp"
#include <unordered_map>
#include <vector>
#include <array>

namespace search {

// Transposition table entry types
enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_LOWER = 1,  // Failed high (beta cutoff)
    TT_UPPER = 2   // Failed low (alpha not improved)
};

// Transposition table entry
struct TTEntry {
    uint64_t hash = 0;
    int depth = 0;
    int value = 0;
    TTFlag flag = TT_EXACT;
    chess::Move best_move;
};

// Search result
struct SearchResult {
    chess::Move best_move;
    int score = 0;
    int depth = 0;
    int nodes = 0;
};

class ChessSearch {
public:
    ChessSearch(size_t tt_size = 1000000);
    
    // Main search function
    SearchResult search(chess::Board& board, int depth);
    
    // Iterative deepening search
    SearchResult search_iterative(chess::Board& board, int max_depth);
    
    // Clear transposition table
    void clear_tt();
    
    // Get nodes searched
    int nodes_searched() const { return nodes_; }
    
    // Parameters
    int quiescence_depth = 3;

private:
    // Alpha-beta with negamax
    int alphabeta(chess::Board& board, int depth, int alpha, int beta, int ply, bool allow_nmp = true);
    
    // Quiescence search
    int quiescence(chess::Board& board, int alpha, int beta, int ply, int max_depth);
    
    // Move ordering
    std::vector<chess::Move> order_moves(const chess::Board& board, const std::vector<chess::Move>& moves, chess::Move tt_move);
    
    // Score a move for ordering
    int score_move(const chess::Board& board, chess::Move m, chess::Move tt_move);
    
    // Update killer moves
    void update_killers(chess::Move m, int ply);
    
    // Update history heuristic  
    void update_history(chess::Color c, chess::Move m, int depth);
    
    // Transposition table
    std::vector<TTEntry> tt_;
    size_t tt_size_;
    
    // Killer moves (2 per ply)
    std::array<std::array<chess::Move, 2>, chess::MAX_PLY> killers_;
    
    // History heuristic [color][from][to]
    std::array<std::array<std::array<int, 64>, 64>, 2> history_;
    
    // Stats
    int nodes_ = 0;
};

} // namespace search
