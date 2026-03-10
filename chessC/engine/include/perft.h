#pragma once

#include "board.h"
#include <cstdint>

// ─── Perft ────────────────────────────────────────────────────────────────
//   Counts leaf nodes at a given depth to validate move generation.

namespace Perft {

uint64_t perft(Board& b, int depth);
void     perft_divide(Board& b, int depth);

// Run standard test suite and print pass/fail
void run_tests();

} // namespace Perft
