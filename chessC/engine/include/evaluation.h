#pragma once

#include "board.h"

// ─── Evaluation ───────────────────────────────────────────────────────────
//   Returns a score in centipawns from the perspective of the side to move.
//   Positive = good for side to move, negative = bad.

namespace Eval {

int evaluate(const Board& b);

// Component accessors (useful for debugging)
int evaluate_material(const Board& b);
int evaluate_pst(const Board& b);
int evaluate_mobility(const Board& b);
int evaluate_pawn_structure(const Board& b);
int evaluate_king_safety(const Board& b);

} // namespace Eval
