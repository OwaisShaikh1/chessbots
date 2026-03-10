#pragma once

#include "board.h"
#include "movegen.h"

// ─── Legal move utilities ─────────────────────────────────────────────────

bool is_square_attacked(const Board& b, Square sq, Color attacker);
bool in_check(const Board& b, Color side);
void generate_legal_moves(Board& b, MoveList& legal);
