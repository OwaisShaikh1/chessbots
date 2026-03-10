#include "../include/tt.h"
#include <cstring>
#include <stdexcept>

TranspositionTable TT(128);

TranspositionTable::TranspositionTable(size_t mb) {
    num_entries_ = (mb * 1024 * 1024) / sizeof(TTEntry);
    table_ = new TTEntry[num_entries_];
    clear();
}

TranspositionTable::~TranspositionTable() {
    delete[] table_;
}

void TranspositionTable::clear() {
    std::memset(table_, 0, num_entries_ * sizeof(TTEntry));
}

void TranspositionTable::store(Key key, int depth, int score, TTFlag flag, Move best_move) {
    size_t idx  = key % num_entries_;
    TTEntry& e  = table_[idx];

    // Always replace with deeper or same-depth entries
    if (e.key == key && e.depth > static_cast<uint8_t>(depth) && flag != TT_EXACT) return;

    e.key       = key;
    e.score     = static_cast<int16_t>(score);
    e.depth     = static_cast<uint8_t>(depth);
    e.flag      = flag;
    e.best_move = best_move;
}

TTEntry* TranspositionTable::probe(Key key) {
    size_t idx  = key % num_entries_;
    TTEntry* e  = &table_[idx];
    if (e->key == key) return e;
    return nullptr;
}
