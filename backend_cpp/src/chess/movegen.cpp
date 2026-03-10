#include "board.hpp"

namespace chess {

std::vector<Move> Board::pseudo_legal_moves() const {
    std::vector<Move> moves;
    moves.reserve(64);
    
    Color us = side_to_move_;
    Color them = ~us;
    Bitboard our_pieces = by_color_[us];
    Bitboard their_pieces = by_color_[them];
    Bitboard empty = ~occupied_;
    
    // Pawn moves
    Bitboard pawns = pieces(us, PAWN);
    if (us == WHITE) {
        // Single push
        Bitboard push1 = (pawns << 8) & empty;
        Bitboard push2 = ((push1 & Rank3BB) << 8) & empty;
        Bitboard promo = push1 & Rank8BB;
        push1 &= ~Rank8BB;
        
        while (push1) {
            Square to = pop_lsb(push1);
            moves.emplace_back(Square(to - 8), to);
        }
        while (push2) {
            Square to = pop_lsb(push2);
            moves.emplace_back(Square(to - 16), to);
        }
        while (promo) {
            Square to = pop_lsb(promo);
            moves.emplace_back(Square(to - 8), to, QUEEN);
            moves.emplace_back(Square(to - 8), to, ROOK);
            moves.emplace_back(Square(to - 8), to, BISHOP);
            moves.emplace_back(Square(to - 8), to, KNIGHT);
        }
        
        // Captures
        Bitboard cap_left = ((pawns & ~FileABB) << 7) & their_pieces;
        Bitboard cap_right = ((pawns & ~FileHBB) << 9) & their_pieces;
        Bitboard promo_left = cap_left & Rank8BB;
        Bitboard promo_right = cap_right & Rank8BB;
        cap_left &= ~Rank8BB;
        cap_right &= ~Rank8BB;
        
        while (cap_left) {
            Square to = pop_lsb(cap_left);
            moves.emplace_back(Square(to - 7), to);
        }
        while (cap_right) {
            Square to = pop_lsb(cap_right);
            moves.emplace_back(Square(to - 9), to);
        }
        while (promo_left) {
            Square to = pop_lsb(promo_left);
            moves.emplace_back(Square(to - 7), to, QUEEN);
            moves.emplace_back(Square(to - 7), to, ROOK);
            moves.emplace_back(Square(to - 7), to, BISHOP);
            moves.emplace_back(Square(to - 7), to, KNIGHT);
        }
        while (promo_right) {
            Square to = pop_lsb(promo_right);
            moves.emplace_back(Square(to - 9), to, QUEEN);
            moves.emplace_back(Square(to - 9), to, ROOK);
            moves.emplace_back(Square(to - 9), to, BISHOP);
            moves.emplace_back(Square(to - 9), to, KNIGHT);
        }
        
        // En passant
        if (ep_square_ != NO_SQUARE) {
            Bitboard ep_attackers = PAWN_ATTACKS[BLACK][ep_square_] & pawns;
            while (ep_attackers) {
                Square from = pop_lsb(ep_attackers);
                moves.emplace_back(from, ep_square_);
            }
        }
    } else {
        // Black pawns (mirror of white)
        Bitboard push1 = (pawns >> 8) & empty;
        Bitboard push2 = ((push1 & Rank6BB) >> 8) & empty;
        Bitboard promo = push1 & Rank1BB;
        push1 &= ~Rank1BB;
        
        while (push1) {
            Square to = pop_lsb(push1);
            moves.emplace_back(Square(to + 8), to);
        }
        while (push2) {
            Square to = pop_lsb(push2);
            moves.emplace_back(Square(to + 16), to);
        }
        while (promo) {
            Square to = pop_lsb(promo);
            moves.emplace_back(Square(to + 8), to, QUEEN);
            moves.emplace_back(Square(to + 8), to, ROOK);
            moves.emplace_back(Square(to + 8), to, BISHOP);
            moves.emplace_back(Square(to + 8), to, KNIGHT);
        }
        
        Bitboard cap_left = ((pawns & ~FileHBB) >> 7) & their_pieces;
        Bitboard cap_right = ((pawns & ~FileABB) >> 9) & their_pieces;
        Bitboard promo_left = cap_left & Rank1BB;
        Bitboard promo_right = cap_right & Rank1BB;
        cap_left &= ~Rank1BB;
        cap_right &= ~Rank1BB;
        
        while (cap_left) {
            Square to = pop_lsb(cap_left);
            moves.emplace_back(Square(to + 7), to);
        }
        while (cap_right) {
            Square to = pop_lsb(cap_right);
            moves.emplace_back(Square(to + 9), to);
        }
        while (promo_left) {
            Square to = pop_lsb(promo_left);
            moves.emplace_back(Square(to + 7), to, QUEEN);
            moves.emplace_back(Square(to + 7), to, ROOK);
            moves.emplace_back(Square(to + 7), to, BISHOP);
            moves.emplace_back(Square(to + 7), to, KNIGHT);
        }
        while (promo_right) {
            Square to = pop_lsb(promo_right);
            moves.emplace_back(Square(to + 9), to, QUEEN);
            moves.emplace_back(Square(to + 9), to, ROOK);
            moves.emplace_back(Square(to + 9), to, BISHOP);
            moves.emplace_back(Square(to + 9), to, KNIGHT);
        }
        
        if (ep_square_ != NO_SQUARE) {
            Bitboard ep_attackers = PAWN_ATTACKS[WHITE][ep_square_] & pawns;
            while (ep_attackers) {
                Square from = pop_lsb(ep_attackers);
                moves.emplace_back(from, ep_square_);
            }
        }
    }
    
    // Knight moves
    Bitboard knights = pieces(us, KNIGHT);
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard attacks = KNIGHT_ATTACKS[from] & ~our_pieces;
        while (attacks) {
            moves.emplace_back(from, pop_lsb(attacks));
        }
    }
    
    // Bishop moves
    Bitboard bishops = pieces(us, BISHOP);
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard attacks = bishop_attacks(from, occupied_) & ~our_pieces;
        while (attacks) {
            moves.emplace_back(from, pop_lsb(attacks));
        }
    }
    
    // Rook moves
    Bitboard rooks = pieces(us, ROOK);
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard attacks = rook_attacks(from, occupied_) & ~our_pieces;
        while (attacks) {
            moves.emplace_back(from, pop_lsb(attacks));
        }
    }
    
    // Queen moves
    Bitboard queens = pieces(us, QUEEN);
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard attacks = queen_attacks(from, occupied_) & ~our_pieces;
        while (attacks) {
            moves.emplace_back(from, pop_lsb(attacks));
        }
    }
    
    // King moves
    Square ksq = king_sq_[us];
    Bitboard king_attacks = KING_ATTACKS[ksq] & ~our_pieces;
    while (king_attacks) {
        moves.emplace_back(ksq, pop_lsb(king_attacks));
    }
    
    // Castling
    if (us == WHITE) {
        if ((castling_ & WHITE_OO) && 
            !(occupied_ & (square_bb(F1) | square_bb(G1))) &&
            !is_attacked_by(BLACK, E1) && !is_attacked_by(BLACK, F1) && !is_attacked_by(BLACK, G1)) {
            moves.emplace_back(E1, G1);
        }
        if ((castling_ & WHITE_OOO) && 
            !(occupied_ & (square_bb(D1) | square_bb(C1) | square_bb(B1))) &&
            !is_attacked_by(BLACK, E1) && !is_attacked_by(BLACK, D1) && !is_attacked_by(BLACK, C1)) {
            moves.emplace_back(E1, C1);
        }
    } else {
        if ((castling_ & BLACK_OO) && 
            !(occupied_ & (square_bb(F8) | square_bb(G8))) &&
            !is_attacked_by(WHITE, E8) && !is_attacked_by(WHITE, F8) && !is_attacked_by(WHITE, G8)) {
            moves.emplace_back(E8, G8);
        }
        if ((castling_ & BLACK_OOO) && 
            !(occupied_ & (square_bb(D8) | square_bb(C8) | square_bb(B8))) &&
            !is_attacked_by(WHITE, E8) && !is_attacked_by(WHITE, D8) && !is_attacked_by(WHITE, C8)) {
            moves.emplace_back(E8, C8);
        }
    }
    
    return moves;
}

std::vector<Move> Board::legal_moves() const {
    std::vector<Move> pseudo = pseudo_legal_moves();
    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    
    for (Move m : pseudo) {
        Board copy = *this;
        if (copy.make_move(m)) {
            legal.push_back(m);
        }
    }
    
    return legal;
}

std::vector<Move> Board::legal_captures() const {
    std::vector<Move> captures;
    for (Move m : legal_moves()) {
        if (piece_at(m.to()) != NO_PIECE || 
            (type_of(piece_at(m.from())) == PAWN && m.to() == ep_square_)) {
            captures.push_back(m);
        }
    }
    return captures;
}

} // namespace chess
