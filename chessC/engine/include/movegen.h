#pragma once

#include "types.h"
#include "board.h"
#include <vector>

// ─── Move list ────────────────────────────────────────────────────────────

struct MoveList {
    Move moves[256];
    int  count = 0;

    void push(Move m)      { moves[count++] = m; }
    Move* begin()          { return moves; }
    Move* end()            { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end()   const { return moves + count; }
    int  size()  const     { return count; }
    bool empty() const     { return count == 0; }
};

// ─── Move generation ──────────────────────────────────────────────────────

namespace MoveGen {

// Generate all pseudo-legal moves for the side to move
void generate_moves(const Board& b, MoveList& list);

// Generate only captures (and promotions) — used in quiescence search
void generate_captures(const Board& b, MoveList& list);

// Generate only quiet moves
void generate_quiets(const Board& b, MoveList& list);

} // namespace MoveGen
