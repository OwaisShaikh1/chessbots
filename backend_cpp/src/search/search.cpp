#include "search.hpp"
#include <algorithm>
#include <cmath>

namespace search {

// Late Move Reduction table
static int LMR_TABLE[64][64];
static bool lmr_initialized = false;

static void init_lmr() {
    if (lmr_initialized) return;
    for (int d = 1; d < 64; d++) {
        for (int m = 1; m < 64; m++) {
            if (d >= 3 && m >= 4) {
                LMR_TABLE[d][m] = std::max(1, static_cast<int>(0.5 + std::log(d) * std::log(m) / 2.0));
            } else {
                LMR_TABLE[d][m] = 0;
            }
        }
    }
    lmr_initialized = true;
}

ChessSearch::ChessSearch(size_t tt_size) : tt_size_(tt_size) {
    init_lmr();
    tt_.resize(tt_size);
    clear_tt();
}

void ChessSearch::clear_tt() {
    std::fill(tt_.begin(), tt_.end(), TTEntry{});
    for (auto& ply_killers : killers_) {
        ply_killers.fill(chess::NULL_MOVE);
    }
    for (auto& color_hist : history_) {
        for (auto& from_hist : color_hist) {
            from_hist.fill(0);
        }
    }
    nodes_ = 0;
}

SearchResult ChessSearch::search(chess::Board& board, int depth) {
    nodes_ = 0;
    
    SearchResult result;
    result.depth = depth;
    
    int alpha = -chess::MATE_VALUE;
    int beta = chess::MATE_VALUE;
    
    std::vector<chess::Move> moves = board.legal_moves();
    if (moves.empty()) {
        result.score = board.is_in_check() ? -chess::MATE_VALUE : 0;
        return result;
    }
    
    moves = order_moves(board, moves, chess::NULL_MOVE);
    
    int best_score = -chess::MATE_VALUE;
    chess::Move best_move = moves[0];
    
    for (chess::Move m : moves) {
        board.make_move(m);
        int score = -alphabeta(board, depth - 1, -beta, -alpha, 1);
        board.unmake_move(m);
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
            alpha = std::max(alpha, score);
        }
    }
    
    result.best_move = best_move;
    result.score = best_score;
    result.nodes = nodes_;
    
    return result;
}

SearchResult ChessSearch::search_iterative(chess::Board& board, int max_depth) {
    SearchResult result;
    
    for (int depth = 1; depth <= max_depth; depth++) {
        result = search(board, depth);
        result.depth = depth;
        
        // Early exit on mate
        if (std::abs(result.score) > chess::MATE_VALUE / 2) {
            break;
        }
    }
    
    return result;
}

int ChessSearch::alphabeta(chess::Board& board, int depth, int alpha, int beta, int ply, bool allow_nmp) {
    nodes_++;
    int alpha_orig = alpha;
    bool in_check = board.is_in_check();
    
    // Transposition table lookup
    uint64_t hash = board.hash();
    size_t tt_idx = hash % tt_size_;
    TTEntry& tt_entry = tt_[tt_idx];
    chess::Move tt_move = chess::NULL_MOVE;
    
    if (tt_entry.hash == hash && tt_entry.depth >= depth) {
        tt_move = tt_entry.best_move;
        int tt_val = tt_entry.value;
        
        // Adjust mate scores for ply
        if (tt_val > chess::MATE_VALUE / 2) tt_val -= ply;
        else if (tt_val < -chess::MATE_VALUE / 2) tt_val += ply;
        
        if (tt_entry.flag == TT_EXACT) return tt_val;
        else if (tt_entry.flag == TT_LOWER) alpha = std::max(alpha, tt_val);
        else if (tt_entry.flag == TT_UPPER) beta = std::min(beta, tt_val);
        
        if (alpha >= beta) return tt_val;
    } else if (tt_entry.hash == hash) {
        tt_move = tt_entry.best_move;
    }
    
    // Terminal conditions
    if (board.is_checkmate()) return -chess::MATE_VALUE + ply;
    if (board.is_stalemate() || board.is_draw()) return 0;
    if (depth <= 0) return quiescence(board, alpha, beta, ply, quiescence_depth);
    
    // Mate distance pruning
    int mate_alpha = -chess::MATE_VALUE + ply;
    int mate_beta = chess::MATE_VALUE - ply - 1;
    if (mate_alpha > alpha) {
        alpha = mate_alpha;
        if (alpha >= beta) return alpha;
    }
    if (mate_beta < beta) {
        beta = mate_beta;
        if (alpha >= beta) return beta;
    }
    
    // Null move pruning
    if (allow_nmp && !in_check && depth >= 3) {
        // Check we have non-pawn material
        chess::Color us = board.side_to_move();
        chess::Bitboard our_pieces = board.pieces(us) & ~board.pieces(us, chess::PAWN) & ~board.pieces(us, chess::KING);
        if (our_pieces) {
            int R = 2 + (depth >= 6 ? 1 : 0);
            board.make_move(chess::NULL_MOVE);
            int null_score = -alphabeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
            board.unmake_move(chess::NULL_MOVE);
            
            if (null_score >= beta) return beta;
        }
    }
    
    // Generate and order moves
    std::vector<chess::Move> moves = board.legal_moves();
    if (moves.empty()) {
        return in_check ? -chess::MATE_VALUE + ply : 0;
    }
    
    moves = order_moves(board, moves, tt_move);
    
    int best_score = -chess::MATE_VALUE;
    chess::Move best_move = moves[0];
    int moves_searched = 0;
    
    for (chess::Move m : moves) {
        bool is_capture = board.piece_at(m.to()) != chess::NO_PIECE;
        bool is_promotion = m.is_promotion();
        
        board.make_move(m);
        bool gives_check = board.is_in_check();
        
        int score;
        
        // Late Move Reduction
        int reduction = 0;
        if (moves_searched >= 4 && depth >= 3 && !in_check && !gives_check && !is_capture && !is_promotion) {
            reduction = LMR_TABLE[std::min(depth, 63)][std::min(moves_searched, 63)];
        }
        
        // PVS
        if (moves_searched == 0) {
            score = -alphabeta(board, depth - 1, -beta, -alpha, ply + 1);
        } else {
            // Reduced depth search
            score = -alphabeta(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            
            // Re-search if reduced search improved alpha
            if (reduction > 0 && score > alpha) {
                score = -alphabeta(board, depth - 1, -alpha - 1, -alpha, ply + 1);
            }
            
            // Full window re-search if null window search improved alpha
            if (score > alpha && score < beta) {
                score = -alphabeta(board, depth - 1, -beta, -alpha, ply + 1);
            }
        }
        
        board.unmake_move(m);
        moves_searched++;
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
            
            if (score > alpha) {
                alpha = score;
                
                if (score >= beta) {
                    // Beta cutoff - update killers and history for quiet moves
                    if (!is_capture) {
                        update_killers(m, ply);
                        update_history(board.side_to_move(), m, depth);
                    }
                    break;
                }
            }
        }
    }
    
    // Store in transposition table
    TTFlag flag;
    if (best_score <= alpha_orig) flag = TT_UPPER;
    else if (best_score >= beta) flag = TT_LOWER;
    else flag = TT_EXACT;
    
    tt_entry.hash = hash;
    tt_entry.depth = depth;
    tt_entry.value = best_score;
    tt_entry.flag = flag;
    tt_entry.best_move = best_move;
    
    return best_score;
}

int ChessSearch::quiescence(chess::Board& board, int alpha, int beta, int ply, int max_depth) {
    nodes_++;
    
    // Stand pat score
    int stand_pat = eval::evaluate(board);
    if (board.side_to_move() == chess::BLACK) stand_pat = -stand_pat;
    
    if (ply >= chess::MAX_PLY) return stand_pat;
    
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;
    
    if (max_depth <= 0) return stand_pat;
    
    // Search only captures
    std::vector<chess::Move> captures = board.legal_captures();
    captures = order_moves(board, captures, chess::NULL_MOVE);
    
    for (chess::Move m : captures) {
        // Delta pruning
        chess::Piece captured = board.piece_at(m.to());
        if (captured != chess::NO_PIECE) {
            int delta = eval::PIECE_VALUES.get(chess::type_of(captured)) + 200;
            if (stand_pat + delta < alpha) continue;
        }
        
        board.make_move(m);
        int score = -quiescence(board, -beta, -alpha, ply + 1, max_depth - 1);
        board.unmake_move(m);
        
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    
    return alpha;
}

int ChessSearch::score_move(const chess::Board& board, chess::Move m, chess::Move tt_move) {
    // TT move gets highest priority
    if (m == tt_move) return 100000;
    
    // Captures scored by MVV-LVA
    chess::Piece captured = board.piece_at(m.to());
    if (captured != chess::NO_PIECE) {
        chess::Piece attacker = board.piece_at(m.from());
        return 10000 + eval::MVV_LVA[chess::type_of(captured)][chess::type_of(attacker)];
    }
    
    // Promotions
    if (m.is_promotion()) {
        return 9000 + (m.promotion() == chess::QUEEN ? 100 : 0);
    }
    
    // Killers
    int ply = 0; // TODO: track ply properly
    if (m == killers_[ply][0]) return 8000;
    if (m == killers_[ply][1]) return 7900;
    
    // History heuristic
    return history_[board.side_to_move()][m.from()][m.to()];
}

std::vector<chess::Move> ChessSearch::order_moves(const chess::Board& board, const std::vector<chess::Move>& moves, chess::Move tt_move) {
    std::vector<std::pair<int, chess::Move>> scored_moves;
    scored_moves.reserve(moves.size());
    
    for (chess::Move m : moves) {
        scored_moves.emplace_back(score_move(board, m, tt_move), m);
    }
    
    std::sort(scored_moves.begin(), scored_moves.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<chess::Move> ordered;
    ordered.reserve(moves.size());
    for (const auto& sm : scored_moves) {
        ordered.push_back(sm.second);
    }
    
    return ordered;
}

void ChessSearch::update_killers(chess::Move m, int ply) {
    if (ply >= chess::MAX_PLY) return;
    if (m == killers_[ply][0]) return;
    killers_[ply][1] = killers_[ply][0];
    killers_[ply][0] = m;
}

void ChessSearch::update_history(chess::Color c, chess::Move m, int depth) {
    int bonus = depth * depth;
    history_[c][m.from()][m.to()] += bonus;
    
    // Age history to prevent overflow
    if (history_[c][m.from()][m.to()] > 10000) {
        for (int f = 0; f < 64; f++) {
            for (int t = 0; t < 64; t++) {
                history_[c][f][t] /= 2;
            }
        }
    }
}

} // namespace search
