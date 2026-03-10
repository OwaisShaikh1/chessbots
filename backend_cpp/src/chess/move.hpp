#pragma once

#include "types.hpp"
#include <string>

namespace chess {

class Move {
public:
    Move() : data_(0) {}
    Move(Square from, Square to, PieceType promotion = NO_PIECE_TYPE)
        : data_((from) | (to << 6) | (promotion << 12)) {}
    
    Square from() const { return Square(data_ & 0x3F); }
    Square to() const { return Square((data_ >> 6) & 0x3F); }
    PieceType promotion() const { return PieceType((data_ >> 12) & 0x7); }
    
    bool is_null() const { return data_ == 0; }
    bool is_promotion() const { return promotion() != NO_PIECE_TYPE; }
    
    bool operator==(const Move& other) const { return data_ == other.data_; }
    bool operator!=(const Move& other) const { return data_ != other.data_; }
    
    // UCI string representation
    std::string uci() const {
        if (is_null()) return "0000";
        std::string s = square_to_string(from()) + square_to_string(to());
        if (is_promotion()) {
            constexpr char promo_chars[] = " nbrq";
            s += promo_chars[promotion()];
        }
        return s;
    }
    
    // Parse from UCI string
    static Move from_uci(const std::string& uci) {
        if (uci.length() < 4) return Move();
        Square from = string_to_square(uci.substr(0, 2));
        Square to = string_to_square(uci.substr(2, 2));
        PieceType promo = NO_PIECE_TYPE;
        if (uci.length() >= 5) {
            char p = uci[4];
            if (p == 'n') promo = KNIGHT;
            else if (p == 'b') promo = BISHOP;
            else if (p == 'r') promo = ROOK;
            else if (p == 'q') promo = QUEEN;
        }
        return Move(from, to, promo);
    }
    
    uint16_t raw() const { return data_; }

private:
    uint16_t data_;
};

// Null move constant
const Move NULL_MOVE = Move();

} // namespace chess
