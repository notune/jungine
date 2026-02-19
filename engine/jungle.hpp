#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <cassert>

enum Color : int8_t { LIGHT = 0, DARK = 1, NO_COLOR = 2 };
enum PieceType : int8_t { EMPTY=0, RAT=1, CAT=2, DOG=3, WOLF=4, LEOPARD=5, TIGER=6, LION=7, ELEPHANT=8 };
enum SquareType : int8_t { LAND=0, WATER=1, DEN_LIGHT=2, DEN_DARK=3, TRAP_LIGHT=4, TRAP_DARK=5 };

struct Piece {
    PieceType type;
    Color color;
    bool operator==(const Piece& o) const { return type == o.type && color == o.color; }
    bool operator!=(const Piece& o) const { return !(*this == o); }
};

const Piece NO_PIECE = {EMPTY, NO_COLOR};

// Squares
constexpr int SQ(int f, int r) { return r * 7 + f; }
constexpr int FILE_OF(int sq) { return sq % 7; }
constexpr int RANK_OF(int sq) { return sq / 7; }

struct Move {
    uint16_t data;
    Move() : data(0) {}
    Move(int from, int to) : data((from & 0x3F) | ((to & 0x3F) << 6)) {}
    int from() const { return data & 0x3F; }
    int to() const { return (data >> 6) & 0x3F; }
    bool operator==(const Move& o) const { return data == o.data; }
    bool operator!=(const Move& o) const { return data != o.data; }
};

struct UndoInfo {
    Move move;
    Piece captured;
    int halfMoveClock;
    uint64_t hash;
};

class Board {
public:
    Piece pieces[63];
    Color sideToMove;
    int halfMoveClock;
    int fullMoveNumber;
    uint64_t currentHash;

    Board();
    void loadPosition(const std::string& fen); 
    void loadStartPos();
    std::string getFen() const;

    bool isLegal(Move m) const;
    void generateMoves(std::vector<Move>& moves) const;
    
    void doMove(Move m, UndoInfo& undo);
    void undoMove(const UndoInfo& undo);

    void print() const;
    
    SquareType getSquareType(int sq) const;
    bool canCapture(Piece p1, int sq1, Piece p2, int sq2) const;
    
    int evaluate() const;
    bool isGameOver(int& winner) const;

    uint64_t hash() const;
    uint64_t computeHash() const;
};

extern const int DIRS[4];
void initZobrist();
