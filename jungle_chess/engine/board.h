#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include <array>
#include <optional>
#include <random>
#include <iostream>

namespace jungle {

class Zobrist {
public:
    static constexpr size_t NUM_PIECES = 8;
    static constexpr size_t NUM_COLORS = 2;
    static constexpr size_t NUM_SQUARES = BOARD_COLS * BOARD_ROWS;
    
    std::array<std::array<std::array<uint64_t, NUM_SQUARES>, NUM_PIECES>, NUM_COLORS> pieceKeys;
    uint64_t sideKey;
    
    static Zobrist& instance() {
        static Zobrist inst;
        return inst;
    }
    
private:
    Zobrist() {
        std::mt19937_64 rng(12345);
        for (int c = 0; c < 2; c++) {
            for (int p = 0; p < 8; p++) {
                for (int s = 0; s < NUM_SQUARES; s++) {
                    pieceKeys[c][p][s] = rng();
                }
            }
        }
        sideKey = rng();
    }
};

class Board {
public:
    std::array<std::optional<Piece>, BOARD_COLS * BOARD_ROWS> squares;
    Color sideToMove;
    uint64_t hash;
    
    std::array<int, 2> pieceCount;
    
    Board() : sideToMove(Color::LIGHT), hash(0), pieceCount{0, 0} {
        squares.fill(std::nullopt);
    }
    
    void setInitialPosition() {
        squares.fill(std::nullopt);
        pieceCount = {8, 8};
        
        setPiece(Square(0, 0), Piece(PieceType::TIGER, Color::LIGHT));
        setPiece(Square(6, 0), Piece(PieceType::LION, Color::LIGHT));
        setPiece(Square(1, 1), Piece(PieceType::CAT, Color::LIGHT));
        setPiece(Square(5, 1), Piece(PieceType::DOG, Color::LIGHT));
        setPiece(Square(0, 2), Piece(PieceType::ELEPHANT, Color::LIGHT));
        setPiece(Square(2, 2), Piece(PieceType::WOLF, Color::LIGHT));
        setPiece(Square(4, 2), Piece(PieceType::LEOPARD, Color::LIGHT));
        setPiece(Square(6, 2), Piece(PieceType::RAT, Color::LIGHT));
        
        setPiece(Square(0, 6), Piece(PieceType::RAT, Color::DARK));
        setPiece(Square(2, 6), Piece(PieceType::LEOPARD, Color::DARK));
        setPiece(Square(4, 6), Piece(PieceType::WOLF, Color::DARK));
        setPiece(Square(6, 6), Piece(PieceType::ELEPHANT, Color::DARK));
        setPiece(Square(1, 7), Piece(PieceType::DOG, Color::DARK));
        setPiece(Square(5, 7), Piece(PieceType::CAT, Color::DARK));
        setPiece(Square(0, 8), Piece(PieceType::LION, Color::DARK));
        setPiece(Square(6, 8), Piece(PieceType::TIGER, Color::DARK));
        
        sideToMove = Color::LIGHT;
        computeHash();
    }
    
    std::optional<Piece> getPiece(const Square& sq) const {
        if (!sq.isValid()) return std::nullopt;
        return squares[sq.index()];
    }
    
    void setPiece(const Square& sq, Piece p) {
        squares[sq.index()] = p;
    }
    
    void removePiece(const Square& sq) {
        squares[sq.index()] = std::nullopt;
    }
    
    void computeHash() {
        hash = 0;
        auto& z = Zobrist::instance();
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto p = squares[idx];
            if (p) {
                int pieceIdx = static_cast<int>(p->type) - 1;
                int colorIdx = static_cast<int>(p->color);
                hash ^= z.pieceKeys[colorIdx][pieceIdx][idx];
            }
        }
        if (sideToMove == Color::DARK) {
            hash ^= z.sideKey;
        }
    }
    
    void makeMove(const Move& m) {
        auto piece = getPiece(m.from);
        if (!piece) return;
        
        auto& z = Zobrist::instance();
        int pieceIdx = static_cast<int>(piece->type) - 1;
        int colorIdx = static_cast<int>(piece->color);
        
        hash ^= z.pieceKeys[colorIdx][pieceIdx][m.from.index()];
        
        if (m.isCapture) {
            int capturedIdx = static_cast<int>(m.capturedType) - 1;
            int capturedColorIdx = static_cast<int>(~piece->color);
            hash ^= z.pieceKeys[capturedColorIdx][capturedIdx][m.to.index()];
            pieceCount[static_cast<int>(~piece->color)]--;
        }
        
        removePiece(m.from);
        setPiece(m.to, *piece);
        hash ^= z.pieceKeys[colorIdx][pieceIdx][m.to.index()];
        
        hash ^= z.sideKey;
        sideToMove = ~sideToMove;
    }
    
    void unmakeMove(const Move& m) {
        auto piece = getPiece(m.to);
        if (!piece) return;
        
        setPiece(m.from, *piece);
        
        if (m.isCapture) {
            setPiece(m.to, Piece(m.capturedType, ~piece->color));
            pieceCount[static_cast<int>(~piece->color)]++;
        } else {
            removePiece(m.to);
        }
        
        sideToMove = ~sideToMove;
        computeHash();
    }
    
    void makeNullMove() {
        hash ^= Zobrist::instance().sideKey;
        sideToMove = ~sideToMove;
    }
    
    void unmakeNullMove() {
        hash ^= Zobrist::instance().sideKey;
        sideToMove = ~sideToMove;
    }
    
    GameResult checkGameResult() const {
        auto lightDen = Square(3, 0);
        auto darkDen = Square(3, 8);
        
        auto pieceAtLightDen = getPiece(lightDen);
        if (pieceAtLightDen && pieceAtLightDen->color == Color::DARK) {
            return GameResult::DARK_WIN;
        }
        
        auto pieceAtDarkDen = getPiece(darkDen);
        if (pieceAtDarkDen && pieceAtDarkDen->color == Color::LIGHT) {
            return GameResult::LIGHT_WIN;
        }
        
        if (pieceCount[static_cast<int>(Color::LIGHT)] == 0) {
            return GameResult::DARK_WIN;
        }
        if (pieceCount[static_cast<int>(Color::DARK)] == 0) {
            return GameResult::LIGHT_WIN;
        }
        
        return GameResult::ONGOING;
    }
    
    bool canCapture(const Square& from, const Square& to) const {
        auto attacker = getPiece(from);
        auto defender = getPiece(to);
        
        if (!attacker || !defender) return false;
        if (attacker->color == defender->color) return false;
        
        bool attackerInWater = isWater(from);
        bool defenderInWater = isWater(to);
        
        if (attacker->type == PieceType::RAT) {
            if (attackerInWater && !defenderInWater) {
                return false;
            }
            if (!attackerInWater && defenderInWater) {
                return false;
            }
        }
        
        if (defender->type == PieceType::ELEPHANT && attacker->type == PieceType::RAT) {
            return true;
        }
        
        if (attacker->type == PieceType::ELEPHANT && defender->type == PieceType::RAT) {
            return false;
        }
        
        if (isEnemyTrap(to, attacker->color)) {
            return true;
        }
        
        return pieceRank(attacker->type) >= pieceRank(defender->type);
    }
    
    void print() const {
        const char* colLabels = "ABCDEFG";
        std::cout << "\n  ";
        for (int c = 0; c < BOARD_COLS; c++) {
            std::cout << " " << colLabels[c] << " ";
        }
        std::cout << "\n";
        
        for (int r = BOARD_ROWS - 1; r >= 0; r--) {
            std::cout << (r + 1) << " ";
            for (int c = 0; c < BOARD_COLS; c++) {
                Square sq(c, r);
                auto p = getPiece(sq);
                
                if (isDen(sq)) {
                    if (p) {
                        std::cout << "[" << pieceChar(p->type, p->color) << "]";
                    } else {
                        std::cout << "[ ]";
                    }
                } else if (isTrap(sq)) {
                    if (p) {
                        std::cout << "(" << pieceChar(p->type, p->color) << ")";
                    } else {
                        std::cout << "( )";
                    }
                } else if (isWater(sq)) {
                    if (p) {
                        std::cout << "{" << pieceChar(p->type, p->color) << "}";
                    } else {
                        std::cout << "{~}";
                    }
                } else {
                    if (p) {
                        std::cout << " " << pieceChar(p->type, p->color) << " ";
                    } else {
                        std::cout << " . ";
                    }
                }
            }
            std::cout << " " << (r + 1) << "\n";
        }
        std::cout << "  ";
        for (int c = 0; c < BOARD_COLS; c++) {
            std::cout << " " << colLabels[c] << " ";
        }
        std::cout << "\n";
        std::cout << "Side to move: " << (sideToMove == Color::LIGHT ? "Light" : "Dark") << "\n";
        std::cout << "Hash: " << std::hex << hash << std::dec << "\n";
    }
};

}

#endif
