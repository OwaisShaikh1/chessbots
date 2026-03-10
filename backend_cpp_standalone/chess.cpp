/*
 * Chess Backend C++ - Standalone Version
 * Chess engine implementation
 */

#include "chess.hpp"
#include <cctype>

namespace chess {

// ============= Global Attack Tables =============
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard PAWN_ATTACKS[2][64];
static uint64_t ZOBRIST_PIECES[16][64];
static uint64_t ZOBRIST_EP[8];
static uint64_t ZOBRIST_CASTLING[16];
static uint64_t ZOBRIST_SIDE;
static bool tables_initialized = false;

PieceValues PIECE_VALUES;
EvalParams EVAL_PARAMS;

// Piece-Square Tables (middlegame and endgame)
static const int PAWN_PST_MG[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
};

static const int KNIGHT_PST_MG[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

static const int BISHOP_PST_MG[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

static const int ROOK_PST_MG[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
};

static const int QUEEN_PST_MG[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

static const int KING_PST_MG[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

static const int KING_PST_EG[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

static int get_pst_value(PieceType pt, int sq, bool is_endgame) {
    switch (pt) {
        case PAWN: return PAWN_PST_MG[sq];
        case KNIGHT: return KNIGHT_PST_MG[sq];
        case BISHOP: return BISHOP_PST_MG[sq];
        case ROOK: return ROOK_PST_MG[sq];
        case QUEEN: return QUEEN_PST_MG[sq];
        case KING: return is_endgame ? KING_PST_EG[sq] : KING_PST_MG[sq];
        default: return 0;
    }
}

void init_attacks() {
    if (tables_initialized) return;
    
    // Knight attacks
    const int knight_offsets[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for (int sq = 0; sq < 64; sq++) {
        Bitboard bb = 0;
        int r = rank_of(sq), f = file_of(sq);
        for (auto& off : knight_offsets) {
            int nr = r + off[0], nf = f + off[1];
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
                bb |= square_bb(make_square(nf, nr));
        }
        KNIGHT_ATTACKS[sq] = bb;
    }
    
    // King attacks
    const int king_offsets[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    for (int sq = 0; sq < 64; sq++) {
        Bitboard bb = 0;
        int r = rank_of(sq), f = file_of(sq);
        for (auto& off : king_offsets) {
            int nr = r + off[0], nf = f + off[1];
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
                bb |= square_bb(make_square(nf, nr));
        }
        KING_ATTACKS[sq] = bb;
    }
    
    // Pawn attacks
    for (int sq = 0; sq < 64; sq++) {
        int r = rank_of(sq), f = file_of(sq);
        Bitboard white_bb = 0, black_bb = 0;
        
        if (r < 7) {
            if (f > 0) white_bb |= square_bb(make_square(f - 1, r + 1));
            if (f < 7) white_bb |= square_bb(make_square(f + 1, r + 1));
        }
        if (r > 0) {
            if (f > 0) black_bb |= square_bb(make_square(f - 1, r - 1));
            if (f < 7) black_bb |= square_bb(make_square(f + 1, r - 1));
        }
        PAWN_ATTACKS[WHITE][sq] = white_bb;
        PAWN_ATTACKS[BLACK][sq] = black_bb;
    }
    
    // Zobrist keys
    std::mt19937_64 rng(12345);
    for (int p = 0; p < 16; p++)
        for (int sq = 0; sq < 64; sq++)
            ZOBRIST_PIECES[p][sq] = rng();
    for (int f = 0; f < 8; f++)
        ZOBRIST_EP[f] = rng();
    for (int c = 0; c < 16; c++)
        ZOBRIST_CASTLING[c] = rng();
    ZOBRIST_SIDE = rng();
    
    tables_initialized = true;
}

// Simple sliding piece attacks (no magic bitboards for simplicity)
Bitboard bishop_attacks(int sq, Bitboard occ) {
    Bitboard result = 0;
    const int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    int r = rank_of(sq), f = file_of(sq);
    
    for (auto& dir : dirs) {
        int nr = r + dir[0], nf = f + dir[1];
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int s = make_square(nf, nr);
            result |= square_bb(s);
            if (occ & square_bb(s)) break;
            nr += dir[0];
            nf += dir[1];
        }
    }
    return result;
}

Bitboard rook_attacks(int sq, Bitboard occ) {
    Bitboard result = 0;
    const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    int r = rank_of(sq), f = file_of(sq);
    
    for (auto& dir : dirs) {
        int nr = r + dir[0], nf = f + dir[1];
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int s = make_square(nf, nr);
            result |= square_bb(s);
            if (occ & square_bb(s)) break;
            nr += dir[0];
            nf += dir[1];
        }
    }
    return result;
}

Bitboard queen_attacks(int sq, Bitboard occ) {
    return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}

// ============= Board Implementation =============
Board::Board() : castling_(0), ep_square_(NO_SQUARE), halfmove_clock_(0), fullmove_(1) {
    init_attacks();
    reset();
}

Board::Board(const std::string& fen) : castling_(0), ep_square_(NO_SQUARE), halfmove_clock_(0), fullmove_(1) {
    init_attacks();
    set_fen(fen);
}

void Board::reset() {
    set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Board::put_piece(Piece p, int s) {
    pieces_[s] = p;
    Bitboard bb = square_bb(s);
    by_color_[color_of(p)] |= bb;
    by_type_[type_of(p)] |= bb;
    occupied_ |= bb;
    if (type_of(p) == KING)
        king_sq_[color_of(p)] = s;
    hash_ ^= ZOBRIST_PIECES[p][s];
}

void Board::remove_piece(int s) {
    Piece p = pieces_[s];
    if (p == NO_PIECE) return;
    pieces_[s] = NO_PIECE;
    Bitboard bb = square_bb(s);
    by_color_[color_of(p)] &= ~bb;
    by_type_[type_of(p)] &= ~bb;
    occupied_ &= ~bb;
    hash_ ^= ZOBRIST_PIECES[p][s];
}

void Board::move_piece(int from, int to) {
    Piece p = pieces_[from];
    hash_ ^= ZOBRIST_PIECES[p][from] ^ ZOBRIST_PIECES[p][to];
    
    Bitboard from_to = square_bb(from) | square_bb(to);
    pieces_[from] = NO_PIECE;
    pieces_[to] = p;
    by_color_[color_of(p)] ^= from_to;
    by_type_[type_of(p)] ^= from_to;
    occupied_ ^= from_to;
    
    if (type_of(p) == KING)
        king_sq_[color_of(p)] = to;
}

void Board::set_fen(const std::string& fen) {
    pieces_.fill(NO_PIECE);
    by_color_.fill(0);
    by_type_.fill(0);
    occupied_ = 0;
    king_sq_ = {-1, -1};
    castling_ = 0;
    ep_square_ = NO_SQUARE;
    halfmove_clock_ = 0;
    fullmove_ = 1;
    hash_ = 0;
    undo_stack_.clear();
    
    std::istringstream iss(fen);
    std::string board_str, side, castle, ep, half, full;
    iss >> board_str >> side >> castle >> ep;
    iss >> half >> full;
    
    int sq = 56;
    for (char c : board_str) {
        if (c == '/') {
            sq -= 16;
        } else if (std::isdigit(c)) {
            sq += c - '0';
        } else {
            Piece p = NO_PIECE;
            switch (std::tolower(c)) {
                case 'p': p = std::isupper(c) ? W_PAWN : B_PAWN; break;
                case 'n': p = std::isupper(c) ? W_KNIGHT : B_KNIGHT; break;
                case 'b': p = std::isupper(c) ? W_BISHOP : B_BISHOP; break;
                case 'r': p = std::isupper(c) ? W_ROOK : B_ROOK; break;
                case 'q': p = std::isupper(c) ? W_QUEEN : B_QUEEN; break;
                case 'k': p = std::isupper(c) ? W_KING : B_KING; break;
            }
            if (p != NO_PIECE) put_piece(p, sq++);
        }
    }
    
    side_to_move_ = (side == "b") ? BLACK : WHITE;
    if (side_to_move_ == BLACK) hash_ ^= ZOBRIST_SIDE;
    
    if (castle.find('K') != std::string::npos) castling_ |= 1;
    if (castle.find('Q') != std::string::npos) castling_ |= 2;
    if (castle.find('k') != std::string::npos) castling_ |= 4;
    if (castle.find('q') != std::string::npos) castling_ |= 8;
    hash_ ^= ZOBRIST_CASTLING[castling_];
    
    if (ep != "-" && ep.length() >= 2) {
        int f = ep[0] - 'a', r = ep[1] - '1';
        if (f >= 0 && f < 8 && r >= 0 && r < 8) {
            ep_square_ = make_square(f, r);
            hash_ ^= ZOBRIST_EP[f];
        }
    }
    
    if (!half.empty()) halfmove_clock_ = std::stoi(half);
    if (!full.empty()) fullmove_ = std::stoi(full);
}

std::string Board::fen() const {
    std::string result;
    
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            Piece p = pieces_[make_square(f, r)];
            if (p == NO_PIECE) {
                empty++;
            } else {
                if (empty > 0) {
                    result += std::to_string(empty);
                    empty = 0;
                }
                const char pieces[] = " PNBRQK  pnbrqk";
                result += pieces[p];
            }
        }
        if (empty > 0) result += std::to_string(empty);
        if (r > 0) result += '/';
    }
    
    result += (side_to_move_ == WHITE) ? " w " : " b ";
    
    std::string castle;
    if (castling_ & 1) castle += 'K';
    if (castling_ & 2) castle += 'Q';
    if (castling_ & 4) castle += 'k';
    if (castling_ & 8) castle += 'q';
    result += castle.empty() ? "-" : castle;
    
    result += ' ';
    if (ep_square_ != NO_SQUARE) {
        result += char('a' + file_of(ep_square_));
        result += char('1' + rank_of(ep_square_));
    } else {
        result += '-';
    }
    
    result += ' ' + std::to_string(halfmove_clock_) + ' ' + std::to_string(fullmove_);
    return result;
}

bool Board::is_attacked_by(Color c, int sq) const {
    if (PAWN_ATTACKS[~c][sq] & pieces(c, PAWN)) return true;
    if (KNIGHT_ATTACKS[sq] & pieces(c, KNIGHT)) return true;
    if (KING_ATTACKS[sq] & pieces(c, KING)) return true;
    if (bishop_attacks(sq, occupied_) & (pieces(c, BISHOP) | pieces(c, QUEEN))) return true;
    if (rook_attacks(sq, occupied_) & (pieces(c, ROOK) | pieces(c, QUEEN))) return true;
    return false;
}

bool Board::is_in_check() const {
    return is_attacked_by(~side_to_move_, king_sq_[side_to_move_]);
}

bool Board::make_move(Move m) {
    UndoInfo undo;
    undo.captured = pieces_[m.to()];
    undo.castling = castling_;
    undo.ep_square = ep_square_;
    undo.halfmove = halfmove_clock_;
    undo.hash = hash_;
    undo_stack_.push_back(undo);
    
    Color us = side_to_move_;
    Color them = ~us;
    Piece moving = pieces_[m.from()];
    PieceType pt = type_of(moving);
    
    // Update hash for old EP
    if (ep_square_ != NO_SQUARE) hash_ ^= ZOBRIST_EP[file_of(ep_square_)];
    
    // Handle captures
    if (undo.captured != NO_PIECE) {
        remove_piece(m.to());
    }
    
    // En passant capture
    if (pt == PAWN && m.to() == ep_square_) {
        int captured_sq = (us == WHITE) ? m.to() - 8 : m.to() + 8;
        remove_piece(captured_sq);
    }
    
    // Move the piece
    move_piece(m.from(), m.to());
    
    // Handle promotion
    if (m.is_promotion()) {
        remove_piece(m.to());
        put_piece(make_piece(us, m.promotion()), m.to());
    }
    
    // Castling
    if (pt == KING && std::abs(file_of(m.from()) - file_of(m.to())) > 1) {
        if (m.to() == G1) move_piece(H1, F1);
        else if (m.to() == C1) move_piece(A1, D1);
        else if (m.to() == G8) move_piece(H8, F8);
        else if (m.to() == C8) move_piece(A8, D8);
    }
    
    // Update castling rights
    hash_ ^= ZOBRIST_CASTLING[castling_];
    if (pt == KING) {
        if (us == WHITE) castling_ &= ~3;
        else castling_ &= ~12;
    }
    if (m.from() == H1 || m.to() == H1) castling_ &= ~1;
    if (m.from() == A1 || m.to() == A1) castling_ &= ~2;
    if (m.from() == H8 || m.to() == H8) castling_ &= ~4;
    if (m.from() == A8 || m.to() == A8) castling_ &= ~8;
    hash_ ^= ZOBRIST_CASTLING[castling_];
    
    // Update en passant
    ep_square_ = NO_SQUARE;
    if (pt == PAWN && std::abs(rank_of(m.from()) - rank_of(m.to())) == 2) {
        ep_square_ = (us == WHITE) ? m.from() + 8 : m.from() - 8;
        hash_ ^= ZOBRIST_EP[file_of(ep_square_)];
    }
    
    // Update clocks
    halfmove_clock_ = (pt == PAWN || undo.captured != NO_PIECE) ? 0 : halfmove_clock_ + 1;
    if (us == BLACK) fullmove_++;
    
    // Switch side
    side_to_move_ = them;
    hash_ ^= ZOBRIST_SIDE;
    
    // Check if move was legal (didn't leave king in check)
    if (is_attacked_by(them, king_sq_[us])) {
        unmake_move(m);
        return false;
    }
    
    return true;
}

void Board::unmake_move(Move m) {
    if (undo_stack_.empty()) return;
    UndoInfo undo = undo_stack_.back();
    undo_stack_.pop_back();
    
    side_to_move_ = ~side_to_move_;
    Color us = side_to_move_;
    
    // Move piece back
    Piece moved = pieces_[m.to()];
    
    // Handle promotion
    if (m.is_promotion()) {
        remove_piece(m.to());
        put_piece(make_piece(us, PAWN), m.from());
    } else {
        move_piece(m.to(), m.from());
    }
    
    // Restore captured piece
    if (undo.captured != NO_PIECE) {
        put_piece(undo.captured, m.to());
    }
    
    // En passant
    if (type_of(moved) == PAWN || (m.is_promotion())) {
        if (m.to() == undo.ep_square && undo.captured == NO_PIECE) {
            int captured_sq = (us == WHITE) ? m.to() - 8 : m.to() + 8;
            put_piece(make_piece(~us, PAWN), captured_sq);
        }
    }
    
    // Castling
    if (type_of(pieces_[m.from()]) == KING) {
        if (m.from() == E1 && m.to() == G1) move_piece(F1, H1);
        else if (m.from() == E1 && m.to() == C1) move_piece(D1, A1);
        else if (m.from() == E8 && m.to() == G8) move_piece(F8, H8);
        else if (m.from() == E8 && m.to() == C8) move_piece(D8, A8);
    }
    
    castling_ = undo.castling;
    ep_square_ = undo.ep_square;
    halfmove_clock_ = undo.halfmove;
    hash_ = undo.hash;
    if (us == BLACK) fullmove_--;
}

bool Board::gives_check(Move m) const {
    Board copy = *this;
    if (copy.make_move(m)) {
        return copy.is_in_check();
    }
    return false;
}

std::vector<Move> Board::legal_moves() const {
    std::vector<Move> moves;
    moves.reserve(218);
    
    Color us = side_to_move_;
    Bitboard our = pieces(us);
    Bitboard their = pieces(~us);
    
    // Pawn moves
    Bitboard pawns = pieces(us, PAWN);
    while (pawns) {
        int from = pop_lsb(pawns);
        int r = rank_of(from), f = file_of(from);
        
        // Pushes
        int push = (us == WHITE) ? from + 8 : from - 8;
        if (push >= 0 && push < 64 && !(occupied_ & square_bb(push))) {
            if ((us == WHITE && r == 6) || (us == BLACK && r == 1)) {
                for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT})
                    moves.push_back(Move(from, push, pt));
            } else {
                moves.push_back(Move(from, push));
                // Double push
                if ((us == WHITE && r == 1) || (us == BLACK && r == 6)) {
                    int dpush = (us == WHITE) ? from + 16 : from - 16;
                    if (!(occupied_ & square_bb(dpush)))
                        moves.push_back(Move(from, dpush));
                }
            }
        }
        
        // Captures
        Bitboard attacks = PAWN_ATTACKS[us][from] & (their | (ep_square_ != NO_SQUARE ? square_bb(ep_square_) : 0));
        while (attacks) {
            int to = pop_lsb(attacks);
            if ((us == WHITE && rank_of(to) == 7) || (us == BLACK && rank_of(to) == 0)) {
                for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT})
                    moves.push_back(Move(from, to, pt));
            } else {
                moves.push_back(Move(from, to));
            }
        }
    }
    
    // Knight moves
    Bitboard knights = pieces(us, KNIGHT);
    while (knights) {
        int from = pop_lsb(knights);
        Bitboard attacks = KNIGHT_ATTACKS[from] & ~our;
        while (attacks) {
            int to = pop_lsb(attacks);
            moves.push_back(Move(from, to));
        }
    }
    
    // Bishop moves
    Bitboard bishops = pieces(us, BISHOP);
    while (bishops) {
        int from = pop_lsb(bishops);
        Bitboard attacks = bishop_attacks(from, occupied_) & ~our;
        while (attacks) {
            int to = pop_lsb(attacks);
            moves.push_back(Move(from, to));
        }
    }
    
    // Rook moves
    Bitboard rooks = pieces(us, ROOK);
    while (rooks) {
        int from = pop_lsb(rooks);
        Bitboard attacks = rook_attacks(from, occupied_) & ~our;
        while (attacks) {
            int to = pop_lsb(attacks);
            moves.push_back(Move(from, to));
        }
    }
    
    // Queen moves
    Bitboard queens = pieces(us, QUEEN);
    while (queens) {
        int from = pop_lsb(queens);
        Bitboard attacks = queen_attacks(from, occupied_) & ~our;
        while (attacks) {
            int to = pop_lsb(attacks);
            moves.push_back(Move(from, to));
        }
    }
    
    // King moves
    int king = king_sq_[us];
    Bitboard attacks = KING_ATTACKS[king] & ~our;
    while (attacks) {
        int to = pop_lsb(attacks);
        moves.push_back(Move(king, to));
    }
    
    // Castling
    if (us == WHITE) {
        if ((castling_ & 1) && !(occupied_ & 0x60ULL) && !is_attacked_by(BLACK, E1) && !is_attacked_by(BLACK, F1) && !is_attacked_by(BLACK, G1))
            moves.push_back(Move(E1, G1));
        if ((castling_ & 2) && !(occupied_ & 0x0EULL) && !is_attacked_by(BLACK, E1) && !is_attacked_by(BLACK, D1) && !is_attacked_by(BLACK, C1))
            moves.push_back(Move(E1, C1));
    } else {
        if ((castling_ & 4) && !(occupied_ & 0x6000000000000000ULL) && !is_attacked_by(WHITE, E8) && !is_attacked_by(WHITE, F8) && !is_attacked_by(WHITE, G8))
            moves.push_back(Move(E8, G8));
        if ((castling_ & 8) && !(occupied_ & 0x0E00000000000000ULL) && !is_attacked_by(WHITE, E8) && !is_attacked_by(WHITE, D8) && !is_attacked_by(WHITE, C8))
            moves.push_back(Move(E8, C8));
    }
    
    // Filter illegal moves
    std::vector<Move> legal;
    Board copy = *this;
    for (const auto& m : moves) {
        if (const_cast<Board&>(copy).make_move(m)) {
            legal.push_back(m);
            const_cast<Board&>(copy).unmake_move(m);
        }
    }
    
    return legal;
}

std::vector<Move> Board::legal_captures() const {
    auto all = legal_moves();
    std::vector<Move> caps;
    for (const auto& m : all) {
        if (pieces_[m.to()] != NO_PIECE || (type_of(pieces_[m.from()]) == PAWN && m.to() == ep_square_))
            caps.push_back(m);
    }
    return caps;
}

bool Board::is_checkmate() const {
    return is_in_check() && legal_moves().empty();
}

bool Board::is_stalemate() const {
    return !is_in_check() && legal_moves().empty();
}

bool Board::is_insufficient_material() const {
    int pieces = popcount(occupied_);
    if (pieces > 4) return false;
    if (pieces == 2) return true;  // K vs K
    if (pieces == 3) {
        if (by_type_[KNIGHT] || by_type_[BISHOP]) return true;
    }
    // K+B vs K+B on same color
    if (pieces == 4 && popcount(by_type_[BISHOP]) == 2) {
        Bitboard bishops = by_type_[BISHOP];
        int sq1 = lsb(bishops);
        bishops &= bishops - 1;
        int sq2 = lsb(bishops);
        if (((file_of(sq1) + rank_of(sq1)) % 2) == ((file_of(sq2) + rank_of(sq2)) % 2))
            return true;
    }
    return false;
}

bool Board::is_draw() const {
    if (halfmove_clock_ >= 100) return true;
    if (is_stalemate()) return true;
    if (is_insufficient_material()) return true;
    return false;
}

bool Board::is_game_over() const {
    return is_checkmate() || is_draw();
}

int Board::material_count(Color c) const {
    int mat = 0;
    mat += PIECE_VALUES.pawn * popcount(pieces(c, PAWN));
    mat += PIECE_VALUES.knight * popcount(pieces(c, KNIGHT));
    mat += PIECE_VALUES.bishop * popcount(pieces(c, BISHOP));
    mat += PIECE_VALUES.rook * popcount(pieces(c, ROOK));
    mat += PIECE_VALUES.queen * popcount(pieces(c, QUEEN));
    return mat;
}

// ============= Evaluation =============
int evaluate(const Board& board) {
    if (board.is_checkmate()) {
        return (board.side_to_move() == WHITE) ? -MATE_VALUE : MATE_VALUE;
    }
    if (board.is_draw()) return 0;
    
    int score = 0;
    
    // Material
    int white_mat = board.material_count(WHITE);
    int black_mat = board.material_count(BLACK);
    score += white_mat - black_mat;
    
    // Determine game phase
    int total_mat = white_mat + black_mat;
    bool is_endgame = total_mat < 2400;
    
    // PST
    for (int sq = 0; sq < 64; sq++) {
        Piece p = board.piece_at(sq);
        if (p == NO_PIECE) continue;
        
        PieceType pt = type_of(p);
        Color c = color_of(p);
        int pst_sq = (c == WHITE) ? flip_rank(sq) : sq;
        int pst_value = get_pst_value(pt, pst_sq, is_endgame);
        
        if (c == WHITE) score += pst_value;
        else score -= pst_value;
    }
    
    // Mobility
    auto moves = board.legal_moves();
    int mobility = (int)moves.size();
    Board flipped(board.fen());
    flipped.set_fen(board.fen().replace(board.fen().find(board.side_to_move() == WHITE ? " w " : " b "), 3, 
                                         board.side_to_move() == WHITE ? " b " : " w "));
    int opp_mobility = (int)flipped.legal_moves().size();
    
    if (board.side_to_move() == WHITE)
        score += (mobility - opp_mobility) * EVAL_PARAMS.mobility_weight;
    else
        score -= (mobility - opp_mobility) * EVAL_PARAMS.mobility_weight;
    
    // Endgame: drive enemy king to corner
    if (is_endgame && std::abs(white_mat - black_mat) > 200) {
        Color winning = (white_mat > black_mat) ? WHITE : BLACK;
        Color losing = ~winning;
        int losing_king = board.king_square(losing);
        int winning_king = board.king_square(winning);
        
        // Distance to center penalty for losing king
        int lk_f = file_of(losing_king), lk_r = rank_of(losing_king);
        int center_dist = std::max(std::abs(lk_f - 3), std::abs(lk_r - 3)) + std::max(std::abs(lk_f - 4), std::abs(lk_r - 4));
        
        // Closer kings bonus
        int king_dist = std::abs(file_of(winning_king) - lk_f) + std::abs(rank_of(winning_king) - lk_r);
        
        int bonus = center_dist * 10 + (14 - king_dist) * 5;
        score += (winning == WHITE) ? bonus : -bonus;
    }
    
    return score;
}

int evaluate_fast(const Board& board) {
    int score = board.material_count(WHITE) - board.material_count(BLACK);
    return score;
}

// ============= Search =============
ChessSearch::ChessSearch(size_t tt_size) : tt_size_(tt_size), nodes_(0) {
    tt_.resize(tt_size);
}

void ChessSearch::clear_tt() {
    for (auto& e : tt_) {
        e = {0, 0, 0, Move()};
    }
}

int ChessSearch::quiescence(Board& board, int alpha, int beta, int ply) {
    nodes_++;
    
    int stand_pat = evaluate(board);
    if (board.side_to_move() == BLACK) stand_pat = -stand_pat;
    
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;
    
    auto captures = board.legal_captures();
    
    // Sort by MVV-LVA
    std::sort(captures.begin(), captures.end(), [&board](const Move& a, const Move& b) {
        int va = PIECE_VALUES.get(type_of(board.piece_at(a.to())));
        int vb = PIECE_VALUES.get(type_of(board.piece_at(b.to())));
        return va > vb;
    });
    
    for (const auto& m : captures) {
        if (!board.make_move(m)) continue;
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmake_move(m);
        
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    
    return alpha;
}

int ChessSearch::alphabeta(Board& board, int depth, int alpha, int beta, int ply) {
    if (depth <= 0) return quiescence(board, alpha, beta, ply);
    
    nodes_++;
    
    // TT lookup
    uint64_t key = board.hash();
    size_t idx = key % tt_size_;
    auto& tt_entry = tt_[idx];
    if (std::get<0>(tt_entry) == key && std::get<1>(tt_entry) >= depth) {
        return std::get<2>(tt_entry);
    }
    
    auto moves = board.legal_moves();
    if (moves.empty()) {
        if (board.is_in_check()) return -MATE_VALUE + ply;
        return 0;
    }
    
    // Sort moves: TT move first, then captures
    Move tt_move = std::get<3>(tt_entry);
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        if (a == tt_move) return true;
        if (b == tt_move) return false;
        bool a_cap = board.piece_at(a.to()) != NO_PIECE;
        bool b_cap = board.piece_at(b.to()) != NO_PIECE;
        return a_cap > b_cap;
    });
    
    Move best_move = moves[0];
    int best_score = -MATE_VALUE;
    
    for (size_t i = 0; i < moves.size(); i++) {
        const auto& m = moves[i];
        if (!board.make_move(m)) continue;
        
        int score;
        if (i == 0) {
            score = -alphabeta(board, depth - 1, -beta, -alpha, ply + 1);
        } else {
            // LMR
            int reduction = 0;
            if (depth >= 3 && i >= 4 && board.piece_at(m.to()) == NO_PIECE && !board.is_in_check())
                reduction = 1;
            
            score = -alphabeta(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && (score < beta || reduction > 0))
                score = -alphabeta(board, depth - 1, -beta, -alpha, ply + 1);
        }
        
        board.unmake_move(m);
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }
    
    // TT store
    tt_[idx] = {key, depth, best_score, best_move};
    
    return best_score;
}

SearchResult ChessSearch::search(Board& board, int depth) {
    SearchResult result;
    nodes_ = 0;
    
    auto moves = board.legal_moves();
    if (moves.empty()) {
        result.depth = depth;
        result.nodes = nodes_;
        return result;
    }
    
    int alpha = -MATE_VALUE;
    int beta = MATE_VALUE;
    Move best_move = moves[0];
    int best_score = -MATE_VALUE;
    
    for (const auto& m : moves) {
        if (!board.make_move(m)) continue;
        int score = -alphabeta(board, depth - 1, -beta, -alpha, 1);
        board.unmake_move(m);
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
        }
        if (score > alpha) alpha = score;
    }
    
    result.best_move = best_move;
    result.score = best_score;
    result.depth = depth;
    result.nodes = nodes_;
    
    return result;
}

SearchResult ChessSearch::search_iterative(Board& board, int max_depth) {
    SearchResult result;
    for (int d = 1; d <= max_depth; d++) {
        result = search(board, d);
    }
    return result;
}

// ============= Bot =============
ChessBot::ChessBot(int depth) : search_depth(depth) {}

std::string ChessBot::get_fen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return board_.fen();
}

void ChessBot::set_fen(const std::string& fen) {
    std::lock_guard<std::mutex> lock(mutex_);
    board_.set_fen(fen);
}

void ChessBot::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    board_.reset();
    search_.clear_tt();
}

bool ChessBot::make_move(const std::string& uci) {
    std::lock_guard<std::mutex> lock(mutex_);
    Move m = Move::from_uci(uci);
    if (m.is_null()) return false;
    return board_.make_move(m);
}

std::string ChessBot::get_best_move() {
    return get_best_move(search_depth);
}

std::string ChessBot::get_best_move(int depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto result = search_.search_iterative(board_, depth);
    return result.best_move.uci();
}

bool ChessBot::is_game_over() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return board_.is_game_over();
}

std::string ChessBot::get_result() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (board_.is_checkmate()) {
        return (board_.side_to_move() == WHITE) ? "0-1" : "1-0";
    }
    if (board_.is_draw()) return "1/2-1/2";
    return "*";
}

} // namespace chess
