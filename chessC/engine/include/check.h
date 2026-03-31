#pragma once

#include "types.h"
#include <array>
#include <vector>

namespace engine {

// AttackersTable keeps track of all squares attacked by each side
class AttackersTable {
public:
    // 0 = white, 1 = black
    std::array<std::vector<int>, 2> attackedSquares;

    void clear() {
        attackedSquares[0].clear();
        attackedSquares[1].clear();
    }

    void addAttackedSquare(int color, int square) {
        attackedSquares[color].push_back(square);
    }

    bool isSquareAttacked(int color, int square) const {
        for (int sq : attackedSquares[color]) {
            if (sq == square) return true;
        }
        return false;
    }
};


// Returns true if the king of the given color is in check
bool is_king_in_check(const Board& board, const AttackersTable& attackers, Color color);

// Updates the attackers table for both sides
void update_attackers(const Board& board, AttackersTable& attackers);

} // namespace engine
