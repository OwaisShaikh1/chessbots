#pragma once

#include "types.hpp"
#include "move.hpp"
#include <string>
#include <vector>
#include <array>

namespace chess {

// Attack tables (initialized at startup)
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[2][64];
extern Bitboard BETWEEN_BB[64][64];
extern Bitboard LINE_BB[64][64];

void init_attacks();

// Magic bitboard attack generation for sliders
Bitboard bishop_attacks(Square s, Bitboard occupied);
Bitboard rook_attacks(Square s, Bitboard occupied);
Bitboard queen_attacks(Square s, Bitboard occupied);

class Board {
public:
    Board();
    Board(const std::string& fen);
    
    // FEN handling
    void set_fen(const std::string& fen);
    std::string fen() const;
    void reset();
    
    // Piece access
    Piece piece_at(Square s) const { return pieces_[s]; }
    Bitboard pieces() const { return occupied_; }
    Bitboard pieces(Color c) const { return by_color_[c]; }
    Bitboard pieces(PieceType pt) const { return by_type_[pt]; }
    Bitboard pieces(Color c, PieceType pt) const { return by_color_[c] & by_type_[pt]; }
    Square king_square(Color c) const { return king_sq_[c]; }
    
    // Game state
    Color side_to_move() const { return side_to_move_; }
    int castling_rights() const { return castling_; }
    Square ep_square() const { return ep_square_; }
    int halfmove_clock() const { return halfmove_clock_; }
    int fullmove_number() const { return fullmove_number_; }
    uint64_t hash() const { return hash_; }
    
    // Move making
    bool make_move(Move m);
    void unmake_move(Move m);
    bool is_legal(Move m) const;
    
    // Attack detection
    Bitboard attackers_to(Square s, Bitboard occupied) const;
    Bitboard attackers_to(Square s) const { return attackers_to(s, occupied_); }
    bool is_attacked_by(Color c, Square s) const;
    bool is_in_check() const { return is_attacked_by(~side_to_move_, king_sq_[side_to_move_]); }
    bool gives_check(Move m) const;
    
    // Game status
    bool is_checkmate() const;
    bool is_stalemate() const;
    bool is_draw() const;
    bool is_game_over() const { return is_checkmate() || is_stalemate() || is_draw(); }
    bool is_insufficient_material() const;
    
    // Move generation
    std::vector<Move> legal_moves() const;
    std::vector<Move> pseudo_legal_moves() const;
    std::vector<Move> legal_captures() const;
    
    // Utility
    int piece_count(Color c, PieceType pt) const { return popcount(pieces(c, pt)); }
    int material_count(Color c) const;
    void print() const;

private:
    void put_piece(Piece p, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);
    void update_castling(Square from, Square to);
    
    std::array<Piece, 64> pieces_;
    std::array<Bitboard, COLOR_NB> by_color_;
    std::array<Bitboard, PIECE_TYPE_NB> by_type_;
    Bitboard occupied_;
    std::array<Square, COLOR_NB> king_sq_;
    
    Color side_to_move_;
    int castling_;
    Square ep_square_;
    int halfmove_clock_;
    int fullmove_number_;
    uint64_t hash_;
    
    // Undo stack
    struct UndoInfo {
        Piece captured;
        int castling;
        Square ep_square;
        int halfmove_clock;
        uint64_t hash;
    };
    std::vector<UndoInfo> undo_stack_;
};

// Starting position FEN
constexpr const char* STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

} // namespace chess
