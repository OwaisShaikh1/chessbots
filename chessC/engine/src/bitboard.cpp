#include "../include/bitboard.h"
#include <iostream>
#include <iomanip>

void print_bitboard(Bitboard b) {
    std::cout << "\n  +-----------------+\n";
    for (int rank = 7; rank >= 0; rank--) {
        std::cout << (rank + 1) << " | ";
        for (int file = 0; file < 8; file++) {
            Square sq = make_square(file, rank);
            std::cout << (get_bit(b, sq) ? '1' : '.') << ' ';
        }
        std::cout << "|\n";
    }
    std::cout << "  +-----------------+\n";
    std::cout << "    a b c d e f g h\n\n";
}
