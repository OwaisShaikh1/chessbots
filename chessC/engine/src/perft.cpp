#include "../include/perft.h"
#include "../include/legal_moves.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

namespace Perft {

// ─── Core perft ───────────────────────────────────────────────────────────

uint64_t perft(Board& b, int depth) {
    if (depth == 0) return 1ULL;

    MoveList legal;
    generate_legal_moves(b, legal);

    if (depth == 1) return static_cast<uint64_t>(legal.count);

    uint64_t nodes = 0;
    for (int i = 0; i < legal.count; i++) {
        b.make_move(legal.moves[i]);
        nodes += perft(b, depth - 1);
        b.undo_move(legal.moves[i]);
    }
    return nodes;
}

// ─── Perft divide ─────────────────────────────────────────────────────────

void perft_divide(Board& b, int depth) {
    MoveList legal;
    generate_legal_moves(b, legal);

    uint64_t total = 0;
    std::cout << "\nPerft divide (depth " << depth << "):\n";

    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        b.make_move(m);
        uint64_t nodes = perft(b, depth - 1);
        b.undo_move(m);

        // Print in UCI notation
        char from_file = 'a' + file_of(m.from());
        char from_rank = '1' + rank_of(m.from());
        char to_file   = 'a' + file_of(m.to());
        char to_rank   = '1' + rank_of(m.to());
        std::cout << from_file << from_rank << to_file << to_rank;
        if (m.is_promotion()) {
            static const char promo_chars[] = "  nbrq";
            std::cout << promo_chars[m.promo_piece()];
        }
        std::cout << ": " << nodes << "\n";
        total += nodes;
    }
    std::cout << "\nTotal: " << total << "\n\n";
}

// ─── Standard test suite ──────────────────────────────────────────────────

struct PerftTest {
    const char*  fen;
    int          depth;
    uint64_t     expected;
    const char*  name;
};

static const PerftTest TESTS[] = {
    // Starting position
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20,       "Start depth 1" },
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400,      "Start depth 2" },
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902,     "Start depth 3" },
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281,   "Start depth 4" },
    // Kiwipete — a rich position with many special moves
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        1, 48,       "Kiwipete depth 1" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        2, 2039,     "Kiwipete depth 2" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        3, 97862,    "Kiwipete depth 3" },
    // En passant test position
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        1, 14,       "EP/pin test depth 1" },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        3, 2812,     "EP/pin test depth 3" },
};

void run_tests() {
    std::cout << "\n=== Perft Test Suite ===\n\n";
    int passed = 0, failed = 0;

    for (auto& t : TESTS) {
        Board b;
        b.load_fen(t.fen);

        auto start = std::chrono::high_resolution_clock::now();
        uint64_t result = perft(b, t.depth);
        auto end   = std::chrono::high_resolution_clock::now();
        double ms  = std::chrono::duration<double, std::milli>(end - start).count();

        bool ok = (result == t.expected);
        if (ok) passed++; else failed++;

        std::cout << std::left << std::setw(28) << t.name
                  << " depth=" << t.depth
                  << " nodes=" << std::setw(10) << result
                  << (ok ? " [PASS]" : " [FAIL]");
        if (!ok) std::cout << "  expected=" << t.expected;
        std::cout << "  (" << std::fixed << std::setprecision(1) << ms << "ms)\n";
    }

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n\n";
}

} // namespace Perft
