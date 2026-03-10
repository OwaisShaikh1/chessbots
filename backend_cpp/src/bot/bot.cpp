#include "bot.hpp"
#include <iostream>

ChessBot::ChessBot(int search_depth) 
    : board_()
    , search_engine_(1000000)
    , search_depth_(search_depth) {
}

std::string ChessBot::get_fen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return board_.fen();
}

void ChessBot::set_fen(const std::string& fen) {
    std::lock_guard<std::mutex> lock(mutex_);
    board_.set_fen(fen);
    search_engine_.clear_tt();
}

void ChessBot::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    board_.reset();
    search_engine_.clear_tt();
}

bool ChessBot::make_move(const std::string& uci) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    chess::Move m = chess::Move::from_uci(uci);
    if (m.is_null()) return false;
    
    // Check if move is legal
    std::vector<chess::Move> legal = board_.legal_moves();
    bool found = false;
    for (chess::Move legal_move : legal) {
        if (legal_move.from() == m.from() && 
            legal_move.to() == m.to() && 
            legal_move.promotion() == m.promotion()) {
            m = legal_move;
            found = true;
            break;
        }
    }
    
    if (!found) return false;
    
    return board_.make_move(m);
}

std::string ChessBot::get_best_move() {
    return get_best_move(search_depth_);
}

std::string ChessBot::get_best_move(int depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (board_.is_game_over()) {
        return "";
    }
    
    search::SearchResult result = search_engine_.search_iterative(board_, depth);
    
    std::cout << "Depth " << result.depth 
              << " Score: " << result.score 
              << " Nodes: " << result.nodes << std::endl;
    
    if (result.best_move.is_null()) {
        return "";
    }
    
    return result.best_move.uci();
}

bool ChessBot::is_game_over() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return board_.is_game_over();
}

bool ChessBot::is_checkmate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return board_.is_checkmate();
}

bool ChessBot::is_stalemate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return board_.is_stalemate();
}

bool ChessBot::is_draw() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return board_.is_draw();
}

std::string ChessBot::get_result() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (board_.is_checkmate()) {
        return board_.side_to_move() == chess::WHITE ? "0-1" : "1-0";
    }
    if (board_.is_stalemate() || board_.is_draw()) {
        return "1/2-1/2";
    }
    return "*";
}
