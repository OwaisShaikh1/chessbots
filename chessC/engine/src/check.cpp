
#include "check.h"
#include "move_generator.h"
#include "board.h"

namespace engine {

// Returns true if the king of the given color is in check
bool is_king_in_check(const Board& board, const AttackersTable& attackers, Color color) {
    int king_sq = board.king_square(color);
    Color opp = (color == Color::White) ? Color::Black : Color::White;
    return attackers.isSquareAttacked(static_cast<int>(opp), king_sq);
}

// Updates the attackers table for both sides
inline void update_attackers(const Board& board, AttackersTable& attackers) {
    attackers.clear();
    for (int color = 0; color < 2; ++color) {
        Color side = (color == 0) ? Color::White : Color::Black;
        auto moves = MoveGenerator::generate_all(board, side);
        for (const auto& move : moves) {
            int sq = move.to_row * 8 + move.to_col;
            attackers.addAttackedSquare(color, sq);
        }
    }
}

} // namespace engine
