#pragma once

enum class Color {
    White,
    Black,
    None
};

enum class PieceType {
    None,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

struct Piece {
    PieceType type = PieceType::None;
    Color color = Color::None;
};

inline bool is_valid_square(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}
