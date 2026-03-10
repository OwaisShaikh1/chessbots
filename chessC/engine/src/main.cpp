#include "../include/attacks.h"
#include "../include/zobrist.h"
#include "../include/search.h"

// Forward declaration of UCI loop
void uci_loop();

int main(int argc, char* argv[]) {
    // ── Engine initialization ──
    Zobrist::init();     // Zobrist hash keys
    Attacks::init();     // Attack tables (magic bitboards)

    // Check for bench argument
    if (argc >= 2 && std::string(argv[1]) == "bench") {
        int depth = (argc >= 3) ? std::stoi(argv[2]) : 10;
        Board b;
        b.load_fen(START_FEN);
        SearchLimits lim;
        lim.depth = depth;
        SearchInfo info;
        ENGINE.search_position(b, lim, info);
        return 0;
    }

    // ── Start UCI loop ──
    uci_loop();

    return 0;
}
