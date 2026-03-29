#pragma once

#include <string>

#include "board.h"
#include "move.h"

namespace PositionHeuristics {

bool set_pst_file(const std::string& path);
bool reload_pst();
std::string current_pst_file();

int evaluate_board(const Board& board, Color perspective);
int evaluate_move(const Board& board, const Move& move, Color perspective);

}  // namespace PositionHeuristics
