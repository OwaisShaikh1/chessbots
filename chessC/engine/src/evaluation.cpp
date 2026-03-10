#include "../include/evaluation.h"
#include "../include/attacks.h"
#include "../include/legal_moves.h"

namespace Eval {

// ─── Piece-Square Tables ──────────────────────────────────────────────────
//   Values from White's perspective (A1 = index 0).
//   For Black, we mirror the rank (index = sq ^ 56).

// clang-format off
static const int PAWN_PST[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};

static const int KNIGHT_PST[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

static const int BISHOP_PST[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

static const int ROOK_PST[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0,
};

static const int QUEEN_PST[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};

static const int KING_MID_PST[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20,
};
// clang-format on

static const int* PST[PIECE_TYPE_NB] = {
    nullptr,        // NO_PIECE_TYPE
    PAWN_PST,
    KNIGHT_PST,
    BISHOP_PST,
    ROOK_PST,
    QUEEN_PST,
    KING_MID_PST,
};

// ─── Helpers ──────────────────────────────────────────────────────────────

static inline int pst_score(PieceType pt, Square sq, Color c) {
    // For White: index sq (A1=0); for Black: mirror rank => sq ^ 56
    int idx = (c == WHITE) ? int(sq) : int(sq) ^ 56;
    return PST[pt][idx];
}

// ─── Material ─────────────────────────────────────────────────────────────

int evaluate_material(const Board& b) {
    int score = 0;
    for (int pt = PAWN; pt <= QUEEN; pt++) {
        score += popcount(b.pieces(WHITE, PieceType(pt))) * PIECE_VALUE[pt];
        score -= popcount(b.pieces(BLACK, PieceType(pt))) * PIECE_VALUE[pt];
    }
    return score;
}

// ─── Piece-square tables ──────────────────────────────────────────────────

int evaluate_pst(const Board& b) {
    int score = 0;
    for (int pt = PAWN; pt <= KING; pt++) {
        Bitboard w = b.pieces(WHITE, PieceType(pt));
        while (w) { Square s = pop_lsb(w); score += pst_score(PieceType(pt), s, WHITE); }
        Bitboard bk = b.pieces(BLACK, PieceType(pt));
        while (bk) { Square s = pop_lsb(bk); score -= pst_score(PieceType(pt), s, BLACK); }
    }
    return score;
}

// ─── Mobility ─────────────────────────────────────────────────────────────

int evaluate_mobility(const Board& b) {
    int score = 0;
    Bitboard occ = b.occupancy[COLOR_NB];

    // Count attacked squares for each side's sliders + knights
    for (Color c : {WHITE, BLACK}) {
        int sign = (c == WHITE) ? 1 : -1;
        Bitboard knights = b.pieces(c, KNIGHT);
        while (knights) {
            Square s = pop_lsb(knights);
            score += sign * popcount(Attacks::get_knight_attacks(s) & ~b.occupancy[c]);
        }
        Bitboard bishops = b.pieces(c, BISHOP);
        while (bishops) {
            Square s = pop_lsb(bishops);
            score += sign * popcount(Attacks::get_bishop_attacks(s, occ) & ~b.occupancy[c]);
        }
        Bitboard rooks = b.pieces(c, ROOK);
        while (rooks) {
            Square s = pop_lsb(rooks);
            score += sign * popcount(Attacks::get_rook_attacks(s, occ) & ~b.occupancy[c]);
        }
        Bitboard queens = b.pieces(c, QUEEN);
        while (queens) {
            Square s = pop_lsb(queens);
            score += sign * popcount(Attacks::get_queen_attacks(s, occ) & ~b.occupancy[c]);
        }
    }
    return score / 5; // Scale down — mobility is a minor bonus
}

// ─── Pawn structure ───────────────────────────────────────────────────────

int evaluate_pawn_structure(const Board& b) {
    int score = 0;
    static const int DOUBLED_PAWN_PENALTY  = -15;
    static const int ISOLATED_PAWN_PENALTY = -20;
    static const int PASSED_PAWN_BONUS[8]  = {0, 10, 20, 35, 60, 100, 150, 0};

    for (Color c : {WHITE, BLACK}) {
        int sign = (c == WHITE) ? 1 : -1;
        Bitboard our_pawns = b.pieces(c, PAWN);
        Bitboard opp_pawns = b.pieces(~c, PAWN);

        // Doubled pawns — two or more pawns on same file
        for (int f = 0; f < 8; f++) {
            Bitboard file_mask = FILE_A_BB << f;
            int cnt = popcount(our_pawns & file_mask);
            if (cnt > 1) score += sign * DOUBLED_PAWN_PENALTY * (cnt - 1);
        }

        // Isolated and passed pawns
        Bitboard pawns_copy = our_pawns;
        while (pawns_copy) {
            Square sq = pop_lsb(pawns_copy);
            int f = file_of(sq);
            int r = rank_of(sq);

            // Adjacent file mask
            Bitboard adj = 0;
            if (f > 0) adj |= FILE_A_BB << (f - 1);
            if (f < 7) adj |= FILE_A_BB << (f + 1);

            if (!(our_pawns & adj)) score += sign * ISOLATED_PAWN_PENALTY;

            // Passed pawn: no opposing pawns ahead on same or adjacent files
            Bitboard ahead_mask = 0;
            if (c == WHITE) {
                for (int rr = r + 1; rr <= 7; rr++)
                    ahead_mask |= (FILE_A_BB << f) | (adj);
                // Simpler: fill ranks above
                Bitboard fill = (FILE_A_BB << f) | adj;
                Bitboard above = 0;
                for (int rr = r + 1; rr <= 7; rr++)
                    above |= fill & (RANK_1_BB << (8 * rr));
                if (!(opp_pawns & above)) {
                    int promo_dist = 7 - r;
                    score += sign * PASSED_PAWN_BONUS[7 - promo_dist];
                }
            } else {
                Bitboard fill = (FILE_A_BB << f) | adj;
                Bitboard below = 0;
                for (int rr = r - 1; rr >= 0; rr--)
                    below |= fill & (RANK_1_BB << (8 * rr));
                if (!(opp_pawns & below)) {
                    int promo_dist = r;
                    score += sign * PASSED_PAWN_BONUS[7 - promo_dist];
                }
            }
        }
    }

    return score;
}

// ─── King safety ──────────────────────────────────────────────────────────

int evaluate_king_safety(const Board& b) {
    int score = 0;
    static const int PAWN_SHIELD_BONUS = 10;

    for (Color c : {WHITE, BLACK}) {
        int sign = (c == WHITE) ? 1 : -1;
        Bitboard king_bb = b.pieces(c, KING);
        if (!king_bb) continue;
        Square ksq = lsb(king_bb);

        // Reward pawns near the king
        Bitboard shield = Attacks::get_king_attacks(ksq);
        int pawn_shield = popcount(shield & b.pieces(c, PAWN));
        score += sign * pawn_shield * PAWN_SHIELD_BONUS;

        // Penalty for being in check
        if (is_square_attacked(b, ksq, ~c)) score -= sign * 50;
    }

    return score;
}

// ─── Top-level evaluate ───────────────────────────────────────────────────

int evaluate(const Board& b) {
    int score = 0;
    score += evaluate_material(b);
    score += evaluate_pst(b);
    score += evaluate_mobility(b);
    score += evaluate_pawn_structure(b);
    score += evaluate_king_safety(b);

    // Return from perspective of side to move
    return (b.side_to_move == WHITE) ? score : -score;
}

} // namespace Eval
