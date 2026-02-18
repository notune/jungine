#ifndef EVALUATE_H
#define EVALUATE_H

#include "types.h"
#include "board.h"
#include "movegen.h"
#include <array>
#include <iostream>

namespace jungle {

class Evaluator {
public:
    static constexpr int PIECE_VALUES[] = {
        0,
        100,    // RAT
        190,    // CAT
        280,    // DOG
        400,    // WOLF
        600,    // LEOPARD
        1000,   // TIGER
        1200,   // LION
        1500    // ELEPHANT
    };
    
    static constexpr int PIECE_ACTIVITY_BONUS[] = {
        0, 5, 8, 12, 15, 20, 25, 30, 35
    };
    
    struct EvalWeights {
        int material = 100;
        int position = 100;
        int advancement = 100;
        int centerControl = 100;
        int trapControl = 100;
        int denThreat = 150;
        int mobility = 100;
        int kingSafety = 100;
        int pieceCoordination = 100;
    };
    
    static int evaluate(const Board& board) {
        int score = 0;
        
        auto result = board.checkGameResult();
        if (result == GameResult::LIGHT_WIN) return MATE_SCORE - 1;
        if (result == GameResult::DARK_WIN) return -MATE_SCORE + 1;
        
        score += evaluateMaterial(board);
        score += evaluatePositions(board);
        score += evaluateTraps(board);
        score += evaluateDenThreat(board);
        score += evaluateSpecialPieces(board);
        score += evaluateKingSafety(board);
        score += evaluatePieceCoordination(board);
        
        score += evaluateEndgame(board);
        
        return board.sideToMove == Color::LIGHT ? score : -score;
    }
    
private:
    static constexpr int DISTANCE_TO_DEN_BONUS = 40;
    static constexpr int TRAP_DEFENSE_BONUS = 25;
    static constexpr int TRAP_ATTACK_BONUS = 50;
    static constexpr int DEN_THREAT_BONUS = 150;
    static constexpr int MOBILITY_BONUS = 3;
    static constexpr int RIVER_CONTROL_BONUS = 30;
    static constexpr int JUMPER_ADVANCEMENT_BONUS = 45;
    static constexpr int RAT_NEAR_ELEPHANT_BONUS = 60;
    static constexpr int CENTER_COL = 3;
    static constexpr int LIGHT_DEN_ROW = 0;
    static constexpr int DARK_DEN_ROW = 8;
    
    static int evaluateMaterial(const Board& board) {
        int score = 0;
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece) continue;
            
            int baseValue = PIECE_VALUES[static_cast<int>(piece->type)];
            int value = baseValue;
            
            Square sq = Square::fromIndex(idx);
            value += getPieceSquareBonus(sq, *piece);
            
            if (piece->color == Color::LIGHT) {
                score += value;
            } else {
                score -= value;
            }
        }
        
        return score;
    }
    
    static int getPieceSquareBonus(const Square& sq, const Piece& piece) {
        static const int PST_LIGHT[9][7] = {
            { 0,  0,  0,  0,  0,  0,  0},
            { 0,  0,  0,  0,  0,  0,  0},
            { 5,  5, 10, 15, 10,  5,  5},
            {10, 15, 20, 25, 20, 15, 10},
            {15, 20, 30, 40, 30, 20, 15},
            {20, 25, 35, 50, 35, 25, 20},
            {25, 30, 45, 60, 45, 30, 25},
            {20, 25, 35, 45, 35, 25, 20},
            {15, 20, 30, 40, 30, 20, 15}
        };
        
        int bonus = 0;
        
        if (piece.color == Color::LIGHT) {
            bonus = PST_LIGHT[sq.row][sq.col];
        } else {
            bonus = PST_LIGHT[8 - sq.row][6 - sq.col];
        }
        
        switch (piece.type) {
            case PieceType::RAT: bonus /= 2; break;
            case PieceType::ELEPHANT: bonus = bonus * 3 / 4; break;
            case PieceType::LION:
            case PieceType::TIGER: bonus = bonus * 4 / 3; break;
            default: break;
        }
        
        return bonus;
    }
    
    static int evaluatePositions(const Board& board) {
        int score = 0;
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece) continue;
            
            Square sq = Square::fromIndex(idx);
            int posValue = getAdvancedPositionValue(sq, *piece);
            
            if (piece->color == Color::LIGHT) {
                score += posValue;
            } else {
                score -= posValue;
            }
        }
        
        return score;
    }
    
    static int getAdvancedPositionValue(const Square& sq, const Piece& piece) {
        int value = 0;
        Color c = piece.color;
        
        int targetRow = (c == Color::LIGHT) ? DARK_DEN_ROW : LIGHT_DEN_ROW;
        int distToDen = std::abs(sq.row - targetRow) + std::abs(sq.col - CENTER_COL);
        
        int advancementBonus = (c == Color::LIGHT ? sq.row : 8 - sq.row) * DISTANCE_TO_DEN_BONUS;
        value += advancementBonus;
        
        int denProximityBonus = (8 - distToDen) * 8;
        value += denProximityBonus;
        
        if (piece.type == PieceType::LION || piece.type == PieceType::TIGER) {
            if (c == Color::LIGHT && sq.row >= 5) {
                value += (sq.row - 4) * JUMPER_ADVANCEMENT_BONUS;
            } else if (c == Color::DARK && sq.row <= 3) {
                value += (4 - sq.row) * JUMPER_ADVANCEMENT_BONUS;
            }
            
            if (sq.col == 0 || sq.col == 6) {
                value += 20;
            }
        }
        
        if (piece.type == PieceType::RAT) {
            if (isWater(sq)) {
                value += RIVER_CONTROL_BONUS;
            }
            
            if (sq.col >= 2 && sq.col <= 4 && sq.row >= 3 && sq.row <= 5) {
                value += 15;
            }
        }
        
        if (piece.type != PieceType::RAT) {
            if (sq.row >= 2 && sq.row <= 6 && sq.col >= 1 && sq.col <= 5) {
                value += 10;
            }
        }
        
        return value;
    }
    
    static int evaluateTraps(const Board& board) {
        int score = 0;
        
        Square lightTraps[] = {Square(2, 0), Square(4, 0), Square(3, 1)};
        Square darkTraps[] = {Square(2, 8), Square(4, 8), Square(3, 7)};
        
        for (const auto& trap : lightTraps) {
            auto piece = board.getPiece(trap);
            if (piece) {
                if (piece->color == Color::DARK) {
                    int trappedValue = PIECE_VALUES[static_cast<int>(piece->type)];
                    score += TRAP_ATTACK_BONUS + trappedValue / 3;
                } else {
                    score += TRAP_DEFENSE_BONUS;
                }
            }
            
            score += evaluateTrapZoneControl(board, trap, Color::LIGHT);
        }
        
        for (const auto& trap : darkTraps) {
            auto piece = board.getPiece(trap);
            if (piece) {
                if (piece->color == Color::LIGHT) {
                    int trappedValue = PIECE_VALUES[static_cast<int>(piece->type)];
                    score -= TRAP_ATTACK_BONUS + trappedValue / 3;
                } else {
                    score -= TRAP_DEFENSE_BONUS;
                }
            }
            
            score -= evaluateTrapZoneControl(board, trap, Color::DARK);
        }
        
        return score;
    }
    
    static int evaluateTrapZoneControl(const Board& board, const Square& trap, Color trapOwner) {
        int control = 0;
        const int dr[] = {0, 0, 1, -1};
        const int dc[] = {1, -1, 0, 0};
        
        for (int d = 0; d < 4; d++) {
            Square adj(trap.col + dc[d], trap.row + dr[d]);
            if (!adj.isValid()) continue;
            
            auto piece = board.getPiece(adj);
            if (piece) {
                if (piece->color == trapOwner) {
                    control += 15 + PIECE_VALUES[static_cast<int>(piece->type)] / 20;
                } else {
                    control -= 10;
                }
            }
        }
        
        return control;
    }
    
    static int evaluateDenThreat(const Board& board) {
        int score = 0;
        
        Square lightDen(3, 0);
        Square darkDen(3, 8);
        
        score += evaluateDenZone(board, darkDen, Color::LIGHT);
        score -= evaluateDenZone(board, lightDen, Color::DARK);
        
        return score;
    }
    
    static int evaluateDenZone(const Board& board, const Square& den, Color attacker) {
        int score = 0;
        const int dr[] = {0, 0, 1, -1, 0, 0, 2, -2};
        const int dc[] = {1, -1, 0, 0, 2, -2, 0, 0};
        
        for (int d = 0; d < 4; d++) {
            Square adj(den.col + dc[d], den.row + dr[d]);
            if (!adj.isValid()) continue;
            
            auto piece = board.getPiece(adj);
            if (piece && piece->color == attacker) {
                int bonus = DEN_THREAT_BONUS * 2;
                
                if (piece->type == PieceType::LION || piece->type == PieceType::TIGER) {
                    bonus += 200;
                }
                if (piece->type == PieceType::RAT) {
                    bonus += 100;
                }
                
                score += bonus;
            }
        }
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece || piece->color != attacker) continue;
            
            Square sq = Square::fromIndex(idx);
            int dist = std::abs(sq.col - den.col) + std::abs(sq.row - den.row);
            
            if (dist <= 2) {
                int bonus = (3 - dist) * DEN_THREAT_BONUS / 2;
                
                if (piece->type == PieceType::LION || piece->type == PieceType::TIGER) {
                    bonus = bonus * 3 / 2;
                }
                
                score += bonus;
            }
        }
        
        return score;
    }
    
    static int evaluateSpecialPieces(const Board& board) {
        int score = 0;
        
        std::optional<Square> lightRat, darkRat;
        std::optional<Square> lightElephant, darkElephant;
        std::optional<Square> lightLion, darkLion;
        std::optional<Square> lightTiger, darkTiger;
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece) continue;
            
            Square sq = Square::fromIndex(idx);
            
            switch (piece->type) {
                case PieceType::RAT:
                    if (piece->color == Color::LIGHT) lightRat = sq;
                    else darkRat = sq;
                    break;
                case PieceType::ELEPHANT:
                    if (piece->color == Color::LIGHT) lightElephant = sq;
                    else darkElephant = sq;
                    break;
                case PieceType::LION:
                    if (piece->color == Color::LIGHT) lightLion = sq;
                    else darkLion = sq;
                    break;
                case PieceType::TIGER:
                    if (piece->color == Color::LIGHT) lightTiger = sq;
                    else darkTiger = sq;
                    break;
                default:
                    break;
            }
        }
        
        if (lightRat && darkElephant) {
            int dist = std::abs(lightRat->col - darkElephant->col) + 
                       std::abs(lightRat->row - darkElephant->row);
            if (dist <= 4 && !isWater(*lightRat)) {
                score += (5 - dist) * RAT_NEAR_ELEPHANT_BONUS / 2;
            }
        }
        
        if (darkRat && lightElephant) {
            int dist = std::abs(darkRat->col - lightElephant->col) + 
                       std::abs(darkRat->row - lightElephant->row);
            if (dist <= 4 && !isWater(*darkRat)) {
                score -= (5 - dist) * RAT_NEAR_ELEPHANT_BONUS / 2;
            }
        }
        
        if (lightRat && darkRat) {
            int dist = std::abs(lightRat->col - darkRat->col) + 
                       std::abs(lightRat->row - darkRat->row);
            if (dist <= 2) {
                if (isWater(*lightRat) && isWater(*darkRat)) {
                    score += 20;
                }
            }
        }
        
        return score;
    }
    
    static int evaluateKingSafety(const Board& board) {
        int score = 0;
        
        Square lightDen(3, 0);
        Square darkDen(3, 8);
        
        score += evaluateDenDefense(board, lightDen, Color::LIGHT);
        score -= evaluateDenDefense(board, darkDen, Color::DARK);
        
        return score;
    }
    
    static int evaluateDenDefense(const Board& board, const Square& den, Color defender) {
        int defense = 0;
        const int dr[] = {0, 0, 1, -1, 1, 1, -1, -1};
        const int dc[] = {1, -1, 0, 0, 1, -1, 1, -1};
        
        int enemyAttackers = 0;
        int friendlyDefenders = 0;
        int defenderStrength = 0;
        
        for (int d = 0; d < 8; d++) {
            Square adj(den.col + dc[d], den.row + dr[d]);
            if (!adj.isValid()) continue;
            if (isWater(adj)) continue;
            
            auto piece = board.getPiece(adj);
            if (piece) {
                if (piece->color == defender) {
                    friendlyDefenders++;
                    defenderStrength += PIECE_VALUES[static_cast<int>(piece->type)];
                } else {
                    enemyAttackers++;
                }
            }
        }
        
        if (enemyAttackers > 0) {
            defense -= enemyAttackers * 30;
            if (friendlyDefenders == 0) {
                defense -= 100;
            } else {
                defense += defenderStrength / 10;
            }
        } else {
            defense += friendlyDefenders * 10;
        }
        
        return defense;
    }
    
    static int evaluatePieceCoordination(const Board& board) {
        int score = 0;
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece) continue;
            
            Square sq = Square::fromIndex(idx);
            int coordination = evaluatePieceSupport(board, sq, *piece);
            
            if (piece->color == Color::LIGHT) {
                score += coordination;
            } else {
                score -= coordination;
            }
        }
        
        return score;
    }
    
    static int evaluatePieceSupport(const Board& board, const Square& sq, const Piece& piece) {
        int support = 0;
        const int dr[] = {0, 0, 1, -1};
        const int dc[] = {1, -1, 0, 0};
        
        for (int d = 0; d < 4; d++) {
            Square adj(sq.col + dc[d], sq.row + dr[d]);
            if (!adj.isValid()) continue;
            
            auto adjPiece = board.getPiece(adj);
            if (adjPiece && adjPiece->color == piece.color) {
                support += 5;
            }
        }
        
        return support;
    }
    
    static int evaluateEndgame(const Board& board) {
        int totalMaterial = 0;
        int lightMaterial = 0;
        int darkMaterial = 0;
        
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto piece = board.squares[idx];
            if (!piece) continue;
            
            int value = PIECE_VALUES[static_cast<int>(piece->type)];
            totalMaterial += value;
            
            if (piece->color == Color::LIGHT) {
                lightMaterial += value;
            } else {
                darkMaterial += value;
            }
        }
        
        int score = 0;
        
        if (totalMaterial < 4000) {
            int materialDiff = lightMaterial - darkMaterial;
            
            if (std::abs(materialDiff) > 300) {
                Color winningSide = materialDiff > 0 ? Color::LIGHT : Color::DARK;
                int sign = materialDiff > 0 ? 1 : -1;
                
                int endgameBonus = (4000 - totalMaterial) / 20;
                score += sign * endgameBonus;
                
                for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
                    auto piece = board.squares[idx];
                    if (!piece || piece->color != winningSide) continue;
                    
                    Square sq = Square::fromIndex(idx);
                    int targetRow = (winningSide == Color::LIGHT) ? 8 : 0;
                    int distToDen = std::abs(sq.row - targetRow) + std::abs(sq.col - 3);
                    
                    score += sign * (8 - distToDen) * 10;
                }
            }
        }
        
        return score;
    }
};

}

#endif
