#pragma once

#include "types.h"

// ─── Zobrist hashing ──────────────────────────────────────────────────────
//   Keys are initialized once at startup and used throughout the engine.

namespace Zobrist {

void init();

Key piece_key(Piece p, Square s);
Key side_key();
Key castling_key(CastlingRights cr);
Key ep_key(int file);

} // namespace Zobrist
