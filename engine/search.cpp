#include "search.hpp"
#include <algorithm>
#include <iostream>

const int MATE_SCORE = 100000;

Search::Search(size_t ttSizeMB) : stopSearch(false), nodes(0), timeLimitMs(-1) {
    size_t numEntries = (ttSizeMB * 1024 * 1024) / sizeof(TTEntry);
    
    // Find next power of 2
    size_t pow2 = 1;
    while (pow2 <= numEntries) pow2 <<= 1;
    pow2 >>= 1;
    
    tt.resize(pow2);
    ttMask = pow2 - 1;
    
    for (size_t i = 0; i < tt.size(); ++i) {
        tt[i].hash = 0;
    }
}

void Search::stop() { stopSearch = true; }

void Search::recordTT(uint64_t hash, int depth, int score, TTFlag flag, Move bestMove) {
    size_t index = hash & ttMask;
    // Simple replacement scheme
    tt[index].hash = hash;
    tt[index].depth = depth;
    tt[index].score = score;
    tt[index].flag = flag;
    tt[index].move = bestMove;
}

bool Search::probeTT(uint64_t hash, int depth, int alpha, int beta, int& score, Move& bestMove) {
    size_t index = hash & ttMask;
    if (tt[index].hash == hash) {
        bestMove = tt[index].move;
        if (tt[index].depth >= depth) {
            int ttScore = tt[index].score;
            if (tt[index].flag == TT_EXACT) {
                score = ttScore;
                return true;
            }
            if (tt[index].flag == TT_ALPHA && ttScore <= alpha) {
                score = alpha;
                return true;
            }
            if (tt[index].flag == TT_BETA && ttScore >= beta) {
                score = beta;
                return true;
            }
        }
    }
    return false;
}

SearchResult Search::search(Board& board, int maxDepth, long long timeLimitMs) {
    this->timeLimitMs = timeLimitMs;
    this->startTime = std::chrono::steady_clock::now();
    this->stopSearch = false;
    this->nodes = 0;

    SearchResult result;
    result.score = -MATE_SCORE * 2;
    result.bestMove = Move();
    result.nodes = 0;

    for (int d = 1; d <= maxDepth; ++d) {
        if (stopSearch) break;

        std::vector<Move> moves;
        board.generateMoves(moves);
        if (moves.empty()) break; 

        // Move ordering for root
        Move ttMove;
        int dummyScore;
        if (probeTT(board.hash(), 0, -MATE_SCORE*2, MATE_SCORE*2, dummyScore, ttMove)) {
            auto it = std::find(moves.begin(), moves.end(), ttMove);
            if (it != moves.end()) {
                std::swap(moves[0], *it);
            }
        }

        int bestScore = -MATE_SCORE * 2;
        Move bestMoveThisIter = moves[0];

        for (Move m : moves) {
            UndoInfo undo;
            board.doMove(m, undo);
            int score = -alphaBeta(board, d - 1, -MATE_SCORE * 2, MATE_SCORE * 2);
            board.undoMove(undo);

            if (stopSearch) break;

            if (score > bestScore) {
                bestScore = score;
                bestMoveThisIter = m;
            }
        }

        if (!stopSearch) {
            result.bestMove = bestMoveThisIter;
            result.score = bestScore;
            std::cout << "info depth " << d << " score cp " << bestScore << " nodes " << this->nodes << std::endl;
        }

        // Time management: if > 50% time used, don't start next depth
        if (timeLimitMs > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            if (elapsed >= timeLimitMs / 2) {
                break;
            }
        }
        
        // If mate found, no need to search deeper
        if (bestScore > MATE_SCORE - 1000 || bestScore < -MATE_SCORE + 1000) {
            break;
        }
    }
    
    result.nodes = this->nodes;
    return result;
}

int Search::alphaBeta(Board& board, int depth, int alpha, int beta) {
    this->nodes++;
    
    if ((this->nodes & 2047) == 0 && timeLimitMs > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= timeLimitMs) {
            stopSearch = true;
            return 0;
        }
    }

    if (stopSearch) return 0;

    int winner;
    if (board.isGameOver(winner)) {
        if (winner == board.sideToMove) return MATE_SCORE - (100 - depth);
        if (winner == NO_COLOR) return 0;
        return -MATE_SCORE + (100 - depth);
    }

    if (depth <= 0) {
        return board.evaluate();
    }

    int ttScore;
    Move ttMove;
    if (probeTT(board.hash(), depth, alpha, beta, ttScore, ttMove)) {
        return ttScore;
    }

    std::vector<Move> moves;
    board.generateMoves(moves);
    if (moves.empty()) {
        return -MATE_SCORE + (100 - depth);
    }

    // Move ordering
    for (auto& m : moves) {
        if (m == ttMove) {
            std::swap(moves[0], m);
            break;
        }
    }

    int bestScore = -MATE_SCORE * 2;
    int originalAlpha = alpha;
    Move bestMove = moves[0];

    for (Move m : moves) {
        UndoInfo undo;
        board.doMove(m, undo);
        int score = -alphaBeta(board, depth - 1, -beta, -alpha);
        board.undoMove(undo);

        if (stopSearch) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }

        if (bestScore > alpha) {
            alpha = bestScore;
        }

        if (alpha >= beta) {
            recordTT(board.hash(), depth, bestScore, TT_BETA, bestMove);
            return bestScore; // Beta cutoff
        }
    }

    TTFlag flag = (bestScore <= originalAlpha) ? TT_ALPHA : TT_EXACT;
    recordTT(board.hash(), depth, bestScore, flag, bestMove);

    return bestScore;
}
