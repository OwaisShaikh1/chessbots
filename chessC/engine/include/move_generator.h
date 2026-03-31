#pragma once

#include <cstdint>
#include <vector>
#include "board.h"
#include "move.h"

namespace MoveGenerator {

struct ScoredMove {
	Move move;
	int score = 0;
	bool valid = false;
};

std::vector<Move> generate_for_piece(const Board& board, int row, int col);
std::vector<Move> generate_all(const Board& board, Color side);
std::uint64_t position_key(const Board& board, Color side_to_move);
ScoredMove choose_best_move(const Board& board, Color side);
ScoredMove choose_best_move(const Board& board, Color side, int depth);
ScoredMove choose_best_move(const Board& board, Color side, int depth, int movetime_ms);
ScoredMove choose_best_move(const Board& board,
						   Color side,
						   int depth,
						   int movetime_ms,
						   const std::vector<std::uint64_t>& prior_position_keys);

}  // namespace MoveGenerator
