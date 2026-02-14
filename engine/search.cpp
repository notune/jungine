#include "search.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ========================================================================
//  Init / Destroy
// ========================================================================
void Search::init(size_t ttSizeMB) {
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
    // Replace if: empty, same position, or shallower
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

    // Captures: MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
    if (target != 0) {
        int victimVal  = MATERIAL_VAL[abs(target)];
        int attackerVal = MATERIAL_VAL[abs(piece)];
        return 500000 + victimVal * 10 - attackerVal;
    }

    // Den entry is like a winning capture
    int color = piece > 0 ? LIGHT : DARK;
    int oppDen = (color == LIGHT) ? DEN_DARK_SQ : DEN_LIGHT_SQ;
    if (to == oppDen) return 900000;

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
    // Just score; we'll use pickBest during iteration
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

        // Delta pruning: skip if even the best capture can't improve alpha
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

    // ---- Razoring ----
    if (!isPV && depth <= 3 && staticEval + 300 * depth <= alpha) {
        if (depth == 1) return quiescence(alpha, beta, ply);
        int qScore = quiescence(alpha, beta, ply);
        if (qScore <= alpha) return qScore;
    }

    // ---- Reverse futility pruning (static null move) ----
    if (!isPV && depth <= 3 && !allowNull
        && staticEval - 120 * depth >= beta
        && abs(beta) < SCORE_MATE - MAX_PLY) {
        return staticEval - 120 * depth;
    }

    // ---- Null move pruning ----
    if (!isPV && allowNull && depth >= 3
        && staticEval >= beta
        && board.pieceCount[board.sideToMove] >= 2
        && abs(beta) < SCORE_MATE - MAX_PLY) {

        int R = 3 + depth / 6;
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

    if (moveCount == 0) return -(SCORE_MATE - ply); // no legal moves = loss

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
            // This is a winning move
            pv[ply][ply] = m;
            pvLen[ply] = ply + 1;
            return SCORE_MATE - ply;
        }

        board.makeMove(m);

        int score;
        int newDepth = depth - 1;

        // ---- Extensions ----
        // Den threat: if after our move, opponent has a piece 1 step from our den
        // (This is checked from opponent's perspective after our move)
        // Actually, extend if WE are threatening the opponent den nearby
        // or if opponent is threatening our den

        // ---- LMR ----
        if (movesSearched == 0) {
            // First move: full window
            score = -alphaBeta(newDepth, -beta, -alpha, ply + 1, isPV, true);
        } else {
            int reduction = 0;
            if (depth >= 3 && movesSearched >= 3 && !isCapture) {
                // LMR table-like reduction
                reduction = 1;
                if (movesSearched >= 6) reduction = 2;
                if (movesSearched >= 12 && depth >= 6) reduction = 3;
                reduction = std::min(reduction, newDepth - 1);
                if (reduction < 0) reduction = 0;
                // Reduce less in PV nodes
                if (isPV && reduction > 1) reduction--;
            }

            // Zero-window search with reduction
            score = -alphaBeta(newDepth - reduction, -alpha - 1, -alpha, ply + 1, false, true);

            // Re-search if needed
            if (score > alpha && (reduction > 0 || !isPV)) {
                score = -alphaBeta(newDepth, -beta, -alpha, ply + 1, isPV, true);
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

                    // Update killers for quiet moves
                    if (!isCapture && ply < MAX_PLY) {
                        if (m != killers[ply][0]) {
                            killers[ply][1] = killers[ply][0];
                            killers[ply][0] = m;
                        }
                        // History bonus
                        history[board.sideToMove][from][to] += depth * depth;
                        // Cap history values
                        if (history[board.sideToMove][from][to] > 100000) {
                            for (int a = 0; a < NUM_SQ; a++)
                                for (int b = 0; b < NUM_SQ; b++) {
                                    history[0][a][b] /= 2;
                                    history[1][a][b] /= 2;
                                }
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

    // Clear killers (keep history across moves for now)
    memset(killers, 0, sizeof(killers));

    rootBest = MOVE_NONE;
    rootScore = 0;

    int prevScore = 0;

    for (int depth = 1; depth <= maxDepth; depth++) {
        selDepth = 0;
        int alpha, beta;

        // Aspiration window
        if (depth >= 5) {
            alpha = prevScore - 50;
            beta  = prevScore + 50;
        } else {
            alpha = -SCORE_INF;
            beta  =  SCORE_INF;
        }

        int score = alphaBeta(depth, alpha, beta, 0, true, true);

        // Re-search with wider window if failed
        if (!stopped && (score <= alpha || score >= beta)) {
            alpha = -SCORE_INF;
            beta  =  SCORE_INF;
            score = alphaBeta(depth, alpha, beta, 0, true, true);
        }

        if (stopped && depth > 1) break; // use previous result

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

        // Time check: if we've used more than half our time, don't start new iteration
        if (timeManaged && ms >= allocatedMs / 2) break;

        // If we found a forced mate within this depth, stop
        if (abs(score) >= SCORE_MATE - depth) break;
    }

    return rootBest;
}

void Search::stop() {
    stopped = true;
}
