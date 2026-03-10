#include "../include/movegen.h"
#include "../include/attacks.h"

// ─── is_square_attacked ───────────────────────────────────────────────────
//   Returns true if `sq` is attacked by any piece of `attacker` color.

bool is_square_attacked(const Board& b, Square sq, Color attacker) {
    Bitboard occ = b.occupancy[COLOR_NB];

    // Pawns
    Bitboard pawns = b.pieces(attacker, PAWN);
    if (Attacks::get_pawn_attacks(~attacker, sq) & pawns) return true;

    // Knights
    if (Attacks::get_knight_attacks(sq) & b.pieces(attacker, KNIGHT)) return true;

    // King
    if (Attacks::get_king_attacks(sq) & b.pieces(attacker, KING)) return true;

    // Bishops + Queen (diagonal)
    Bitboard diag = b.pieces(attacker, BISHOP) | b.pieces(attacker, QUEEN);
    if (Attacks::get_bishop_attacks(sq, occ) & diag) return true;

    // Rooks + Queen (straight)
    Bitboard straight = b.pieces(attacker, ROOK) | b.pieces(attacker, QUEEN);
    if (Attacks::get_rook_attacks(sq, occ) & straight) return true;

    return false;
}

// ─── in_check ─────────────────────────────────────────────────────────────

bool in_check(const Board& b, Color side) {
    Bitboard king_bb = b.pieces(side, KING);
    if (!king_bb) return false;
    return is_square_attacked(b, lsb(king_bb), ~side);
}

// ─── generate_legal_moves ─────────────────────────────────────────────────
//   Filters pseudo-legal moves to only legal ones (king not left in check).

void generate_legal_moves(Board& b, MoveList& legal) {
    MoveList pseudo;
    MoveGen::generate_moves(b, pseudo);

    Color us = b.side_to_move;

    for (int i = 0; i < pseudo.count; i++) {
        Move m = pseudo.moves[i];

        // Castling: additionally check that king doesn't pass through check
        if (m.is_castling()) {
            // King must not be in check currently
            Bitboard king_bb = b.pieces(us, KING);
            Square ksq = lsb(king_bb);
            if (is_square_attacked(b, ksq, ~us)) continue;

            // King must not pass through attacked square
            Square pass_sq = (m.flag() == CASTLE_KINGSIDE) ? Square(ksq + 1) : Square(ksq - 1);
            if (is_square_attacked(b, pass_sq, ~us)) continue;
        }

        b.make_move(m);
        // After making the move, the king of `us` must not be in check
        Bitboard king_bb = b.pieces(us, KING);
        bool leaves_check = king_bb && is_square_attacked(b, lsb(king_bb), ~us);
        b.undo_move(m);

        if (!leaves_check) legal.push(m);
    }
}
