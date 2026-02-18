#ifndef TT_H
#define TT_H

#include "types.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace jungle {

enum class TTFlag : uint8_t {
    EXACT = 0,
    LOWER = 1,
    UPPER = 2
};

struct TTEntry {
    uint64_t key;
    int16_t score;
    uint8_t depth;
    TTFlag flag;
    Move bestMove;
    
    TTEntry() : key(0), score(0), depth(0), flag(TTFlag::EXACT), bestMove() {}
};

class TranspositionTable {
public:
    static constexpr size_t DEFAULT_SIZE = 64 * 1024 * 1024;
    
    TranspositionTable(size_t size = DEFAULT_SIZE) {
        resize(size);
    }
    
    void resize(size_t bytes) {
        size_t numEntries = bytes / sizeof(TTEntry);
        numEntries = std::max(numEntries, size_t(1024));
        
        numEntries = 1ULL << (63 - __builtin_clzll(numEntries));
        
        table_.resize(numEntries);
        mask_ = numEntries - 1;
        clear();
    }
    
    void clear() {
        std::fill(table_.begin(), table_.end(), TTEntry());
    }
    
    bool probe(uint64_t key, TTEntry& entry) const {
        size_t idx = key & mask_;
        if (table_[idx].key == key) {
            entry = table_[idx];
            return true;
        }
        return false;
    }
    
    void store(uint64_t key, int score, uint8_t depth, TTFlag flag, const Move& bestMove) {
        size_t idx = key & mask_;
        TTEntry& entry = table_[idx];
        
        if (entry.key != key || depth >= entry.depth || flag == TTFlag::EXACT) {
            entry.key = key;
            entry.score = static_cast<int16_t>(score);
            entry.depth = depth;
            entry.flag = flag;
            entry.bestMove = bestMove;
        }
    }
    
    Move getBestMove(uint64_t key) const {
        size_t idx = key & mask_;
        if (table_[idx].key == key) {
            return table_[idx].bestMove;
        }
        return Move();
    }
    
    size_t size() const {
        return table_.size();
    }
    
private:
    std::vector<TTEntry> table_;
    size_t mask_;
};

}

#endif
