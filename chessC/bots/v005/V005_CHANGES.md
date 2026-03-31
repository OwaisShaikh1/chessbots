# v005 Changes (vs v004)

# v005_check_awareness_and_checkmate_weight

This document summarizes what was changed for the `v005` engine compared to `v004`.

## Engine evaluation updates

1. Modified `engine/src/evaluation.cpp`:
   - Added code to add the value difference for all possible promotions (queen, rook, bishop, knight) minus pawn value for each pawn on the 7th rank (white) or 2nd rank (black).
   - Added checkmate/stalemate detection in `engine/src/evaluation.cpp`:
     - If there are no legal moves, the evaluation returns +10000 or -10000 for checkmate (depending on which side is mated), or 0 for stalemate.

## Check awareness module

1. Added `AttackersTable` class in `engine/include/check.h` and `engine/src/check.cpp`.
2. Added `update_attackers(const Board&, AttackersTable&)` in `engine/include/check.h` and `engine/src/check.cpp`.
3. Added `is_king_in_check(const Board&, const AttackersTable&, Color)` in `engine/include/check.h` and `engine/src/check.cpp`.
4. Added `king_square(Color)` method to `Board` in `engine/include/board.h` and `engine/src/board.cpp`.

### Files updated/added

- Added check logic in engine/include/check.h
- Added check logic in engine/src/check.cpp
- Added check logic in engine/include/board.h
- Added check logic in engine/src/board.cpp