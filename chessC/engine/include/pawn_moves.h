#pragma once

#include <vector>
#include "board.h"
#include "move.h"

std::vector<Move> generate_pawn_moves(const Board& board, int row, int col, Color color);
