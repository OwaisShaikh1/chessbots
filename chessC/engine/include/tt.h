#pragma once

#include "types.h"
#include <cstdint>
#include <cstddef>

// ─── Transposition Table ──────────────────────────────────────────────────

enum TTFlag : uint8_t {
    TT_NONE  = 0,
    TT_EXACT = 1,   // Exact score
    TT_ALPHA = 2,   // Upper bound (failed low)
    TT_BETA  = 3,   // Lower bound (cutoff)
};

struct TTEntry {
    Key     key       = 0;
    int16_t score     = 0;
    Move    best_move;
    uint8_t depth     = 0;
    TTFlag  flag      = TT_NONE;
};

class TranspositionTable {
public:
    explicit TranspositionTable(size_t mb = 128);
    ~TranspositionTable();

    void     clear();
    void     store(Key key, int depth, int score, TTFlag flag, Move best_move);
    TTEntry* probe(Key key);

    size_t size() const { return num_entries_; }

private:
    TTEntry* table_;
    size_t   num_entries_;
};

// Global TT instance
extern TranspositionTable TT;
