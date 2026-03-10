#include "../include/board.h"
#include "../include/zobrist.h"
#include <iostream>
#include <sstream>
#include <cassert>

// ─── Constructor ──────────────────────────────────────────────────────────

Board::Board() {
    init_board();
}

// ─── init_board ───────────────────────────────────────────────────────────

void Board::init_board() {
    for (int i = 0; i < PIECE_NB; i++)   bitboards[i] = 0ULL;
    for (int i = 0; i < COLOR_NB + 1; i++) occupancy[i] = 0ULL;
    for (int s = 0; s < SQUARE_NB; s++)   piece_on[s] = NO_PIECE;

    side_to_move      = WHITE;
    castling_rights   = ANY_CASTLING;
    en_passant_square = NO_SQUARE;
    halfmove_clock    = 0;
    fullmove_number   = 1;
    zobrist_hash      = 0ULL;
    history.clear();
}

// ─── Internal helpers ─────────────────────────────────────────────────────

void Board::put_piece(Piece p, Square s) {
    assert(p != NO_PIECE);
    assert(piece_on[s] == NO_PIECE);
    set_bit(bitboards[p], s);
    set_bit(occupancy[color_of(p)], s);
    set_bit(occupancy[COLOR_NB], s);
    piece_on[s] = p;
    zobrist_hash ^= Zobrist::piece_key(p, s);
}

void Board::remove_piece(Square s) {
    Piece p = piece_on[s];
    assert(p != NO_PIECE);
    clear_bit(bitboards[p], s);
    clear_bit(occupancy[color_of(p)], s);
    clear_bit(occupancy[COLOR_NB], s);
    piece_on[s] = NO_PIECE;
    zobrist_hash ^= Zobrist::piece_key(p, s);
}

void Board::move_piece(Square from, Square to) {
    Piece p = piece_on[from];
    assert(p != NO_PIECE);
    assert(piece_on[to] == NO_PIECE);
    // XOR out from, XOR in to
    Bitboard mask = square_bb(from) | square_bb(to);
    bitboards[p]                ^= mask;
    occupancy[color_of(p)]      ^= mask;
    occupancy[COLOR_NB]         ^= mask;
    piece_on[to]   = p;
    piece_on[from] = NO_PIECE;
    zobrist_hash ^= Zobrist::piece_key(p, from) ^ Zobrist::piece_key(p, to);
}

void Board::update_occupancy() {
    occupancy[WHITE] = 0; occupancy[BLACK] = 0;
    for (int p = W_PAWN; p <= W_KING; p++) occupancy[WHITE] |= bitboards[p];
    for (int p = B_PAWN; p <= B_KING; p++) occupancy[BLACK] |= bitboards[p];
    occupancy[COLOR_NB] = occupancy[WHITE] | occupancy[BLACK];
}

// ─── pieces(PieceType) ────────────────────────────────────────────────────

Bitboard Board::pieces(PieceType pt) const {
    return bitboards[make_piece(WHITE, pt)] | bitboards[make_piece(BLACK, pt)];
}

// ─── load_fen ─────────────────────────────────────────────────────────────

bool Board::load_fen(const std::string& fen) {
    init_board();
    std::istringstream ss(fen);
    std::string board_str, side_str, castle_str, ep_str;
    int halfmove, fullmove;

    ss >> board_str >> side_str >> castle_str >> ep_str >> halfmove >> fullmove;

    // Parse piece placement
    int rank = 7, file = 0;
    for (char c : board_str) {
        if (c == '/') { rank--; file = 0; }
        else if (c >= '1' && c <= '8') { file += (c - '0'); }
        else {
            // Map FEN character to Piece
            static const std::string fen_chars = "PNBRQKpnbrqk";
            auto pos = fen_chars.find(c);
            if (pos == std::string::npos) return false;
            Piece p = Piece(pos); // W_PAWN=0…B_KING=11
            Square sq = make_square(file, rank);
            put_piece(p, sq);
            file++;
        }
    }

    // Side to move
    side_to_move = (side_str == "w") ? WHITE : BLACK;
    if (side_to_move == BLACK) zobrist_hash ^= Zobrist::side_key();

    // Castling rights
    castling_rights = NO_CASTLING;
    for (char c : castle_str) {
        switch (c) {
            case 'K': castling_rights |= WHITE_OO;   break;
            case 'Q': castling_rights |= WHITE_OOO;  break;
            case 'k': castling_rights |= BLACK_OO;   break;
            case 'q': castling_rights |= BLACK_OOO;  break;
            default: break;
        }
    }
    zobrist_hash ^= Zobrist::castling_key(castling_rights);

    // En passant
    en_passant_square = NO_SQUARE;
    if (ep_str != "-" && ep_str.size() >= 2) {
        int ep_file = ep_str[0] - 'a';
        int ep_rank = ep_str[1] - '1';
        if (ep_file >= 0 && ep_file < 8 && ep_rank >= 0 && ep_rank < 8) {
            en_passant_square = make_square(ep_file, ep_rank);
            zobrist_hash ^= Zobrist::ep_key(file_of(en_passant_square));
        }
    }

    if (!(ss.fail())) {
        halfmove_clock  = halfmove;
        fullmove_number = fullmove;
    }

    return true;
}

// ─── to_fen ───────────────────────────────────────────────────────────────

std::string Board::to_fen() const {
    static const std::string fen_chars = "PNBRQKpnbrqk";
    std::string result;

    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            Square sq = make_square(file, rank);
            Piece p = piece_on[sq];
            if (p == NO_PIECE) { empty++; }
            else {
                if (empty) { result += ('0' + empty); empty = 0; }
                result += fen_chars[p];
            }
        }
        if (empty) result += ('0' + empty);
        if (rank > 0) result += '/';
    }

    result += (side_to_move == WHITE) ? " w " : " b ";

    std::string castle_str;
    if (castling_rights & WHITE_OO)  castle_str += 'K';
    if (castling_rights & WHITE_OOO) castle_str += 'Q';
    if (castling_rights & BLACK_OO)  castle_str += 'k';
    if (castling_rights & BLACK_OOO) castle_str += 'q';
    if (castle_str.empty()) castle_str = "-";
    result += castle_str + " ";

    if (en_passant_square == NO_SQUARE) {
        result += "- ";
    } else {
        char ep[3] = { char('a' + file_of(en_passant_square)),
                       char('1' + rank_of(en_passant_square)), '\0' };
        result += std::string(ep) + " ";
    }

    result += std::to_string(halfmove_clock) + " " + std::to_string(fullmove_number);
    return result;
}

// ─── print_board ──────────────────────────────────────────────────────────

void Board::print_board() const {
    static const char piece_chars[] = "PNBRQKpnbrqk.";
    std::cout << "\n  +-----------------+\n";
    for (int rank = 7; rank >= 0; rank--) {
        std::cout << (rank + 1) << " | ";
        for (int file = 0; file < 8; file++) {
            Square sq = make_square(file, rank);
            std::cout << piece_chars[piece_on[sq]] << ' ';
        }
        std::cout << "|\n";
    }
    std::cout << "  +-----------------+\n";
    std::cout << "    a b c d e f g h\n\n";
    std::cout << "FEN: " << to_fen() << "\n";
    std::cout << "Key: " << std::hex << zobrist_hash << std::dec << "\n\n";
}

// ─── make_move ────────────────────────────────────────────────────────────

void Board::make_move(Move m) {
    Square from = m.from();
    Square to   = m.to();
    MoveFlag flag = m.flag();
    Piece moving = piece_on[from];
    assert(moving != NO_PIECE);

    // Save undo info
    UndoInfo info;
    info.zobrist_hash      = zobrist_hash;
    info.castling_rights   = castling_rights;
    info.en_passant_square = en_passant_square;
    info.halfmove_clock    = halfmove_clock;
    info.captured_piece    = NO_PIECE;
    history.push_back(info);

    // Clear en passant from hash
    if (en_passant_square != NO_SQUARE)
        zobrist_hash ^= Zobrist::ep_key(file_of(en_passant_square));
    en_passant_square = NO_SQUARE;

    // Update castling hash
    zobrist_hash ^= Zobrist::castling_key(castling_rights);

    halfmove_clock++;

    if (flag == CAPTURE) {
        // Standard capture
        history.back().captured_piece = piece_on[to];
        remove_piece(to);
        halfmove_clock = 0;
        move_piece(from, to);
    }
    else if (flag == EN_PASSANT) {
        // En passant capture
        Square cap_sq = make_square(file_of(to),
                                    rank_of(from)); // same rank as pawn, target file
        history.back().captured_piece = piece_on[cap_sq];
        remove_piece(cap_sq);
        move_piece(from, to);
        halfmove_clock = 0;
    }
    else if (flag == CASTLE_KINGSIDE) {
        // Move king
        move_piece(from, to);
        // Move rook
        Square rook_from = make_square(FILE_H, rank_of(from));
        Square rook_to   = make_square(FILE_F, rank_of(from));
        move_piece(rook_from, rook_to);
    }
    else if (flag == CASTLE_QUEENSIDE) {
        move_piece(from, to);
        Square rook_from = make_square(FILE_A, rank_of(from));
        Square rook_to   = make_square(FILE_D, rank_of(from));
        move_piece(rook_from, rook_to);
    }
    else if (m.is_promotion()) {
        // Remove pawn, optionally capture
        if (flag & CAPTURE) {
            history.back().captured_piece = piece_on[to];
            remove_piece(to);
        }
        remove_piece(from);
        put_piece(make_piece(side_to_move, m.promo_piece()), to);
        halfmove_clock = 0;
    }
    else {
        // Quiet move (includes double pawn push)
        move_piece(from, to);
        if (type_of(moving) == PAWN) halfmove_clock = 0;
    }

    // Update en passant square for double pawn push
    if (flag == DOUBLE_PAWN_PUSH) {
        en_passant_square = make_square(file_of(from),
            rank_of(from) + (side_to_move == WHITE ? 1 : -1));
        zobrist_hash ^= Zobrist::ep_key(file_of(en_passant_square));
    }

    // Update castling rights based on moved piece/square
    // Using integer AND mask: 15 = no change, clear bits where rooks/king moved
    static const int castling_mask[SQUARE_NB] = {
        // Rank 1 (indices 0-7): A1=0, B1=1, ..., H1=7
        13, 15, 15, 15,  12, 15, 15, 14,
        // Ranks 2-7 (indices 8-55): no castling rights affected
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        // Rank 8 (indices 56-63): A8=56, ..., H8=63
         7, 15, 15, 15,   3, 15, 15, 11,
    };
    // WHITE_OO=1, WHITE_OOO=2, BLACK_OO=4, BLACK_OOO=8
    // A1 move: clear WHITE_OOO(2) → mask=13(=15-2)
    // H1 move: clear WHITE_OO(1)  → mask=14(=15-1)
    // E1 move: clear both white   → mask=12(=15-3)
    // A8 move: clear BLACK_OOO(8) → mask=7(=15-8)
    // H8 move: clear BLACK_OO(4)  → mask=11(=15-4)
    // E8 move: clear both black   → mask=3(=15-12)
    castling_rights = CastlingRights(int(castling_rights) & castling_mask[int(from)] & castling_mask[int(to)]);
    zobrist_hash ^= Zobrist::castling_key(castling_rights);

    // Flip side
    side_to_move = ~side_to_move;
    zobrist_hash ^= Zobrist::side_key();

    if (side_to_move == WHITE) fullmove_number++;
}

// ─── undo_move ────────────────────────────────────────────────────────────

void Board::undo_move(Move m) {
    assert(!history.empty());
    UndoInfo info = history.back();
    history.pop_back();

    // Flip side back
    side_to_move = ~side_to_move;
    if (side_to_move == BLACK) fullmove_number--;

    Square from = m.from();
    Square to   = m.to();
    MoveFlag flag = m.flag();

    if (flag == CASTLE_KINGSIDE) {
        move_piece(to, from); // move king back
        Square rook_to   = make_square(FILE_F, rank_of(from));
        Square rook_from = make_square(FILE_H, rank_of(from));
        move_piece(rook_to, rook_from);
    }
    else if (flag == CASTLE_QUEENSIDE) {
        move_piece(to, from);
        Square rook_to   = make_square(FILE_D, rank_of(from));
        Square rook_from = make_square(FILE_A, rank_of(from));
        move_piece(rook_to, rook_from);
    }
    else if (m.is_promotion()) {
        // Remove promoted piece, restore pawn
        remove_piece(to);
        put_piece(make_piece(side_to_move, PAWN), from);
        if (info.captured_piece != NO_PIECE)
            put_piece(info.captured_piece, to);
    }
    else if (flag == EN_PASSANT) {
        move_piece(to, from);
        Square cap_sq = make_square(file_of(to), rank_of(from));
        put_piece(info.captured_piece, cap_sq);
    }
    else {
        move_piece(to, from);
        if (info.captured_piece != NO_PIECE)
            put_piece(info.captured_piece, to);
    }

    // Restore saved state
    castling_rights   = info.castling_rights;
    en_passant_square = info.en_passant_square;
    halfmove_clock    = info.halfmove_clock;
    zobrist_hash      = info.zobrist_hash;
}
