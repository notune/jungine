#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"
#include "board.h"
#include <vector>

namespace jungle {

class MoveGenerator {
public:
    static void generateMoves(const Board& board, MoveList& moves) {
        moves.clear();
        Color us = board.sideToMove;
        Color them = ~us;
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece || piece->color != us) continue;
            
            Square from = Square::fromIndex(idx);
            
            if (piece->type == PieceType::RAT) {
                generateRatMoves(board, from, moves);
            } else if (piece->type == PieceType::LION || piece->type == PieceType::TIGER) {
                generateLionTigerMoves(board, from, moves);
            } else {
                generateNormalMoves(board, from, moves);
            }
        }
    }
    
    static void generateCaptures(const Board& board, MoveList& moves) {
        moves.clear();
        Color us = board.sideToMove;
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece || piece->color != us) continue;
            
            Square from = Square::fromIndex(idx);
            
            const int dr[] = {0, 0, 1, -1};
            const int dc[] = {1, -1, 0, 0};
            
            for (int d = 0; d < 4; d++) {
                Square to(from.col + dc[d], from.row + dr[d]);
                if (!to.isValid()) continue;
                
                auto target = board.getPiece(to);
                if (target && target->color != us && board.canCapture(from, to)) {
                    moves.emplace_back(from, to, target->type);
                }
            }
            
            if (piece->type == PieceType::LION || piece->type == PieceType::TIGER) {
                generateJumpCaptures(board, from, moves);
            }
        }
    }
    
private:
    static void generateNormalMoves(const Board& board, const Square& from, MoveList& moves) {
        Color us = board.sideToMove;
        
        const int dr[] = {0, 0, 1, -1};
        const int dc[] = {1, -1, 0, 0};
        
        for (int d = 0; d < 4; d++) {
            Square to(from.col + dc[d], from.row + dr[d]);
            if (!to.isValid()) continue;
            
            if (isWater(to)) continue;
            if (isOwnDen(to, us)) continue;
            
            auto target = board.getPiece(to);
            if (!target) {
                moves.emplace_back(from, to);
            } else if (target->color != us && board.canCapture(from, to)) {
                moves.emplace_back(from, to, target->type);
            }
        }
    }
    
    static void generateRatMoves(const Board& board, const Square& from, MoveList& moves) {
        Color us = board.sideToMove;
        bool ratInWater = isWater(from);
        
        const int dr[] = {0, 0, 1, -1};
        const int dc[] = {1, -1, 0, 0};
        
        for (int d = 0; d < 4; d++) {
            Square to(from.col + dc[d], from.row + dr[d]);
            if (!to.isValid()) continue;
            
            if (isOwnDen(to, us)) continue;
            
            bool toIsWater = isWater(to);
            
            if (ratInWater && !toIsWater) {
                auto target = board.getPiece(to);
                if (!target) {
                    moves.emplace_back(from, to);
                } else if (target->color != us && target->type == PieceType::RAT) {
                    moves.emplace_back(from, to, target->type);
                }
            } else if (!ratInWater && toIsWater) {
                auto target = board.getPiece(to);
                if (!target) {
                    moves.emplace_back(from, to);
                } else if (target->color != us && target->type == PieceType::RAT) {
                    moves.emplace_back(from, to, target->type);
                }
            } else {
                auto target = board.getPiece(to);
                if (!target) {
                    moves.emplace_back(from, to);
                } else if (target->color != us && board.canCapture(from, to)) {
                    moves.emplace_back(from, to, target->type);
                }
            }
        }
    }
    
    static void generateLionTigerMoves(const Board& board, const Square& from, MoveList& moves) {
        generateNormalMoves(board, from, moves);
        generateJumpMoves(board, from, moves);
    }
    
    static void generateJumpMoves(const Board& board, const Square& from, MoveList& moves) {
        Color us = board.sideToMove;
        
        static const struct { int dc, dr; } dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        
        for (const auto& dir : dirs) {
            Square waterStart(from.col + dir.dc, from.row + dir.dr);
            if (!waterStart.isValid() || !isWater(waterStart)) continue;
            
            bool blocked = false;
            Square to = waterStart;
            
            while (to.isValid() && isWater(to)) {
                auto piece = board.getPiece(to);
                if (piece && piece->type == PieceType::RAT) {
                    blocked = true;
                    break;
                }
                to.col += dir.dc;
                to.row += dir.dr;
            }
            
            if (blocked || !to.isValid()) continue;
            if (isOwnDen(to, us)) continue;
            
            auto target = board.getPiece(to);
            if (!target) {
                moves.emplace_back(from, to);
            } else if (target->color != us && board.canCapture(from, to)) {
                moves.emplace_back(from, to, target->type);
            }
        }
    }
    
    static void generateJumpCaptures(const Board& board, const Square& from, MoveList& moves) {
        Color us = board.sideToMove;
        
        static const struct { int dc, dr; } dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        
        for (const auto& dir : dirs) {
            Square waterStart(from.col + dir.dc, from.row + dir.dr);
            if (!waterStart.isValid() || !isWater(waterStart)) continue;
            
            bool blocked = false;
            Square to = waterStart;
            
            while (to.isValid() && isWater(to)) {
                auto piece = board.getPiece(to);
                if (piece && piece->type == PieceType::RAT) {
                    blocked = true;
                    break;
                }
                to.col += dir.dc;
                to.row += dir.dr;
            }
            
            if (blocked || !to.isValid()) continue;
            
            auto target = board.getPiece(to);
            if (target && target->color != us && board.canCapture(from, to)) {
                moves.emplace_back(from, to, target->type);
            }
        }
    }
};

}

#endif
