#include "board.hpp"
#include <sstream>
#include <iostream>
#include <random>
#include <algorithm>

namespace chess {

// Attack tables
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard PAWN_ATTACKS[2][64];
Bitboard BETWEEN_BB[64][64];
Bitboard LINE_BB[64][64];

// Zobrist hashing
uint64_t ZOBRIST_PIECES[16][64];
uint64_t ZOBRIST_SIDE;
uint64_t ZOBRIST_CASTLING[16];
uint64_t ZOBRIST_EP[8];

namespace {

// Simple magic bitboard implementation using PEXT when available
Bitboard ROOK_MASKS[64];
Bitboard BISHOP_MASKS[64];
Bitboard ROOK_ATTACKS_TABLE[64][4096];
Bitboard BISHOP_ATTACKS_TABLE[64][512];

Bitboard sliding_attacks(Square s, Bitboard occupied, const int deltas[4][2]) {
    Bitboard attacks = 0;
    for (int i = 0; i < 4; i++) {
        int df = deltas[i][0];
        int dr = deltas[i][1];
        int f = file_of(s) + df;
        int r = rank_of(s) + dr;
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            Square sq = make_square(File(f), Rank(r));
            attacks |= square_bb(sq);
            if (occupied & square_bb(sq)) break;
            f += df;
            r += dr;
        }
    }
    return attacks;
}

void init_magic_tables() {
    const int rook_deltas[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    const int bishop_deltas[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    
    for (int s = 0; s < 64; s++) {
        // Generate masks (exclude edges)
        ROOK_MASKS[s] = sliding_attacks(Square(s), 0, rook_deltas);
        ROOK_MASKS[s] &= ~(((FileABB | FileHBB) & ~(square_bb(Square(s)))) | 
                          ((Rank1BB | Rank8BB) & ~(square_bb(Square(s)))));
        
        BISHOP_MASKS[s] = sliding_attacks(Square(s), 0, bishop_deltas);
        BISHOP_MASKS[s] &= ~(FileABB | FileHBB | Rank1BB | Rank8BB);
        
        // Generate attack tables for all occupancy patterns
        int rook_bits = popcount(ROOK_MASKS[s]);
        for (int i = 0; i < (1 << rook_bits); i++) {
            Bitboard occ = 0;
            Bitboard mask = ROOK_MASKS[s];
            int j = 0;
            while (mask) {
                Square sq = pop_lsb(mask);
                if (i & (1 << j)) occ |= square_bb(sq);
                j++;
            }
            ROOK_ATTACKS_TABLE[s][i] = sliding_attacks(Square(s), occ, rook_deltas);
        }
        
        int bishop_bits = popcount(BISHOP_MASKS[s]);
        for (int i = 0; i < (1 << bishop_bits); i++) {
            Bitboard occ = 0;
            Bitboard mask = BISHOP_MASKS[s];
            int j = 0;
            while (mask) {
                Square sq = pop_lsb(mask);
                if (i & (1 << j)) occ |= square_bb(sq);
                j++;
            }
            BISHOP_ATTACKS_TABLE[s][i] = sliding_attacks(Square(s), occ, bishop_deltas);
        }
    }
}

void init_non_slider_attacks() {
    const int knight_deltas[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    const int king_deltas[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    
    for (int s = 0; s < 64; s++) {
        int f = s & 7;
        int r = s >> 3;
        
        KNIGHT_ATTACKS[s] = 0;
        for (auto& d : knight_deltas) {
            int nf = f + d[0], nr = r + d[1];
            if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
                KNIGHT_ATTACKS[s] |= square_bb(make_square(File(nf), Rank(nr)));
            }
        }
        
        KING_ATTACKS[s] = 0;
        for (auto& d : king_deltas) {
            int nf = f + d[0], nr = r + d[1];
            if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
                KING_ATTACKS[s] |= square_bb(make_square(File(nf), Rank(nr)));
            }
        }
        
        // Pawn attacks
        PAWN_ATTACKS[WHITE][s] = 0;
        PAWN_ATTACKS[BLACK][s] = 0;
        if (f > 0 && r < 7) PAWN_ATTACKS[WHITE][s] |= square_bb(make_square(File(f-1), Rank(r+1)));
        if (f < 7 && r < 7) PAWN_ATTACKS[WHITE][s] |= square_bb(make_square(File(f+1), Rank(r+1)));
        if (f > 0 && r > 0) PAWN_ATTACKS[BLACK][s] |= square_bb(make_square(File(f-1), Rank(r-1)));
        if (f < 7 && r > 0) PAWN_ATTACKS[BLACK][s] |= square_bb(make_square(File(f+1), Rank(r-1)));
    }
}

void init_between_and_line() {
    for (int s1 = 0; s1 < 64; s1++) {
        for (int s2 = 0; s2 < 64; s2++) {
            BETWEEN_BB[s1][s2] = 0;
            LINE_BB[s1][s2] = 0;
            
            if (s1 == s2) continue;
            
            Bitboard attacks = bishop_attacks(Square(s1), 0) | rook_attacks(Square(s1), 0);
            if (attacks & square_bb(Square(s2))) {
                // s2 is on a ray from s1
                int f1 = s1 & 7, r1 = s1 >> 3;
                int f2 = s2 & 7, r2 = s2 >> 3;
                int df = (f2 > f1) ? 1 : (f2 < f1 ? -1 : 0);
                int dr = (r2 > r1) ? 1 : (r2 < r1 ? -1 : 0);
                
                // Between squares (exclusive)
                int f = f1 + df, r = r1 + dr;
                while (f != f2 || r != r2) {
                    BETWEEN_BB[s1][s2] |= square_bb(make_square(File(f), Rank(r)));
                    f += df;
                    r += dr;
                }
                
                // Line (extends beyond s2)
                LINE_BB[s1][s2] = BETWEEN_BB[s1][s2] | square_bb(Square(s1)) | square_bb(Square(s2));
                f = f2 + df; r = r2 + dr;
                while (f >= 0 && f < 8 && r >= 0 && r < 8) {
                    LINE_BB[s1][s2] |= square_bb(make_square(File(f), Rank(r)));
                    f += df;
                    r += dr;
                }
                f = f1 - df; r = r1 - dr;
                while (f >= 0 && f < 8 && r >= 0 && r < 8) {
                    LINE_BB[s1][s2] |= square_bb(make_square(File(f), Rank(r)));
                    f -= df;
                    r -= dr;
                }
            }
        }
    }
}

void init_zobrist() {
    std::mt19937_64 rng(0x1234567890ABCDEFULL);
    
    for (int p = 0; p < 16; p++) {
        for (int s = 0; s < 64; s++) {
            ZOBRIST_PIECES[p][s] = rng();
        }
    }
    ZOBRIST_SIDE = rng();
    for (int i = 0; i < 16; i++) {
        ZOBRIST_CASTLING[i] = rng();
    }
    for (int i = 0; i < 8; i++) {
        ZOBRIST_EP[i] = rng();
    }
}

bool attacks_initialized = false;

} // anonymous namespace

// Simple lookup for magic bitboards
static inline Bitboard pext(Bitboard b, Bitboard mask) {
    Bitboard result = 0;
    int bit = 0;
    while (mask) {
        Square s = pop_lsb(mask);
        if (b & square_bb(s)) {
            result |= 1ULL << bit;
        }
        bit++;
    }
    return result;
}

Bitboard bishop_attacks(Square s, Bitboard occupied) {
    return BISHOP_ATTACKS_TABLE[s][pext(occupied, BISHOP_MASKS[s])];
}

Bitboard rook_attacks(Square s, Bitboard occupied) {
    return ROOK_ATTACKS_TABLE[s][pext(occupied, ROOK_MASKS[s])];
}

Bitboard queen_attacks(Square s, Bitboard occupied) {
    return bishop_attacks(s, occupied) | rook_attacks(s, occupied);
}

void init_attacks() {
    if (attacks_initialized) return;
    init_magic_tables();
    init_non_slider_attacks();
    init_between_and_line();
    init_zobrist();
    attacks_initialized = true;
}

// Board implementation
Board::Board() {
    init_attacks();
    reset();
}

Board::Board(const std::string& fen) {
    init_attacks();
    set_fen(fen);
}

void Board::reset() {
    set_fen(STARTING_FEN);
}

void Board::set_fen(const std::string& fen) {
    pieces_.fill(NO_PIECE);
    by_color_.fill(0);
    by_type_.fill(0);
    occupied_ = 0;
    king_sq_ = {NO_SQUARE, NO_SQUARE};
    side_to_move_ = WHITE;
    castling_ = 0;
    ep_square_ = NO_SQUARE;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    hash_ = 0;
    undo_stack_.clear();
    
    std::istringstream ss(fen);
    std::string board_str, turn, castling, ep, halfmove, fullmove;
    ss >> board_str >> turn >> castling >> ep >> halfmove >> fullmove;
    
    // Parse board
    int sq = 56; // Start from a8
    for (char c : board_str) {
        if (c == '/') {
            sq -= 16; // Move to next rank
        } else if (c >= '1' && c <= '8') {
            sq += c - '0';
        } else {
            Piece p = char_to_piece(c);
            if (p != NO_PIECE) {
                put_piece(p, Square(sq));
            }
            sq++;
        }
    }
    
    // Side to move
    side_to_move_ = (turn == "b") ? BLACK : WHITE;
    if (side_to_move_ == BLACK) hash_ ^= ZOBRIST_SIDE;
    
    // Castling rights
    if (castling != "-") {
        for (char c : castling) {
            if (c == 'K') castling_ |= WHITE_OO;
            else if (c == 'Q') castling_ |= WHITE_OOO;
            else if (c == 'k') castling_ |= BLACK_OO;
            else if (c == 'q') castling_ |= BLACK_OOO;
        }
    }
    hash_ ^= ZOBRIST_CASTLING[castling_];
    
    // En passant
    if (ep != "-") {
        ep_square_ = string_to_square(ep);
        if (ep_square_ != NO_SQUARE) {
            hash_ ^= ZOBRIST_EP[file_of(ep_square_)];
        }
    }
    
    // Clocks
    if (!halfmove.empty()) halfmove_clock_ = std::stoi(halfmove);
    if (!fullmove.empty()) fullmove_number_ = std::stoi(fullmove);
}

std::string Board::fen() const {
    std::string result;
    
    // Board
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            Piece p = pieces_[r * 8 + f];
            if (p == NO_PIECE) {
                empty++;
            } else {
                if (empty > 0) {
                    result += std::to_string(empty);
                    empty = 0;
                }
                result += piece_to_char(p);
            }
        }
        if (empty > 0) result += std::to_string(empty);
        if (r > 0) result += '/';
    }
    
    // Side to move
    result += (side_to_move_ == WHITE) ? " w " : " b ";
    
    // Castling
    if (castling_ == 0) {
        result += "-";
    } else {
        if (castling_ & WHITE_OO) result += 'K';
        if (castling_ & WHITE_OOO) result += 'Q';
        if (castling_ & BLACK_OO) result += 'k';
        if (castling_ & BLACK_OOO) result += 'q';
    }
    result += ' ';
    
    // En passant
    if (ep_square_ == NO_SQUARE) {
        result += "-";
    } else {
        result += square_to_string(ep_square_);
    }
    
    // Clocks
    result += ' ' + std::to_string(halfmove_clock_);
    result += ' ' + std::to_string(fullmove_number_);
    
    return result;
}

void Board::put_piece(Piece p, Square s) {
    pieces_[s] = p;
    Bitboard bb = square_bb(s);
    Color c = color_of(p);
    PieceType pt = type_of(p);
    by_color_[c] |= bb;
    by_type_[pt] |= bb;
    occupied_ |= bb;
    if (pt == KING) king_sq_[c] = s;
    hash_ ^= ZOBRIST_PIECES[p][s];
}

void Board::remove_piece(Square s) {
    Piece p = pieces_[s];
    if (p == NO_PIECE) return;
    Bitboard bb = square_bb(s);
    Color c = color_of(p);
    PieceType pt = type_of(p);
    by_color_[c] &= ~bb;
    by_type_[pt] &= ~bb;
    occupied_ &= ~bb;
    pieces_[s] = NO_PIECE;
    hash_ ^= ZOBRIST_PIECES[p][s];
}

void Board::move_piece(Square from, Square to) {
    Piece p = pieces_[from];
    hash_ ^= ZOBRIST_PIECES[p][from];
    hash_ ^= ZOBRIST_PIECES[p][to];
    
    Bitboard from_bb = square_bb(from);
    Bitboard to_bb = square_bb(to);
    Bitboard from_to = from_bb | to_bb;
    
    Color c = color_of(p);
    PieceType pt = type_of(p);
    
    pieces_[from] = NO_PIECE;
    pieces_[to] = p;
    by_color_[c] ^= from_to;
    by_type_[pt] ^= from_to;
    occupied_ ^= from_to;
    
    if (pt == KING) king_sq_[c] = to;
}

void Board::update_castling(Square from, Square to) {
    // Update castling rights based on moved piece
    static const int castling_update[64] = {
        13, 15, 15, 15, 12, 15, 15, 14,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        7,  15, 15, 15,  3, 15, 15, 11
    };
    
    hash_ ^= ZOBRIST_CASTLING[castling_];
    castling_ &= castling_update[from] & castling_update[to];
    hash_ ^= ZOBRIST_CASTLING[castling_];
}

bool Board::make_move(Move m) {
    if (m.is_null()) return false;
    
    Square from = m.from();
    Square to = m.to();
    Piece moved = pieces_[from];
    Piece captured = pieces_[to];
    PieceType pt = type_of(moved);
    Color us = side_to_move_;
    
    // Save undo info
    UndoInfo undo;
    undo.captured = captured;
    undo.castling = castling_;
    undo.ep_square = ep_square_;
    undo.halfmove_clock = halfmove_clock_;
    undo.hash = hash_;
    undo_stack_.push_back(undo);
    
    // Clear ep square
    if (ep_square_ != NO_SQUARE) {
        hash_ ^= ZOBRIST_EP[file_of(ep_square_)];
        ep_square_ = NO_SQUARE;
    }
    
    // Handle captures
    if (captured != NO_PIECE) {
        remove_piece(to);
        halfmove_clock_ = 0;
    } else if (pt == PAWN) {
        // En passant capture
        if (file_of(from) != file_of(to)) {
            Square ep_victim = make_square(file_of(to), rank_of(from));
            remove_piece(ep_victim);
        }
        halfmove_clock_ = 0;
    } else {
        halfmove_clock_++;
    }
    
    // Move the piece
    move_piece(from, to);
    
    // Handle promotion
    if (m.is_promotion()) {
        remove_piece(to);
        put_piece(make_piece(us, m.promotion()), to);
    }
    
    // Handle castling
    if (pt == KING) {
        if (from == E1 && to == G1) move_piece(H1, F1);
        else if (from == E1 && to == C1) move_piece(A1, D1);
        else if (from == E8 && to == G8) move_piece(H8, F8);
        else if (from == E8 && to == C8) move_piece(A8, D8);
    }
    
    // Double pawn push creates ep square
    if (pt == PAWN && std::abs(rank_of(to) - rank_of(from)) == 2) {
        ep_square_ = make_square(file_of(from), Rank((rank_of(from) + rank_of(to)) / 2));
        hash_ ^= ZOBRIST_EP[file_of(ep_square_)];
    }
    
    // Update castling rights
    update_castling(from, to);
    
    // Switch side
    side_to_move_ = ~side_to_move_;
    hash_ ^= ZOBRIST_SIDE;
    
    if (us == BLACK) fullmove_number_++;
    
    // Check if king is in check (move was illegal)
    if (is_attacked_by(~us, king_sq_[us])) {
        unmake_move(m);
        return false;
    }
    
    return true;
}

void Board::unmake_move(Move m) {
    if (undo_stack_.empty()) return;
    
    UndoInfo& undo = undo_stack_.back();
    
    Square from = m.from();
    Square to = m.to();
    Color us = ~side_to_move_;
    Piece moved = pieces_[to];
    PieceType pt = m.is_promotion() ? PAWN : type_of(moved);
    
    // Switch side back
    side_to_move_ = us;
    if (us == BLACK) fullmove_number_--;
    
    // Handle castling
    if (pt == KING) {
        if (from == E1 && to == G1) move_piece(F1, H1);
        else if (from == E1 && to == C1) move_piece(D1, A1);
        else if (from == E8 && to == G8) move_piece(F8, H8);
        else if (from == E8 && to == C8) move_piece(D8, A8);
    }
    
    // Undo promotion
    if (m.is_promotion()) {
        remove_piece(to);
        put_piece(make_piece(us, PAWN), to);
    }
    
    // Move piece back
    move_piece(to, from);
    
    // Restore captured piece
    if (undo.captured != NO_PIECE) {
        put_piece(undo.captured, to);
    } else if (pt == PAWN && file_of(from) != file_of(to)) {
        // Restore en passant captured pawn
        Square ep_victim = make_square(file_of(to), rank_of(from));
        put_piece(make_piece(~us, PAWN), ep_victim);
    }
    
    // Restore state
    castling_ = undo.castling;
    ep_square_ = undo.ep_square;
    halfmove_clock_ = undo.halfmove_clock;
    hash_ = undo.hash;
    
    undo_stack_.pop_back();
}

Bitboard Board::attackers_to(Square s, Bitboard occupied) const {
    return (PAWN_ATTACKS[BLACK][s] & pieces(WHITE, PAWN))
         | (PAWN_ATTACKS[WHITE][s] & pieces(BLACK, PAWN))
         | (KNIGHT_ATTACKS[s] & by_type_[KNIGHT])
         | (bishop_attacks(s, occupied) & (by_type_[BISHOP] | by_type_[QUEEN]))
         | (rook_attacks(s, occupied) & (by_type_[ROOK] | by_type_[QUEEN]))
         | (KING_ATTACKS[s] & by_type_[KING]);
}

bool Board::is_attacked_by(Color c, Square s) const {
    return attackers_to(s) & by_color_[c];
}

bool Board::gives_check(Move m) const {
    Board copy = *this;
    copy.make_move(m);
    return copy.is_in_check();
}

bool Board::is_legal(Move m) const {
    Board copy = *this;
    return copy.make_move(m);
}

bool Board::is_checkmate() const {
    if (!is_in_check()) return false;
    return legal_moves().empty();
}

bool Board::is_stalemate() const {
    if (is_in_check()) return false;
    return legal_moves().empty();
}

bool Board::is_insufficient_material() const {
    if (by_type_[PAWN] | by_type_[ROOK] | by_type_[QUEEN]) return false;
    
    int knights = popcount(by_type_[KNIGHT]);
    int bishops = popcount(by_type_[BISHOP]);
    
    // K vs K
    if (knights == 0 && bishops == 0) return true;
    // KN vs K or KB vs K
    if (knights + bishops == 1) return true;
    // KNN vs K (technically insufficient but won't check)
    
    return false;
}

bool Board::is_draw() const {
    if (halfmove_clock_ >= 100) return true; // 50 move rule
    if (is_insufficient_material()) return true;
    // TODO: Add repetition detection
    return false;
}

int Board::material_count(Color c) const {
    int score = 0;
    score += popcount(pieces(c, PAWN)) * 100;
    score += popcount(pieces(c, KNIGHT)) * 320;
    score += popcount(pieces(c, BISHOP)) * 330;
    score += popcount(pieces(c, ROOK)) * 500;
    score += popcount(pieces(c, QUEEN)) * 900;
    return score;
}

void Board::print() const {
    std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
    for (int r = 7; r >= 0; r--) {
        std::cout << (r + 1) << " |";
        for (int f = 0; f < 8; f++) {
            Piece p = pieces_[r * 8 + f];
            char c = (p == NO_PIECE) ? ' ' : piece_to_char(p);
            std::cout << " " << c << " |";
        }
        std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
    }
    std::cout << "    a   b   c   d   e   f   g   h\n\n";
    std::cout << "FEN: " << fen() << "\n";
}

} // namespace chess
