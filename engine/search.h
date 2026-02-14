#pragma once
#include "board.h"
#include <chrono>

// ---- Transposition Table ----
constexpr uint8_t TT_NONE  = 0;
constexpr uint8_t TT_EXACT = 1;
constexpr uint8_t TT_ALPHA = 2; // upper bound (fail-low)
constexpr uint8_t TT_BETA  = 3; // lower bound (fail-high)

struct TTEntry {
    uint64_t key;
    int16_t  score;
    Move     bestMove;
    int8_t   depth;
    uint8_t  flag;
};

class Search {
public:
    Board board;

    void init(size_t ttSizeMB = 128);
    void destroy();

    // Returns best move. Output info lines to stdout.
    Move think(int maxDepth, int64_t moveTimeMs, bool infinite);
    void stop();
    void clearHistory();

private:
    // Transposition table
    TTEntry* tt;
    size_t   ttMask;
    size_t   ttEntries;

    // Search state
    bool     stopped;
    int64_t  nodes;
    int      selDepth;

    // Time management
    std::chrono::steady_clock::time_point startTime;
    int64_t  allocatedMs;
    int64_t  maxMs;
    bool     timeManaged;
    bool     isInfinite;

    // Move ordering
    Move     killers[MAX_PLY][2];
    int      history[2][NUM_SQ][NUM_SQ];
    int      counterMove[NUM_SQ][NUM_SQ]; // indexed by prev from/to

    // Principal variation
    Move     pv[MAX_PLY][MAX_PLY];
    int      pvLen[MAX_PLY];

    // Root best
    Move     rootBest;
    int      rootScore;

    // Internal methods
    int  alphaBeta(int depth, int alpha, int beta, int ply, bool isPV, bool allowNull);
    int  quiescence(int alpha, int beta, int ply);
    void checkTime();
    int64_t elapsed() const;

    void orderMoves(Move* moves, int count, int ply, Move hashMove) const;
    int  scoreMove(Move m, int ply, Move hashMove) const;
    void pickBest(Move* moves, int* scores, int count, int cur) const;

    TTEntry* probeTT(uint64_t key) const;
    void     storeTT(uint64_t key, int score, Move bestMove, int depth, uint8_t flag);
    int      scoreToTT(int score, int ply) const;
    int      scoreFromTT(int score, int ply) const;
};
