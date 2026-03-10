#pragma once

#include <cstdint>
#include <cassert>

// ─── Basic type aliases ────────────────────────────────────────────────────

using Bitboard = uint64_t;
using Key      = uint64_t;   // Zobrist hash key

// ─── Colors ───────────────────────────────────────────────────────────────

enum Color : int {
    WHITE = 0,
    BLACK = 1,
    COLOR_NB = 2
};

inline Color operator~(Color c) { return Color(c ^ 1); }

// ─── Piece types ──────────────────────────────────────────────────────────

enum PieceType : int {
    NO_PIECE_TYPE = 0,
    PAWN   = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK   = 4,
    QUEEN  = 5,
    KING   = 6,
    PIECE_TYPE_NB = 7
};

// ─── Pieces (color + type encoded as single int) ─────────────────────────
//   White pieces: 0–5  (PAWN=0 … KING=5)
//   Black pieces: 6–11
//   NO_PIECE:     12

enum Piece : int {
    W_PAWN = 0, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = 6, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    NO_PIECE = 12,
    PIECE_NB = 13
};

inline Piece make_piece(Color c, PieceType pt) {
    return Piece((c * 6) + (pt - 1));
}

inline PieceType type_of(Piece p) {
    if (p == NO_PIECE) return NO_PIECE_TYPE;
    return PieceType((p % 6) + 1);
}

inline Color color_of(Piece p) {
    assert(p != NO_PIECE);
    return Color(p / 6);
}

// ─── Squares ──────────────────────────────────────────────────────────────
//   A1 = 0, B1 = 1, …, H1 = 7
//   A8 = 56, …, H8 = 63

enum Square : int {
    A1=0, B1, C1, D1, E1, F1, G1, H1,
    A2=8, B2, C2, D2, E2, F2, G2, H2,
    A3=16,B3, C3, D3, E3, F3, G3, H3,
    A4=24,B4, C4, D4, E4, F4, G4, H4,
    A5=32,B5, C5, D5, E5, F5, G5, H5,
    A6=40,B6, C6, D6, E6, F6, G6, H6,
    A7=48,B7, C7, D7, E7, F7, G7, H7,
    A8=56,B8, C8, D8, E8, F8, G8, H8,
    NO_SQUARE = 64,
    SQUARE_NB = 64
};

inline Square make_square(int file, int rank) {
    return Square(rank * 8 + file);
}

inline int file_of(Square s) { return s & 7; }
inline int rank_of(Square s) { return s >> 3; }

inline Square operator+(Square s, int d) { return Square(int(s) + d); }
inline Square operator-(Square s, int d) { return Square(int(s) - d); }
inline Square& operator+=(Square& s, int d) { return s = s + d; }

// ─── Files and Ranks ──────────────────────────────────────────────────────

enum File : int { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB };
enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB };

// ─── Castling rights ──────────────────────────────────────────────────────

enum CastlingRights : int {
    NO_CASTLING       = 0,
    WHITE_OO          = 1,   // White kingside
    WHITE_OOO         = 2,   // White queenside
    BLACK_OO          = 4,   // Black kingside
    BLACK_OOO         = 8,   // Black queenside
    CASTLING_RIGHTS_NB = 16,
    ANY_CASTLING      = WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO
};

inline CastlingRights operator|(CastlingRights a, CastlingRights b) {
    return CastlingRights(int(a) | int(b));
}
inline CastlingRights& operator|=(CastlingRights& a, CastlingRights b) {
    return a = a | b;
}
inline CastlingRights operator&(CastlingRights a, CastlingRights b) {
    return CastlingRights(int(a) & int(b));
}

// ─── Move flags ───────────────────────────────────────────────────────────

enum MoveFlag : uint32_t {
    QUIET            = 0,
    DOUBLE_PAWN_PUSH = 1,
    CASTLE_KINGSIDE  = 2,
    CASTLE_QUEENSIDE = 3,
    CAPTURE          = 4,
    EN_PASSANT       = 5,
    // Promotions (bit 3 set)
    PROMO_KNIGHT     = 8,
    PROMO_BISHOP     = 9,
    PROMO_ROOK       = 10,
    PROMO_QUEEN      = 11,
    PROMO_CAPTURE_KNIGHT = 12,
    PROMO_CAPTURE_BISHOP = 13,
    PROMO_CAPTURE_ROOK   = 14,
    PROMO_CAPTURE_QUEEN  = 15
};

// ─── Move ─────────────────────────────────────────────────────────────────
//
//  Compact 32-bit encoding:
//    bits  0– 5  : from square (6 bits)
//    bits  6–11  : to   square (6 bits)
//    bits 12–15  : flags       (4 bits)
//    bits 16–31  : reserved / score hint
//

struct Move {
    uint32_t data;

    constexpr Move() : data(0) {}

    constexpr Move(Square from, Square to, MoveFlag flag = QUIET)
        : data(uint32_t(from) | (uint32_t(to) << 6) | (uint32_t(flag) << 12)) {}

    Square   from()  const { return Square(data & 0x3F); }
    Square   to()    const { return Square((data >> 6) & 0x3F); }
    MoveFlag flag()  const { return MoveFlag((data >> 12) & 0xF); }

    bool is_capture()   const { return (flag() & CAPTURE) != 0; }
    bool is_promotion() const { return (flag() & 8) != 0; }
    bool is_castling()  const { return flag() == CASTLE_KINGSIDE || flag() == CASTLE_QUEENSIDE; }
    bool is_en_passant()const { return flag() == EN_PASSANT; }

    // Promotion piece type from flag
    PieceType promo_piece() const {
        if (!is_promotion()) return NO_PIECE_TYPE;
        return PieceType((flag() & 3) + 2); // 0→KNIGHT(2), 1→BISHOP(3), 2→ROOK(4), 3→QUEEN(5)
    }

    bool is_null() const { return data == 0; }

    bool operator==(Move o) const { return (data & 0xFFFF) == (o.data & 0xFFFF); }
    bool operator!=(Move o) const { return !(*this == o); }
};

inline const Move NULL_MOVE = Move();

// ─── Piece values ─────────────────────────────────────────────────────────

constexpr int PIECE_VALUE[PIECE_TYPE_NB] = {
    0,     // NO_PIECE_TYPE
    100,   // PAWN
    320,   // KNIGHT
    330,   // BISHOP
    500,   // ROOK
    900,   // QUEEN
    20000  // KING
};

// ─── Score constants ──────────────────────────────────────────────────────

constexpr int SCORE_INFINITE   =  32767;
constexpr int SCORE_NONE       = -32768;
constexpr int CHECKMATE_SCORE  =  30000;
constexpr int DRAW_SCORE       =  0;

// Mate score helpers
inline bool is_mate_score(int s) { return s >= CHECKMATE_SCORE - 100 || s <= -CHECKMATE_SCORE + 100; }
inline int  mate_in(int ply)     { return CHECKMATE_SCORE - ply; }
inline int  mated_in(int ply)    { return -CHECKMATE_SCORE + ply; }
