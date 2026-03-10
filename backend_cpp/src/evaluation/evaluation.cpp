#include "evaluation.hpp"
#include <algorithm>
#include <cmath>

namespace eval {

// Global instances
PieceValues PIECE_VALUES;
EvalParams EVAL_PARAMS;

int material_score(const chess::Board& board) {
    int score = 0;
    
    score += board.piece_count(chess::WHITE, chess::PAWN) * PIECE_VALUES.pawn;
    score += board.piece_count(chess::WHITE, chess::KNIGHT) * PIECE_VALUES.knight;
    score += board.piece_count(chess::WHITE, chess::BISHOP) * PIECE_VALUES.bishop;
    score += board.piece_count(chess::WHITE, chess::ROOK) * PIECE_VALUES.rook;
    score += board.piece_count(chess::WHITE, chess::QUEEN) * PIECE_VALUES.queen;
    
    score -= board.piece_count(chess::BLACK, chess::PAWN) * PIECE_VALUES.pawn;
    score -= board.piece_count(chess::BLACK, chess::KNIGHT) * PIECE_VALUES.knight;
    score -= board.piece_count(chess::BLACK, chess::BISHOP) * PIECE_VALUES.bishop;
    score -= board.piece_count(chess::BLACK, chess::ROOK) * PIECE_VALUES.rook;
    score -= board.piece_count(chess::BLACK, chess::QUEEN) * PIECE_VALUES.queen;
    
    return score;
}

bool is_endgame(const chess::Board& board) {
    // Endgame if both sides have no queens or limited material
    bool white_has_queen = board.piece_count(chess::WHITE, chess::QUEEN) > 0;
    bool black_has_queen = board.piece_count(chess::BLACK, chess::QUEEN) > 0;
    
    if (!white_has_queen && !black_has_queen) return true;
    
    // Or if limited material
    int white_material = board.material_count(chess::WHITE);
    int black_material = board.material_count(chess::BLACK);
    
    return (white_material + black_material) < 2600; // ~2 rooks + minor piece each
}

int endgame_mate_bonus(const chess::Board& board) {
    int white_material = board.material_count(chess::WHITE);
    int black_material = board.material_count(chess::BLACK);
    
    int bonus = 0;
    
    // White has mating material, black only has king
    if (white_material >= 900 && black_material == 0) {
        chess::Square black_king = board.king_square(chess::BLACK);
        if (black_king != chess::NO_SQUARE) {
            // Push king to edges
            int file = chess::file_of(black_king);
            int rank = chess::rank_of(black_king);
            float center_dist = std::max(std::abs(file - 3.5f), std::abs(rank - 3.5f));
            bonus += static_cast<int>(center_dist * 30);
            
            // Reduce king distance
            chess::Square white_king = board.king_square(chess::WHITE);
            if (white_king != chess::NO_SQUARE) {
                int king_dist = std::max(std::abs(chess::file_of(white_king) - file),
                                        std::abs(chess::rank_of(white_king) - rank));
                bonus += (7 - king_dist) * 10;
            }
        }
    }
    // Black has mating material, white only has king  
    else if (black_material >= 900 && white_material == 0) {
        chess::Square white_king = board.king_square(chess::WHITE);
        if (white_king != chess::NO_SQUARE) {
            int file = chess::file_of(white_king);
            int rank = chess::rank_of(white_king);
            float center_dist = std::max(std::abs(file - 3.5f), std::abs(rank - 3.5f));
            bonus -= static_cast<int>(center_dist * 30);
            
            chess::Square black_king = board.king_square(chess::BLACK);
            if (black_king != chess::NO_SQUARE) {
                int king_dist = std::max(std::abs(chess::file_of(black_king) - file),
                                        std::abs(chess::rank_of(black_king) - rank));
                bonus -= (7 - king_dist) * 10;
            }
        }
    }
    
    return bonus;
}

int evaluate_fast(const chess::Board& board) {
    if (board.is_checkmate()) {
        return board.side_to_move() == chess::WHITE ? -chess::MATE_VALUE : chess::MATE_VALUE;
    }
    if (board.is_stalemate() || board.is_insufficient_material()) {
        return 0;
    }
    
    int score = 0;
    bool endgame = is_endgame(board);
    
    // Material and PST
    for (int sq = 0; sq < 64; sq++) {
        chess::Piece p = board.piece_at(chess::Square(sq));
        if (p == chess::NO_PIECE) continue;
        
        chess::PieceType pt = chess::type_of(p);
        chess::Color c = chess::color_of(p);
        
        int piece_val = PIECE_VALUES.get(pt);
        int pst_val = pst_score(pt, c == chess::WHITE ? chess::flip_rank(chess::Square(sq)) : chess::Square(sq), endgame);
        
        if (c == chess::WHITE) {
            score += piece_val + pst_val;
        } else {
            score -= piece_val + pst_val;
        }
    }
    
    return score;
}

int evaluate(const chess::Board& board) {
    if (board.is_checkmate()) {
        return board.side_to_move() == chess::WHITE ? -chess::MATE_VALUE : chess::MATE_VALUE;
    }
    if (board.is_stalemate() || board.is_insufficient_material() || board.is_draw()) {
        return 0;
    }
    
    int mg_score = 0;
    int eg_score = 0;
    bool endgame = is_endgame(board);
    
    // Phase calculation
    int phase = 0;
    phase += board.piece_count(chess::WHITE, chess::KNIGHT) + board.piece_count(chess::BLACK, chess::KNIGHT);
    phase += board.piece_count(chess::WHITE, chess::BISHOP) + board.piece_count(chess::BLACK, chess::BISHOP);
    phase += (board.piece_count(chess::WHITE, chess::ROOK) + board.piece_count(chess::BLACK, chess::ROOK)) * 2;
    phase += (board.piece_count(chess::WHITE, chess::QUEEN) + board.piece_count(chess::BLACK, chess::QUEEN)) * 4;
    
    int max_phase = 24;
    phase = std::min(phase, max_phase);
    
    // Material and PST
    for (int sq = 0; sq < 64; sq++) {
        chess::Piece p = board.piece_at(chess::Square(sq));
        if (p == chess::NO_PIECE) continue;
        
        chess::PieceType pt = chess::type_of(p);
        chess::Color c = chess::color_of(p);
        
        int piece_val = PIECE_VALUES.get(pt);
        int pst_sq = c == chess::WHITE ? chess::flip_rank(chess::Square(sq)) : sq;
        int mg_pst = pst_score(pt, chess::Square(pst_sq), false);
        int eg_pst = pst_score(pt, chess::Square(pst_sq), true);
        
        if (c == chess::WHITE) {
            mg_score += piece_val + mg_pst;
            eg_score += piece_val + eg_pst;
        } else {
            mg_score -= piece_val + mg_pst;
            eg_score -= piece_val + eg_pst;
        }
    }
    
    // Mobility (simplified)
    int white_mobility = 0;
    int black_mobility = 0;
    
    for (int sq = 0; sq < 64; sq++) {
        chess::Piece p = board.piece_at(chess::Square(sq));
        if (p == chess::NO_PIECE) continue;
        
        chess::PieceType pt = chess::type_of(p);
        if (pt == chess::PAWN || pt == chess::KING) continue;
        
        chess::Bitboard attacks = 0;
        switch (pt) {
            case chess::KNIGHT:
                attacks = chess::KNIGHT_ATTACKS[sq];
                break;
            case chess::BISHOP:
                attacks = chess::bishop_attacks(chess::Square(sq), board.pieces());
                break;
            case chess::ROOK:
                attacks = chess::rook_attacks(chess::Square(sq), board.pieces());
                break;
            case chess::QUEEN:
                attacks = chess::queen_attacks(chess::Square(sq), board.pieces());
                break;
            default:
                break;
        }
        
        int mobility = chess::popcount(attacks);
        if (chess::color_of(p) == chess::WHITE) {
            white_mobility += mobility;
        } else {
            black_mobility += mobility;
        }
    }
    
    mg_score += (white_mobility - black_mobility) * EVAL_PARAMS.mobility_weight;
    eg_score += (white_mobility - black_mobility) * EVAL_PARAMS.mobility_weight;
    
    // Castling bonus
    chess::Square wk = board.king_square(chess::WHITE);
    chess::Square bk = board.king_square(chess::BLACK);
    
    if (wk == chess::G1 || wk == chess::C1) {
        mg_score += EVAL_PARAMS.castling_bonus;
    }
    if (bk == chess::G8 || bk == chess::C8) {
        mg_score -= EVAL_PARAMS.castling_bonus;
    }
    
    // Rook on open file
    for (int f = 0; f < 8; f++) {
        chess::Bitboard file_bb = chess::FileABB << f;
        chess::Bitboard white_pawns = board.pieces(chess::WHITE, chess::PAWN);
        chess::Bitboard black_pawns = board.pieces(chess::BLACK, chess::PAWN);
        chess::Bitboard white_rooks = board.pieces(chess::WHITE, chess::ROOK) & file_bb;
        chess::Bitboard black_rooks = board.pieces(chess::BLACK, chess::ROOK) & file_bb;
        
        if (white_rooks) {
            if (!(file_bb & white_pawns)) {
                if (!(file_bb & black_pawns)) {
                    mg_score += EVAL_PARAMS.rook_open_file;
                } else {
                    mg_score += EVAL_PARAMS.rook_semi_open;
                }
            }
        }
        if (black_rooks) {
            if (!(file_bb & black_pawns)) {
                if (!(file_bb & white_pawns)) {
                    mg_score -= EVAL_PARAMS.rook_open_file;
                } else {
                    mg_score -= EVAL_PARAMS.rook_semi_open;
                }
            }
        }
    }
    
    // Tapered evaluation
    int score = (mg_score * phase + eg_score * (max_phase - phase)) / max_phase;
    
    // Endgame mate bonus
    score += endgame_mate_bonus(board);
    
    return score;
}

} // namespace eval
