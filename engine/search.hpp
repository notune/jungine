#pragma once
#include "jungle.hpp"
#include <chrono>
#include <vector>

struct SearchResult {
    Move bestMove;
    int score;
    int nodes;
};

enum TTFlag { TT_EXACT, TT_ALPHA, TT_BETA };

struct TTEntry {
    uint64_t hash;
    Move move;
    int score;
    int depth;
    TTFlag flag;
};

class Search {
    bool stopSearch;
    int nodes;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    long long timeLimitMs;
    
    std::vector<TTEntry> tt;
    size_t ttMask;

    void recordTT(uint64_t hash, int depth, int score, TTFlag flag, Move bestMove);
    bool probeTT(uint64_t hash, int depth, int alpha, int beta, int& score, Move& bestMove);

public:
    Search(size_t ttSizeMB = 64);
    void stop();
    SearchResult search(Board& board, int maxDepth, long long timeLimitMs = -1);

private:
    int alphaBeta(Board& board, int depth, int alpha, int beta);
};
