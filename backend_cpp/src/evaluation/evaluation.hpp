#pragma once

#include "../chess/board.hpp"
#include "pst.hpp"

namespace eval {

// Global evaluation parameters (can be modified at runtime)
extern PieceValues PIECE_VALUES;
extern EvalParams EVAL_PARAMS;

// Main evaluation function - returns score from White's perspective
int evaluate(const chess::Board& board);

// Fast evaluation (material + PST only)
int evaluate_fast(const chess::Board& board);

// Material score
int material_score(const chess::Board& board);

// Endgame detection
bool is_endgame(const chess::Board& board);

// Endgame mate bonus (for driving king to corner)
int endgame_mate_bonus(const chess::Board& board);

} // namespace eval
