#pragma once

#include <cstdint>
#include <string>
#include <array>

namespace chess {

// Piece types
enum PieceType : int {
    NO_PIECE_TYPE = 0,
    PAWN = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK = 4,
    QUEEN = 5,
    KING = 6,
    PIECE_TYPE_NB = 7
};

// Colors
enum Color : int {
    WHITE = 0,
    BLACK = 1,
    COLOR_NB = 2
};

// Pieces (color + type combined)
enum Piece : int {
    NO_PIECE = 0,
    W_PAWN = 1, W_KNIGHT = 2, W_BISHOP = 3, W_ROOK = 4, W_QUEEN = 5, W_KING = 6,
    B_PAWN = 9, B_KNIGHT = 10, B_BISHOP = 11, B_ROOK = 12, B_QUEEN = 13, B_KING = 14,
    PIECE_NB = 16
};

// Squares
enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQUARE_NB = 64,
    NO_SQUARE = 65
};

// Files and Ranks
enum File : int { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB };
enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB };

// Castling rights
enum CastlingRights : int {
    NO_CASTLING = 0,
    WHITE_OO = 1,
    WHITE_OOO = 2,
    BLACK_OO = 4,
    BLACK_OOO = 8,
    WHITE_CASTLING = WHITE_OO | WHITE_OOO,
    BLACK_CASTLING = BLACK_OO | BLACK_OOO,
    ALL_CASTLING = WHITE_CASTLING | BLACK_CASTLING
};

// Bitboard type
using Bitboard = uint64_t;

// Constants
constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY = 128;
constexpr int MATE_VALUE = 100000;
constexpr int DRAW_VALUE = 0;

// Utility functions
constexpr Square make_square(File f, Rank r) {
    return Square(r * 8 + f);
}

constexpr File file_of(Square s) {
    return File(s & 7);
}

constexpr Rank rank_of(Square s) {
    return Rank(s >> 3);
}

constexpr Color operator~(Color c) {
    return Color(c ^ 1);
}

constexpr Piece make_piece(Color c, PieceType pt) {
    return Piece((c << 3) | pt);
}

constexpr PieceType type_of(Piece p) {
    return PieceType(p & 7);
}

constexpr Color color_of(Piece p) {
    return Color(p >> 3);
}

constexpr Square flip_rank(Square s) {
    return Square(s ^ 56);
}

// Bitboard utilities
constexpr Bitboard square_bb(Square s) {
    return 1ULL << s;
}

inline int popcount(Bitboard b) {
#if defined(_MSC_VER)
    return (int)__popcnt64(b);
#else
    return __builtin_popcountll(b);
#endif
}

inline Square lsb(Bitboard b) {
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return Square(idx);
#else
    return Square(__builtin_ctzll(b));
#endif
}

inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// File and Rank bitboards
constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFFULL;
constexpr Bitboard Rank2BB = Rank1BB << 8;
constexpr Bitboard Rank3BB = Rank1BB << 16;
constexpr Bitboard Rank4BB = Rank1BB << 24;
constexpr Bitboard Rank5BB = Rank1BB << 32;
constexpr Bitboard Rank6BB = Rank1BB << 40;
constexpr Bitboard Rank7BB = Rank1BB << 48;
constexpr Bitboard Rank8BB = Rank1BB << 56;

// String conversions
inline std::string square_to_string(Square s) {
    return std::string(1, 'a' + file_of(s)) + std::string(1, '1' + rank_of(s));
}

inline Square string_to_square(const std::string& s) {
    if (s.length() < 2) return NO_SQUARE;
    File f = File(s[0] - 'a');
    Rank r = Rank(s[1] - '1');
    if (f < FILE_A || f > FILE_H || r < RANK_1 || r > RANK_8) return NO_SQUARE;
    return make_square(f, r);
}

inline char piece_to_char(Piece p) {
    constexpr char chars[] = " PNBRQK  pnbrqk";
    return chars[p];
}

inline Piece char_to_piece(char c) {
    const std::string pieces = " PNBRQK  pnbrqk";
    size_t idx = pieces.find(c);
    return idx != std::string::npos ? Piece(idx) : NO_PIECE;
}

} // namespace chess
