#include "../include/zobrist.h"
#include <random>

namespace Zobrist {

static Key piece_keys[PIECE_NB][SQUARE_NB];
static Key side_key_val;
static Key castling_keys[CASTLING_RIGHTS_NB];
static Key ep_keys[8]; // one per file

void init() {
    std::mt19937_64 rng(0x1234567890ABCDEFULL);
    auto rand64 = [&]() { return rng(); };

    for (int p = 0; p < PIECE_NB; p++)
        for (int s = 0; s < SQUARE_NB; s++)
            piece_keys[p][s] = rand64();

    side_key_val = rand64();

    for (int cr = 0; cr < CASTLING_RIGHTS_NB; cr++)
        castling_keys[cr] = rand64();

    for (int f = 0; f < 8; f++)
        ep_keys[f] = rand64();
}

Key piece_key(Piece p, Square s)     { return piece_keys[p][s]; }
Key side_key()                        { return side_key_val; }
Key castling_key(CastlingRights cr)   { return castling_keys[cr]; }
Key ep_key(int file)                  { return ep_keys[file]; }

} // namespace Zobrist
