#include "../include/board.h"

#include <cctype>
#include <sstream>

Board::Board() {
    clear();
    setup_start_position();
}

const Piece& Board::at(int row, int col) const {
    return squares_[row][col];
}

bool Board::is_empty(int row, int col) const {
    return at(row, col).type == PieceType::None;
}

bool Board::has_enemy_piece(int row, int col, Color my_color) const {
    const Piece piece = at(row, col);
    return piece.type != PieceType::None && piece.color != my_color;
}

bool Board::has_friendly_piece(int row, int col, Color my_color) const {
    const Piece piece = at(row, col);
    return piece.type != PieceType::None && piece.color == my_color;
}

void Board::apply_move(const Move& move) {
    if (!is_valid_square(move.from_row, move.from_col) || !is_valid_square(move.to_row, move.to_col)) {
        return;
    }

    Piece moving_piece = squares_[move.from_row][move.from_col];
    if (moving_piece.type == PieceType::None) {
        return;
    }

    if (moving_piece.type == PieceType::Pawn && move.promotion != PieceType::None) {
        moving_piece.type = move.promotion;
    }

    squares_[move.to_row][move.to_col] = moving_piece;
    squares_[move.from_row][move.from_col] = Piece{};
}

Board Board::simulate_move(const Move& move) const {
    Board copy = *this;
    copy.apply_move(move);
    return copy;
}

void Board::clear() {
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            squares_[row][col] = Piece{};
        }
    }
}

void Board::setup_start_position() {
    for (int col = 0; col < 8; ++col) {
        squares_[1][col] = Piece{PieceType::Pawn, Color::White};
        squares_[6][col] = Piece{PieceType::Pawn, Color::Black};
    }

    squares_[0][0] = squares_[0][7] = Piece{PieceType::Rook, Color::White};
    squares_[7][0] = squares_[7][7] = Piece{PieceType::Rook, Color::Black};

    squares_[0][1] = squares_[0][6] = Piece{PieceType::Knight, Color::White};
    squares_[7][1] = squares_[7][6] = Piece{PieceType::Knight, Color::Black};

    squares_[0][2] = squares_[0][5] = Piece{PieceType::Bishop, Color::White};
    squares_[7][2] = squares_[7][5] = Piece{PieceType::Bishop, Color::Black};

    squares_[0][3] = Piece{PieceType::Queen, Color::White};
    squares_[7][3] = Piece{PieceType::Queen, Color::Black};

    squares_[0][4] = Piece{PieceType::King, Color::White};
    squares_[7][4] = Piece{PieceType::King, Color::Black};
}

void Board::load_from_fen(const std::string& fen) {
    clear();

    std::istringstream ss(fen);
    std::string placement;
    ss >> placement;
    if (placement.empty()) {
        setup_start_position();
        return;
    }

    int row = 7;
    int col = 0;
    for (char ch : placement) {
        if (ch == '/') {
            row--;
            col = 0;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch))) {
            col += ch - '0';
            continue;
        }

        if (!is_valid_square(row, col)) {
            continue;
        }

        Piece p;
        p.color = std::isupper(static_cast<unsigned char>(ch)) ? Color::White : Color::Black;

        switch (std::tolower(static_cast<unsigned char>(ch))) {
            case 'p': p.type = PieceType::Pawn; break;
            case 'n': p.type = PieceType::Knight; break;
            case 'b': p.type = PieceType::Bishop; break;
            case 'r': p.type = PieceType::Rook; break;
            case 'q': p.type = PieceType::Queen; break;
            case 'k': p.type = PieceType::King; break;
            default:  p.type = PieceType::None; p.color = Color::None; break;
        }

        squares_[row][col] = p;
        col++;
    }
}
