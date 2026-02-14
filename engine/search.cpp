#include "search.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

// LMR reduction table (precomputed)
static int lmrTable[64][64]; // [depth][moveIndex]
static bool lmrInit = false;

static void initLMR() {
    if (lmrInit) return;
    for (int d = 0; d < 64; d++)
        for (int m = 0; m < 64; m++)
            lmrTable[d][m] = (d > 0 && m > 0) ? (int)(0.75 + log(d) * log(m) / 2.5) : 0;
    lmrInit = true;
}

// ========================================================================
//  Init / Destroy
// ========================================================================
void Search::init(size_t ttSizeMB) {
    initLMR();
    size_t bytes = ttSizeMB * 1024ULL * 1024ULL;
    ttEntries = bytes / sizeof(TTEntry);
    // Round down to power of 2
    size_t p = 1;
    while (p * 2 <= ttEntries) p *= 2;
    ttEntries = p;
    ttMask = ttEntries - 1;

    tt = new TTEntry[ttEntries];
    memset(tt, 0, ttEntries * sizeof(TTEntry));

    clearHistory();
    stopped = false;
    rootBest = MOVE_NONE;
    rootScore = 0;
}

void Search::destroy() {
    delete[] tt;
    tt = nullptr;
}

void Search::clearHistory() {
    memset(killers, 0, sizeof(killers));
    memset(history, 0, sizeof(history));
    memset(counterMove, 0, sizeof(counterMove));
}

// ========================================================================
//  Time helpers
// ========================================================================
int64_t Search::elapsed() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
}

void Search::checkTime() {
    if (isInfinite) return;
    if (elapsed() >= maxMs) stopped = true;
}

// ========================================================================
//  TT helpers
// ========================================================================
TTEntry* Search::probeTT(uint64_t key) const {
    TTEntry* e = &tt[key & ttMask];
    if (e->flag != TT_NONE && e->key == key) return e;
    return nullptr;
}

void Search::storeTT(uint64_t key, int score, Move bestMove, int depth, uint8_t flag) {
    TTEntry* e = &tt[key & ttMask];
    // Replace if: empty, same position, or shallower depth
    if (e->flag == TT_NONE || e->key == key || e->depth <= depth) {
        e->key      = key;
        e->score    = (int16_t)score;
        e->bestMove = bestMove;
        e->depth    = (int8_t)depth;
        e->flag     = flag;
    }
}

int Search::scoreToTT(int score, int ply) const {
    if (score >= SCORE_MATE - MAX_PLY) return score + ply;
    if (score <= -SCORE_MATE + MAX_PLY) return score - ply;
    return score;
}

int Search::scoreFromTT(int score, int ply) const {
    if (score >= SCORE_MATE - MAX_PLY) return score - ply;
    if (score <= -SCORE_MATE + MAX_PLY) return score + ply;
    return score;
}

// ========================================================================
//  Move ordering
// ========================================================================
int Search::scoreMove(Move m, int ply, Move hashMove) const {
    if (m == hashMove) return 1000000;

    int from = moveFrom(m);
    int to   = moveTo(m);
    int target = board.squares[to];
    int piece  = board.squares[from];

    // Den entry is a winning move - highest priority after hash
    int color = piece > 0 ? LIGHT : DARK;
    int oppDen = (color == LIGHT) ? DEN_DARK_SQ : DEN_LIGHT_SQ;
    if (to == oppDen) return 900000;

    // Captures: MVV-LVA
    if (target != 0) {
        int victimVal  = MATERIAL_VAL[abs(target)];
        int attackerVal = MATERIAL_VAL[abs(piece)];
        return 500000 + victimVal * 10 - attackerVal;
    }

    // Killers
    if (ply < MAX_PLY) {
        if (m == killers[ply][0]) return 400000;
        if (m == killers[ply][1]) return 399000;
    }

    // History heuristic
    int col = piece > 0 ? LIGHT : DARK;
    return history[col][from][to];
}

void Search::orderMoves(Move* moves, int count, int ply, Move hashMove) const {
    (void)moves; (void)count; (void)ply; (void)hashMove;
}

void Search::pickBest(Move* moves, int* scores, int count, int cur) const {
    int bestIdx = cur;
    int bestScore = scores[cur];
    for (int i = cur + 1; i < count; i++) {
        if (scores[i] > bestScore) {
            bestScore = scores[i];
            bestIdx = i;
        }
    }
    if (bestIdx != cur) {
        std::swap(moves[cur], moves[bestIdx]);
        std::swap(scores[cur], scores[bestIdx]);
    }
}

// ========================================================================
//  Quiescence search
// ========================================================================
int Search::quiescence(int alpha, int beta, int ply) {
    nodes++;
    if (ply > selDepth) selDepth = ply;

    // Check game over
    int gameRes = board.checkGameOver();
    if (gameRes != 0) {
        return gameRes > 0 ? (SCORE_MATE - ply) : -(SCORE_MATE - ply);
    }

    // Stand pat
    int standPat = board.evaluate();
    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;

    // Generate captures only
    Move moves[MAX_MOVES];
    int count = 0;
    board.generateCaptures(moves, count);

    // Score and sort captures
    int scores[MAX_MOVES];
    for (int i = 0; i < count; i++)
        scores[i] = scoreMove(moves[i], ply, MOVE_NONE);

    for (int i = 0; i < count; i++) {
        pickBest(moves, scores, count, i);

        // Delta pruning
        int target = board.squares[moveTo(moves[i])];
        if (target != 0) {
            int gain = MATERIAL_VAL[abs(target)];
            if (standPat + gain + 200 < alpha) continue;
        }

        board.makeMove(moves[i]);
        int score = -quiescence(-beta, -alpha, ply + 1);
        board.unmakeMove();

        if (score > alpha) {
            alpha = score;
            if (score >= beta) return beta;
        }
    }

    return alpha;
}

// ========================================================================
//  Alpha-Beta with PVS, LMR, NMP, etc.
// ========================================================================
int Search::alphaBeta(int depth, int alpha, int beta, int ply, bool isPV, bool allowNull) {
    pvLen[ply] = ply;

    // Game over?
    int gameRes = board.checkGameOver();
    if (gameRes != 0) {
        return gameRes > 0 ? (SCORE_MATE - ply) : -(SCORE_MATE - ply);
    }

    // Repetition
    if (ply > 0 && board.isRepetition()) return SCORE_DRAW;

    // 200 half-move draw
    if (board.halfmove >= 200) return SCORE_DRAW;

    // Max ply
    if (ply >= MAX_PLY - 1) return board.evaluate();

    // Leaf: quiescence
    if (depth <= 0) return quiescence(alpha, beta, ply);

    nodes++;
    if (ply > selDepth) selDepth = ply;

    // Check time every 4096 nodes
    if ((nodes & 4095) == 0) checkTime();
    if (stopped) return 0;

    // ---- TT probe ----
    Move hashMove = MOVE_NONE;
    TTEntry* tte = probeTT(board.hash);
    if (tte) {
        hashMove = tte->bestMove;
        if (!isPV && tte->depth >= depth) {
            int ttScore = scoreFromTT(tte->score, ply);
            if (tte->flag == TT_EXACT) return ttScore;
            if (tte->flag == TT_BETA  && ttScore >= beta)  return ttScore;
            if (tte->flag == TT_ALPHA && ttScore <= alpha) return ttScore;
        }
    }

    int staticEval = board.evaluate();
    bool inDanger = false;

    // Are we in danger? (opponent has piece 1-2 steps from our den)
    {
        int opp = 1 - board.sideToMove;
        int ourDenSq = (board.sideToMove == LIGHT) ? DEN_LIGHT_SQ : DEN_DARK_SQ;
        for (int rk = 1; rk <= 8; rk++) {
            int sq = board.pieceSq[opp][rk];
            if (sq < 0) continue;
            int dr = abs(sqRow(sq) - sqRow(ourDenSq));
            int dc = abs(sqCol(sq) - sqCol(ourDenSq));
            if (dr + dc <= 2) { inDanger = true; break; }
        }
    }

    // ---- Razoring ----
    if (!isPV && !inDanger && depth <= 2 && staticEval + 300 * depth <= alpha
        && abs(alpha) < SCORE_MATE - MAX_PLY) {
        int qScore = quiescence(alpha, beta, ply);
        if (qScore <= alpha) return qScore;
    }

    // ---- Reverse futility pruning ----
    if (!isPV && !inDanger && depth <= 3
        && staticEval - 120 * depth >= beta
        && abs(beta) < SCORE_MATE - MAX_PLY) {
        return staticEval - 120 * depth;
    }

    // ---- Null move pruning ----
    if (!isPV && allowNull && depth >= 3
        && !inDanger
        && staticEval >= beta
        && board.pieceCount[board.sideToMove] >= 2
        && abs(beta) < SCORE_MATE - MAX_PLY) {

        int R = 3 + depth / 6;
        if (R > depth - 1) R = depth - 1;
        board.makeNullMove();
        int nullScore = -alphaBeta(depth - 1 - R, -beta, -beta + 1, ply + 1, false, false);
        board.unmakeNullMove();

        if (stopped) return 0;
        if (nullScore >= beta) {
            if (nullScore >= SCORE_MATE - MAX_PLY) nullScore = beta;
            return nullScore;
        }
    }

    // ---- Internal iterative deepening ----
    if (isPV && hashMove == MOVE_NONE && depth >= 4) {
        alphaBeta(depth - 2, alpha, beta, ply, true, false);
        if (stopped) return 0;
        tte = probeTT(board.hash);
        if (tte) hashMove = tte->bestMove;
    }

    // ---- Generate moves ----
    Move moves[MAX_MOVES];
    int moveCount = 0;
    board.generateMoves(moves, moveCount);

    if (moveCount == 0) return -(SCORE_MATE - ply);

    // Score moves
    int scores[MAX_MOVES];
    for (int i = 0; i < moveCount; i++)
        scores[i] = scoreMove(moves[i], ply, hashMove);

    int bestScore = -SCORE_INF;
    Move bestMove = MOVE_NONE;
    uint8_t ttFlag = TT_ALPHA;
    int movesSearched = 0;

    for (int i = 0; i < moveCount; i++) {
        pickBest(moves, scores, moveCount, i);
        Move m = moves[i];
        int from = moveFrom(m);
        int to   = moveTo(m);
        bool isCapture = (board.squares[to] != 0);

        // Check if this move enters opponent's den (immediate win)
        int piece = board.squares[from];
        int color = piece > 0 ? LIGHT : DARK;
        int oppDen = (color == LIGHT) ? DEN_DARK_SQ : DEN_LIGHT_SQ;
        if (to == oppDen) {
            pv[ply][ply] = m;
            pvLen[ply] = ply + 1;
            storeTT(board.hash, scoreToTT(SCORE_MATE - ply, ply), m, depth, TT_EXACT);
            return SCORE_MATE - ply;
        }

        // ---- Extensions ----
        int extension = 0;
        // Extend when opponent piece is very close to our den
        if (inDanger) extension = 1;
        // Extend captures of high-value pieces
        if (isCapture && abs(board.squares[to]) >= TIGER) extension = std::max(extension, 1);

        int newDepth = depth - 1 + extension;

        // ---- Futility pruning ----
        if (!isPV && !inDanger && depth <= 2 && !isCapture
            && movesSearched > 0
            && abs(alpha) < SCORE_MATE - MAX_PLY) {
            int futilityMargin = depth * 150;
            if (staticEval + futilityMargin <= alpha) {
                movesSearched++;
                continue;
            }
        }

        board.makeMove(m);

        int score;

        if (movesSearched == 0) {
            // First move: full window search
            score = -alphaBeta(newDepth, -beta, -alpha, ply + 1, isPV, true);
        } else {
            // ---- LMR ----
            int reduction = 0;
            if (depth >= 3 && movesSearched >= 2 && !isCapture && !inDanger) {
                reduction = lmrTable[std::min(depth, 63)][std::min(movesSearched, 63)];
                // Reduce less in PV nodes
                if (isPV) reduction = std::max(0, reduction - 1);
                // Don't reduce into negative
                reduction = std::min(reduction, newDepth - 1);
                if (reduction < 0) reduction = 0;
            }

            // Step 1: Zero-window search with reduction
            score = -alphaBeta(newDepth - reduction, -alpha - 1, -alpha, ply + 1, false, true);

            // Step 2: If reduced and failed high, re-search without reduction (still zero window)
            if (score > alpha && reduction > 0) {
                score = -alphaBeta(newDepth, -alpha - 1, -alpha, ply + 1, false, true);
            }

            // Step 3: If PV node and failed high, full window re-search
            if (isPV && score > alpha && score < beta) {
                score = -alphaBeta(newDepth, -beta, -alpha, ply + 1, true, true);
            }
        }

        board.unmakeMove();

        if (stopped) return 0;

        movesSearched++;

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;

            if (score > alpha) {
                alpha = score;
                ttFlag = TT_EXACT;

                // Update PV
                pv[ply][ply] = m;
                for (int j = ply + 1; j < pvLen[ply + 1]; j++)
                    pv[ply][j] = pv[ply + 1][j];
                pvLen[ply] = pvLen[ply + 1];

                if (score >= beta) {
                    ttFlag = TT_BETA;

                    // Update killers and history for quiet moves
                    if (!isCapture && ply < MAX_PLY) {
                        if (m != killers[ply][0]) {
                            killers[ply][1] = killers[ply][0];
                            killers[ply][0] = m;
                        }
                        // History bonus
                        int bonus = depth * depth;
                        history[board.sideToMove][from][to] += bonus;
                        // Age history periodically
                        if (history[board.sideToMove][from][to] > 100000) {
                            for (auto& row : history)
                                for (auto& col : row)
                                    for (auto& v : col)
                                        v /= 2;
                        }
                    }
                    break;
                }
            }
        }
    }

    // Store TT
    storeTT(board.hash, scoreToTT(bestScore, ply), bestMove, depth, ttFlag);

    return bestScore;
}

// ========================================================================
//  Iterative deepening + aspiration windows
// ========================================================================
Move Search::think(int maxDepth, int64_t moveTimeMs, bool infinite) {
    startTime = std::chrono::steady_clock::now();
    stopped = false;
    isInfinite = infinite;
    nodes = 0;
    selDepth = 0;

    if (maxDepth <= 0) maxDepth = 100;
    if (moveTimeMs <= 0 && !infinite) moveTimeMs = 5000;

    allocatedMs = moveTimeMs;
    maxMs = infinite ? 999999999LL : (int64_t)(moveTimeMs * 1.5);
    timeManaged = !infinite;

    // Clear killers (keep history across searches for strength)
    memset(killers, 0, sizeof(killers));

    rootBest = MOVE_NONE;
    rootScore = 0;

    int prevScore = 0;

    for (int depth = 1; depth <= maxDepth; depth++) {
        selDepth = 0;
        int alpha, beta;

        // Aspiration window
        if (depth >= 5) {
            int window = 40;
            alpha = prevScore - window;
            beta  = prevScore + window;
        } else {
            alpha = -SCORE_INF;
            beta  =  SCORE_INF;
        }

        int score = alphaBeta(depth, alpha, beta, 0, true, true);

        // Aspiration window re-search with wider windows
        if (!stopped && (score <= alpha || score >= beta)) {
            // Widen by 3x
            int window2 = 150;
            alpha = std::max(-SCORE_INF, prevScore - window2);
            beta  = std::min( SCORE_INF, prevScore + window2);
            score = alphaBeta(depth, alpha, beta, 0, true, true);

            // If still fails, full window
            if (!stopped && (score <= alpha || score >= beta)) {
                score = alphaBeta(depth, -SCORE_INF, SCORE_INF, 0, true, true);
            }
        }

        if (stopped && depth > 1) break;

        // Update root best
        if (pvLen[0] > 0) {
            rootBest = pv[0][0];
            rootScore = score;
        }
        prevScore = score;

        // Print info
        int64_t ms = elapsed();
        int64_t nps = (ms > 0) ? (nodes * 1000 / ms) : nodes;

        std::printf("info depth %d seldepth %d score ", depth, selDepth);
        if (abs(score) >= SCORE_MATE - MAX_PLY) {
            int matePly = SCORE_MATE - abs(score);
            int mateIn = (matePly + 1) / 2;
            if (score > 0) std::printf("mate %d", mateIn);
            else std::printf("mate -%d", mateIn);
        } else {
            std::printf("cp %d", score);
        }

        std::printf(" nodes %lld nps %lld time %lld pv",
                     (long long)nodes, (long long)nps, (long long)ms);
        for (int i = 0; i < pvLen[0]; i++)
            std::printf(" %s", moveToStr(pv[0][i]).c_str());
        std::printf("\n");
        fflush(stdout);

        // Time: don't start new iteration if we've used >40% of time
        if (timeManaged && ms >= allocatedMs * 2 / 5) break;

        // Proven mate at this depth - stop
        if (abs(score) >= SCORE_MATE - depth) break;
    }

    return rootBest;
}

void Search::stop() {
    stopped = true;
}
