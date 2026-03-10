#pragma once

#include "types.h"
#include <cstdint>
#include <string>

// ─── Compiler intrinsics ──────────────────────────────────────────────────

#if defined(__GNUC__) || defined(__clang__)
    #define HAS_BUILTIN_POPCOUNT
    #define HAS_BUILTIN_CTZ
    #define HAS_BUILTIN_CLZ
#endif

// ─── Popcount ─────────────────────────────────────────────────────────────

inline int popcount(Bitboard b) {
#ifdef HAS_BUILTIN_POPCOUNT
    return __builtin_popcountll(b);
#else
    int count = 0;
    while (b) { count++; b &= b - 1; }
    return count;
#endif
}

// ─── Least-significant bit ────────────────────────────────────────────────

inline Square lsb(Bitboard b) {
#ifdef HAS_BUILTIN_CTZ
    return Square(__builtin_ctzll(b));
#else
    // De Bruijn sequence fallback
    static const int debruijn_table[64] = {
        0, 47, 1, 56, 48, 27, 2, 60, 57, 49, 41, 37, 28, 16, 3, 61,
        54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4, 62,
        46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30, 9, 24, 13, 18, 8, 12, 7, 6, 5, 63
    };
    return Square(debruijn_table[((b ^ (b - 1)) * 0x03F79D71B4CA8B09ULL) >> 58]);
#endif
}

// ─── Most-significant bit ─────────────────────────────────────────────────

inline Square msb(Bitboard b) {
#ifdef HAS_BUILTIN_CLZ
    return Square(63 - __builtin_clzll(b));
#else
    static const int debruijn_table[64] = {
        0, 47, 1, 56, 48, 27, 2, 60, 57, 49, 41, 37, 28, 16, 3, 61,
        54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4, 62,
        46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30, 9, 24, 13, 18, 8, 12, 7, 6, 5, 63
    };
    b |= b >> 1; b |= b >> 2; b |= b >> 4;
    b |= b >> 8; b |= b >> 16; b |= b >> 32;
    return Square(debruijn_table[(b * 0x03F79D71B4CA8B09ULL) >> 58]);
#endif
}

// ─── Pop lsb — returns the lsb square and clears it ──────────────────────

inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// ─── Bit manipulation ─────────────────────────────────────────────────────

inline void set_bit(Bitboard& b, Square s) { b |=  (Bitboard(1) << s); }
inline void clear_bit(Bitboard& b, Square s) { b &= ~(Bitboard(1) << s); }
inline bool get_bit(Bitboard b, Square s) { return (b >> s) & 1; }
inline Bitboard square_bb(Square s) { return Bitboard(1) << s; }

// ─── Shift helpers ────────────────────────────────────────────────────────

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_B_BB = 0x0202020202020202ULL;
constexpr Bitboard FILE_C_BB = 0x0404040404040404ULL;
constexpr Bitboard FILE_D_BB = 0x0808080808080808ULL;
constexpr Bitboard FILE_E_BB = 0x1010101010101010ULL;
constexpr Bitboard FILE_F_BB = 0x2020202020202020ULL;
constexpr Bitboard FILE_G_BB = 0x4040404040404040ULL;
constexpr Bitboard FILE_H_BB = 0x8080808080808080ULL;

constexpr Bitboard RANK_1_BB = 0x00000000000000FFULL;
constexpr Bitboard RANK_2_BB = 0x000000000000FF00ULL;
constexpr Bitboard RANK_3_BB = 0x0000000000FF0000ULL;
constexpr Bitboard RANK_4_BB = 0x00000000FF000000ULL;
constexpr Bitboard RANK_5_BB = 0x000000FF00000000ULL;
constexpr Bitboard RANK_6_BB = 0x0000FF0000000000ULL;
constexpr Bitboard RANK_7_BB = 0x00FF000000000000ULL;
constexpr Bitboard RANK_8_BB = 0xFF00000000000000ULL;

constexpr Bitboard NOT_FILE_A = ~FILE_A_BB;
constexpr Bitboard NOT_FILE_H = ~FILE_H_BB;

// Directional shifts
inline Bitboard shift_north(Bitboard b) { return b << 8; }
inline Bitboard shift_south(Bitboard b) { return b >> 8; }
inline Bitboard shift_east(Bitboard b)  { return (b & NOT_FILE_H) << 1; }
inline Bitboard shift_west(Bitboard b)  { return (b & NOT_FILE_A) >> 1; }
inline Bitboard shift_ne(Bitboard b)    { return (b & NOT_FILE_H) << 9; }
inline Bitboard shift_nw(Bitboard b)    { return (b & NOT_FILE_A) << 7; }
inline Bitboard shift_se(Bitboard b)    { return (b & NOT_FILE_H) >> 7; }
inline Bitboard shift_sw(Bitboard b)    { return (b & NOT_FILE_A) >> 9; }

// Debug: print bitboard as 8x8 grid
void print_bitboard(Bitboard b);
