#include "../include/movegen.h"
#include "../include/attacks.h"

namespace MoveGen {

// ─── Internal helpers ─────────────────────────────────────────────────────

// Add all promotion moves for a pawn moving from→to (capture or quiet)
static inline void add_promotions(MoveList& list, Square from, Square to, bool capture) {
    MoveFlag base = capture ? PROMO_CAPTURE_QUEEN : PROMO_QUEEN;
    // Add all four promotions
    if (capture) {
        list.push(Move(from, to, PROMO_CAPTURE_QUEEN));
        list.push(Move(from, to, PROMO_CAPTURE_ROOK));
        list.push(Move(from, to, PROMO_CAPTURE_BISHOP));
        list.push(Move(from, to, PROMO_CAPTURE_KNIGHT));
    } else {
        list.push(Move(from, to, PROMO_QUEEN));
        list.push(Move(from, to, PROMO_ROOK));
        list.push(Move(from, to, PROMO_BISHOP));
        list.push(Move(from, to, PROMO_KNIGHT));
    }
}

// ─── Pawn move generation ─────────────────────────────────────────────────

static void gen_pawns(const Board& b, MoveList& list, bool captures_only) {
    Color us   = b.side_to_move;
    Color them = ~us;
    Bitboard pawns   = b.pieces(us, PAWN);
    Bitboard occ     = b.occupancy[COLOR_NB];
    Bitboard enemies = b.occupancy[them];

    if (us == WHITE) {
        // Single push
        if (!captures_only) {
            Bitboard push1 = shift_north(pawns) & ~occ;
            Bitboard push2 = shift_north(push1 & RANK_3_BB) & ~occ;

            // Regular single push (non-promotion)
            Bitboard non_promo = push1 & ~RANK_8_BB;
            while (non_promo) {
                Square to = pop_lsb(non_promo);
                list.push(Move(Square(to - 8), to, QUIET));
            }
            // Promotion single push
            Bitboard promo = push1 & RANK_8_BB;
            while (promo) {
                Square to = pop_lsb(promo);
                add_promotions(list, Square(to - 8), to, false);
            }
            // Double push
            while (push2) {
                Square to = pop_lsb(push2);
                list.push(Move(Square(to - 16), to, DOUBLE_PAWN_PUSH));
            }
        }

        // Captures
        Bitboard cap_e = shift_ne(pawns) & enemies;
        Bitboard cap_w = shift_nw(pawns) & enemies;

        Bitboard cap_e_promo  = cap_e & RANK_8_BB;
        Bitboard cap_e_normal = cap_e & ~RANK_8_BB;
        Bitboard cap_w_promo  = cap_w & RANK_8_BB;
        Bitboard cap_w_normal = cap_w & ~RANK_8_BB;

        while (cap_e_normal) { Square to = pop_lsb(cap_e_normal); list.push(Move(Square(to - 9), to, CAPTURE)); }
        while (cap_w_normal) { Square to = pop_lsb(cap_w_normal); list.push(Move(Square(to - 7), to, CAPTURE)); }
        while (cap_e_promo)  { Square to = pop_lsb(cap_e_promo);  add_promotions(list, Square(to - 9), to, true); }
        while (cap_w_promo)  { Square to = pop_lsb(cap_w_promo);  add_promotions(list, Square(to - 7), to, true); }

        // En passant
        if (b.en_passant_square != NO_SQUARE) {
            Square ep = b.en_passant_square;
            Bitboard ep_bb = square_bb(ep);
            Bitboard ep_pawns = shift_se(ep_bb) | shift_sw(ep_bb);
            ep_pawns &= pawns;
            while (ep_pawns) {
                Square from = pop_lsb(ep_pawns);
                list.push(Move(from, ep, EN_PASSANT));
            }
        }

    } else { // BLACK
        if (!captures_only) {
            Bitboard push1 = shift_south(pawns) & ~occ;
            Bitboard push2 = shift_south(push1 & RANK_6_BB) & ~occ;

            Bitboard non_promo = push1 & ~RANK_1_BB;
            while (non_promo) {
                Square to = pop_lsb(non_promo);
                list.push(Move(Square(to + 8), to, QUIET));
            }
            Bitboard promo = push1 & RANK_1_BB;
            while (promo) {
                Square to = pop_lsb(promo);
                add_promotions(list, Square(to + 8), to, false);
            }
            while (push2) {
                Square to = pop_lsb(push2);
                list.push(Move(Square(to + 16), to, DOUBLE_PAWN_PUSH));
            }
        }

        Bitboard cap_e = shift_se(pawns) & enemies;
        Bitboard cap_w = shift_sw(pawns) & enemies;

        Bitboard cap_e_promo  = cap_e & RANK_1_BB;
        Bitboard cap_e_normal = cap_e & ~RANK_1_BB;
        Bitboard cap_w_promo  = cap_w & RANK_1_BB;
        Bitboard cap_w_normal = cap_w & ~RANK_1_BB;

        while (cap_e_normal) { Square to = pop_lsb(cap_e_normal); list.push(Move(Square(to + 7), to, CAPTURE)); }
        while (cap_w_normal) { Square to = pop_lsb(cap_w_normal); list.push(Move(Square(to + 9), to, CAPTURE)); }
        while (cap_e_promo)  { Square to = pop_lsb(cap_e_promo);  add_promotions(list, Square(to + 7), to, true); }
        while (cap_w_promo)  { Square to = pop_lsb(cap_w_promo);  add_promotions(list, Square(to + 9), to, true); }

        if (b.en_passant_square != NO_SQUARE) {
            Square ep = b.en_passant_square;
            Bitboard ep_bb = square_bb(ep);
            Bitboard ep_pawns = shift_ne(ep_bb) | shift_nw(ep_bb);
            ep_pawns &= pawns;
            while (ep_pawns) {
                Square from = pop_lsb(ep_pawns);
                list.push(Move(from, ep, EN_PASSANT));
            }
        }
    }
}

// ─── Knight move generation ───────────────────────────────────────────────

static void gen_knights(const Board& b, MoveList& list, bool captures_only) {
    Color us      = b.side_to_move;
    Bitboard pieces = b.pieces(us, KNIGHT);
    Bitboard occ    = b.occupancy[COLOR_NB];
    Bitboard enemies = b.occupancy[~us];
    while (pieces) {
        Square from = pop_lsb(pieces);
        Bitboard atk = Attacks::get_knight_attacks(from);
        Bitboard caps   = atk & enemies;
        Bitboard quiets = atk & ~occ;
        while (caps)   { Square to = pop_lsb(caps);   list.push(Move(from, to, CAPTURE)); }
        if (!captures_only)
        while (quiets) { Square to = pop_lsb(quiets); list.push(Move(from, to, QUIET)); }
    }
}

// ─── Bishop/rook/queen move generation ───────────────────────────────────

static void gen_sliders(const Board& b, MoveList& list, bool captures_only, PieceType pt) {
    Color us      = b.side_to_move;
    Bitboard pieces = b.pieces(us, pt);
    Bitboard occ    = b.occupancy[COLOR_NB];
    Bitboard enemies = b.occupancy[~us];
    while (pieces) {
        Square from = pop_lsb(pieces);
        Bitboard atk;
        if (pt == BISHOP)       atk = Attacks::get_bishop_attacks(from, occ);
        else if (pt == ROOK)    atk = Attacks::get_rook_attacks(from, occ);
        else /* QUEEN */        atk = Attacks::get_queen_attacks(from, occ);

        Bitboard caps   = atk & enemies;
        Bitboard quiets = atk & ~occ;
        while (caps)   { Square to = pop_lsb(caps);   list.push(Move(from, to, CAPTURE)); }
        if (!captures_only)
        while (quiets) { Square to = pop_lsb(quiets); list.push(Move(from, to, QUIET)); }
    }
}

// ─── King move generation ─────────────────────────────────────────────────

static void gen_king(const Board& b, MoveList& list, bool captures_only) {
    Color us       = b.side_to_move;
    Bitboard piece = b.pieces(us, KING);
    if (!piece) return;
    Square from = lsb(piece);
    Bitboard occ    = b.occupancy[COLOR_NB];
    Bitboard enemies = b.occupancy[~us];
    Bitboard atk    = Attacks::get_king_attacks(from);

    Bitboard caps   = atk & enemies;
    Bitboard quiets = atk & ~occ;
    while (caps)   { Square to = pop_lsb(caps);   list.push(Move(from, to, CAPTURE)); }
    if (!captures_only)
    while (quiets) { Square to = pop_lsb(quiets); list.push(Move(from, to, QUIET)); }
}

// ─── Castling generation ──────────────────────────────────────────────────

static void gen_castling(const Board& b, MoveList& list) {
    // We generate pseudo-legal castling. Legality (attack checks) are
    // verified in the legal move filter using is_square_attacked.
    Color us  = b.side_to_move;
    Bitboard occ = b.occupancy[COLOR_NB];

    if (us == WHITE) {
        // Kingside: E1-F1-G1 must be empty
        if ((b.castling_rights & WHITE_OO) &&
            !(occ & (square_bb(F1) | square_bb(G1))))
            list.push(Move(E1, G1, CASTLE_KINGSIDE));
        // Queenside: D1-C1-B1 must be empty
        if ((b.castling_rights & WHITE_OOO) &&
            !(occ & (square_bb(D1) | square_bb(C1) | square_bb(B1))))
            list.push(Move(E1, C1, CASTLE_QUEENSIDE));
    } else {
        if ((b.castling_rights & BLACK_OO) &&
            !(occ & (square_bb(F8) | square_bb(G8))))
            list.push(Move(E8, G8, CASTLE_KINGSIDE));
        if ((b.castling_rights & BLACK_OOO) &&
            !(occ & (square_bb(D8) | square_bb(C8) | square_bb(B8))))
            list.push(Move(E8, C8, CASTLE_QUEENSIDE));
    }
}

// ─── Public API ───────────────────────────────────────────────────────────

void generate_moves(const Board& b, MoveList& list) {
    gen_pawns(b, list, false);
    gen_knights(b, list, false);
    gen_sliders(b, list, false, BISHOP);
    gen_sliders(b, list, false, ROOK);
    gen_sliders(b, list, false, QUEEN);
    gen_king(b, list, false);
    gen_castling(b, list);
}

void generate_captures(const Board& b, MoveList& list) {
    gen_pawns(b, list, true);
    gen_knights(b, list, true);
    gen_sliders(b, list, true, BISHOP);
    gen_sliders(b, list, true, ROOK);
    gen_sliders(b, list, true, QUEEN);
    gen_king(b, list, true);
    // No castling — castling is never a capture
}

void generate_quiets(const Board& b, MoveList& list) {
    gen_pawns(b, list, false);
    gen_knights(b, list, false);
    gen_sliders(b, list, false, BISHOP);
    gen_sliders(b, list, false, ROOK);
    gen_sliders(b, list, false, QUEEN);
    gen_king(b, list, false);
    gen_castling(b, list);
    // Remove captures from the list — for true "quiets only" pass, caller
    // can call generate_moves and skip captures, or use this helper directly.
    // Here we generate all and the caller filters as needed.
}

} // namespace MoveGen
