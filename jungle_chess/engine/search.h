#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"
#include "movegen.h"
#include "evaluate.h"
#include "tt.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <cstring>

namespace jungle {

struct SearchResult {
    Move bestMove;
    int score;
    int depth;
    int nodes;
    int64_t timeMs;
    std::vector<Move> pv;
};

class KillerMoves {
    static constexpr int MAX_DEPTH = 64;
    std::array<Move, MAX_DEPTH * 2> killers;
    
public:
    KillerMoves() { clear(); }
    
    void clear() {
        killers.fill(Move());
    }
    
    void add(int ply, const Move& m) {
        if (ply >= MAX_DEPTH) return;
        
        if (m != killers[ply * 2]) {
            killers[ply * 2 + 1] = killers[ply * 2];
            killers[ply * 2] = m;
        }
    }
    
    bool isKiller(int ply, const Move& m) const {
        if (ply >= MAX_DEPTH) return false;
        return m == killers[ply * 2] || m == killers[ply * 2 + 1];
    }
};

class HistoryTable {
    std::array<std::array<int, 64>, 2> table;
    
public:
    HistoryTable() { clear(); }
    
    void clear() {
        for (auto& row : table) {
            row.fill(0);
        }
    }
    
    void add(Color c, int fromIdx, int toIdx, int bonus) {
        bonus = std::min(bonus, 2000);
        table[static_cast<int>(c)][fromIdx * 8 + toIdx % 8] += bonus;
        table[static_cast<int>(c)][fromIdx * 8 + toIdx % 8] -= 
            table[static_cast<int>(c)][fromIdx * 8 + toIdx % 8] * std::abs(bonus) / 16384;
    }
    
    int get(Color c, int fromIdx, int toIdx) const {
        return table[static_cast<int>(c)][fromIdx * 8 + toIdx % 8];
    }
};

class Search {
public:
    Search() : tt_(256 * 1024 * 1024), nodes_(0), stop_(false), maxTime_(10000), ply_(0) {}
    
    SearchResult search(Board& board, int maxDepth = 20, int64_t maxTimeMs = 5000) {
        SearchResult result;
        result.bestMove = Move();
        result.score = 0;
        result.depth = 0;
        result.nodes = 0;
        result.timeMs = 0;
        
        nodes_.store(0);
        stop_.store(false);
        maxTime_ = maxTimeMs;
        startTime_ = std::chrono::high_resolution_clock::now();
        
        killers_.clear();
        history_.clear();
        
        Move bestMove;
        int bestScore = -INF;
        int prevScore = 0;
        
        int alpha = -INF;
        int beta = INF;
        
        for (int depth = 1; depth <= maxDepth; depth++) {
            ply_ = 0;
            
            int score = aspirationWindow(board, depth, alpha, beta, prevScore);
            
            if (stop_.load()) {
                break;
            }
            
            auto now = std::chrono::high_resolution_clock::now();
            int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
            
            if (elapsed > maxTimeMs) {
                stop_.store(true);
            }
            
            bestScore = score;
            prevScore = score;
            bestMove = tt_.getBestMove(board.hash);
            
            result.depth = depth;
            result.score = score;
            result.bestMove = bestMove;
            result.nodes = nodes_.load();
            result.timeMs = elapsed;
            
            result.pv.clear();
            extractPV(board, result.pv, depth);
            
            std::string scoreStr = "cp " + std::to_string(score);
            if (std::abs(score) >= MATE_SCORE - 100) {
                int mateIn = (MATE_SCORE - std::abs(score) + 1) / 2;
                scoreStr = "mate " + std::to_string(score > 0 ? mateIn : -mateIn);
            }
            
            std::cout << "info depth " << depth 
                      << " score " << scoreStr
                      << " nodes " << result.nodes
                      << " time " << result.timeMs
                      << " nps " << (result.timeMs > 0 ? result.nodes * 1000 / result.timeMs : 0)
                      << " pv";
            for (const auto& m : result.pv) {
                std::cout << " " << m.toString();
            }
            std::cout << std::endl;
            
            if (std::abs(score) >= MATE_SCORE - 100) {
                break;
            }
            
            if (elapsed > maxTimeMs / 4 && depth >= 6) {
                break;
            }
        }
        
        return result;
    }
    
    void stop() {
        stop_.store(true);
    }
    
    void clearTT() {
        tt_.clear();
        killers_.clear();
        history_.clear();
    }
    
    size_t ttSize() const {
        return tt_.size();
    }
    
private:
    TranspositionTable tt_;
    std::atomic<uint64_t> nodes_;
    std::atomic<bool> stop_;
    int64_t maxTime_;
    std::chrono::high_resolution_clock::time_point startTime_;
    KillerMoves killers_;
    HistoryTable history_;
    int ply_;
    
    int aspirationWindow(Board& board, int depth, int& alpha, int& beta, int prevScore) {
        int windowSize = 25;
        
        if (depth >= 4) {
            alpha = std::max(-INF, prevScore - windowSize);
            beta = std::min(INF, prevScore + windowSize);
        } else {
            alpha = -INF;
            beta = INF;
        }
        
        while (true) {
            int score = alphaBeta(board, depth, alpha, beta, true);
            
            if (stop_.load()) {
                return score;
            }
            
            if (score <= alpha) {
                alpha = std::max(-INF, alpha - windowSize);
                windowSize += windowSize / 2;
            } else if (score >= beta) {
                beta = std::min(INF, beta + windowSize);
                windowSize += windowSize / 2;
            } else {
                return score;
            }
        }
    }
    
    int alphaBeta(Board& board, int depth, int alpha, int beta, bool isPV) {
        if (stop_.load()) {
            return 0;
        }
        
        nodes_.fetch_add(1);
        
        if (nodes_.load() % 16384 == 0) {
            checkTime();
        }
        
        GameResult gameResult = board.checkGameResult();
        if (gameResult != GameResult::ONGOING) {
            if (gameResult == GameResult::LIGHT_WIN) {
                return board.sideToMove == Color::LIGHT ? MATE_SCORE - ply_ : -MATE_SCORE + ply_;
            } else if (gameResult == GameResult::DARK_WIN) {
                return board.sideToMove == Color::DARK ? MATE_SCORE - ply_ : -MATE_SCORE + ply_;
            }
            return 0;
        }
        
        if (depth <= 0) {
            return quiescence(board, alpha, beta, 6);
        }
        
        bool inCheck = isInCheck(board);
        if (inCheck) {
            depth++;
        }
        
        TTEntry ttEntry;
        bool ttHit = tt_.probe(board.hash, ttEntry);
        if (ttHit && ttEntry.depth >= depth && !isPV) {
            if (ttEntry.flag == TTFlag::EXACT) {
                return static_cast<int>(ttEntry.score);
            } else if (ttEntry.flag == TTFlag::LOWER) {
                alpha = std::max(alpha, static_cast<int>(ttEntry.score));
            } else if (ttEntry.flag == TTFlag::UPPER) {
                beta = std::min(beta, static_cast<int>(ttEntry.score));
            }
            
            if (alpha >= beta) {
                return static_cast<int>(ttEntry.score);
            }
        }
        
        MoveList moves;
        MoveGenerator::generateMoves(board, moves);
        
        if (moves.empty()) {
            return -MATE_SCORE + ply_;
        }
        
        Move ttMove;
        if (ttHit && ttEntry.bestMove.from.isValid()) {
            for (const auto& m : moves) {
                if (m == ttEntry.bestMove) {
                    ttMove = ttEntry.bestMove;
                    break;
                }
            }
        }
        
        orderMoves(board, moves, ttMove);
        
        Move bestMove = moves[0];
        int bestScore = -INF;
        TTFlag flag = TTFlag::UPPER;
        int moveCount = 0;
        
        for (size_t i = 0; i < moves.size(); i++) {
            Move m = moves[i];
            moveCount++;
            
            int extension = 0;
            if (isEnemyDen(m.to, board.sideToMove)) {
                extension = 1;
            }
            
            bool doLMR = false;
            int reduction = 0;
            
            if (depth >= 3 && moveCount > 3 && !isPV && !inCheck && !m.isCapture && extension == 0) {
                doLMR = true;
                reduction = 1;
                if (moveCount > 8) reduction = 2;
            }
            
            board.makeMove(m);
            ply_++;
            
            int score;
            if (doLMR) {
                score = -alphaBeta(board, depth - 1 - reduction, -alpha - 1, -alpha, false);
                if (score > alpha) {
                    score = -alphaBeta(board, depth - 1, -beta, -alpha, true);
                }
            } else if (isPV && moveCount == 1) {
                score = -alphaBeta(board, depth - 1 + extension, -beta, -alpha, true);
            } else {
                score = -alphaBeta(board, depth - 1 + extension, -alpha - 1, -alpha, false);
                if (score > alpha && score < beta) {
                    score = -alphaBeta(board, depth - 1 + extension, -beta, -alpha, true);
                }
            }
            
            ply_--;
            board.unmakeMove(m);
            
            if (stop_.load()) {
                return 0;
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = m;
            }
            
            if (score > alpha) {
                alpha = score;
                flag = TTFlag::EXACT;
                
                if (!m.isCapture) {
                    history_.add(board.sideToMove, m.from.index(), m.to.index(), depth * depth);
                }
                
                if (alpha >= beta) {
                    flag = TTFlag::LOWER;
                    
                    if (!m.isCapture) {
                        killers_.add(ply_, m);
                    }
                    
                    break;
                }
            }
        }
        
        tt_.store(board.hash, bestScore, static_cast<uint8_t>(depth), flag, bestMove);
        
        return bestScore;
    }
    
    int quiescence(Board& board, int alpha, int beta, int depth) {
        if (stop_.load()) {
            return 0;
        }
        
        nodes_.fetch_add(1);
        
        GameResult gameResult = board.checkGameResult();
        if (gameResult != GameResult::ONGOING) {
            if (gameResult == GameResult::LIGHT_WIN) {
                return board.sideToMove == Color::LIGHT ? MATE_SCORE - ply_ : -MATE_SCORE + ply_;
            } else if (gameResult == GameResult::DARK_WIN) {
                return board.sideToMove == Color::DARK ? MATE_SCORE - ply_ : -MATE_SCORE + ply_;
            }
            return 0;
        }
        
        int standPat = Evaluator::evaluate(board);
        
        if (depth <= 0) {
            return standPat;
        }
        
        if (standPat >= beta) {
            return beta;
        }
        
        int delta = 600;
        if (standPat < alpha - delta) {
            return alpha;
        }
        
        if (standPat > alpha) {
            alpha = standPat;
        }
        
        MoveList captures;
        MoveGenerator::generateCaptures(board, captures);
        
        if (captures.empty()) {
            return standPat;
        }
        
        orderMoves(board, captures, Move());
        
        for (const auto& m : captures) {
            board.makeMove(m);
            ply_++;
            int score = -quiescence(board, -beta, -alpha, depth - 1);
            ply_--;
            board.unmakeMove(m);
            
            if (stop_.load()) {
                return 0;
            }
            
            if (score >= beta) {
                return beta;
            }
            if (score > alpha) {
                alpha = score;
            }
        }
        
        return alpha;
    }
    
    bool isInCheck(const Board& board) {
        Color us = board.sideToMove;
        Square den(3, us == Color::LIGHT ? 0 : 8);
        
        const int dr[] = {0, 0, 1, -1};
        const int dc[] = {1, -1, 0, 0};
        
        for (int d = 0; d < 4; d++) {
            Square adj(den.col + dc[d], den.row + dr[d]);
            if (!adj.isValid()) continue;
            
            auto piece = board.getPiece(adj);
            if (piece && piece->color != us) {
                return true;
            }
        }
        
        return false;
    }
    
    void orderMoves(Board& board, MoveList& moves, const Move& ttMove) {
        std::vector<std::pair<int, size_t>> moveScores;
        moveScores.reserve(moves.size());
        
        for (size_t i = 0; i < moves.size(); i++) {
            int score = 0;
            const Move& m = moves[i];
            
            if (m == ttMove) {
                score = 2000000;
            } else if (m.isCapture) {
                auto attacker = board.getPiece(m.from);
                int mvvLva = Evaluator::PIECE_VALUES[static_cast<int>(m.capturedType)] * 10;
                if (attacker) {
                    mvvLva -= Evaluator::PIECE_VALUES[static_cast<int>(attacker->type)];
                }
                score = 1000000 + mvvLva;
            }
            
            if (isEnemyDen(m.to, board.sideToMove)) {
                score += 500000;
            }
            
            if (killers_.isKiller(ply_, m)) {
                score += 900000;
            }
            
            score += history_.get(board.sideToMove, m.from.index(), m.to.index());
            
            auto piece = board.getPiece(m.from);
            if (piece) {
                if (piece->type == PieceType::LION || piece->type == PieceType::TIGER) {
                    int targetRow = (board.sideToMove == Color::LIGHT) ? 8 : 0;
                    score += std::abs(m.to.row - targetRow) * 50;
                }
            }
            
            moveScores.emplace_back(score, i);
        }
        
        std::sort(moveScores.begin(), moveScores.end(), 
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        MoveList sorted;
        sorted.reserve(moves.size());
        for (const auto& ms : moveScores) {
            sorted.push_back(moves[ms.second]);
        }
        moves = std::move(sorted);
    }
    
    void extractPV(Board board, std::vector<Move>& pv, int maxDepth) {
        for (int i = 0; i < maxDepth; i++) {
            Move bestMove = tt_.getBestMove(board.hash);
            if (!bestMove.from.isValid()) break;
            
            pv.push_back(bestMove);
            board.makeMove(bestMove);
            
            if (board.checkGameResult() != GameResult::ONGOING) break;
        }
    }
    
    void checkTime() {
        auto now = std::chrono::high_resolution_clock::now();
        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
        
        if (elapsed > maxTime_) {
            stop_.store(true);
        }
    }
};

}

#endif
