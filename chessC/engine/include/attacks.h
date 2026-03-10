#pragma once

#include "types.h"
#include "bitboard.h"

// ─── Attack tables (precomputed at startup) ───────────────────────────────

namespace Attacks {

// Non-sliding attack tables
extern Bitboard pawn_attacks[COLOR_NB][SQUARE_NB];
extern Bitboard knight_attacks[SQUARE_NB];
extern Bitboard king_attacks[SQUARE_NB];

// Magic bitboard sliding attack tables
extern Bitboard bishop_attacks[SQUARE_NB][512];
extern Bitboard rook_attacks[SQUARE_NB][4096];
extern Bitboard bishop_masks[SQUARE_NB];
extern Bitboard rook_masks[SQUARE_NB];
extern uint64_t bishop_magics[SQUARE_NB];
extern uint64_t rook_magics[SQUARE_NB];
extern int      bishop_shifts[SQUARE_NB];
extern int      rook_shifts[SQUARE_NB];

// Initialize all attack tables — call once at engine startup
void init();

// ── Inline attack getters ─────────────────────────────────────────────

inline Bitboard get_pawn_attacks(Color c, Square s) {
    return pawn_attacks[c][s];
}
inline Bitboard get_knight_attacks(Square s) {
    return knight_attacks[s];
}
inline Bitboard get_king_attacks(Square s) {
    return king_attacks[s];
}

inline Bitboard get_bishop_attacks(Square s, Bitboard occ) {
    occ &= bishop_masks[s];
    return bishop_attacks[s][(occ * bishop_magics[s]) >> bishop_shifts[s]];
}
inline Bitboard get_rook_attacks(Square s, Bitboard occ) {
    occ &= rook_masks[s];
    return rook_attacks[s][(occ * rook_magics[s]) >> rook_shifts[s]];
}
inline Bitboard get_queen_attacks(Square s, Bitboard occ) {
    return get_bishop_attacks(s, occ) | get_rook_attacks(s, occ);
}

} // namespace Attacks

