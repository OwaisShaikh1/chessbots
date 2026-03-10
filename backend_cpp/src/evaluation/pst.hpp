#pragma once

#include "../chess/types.hpp"
#include <array>

namespace eval {

// Piece values (in centipawns)
struct PieceValues {
    int pawn = 100;
    int knight = 320;
    int bishop = 330;
    int rook = 500;
    int queen = 900;
    int king = 20000;
    
    int get(chess::PieceType pt) const {
        switch (pt) {
            case chess::PAWN: return pawn;
            case chess::KNIGHT: return knight;
            case chess::BISHOP: return bishop;
            case chess::ROOK: return rook;
            case chess::QUEEN: return queen;
            case chess::KING: return king;
            default: return 0;
        }
    }
};

// Evaluation parameters
struct EvalParams {
    int mobility_weight = 5;
    int castling_bonus = 50;
    int king_exposure_penalty = 25;
    int king_safety_penalty = 30;
    int rook_open_file = 15;
    int rook_semi_open = 10;
    int passed_pawn_scale = 10;
    int threat_divisor = 5;
    int lpdo_divisor = 2;
    int queen_early_penalty = 20;
    int queen_exposure_penalty = 40;
    int pin_penalty = 50;
};

// Piece-Square Tables - Middlegame
constexpr std::array<int, 64> MG_PAWN_PST = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
};

constexpr std::array<int, 64> MG_KNIGHT_PST = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

constexpr std::array<int, 64> MG_BISHOP_PST = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

constexpr std::array<int, 64> MG_ROOK_PST = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
};

constexpr std::array<int, 64> MG_QUEEN_PST = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

constexpr std::array<int, 64> MG_KING_PST = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

// Piece-Square Tables - Endgame
constexpr std::array<int, 64> EG_PAWN_PST = {
    0,  0,  0,  0,  0,  0,  0,  0,
    170,170,170,170,170,170,170,170,
    145,145,145,145,145,145,145,145,
    120,120,120,120,120,120,120,120,
    95, 95, 95, 95, 95, 95, 95, 95,
    70, 70, 70, 70, 70, 70, 70, 70,
    45, 45, 45, 45, 45, 45, 45, 45,
    0,  0,  0,  0,  0,  0,  0,  0
};

constexpr std::array<int, 64> EG_KING_PST = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,-10,  0,  0,-10,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

// MVV-LVA scores for move ordering
// MVV_LVA[victim][attacker]
constexpr int MVV_LVA[7][7] = {
    {0, 0, 0, 0, 0, 0, 0},           // NO_PIECE
    {0, 105, 104, 103, 102, 101, 100}, // PAWN victim
    {0, 205, 204, 203, 202, 201, 200}, // KNIGHT victim
    {0, 305, 304, 303, 302, 301, 300}, // BISHOP victim
    {0, 405, 404, 403, 402, 401, 400}, // ROOK victim
    {0, 505, 504, 503, 502, 501, 500}, // QUEEN victim
    {0, 605, 604, 603, 602, 601, 600}  // KING victim
};

inline int pst_score(chess::PieceType pt, chess::Square sq, bool is_endgame) {
    int idx = sq;
    switch (pt) {
        case chess::PAWN: return is_endgame ? EG_PAWN_PST[idx] : MG_PAWN_PST[idx];
        case chess::KNIGHT: return MG_KNIGHT_PST[idx];
        case chess::BISHOP: return MG_BISHOP_PST[idx];
        case chess::ROOK: return MG_ROOK_PST[idx];
        case chess::QUEEN: return MG_QUEEN_PST[idx];
        case chess::KING: return is_endgame ? EG_KING_PST[idx] : MG_KING_PST[idx];
        default: return 0;
    }
}

} // namespace eval
