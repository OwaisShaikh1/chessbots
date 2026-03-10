#pragma once

#include "../chess/board.hpp"
#include "../search/search.hpp"
#include <string>
#include <mutex>

class ChessBot {
public:
    ChessBot(int search_depth = 4);
    
    // Board state
    std::string get_fen() const;
    void set_fen(const std::string& fen);
    void reset();
    
    // Move making
    bool make_move(const std::string& uci);
    
    // Bot move
    std::string get_best_move();
    std::string get_best_move(int depth);
    
    // Game status
    bool is_game_over() const;
    bool is_checkmate() const;
    bool is_stalemate() const;
    bool is_draw() const;
    std::string get_result() const;
    
    // Parameters
    int get_search_depth() const { return search_depth_; }
    void set_search_depth(int depth) { search_depth_ = depth; }
    
    // Thread safety
    std::mutex& get_mutex() { return mutex_; }

private:
    chess::Board board_;
    search::ChessSearch search_engine_;
    int search_depth_;
    mutable std::mutex mutex_;
};
