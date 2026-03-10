#pragma once

#include "types.h"
#include "bitboard.h"
#include <string>
#include <vector>

// ─── Undo info saved per half-move ────────────────────────────────────────

struct UndoInfo {
    Key            zobrist_hash;
    CastlingRights castling_rights;
    Square         en_passant_square;
    int            halfmove_clock;
    Piece          captured_piece;
};

// ─── Board ────────────────────────────────────────────────────────────────

class Board {
public:
    // ── State ──
    Bitboard       bitboards[PIECE_NB];   // bitboard per piece (index = Piece enum)
    Bitboard       occupancy[COLOR_NB + 1]; // [WHITE], [BLACK], [BOTH]
    Piece          piece_on[SQUARE_NB];   // piece occupying each square

    Color          side_to_move;
    CastlingRights castling_rights;
    Square         en_passant_square;
    int            halfmove_clock;
    int            fullmove_number;
    Key            zobrist_hash;

    // ── History ──
    std::vector<UndoInfo> history;

    // ── Public API ──
    Board();

    void     init_board();
    bool     load_fen(const std::string& fen);
    std::string to_fen() const;
    void     print_board() const;

    void     make_move(Move m);
    void     undo_move(Move m);

    // Inline accessors
    Bitboard pieces(Color c)             const { return occupancy[c]; }
    Bitboard pieces(PieceType pt)        const;
    Bitboard pieces(Color c, PieceType pt) const { return bitboards[make_piece(c, pt)]; }
    Piece    piece_at(Square s)          const { return piece_on[s]; }
    bool     is_empty(Square s)          const { return piece_on[s] == NO_PIECE; }

private:
    void put_piece(Piece p, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);
    void update_occupancy();
};

// ─── FEN constants ────────────────────────────────────────────────────────

constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
