/*
 * Chess Backend C++ - Standalone Version
 * A complete chess engine and HTTP server in minimal files
 * 
 * Build: g++ -O2 -std=c++17 main.cpp chess.cpp -o chess_backend -lws2_32 (Windows)
 *        g++ -O2 -std=c++17 -pthread main.cpp chess.cpp -o chess_backend (Linux)
 */

#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iostream>
#include <random>
#include <mutex>

namespace chess {

// ============= Types =============
using Bitboard = uint64_t;

enum PieceType { NO_PIECE_TYPE = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6 };
enum Color { WHITE = 0, BLACK = 1 };
enum Piece { 
    NO_PIECE = 0,
    W_PAWN = 1, W_KNIGHT = 2, W_BISHOP = 3, W_ROOK = 4, W_QUEEN = 5, W_KING = 6,
    B_PAWN = 9, B_KNIGHT = 10, B_BISHOP = 11, B_ROOK = 12, B_QUEEN = 13, B_KING = 14
};

enum Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQUARE = 64
};

constexpr int MATE_VALUE = 100000;

// Utility functions
inline int file_of(int s) { return s & 7; }
inline int rank_of(int s) { return s >> 3; }
inline int make_square(int f, int r) { return r * 8 + f; }
inline int flip_rank(int s) { return s ^ 56; }
inline Color operator~(Color c) { return Color(c ^ 1); }
inline Piece make_piece(Color c, PieceType pt) { return Piece((c << 3) | pt); }
inline PieceType type_of(Piece p) { return PieceType(p & 7); }
inline Color color_of(Piece p) { return Color(p >> 3); }
inline Bitboard square_bb(int s) { return 1ULL << s; }

inline int popcount(Bitboard b) {
#if defined(_MSC_VER)
    return (int)__popcnt64(b);
#else
    return __builtin_popcountll(b);
#endif
}

inline int lsb(Bitboard b) {
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return (int)idx;
#else
    return __builtin_ctzll(b);
#endif
}

inline int pop_lsb(Bitboard& b) {
    int s = lsb(b);
    b &= b - 1;
    return s;
}

// File/Rank bitboards
constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileHBB = FileABB << 7;
constexpr Bitboard Rank1BB = 0xFFULL;
constexpr Bitboard Rank2BB = Rank1BB << 8;
constexpr Bitboard Rank3BB = Rank1BB << 16;
constexpr Bitboard Rank6BB = Rank1BB << 40;
constexpr Bitboard Rank7BB = Rank1BB << 48;
constexpr Bitboard Rank8BB = Rank1BB << 56;

// ============= Move =============
class Move {
    uint16_t data_ = 0;
public:
    Move() = default;
    Move(int from, int to, PieceType promo = NO_PIECE_TYPE) 
        : data_(from | (to << 6) | (promo << 12)) {}
    
    int from() const { return data_ & 0x3F; }
    int to() const { return (data_ >> 6) & 0x3F; }
    PieceType promotion() const { return PieceType((data_ >> 12) & 0x7); }
    bool is_null() const { return data_ == 0; }
    bool is_promotion() const { return promotion() != NO_PIECE_TYPE; }
    bool operator==(const Move& o) const { return data_ == o.data_; }
    bool operator!=(const Move& o) const { return data_ != o.data_; }
    
    std::string uci() const {
        if (is_null()) return "0000";
        std::string s;
        s += char('a' + file_of(from()));
        s += char('1' + rank_of(from()));
        s += char('a' + file_of(to()));
        s += char('1' + rank_of(to()));
        if (is_promotion()) {
            const char p[] = " nbrq";
            s += p[promotion()];
        }
        return s;
    }
    
    static Move from_uci(const std::string& uci) {
        if (uci.length() < 4) return Move();
        int ff = uci[0] - 'a', fr = uci[1] - '1';
        int tf = uci[2] - 'a', tr = uci[3] - '1';
        if (ff < 0 || ff > 7 || fr < 0 || fr > 7) return Move();
        if (tf < 0 || tf > 7 || tr < 0 || tr > 7) return Move();
        PieceType promo = NO_PIECE_TYPE;
        if (uci.length() >= 5) {
            char p = uci[4];
            if (p == 'n') promo = KNIGHT;
            else if (p == 'b') promo = BISHOP;
            else if (p == 'r') promo = ROOK;
            else if (p == 'q') promo = QUEEN;
        }
        return Move(make_square(ff, fr), make_square(tf, tr), promo);
    }
};

const Move NULL_MOVE;

// ============= Attack Tables =============
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[2][64];

void init_attacks();
Bitboard bishop_attacks(int sq, Bitboard occ);
Bitboard rook_attacks(int sq, Bitboard occ);
Bitboard queen_attacks(int sq, Bitboard occ);

// ============= Board =============
class Board {
public:
    Board();
    Board(const std::string& fen);
    
    void set_fen(const std::string& fen);
    std::string fen() const;
    void reset();
    
    Piece piece_at(int s) const { return pieces_[s]; }
    Bitboard pieces() const { return occupied_; }
    Bitboard pieces(Color c) const { return by_color_[c]; }
    Bitboard pieces(PieceType pt) const { return by_type_[pt]; }
    Bitboard pieces(Color c, PieceType pt) const { return by_color_[c] & by_type_[pt]; }
    int king_square(Color c) const { return king_sq_[c]; }
    Color side_to_move() const { return side_to_move_; }
    uint64_t hash() const { return hash_; }
    
    bool make_move(Move m);
    void unmake_move(Move m);
    
    bool is_attacked_by(Color c, int sq) const;
    bool is_in_check() const;
    bool gives_check(Move m) const;
    
    bool is_checkmate() const;
    bool is_stalemate() const;
    bool is_draw() const;
    bool is_game_over() const;
    bool is_insufficient_material() const;
    
    std::vector<Move> legal_moves() const;
    std::vector<Move> legal_captures() const;
    
    int piece_count(Color c, PieceType pt) const { return popcount(pieces(c, pt)); }
    int material_count(Color c) const;

private:
    void put_piece(Piece p, int s);
    void remove_piece(int s);
    void move_piece(int from, int to);
    
    std::array<Piece, 64> pieces_;
    std::array<Bitboard, 2> by_color_;
    std::array<Bitboard, 7> by_type_;
    Bitboard occupied_;
    std::array<int, 2> king_sq_;
    Color side_to_move_;
    int castling_;
    int ep_square_;
    int halfmove_clock_;
    int fullmove_;
    uint64_t hash_;
    
    struct UndoInfo {
        Piece captured;
        int castling, ep_square, halfmove;
        uint64_t hash;
    };
    std::vector<UndoInfo> undo_stack_;
};

// ============= Evaluation =============
struct PieceValues {
    int pawn = 100, knight = 320, bishop = 330, rook = 500, queen = 900;
    int get(PieceType pt) const {
        switch(pt) {
            case PAWN: return pawn;
            case KNIGHT: return knight;
            case BISHOP: return bishop;
            case ROOK: return rook;
            case QUEEN: return queen;
            default: return 0;
        }
    }
};

struct EvalParams {
    int mobility_weight = 5;
    int castling_bonus = 50;
    int rook_open_file = 15;
};

extern PieceValues PIECE_VALUES;
extern EvalParams EVAL_PARAMS;

int evaluate(const Board& board);
int evaluate_fast(const Board& board);

// ============= Search =============
struct SearchResult {
    Move best_move;
    int score = 0;
    int depth = 0;
    int nodes = 0;
};

class ChessSearch {
public:
    ChessSearch(size_t tt_size = 500000);
    SearchResult search(Board& board, int depth);
    SearchResult search_iterative(Board& board, int max_depth);
    void clear_tt();
    
private:
    int alphabeta(Board& board, int depth, int alpha, int beta, int ply);
    int quiescence(Board& board, int alpha, int beta, int ply);
    
    std::vector<std::tuple<uint64_t, int, int, Move>> tt_;
    size_t tt_size_;
    int nodes_;
};

// ============= Bot =============
class ChessBot {
public:
    ChessBot(int depth = 4);
    
    std::string get_fen() const;
    void set_fen(const std::string& fen);
    void reset();
    bool make_move(const std::string& uci);
    std::string get_best_move();
    std::string get_best_move(int depth);
    bool is_game_over() const;
    std::string get_result() const;
    
    int search_depth;

private:
    Board board_;
    ChessSearch search_;
    mutable std::mutex mutex_;
};

} // namespace chess
