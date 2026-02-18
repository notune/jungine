#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <optional>
#include <functional>

namespace jungle {

constexpr int BOARD_COLS = 7;
constexpr int BOARD_ROWS = 9;
constexpr int MAX_MOVES = 64;

enum class Color : uint8_t {
    LIGHT = 0,
    DARK = 1
};

inline Color operator~(Color c) {
    return static_cast<Color>(static_cast<uint8_t>(c) ^ 1);
}

enum class PieceType : uint8_t {
    RAT = 1,
    CAT = 2,
    DOG = 3,
    WOLF = 4,
    LEOPARD = 5,
    TIGER = 6,
    LION = 7,
    ELEPHANT = 8
};

inline int pieceRank(PieceType pt) {
    return static_cast<int>(pt);
}

inline std::string pieceChar(PieceType pt, Color c) {
    const char* lightPieces = " rcdwptle";
    const char* darkPieces = " RCDWPTLE";
    int idx = static_cast<int>(pt);
    return c == Color::LIGHT 
        ? std::string(1, lightPieces[idx])
        : std::string(1, darkPieces[idx]);
}

struct Square {
    uint8_t col;
    uint8_t row;
    
    Square() : col(0), row(0) {}
    Square(uint8_t c, uint8_t r) : col(c), row(r) {}
    
    bool operator==(const Square& other) const {
        return col == other.col && row == other.row;
    }
    bool operator!=(const Square& other) const {
        return !(*this == other);
    }
    
    bool isValid() const {
        return col < BOARD_COLS && row < BOARD_ROWS;
    }
    
    int index() const {
        return row * BOARD_COLS + col;
    }
    
    static Square fromIndex(int idx) {
        return Square(idx % BOARD_COLS, idx / BOARD_COLS);
    }
    
    std::string toString() const {
        return std::string(1, 'A' + col) + std::to_string(row + 1);
    }
    
    static std::optional<Square> fromString(const std::string& s) {
        if (s.length() < 2) return std::nullopt;
        int c = toupper(s[0]) - 'A';
        int r = s[1] - '1';
        if (c < 0 || c >= BOARD_COLS || r < 0 || r >= BOARD_ROWS) return std::nullopt;
        return Square(c, r);
    }
};

struct Piece {
    PieceType type;
    Color color;
    
    Piece() : type(PieceType::RAT), color(Color::LIGHT) {}
    Piece(PieceType t, Color c) : type(t), color(c) {}
    
    bool operator==(const Piece& other) const {
        return type == other.type && color == other.color;
    }
};

struct Move {
    Square from;
    Square to;
    bool isCapture;
    PieceType capturedType;
    
    Move() : from(), to(), isCapture(false), capturedType(PieceType::RAT) {}
    Move(Square f, Square t) : from(f), to(t), isCapture(false), capturedType(PieceType::RAT) {}
    Move(Square f, Square t, PieceType captured) 
        : from(f), to(t), isCapture(true), capturedType(captured) {}
    
    bool operator==(const Move& other) const {
        return from == other.from && to == other.to;
    }
    
    bool operator!=(const Move& other) const {
        return from != other.from || to != other.to;
    }
    
    std::string toString() const {
        return from.toString() + to.toString();
    }
    
    static std::optional<Move> fromString(const std::string& s) {
        if (s.length() < 4) return std::nullopt;
        auto from = Square::fromString(s.substr(0, 2));
        auto to = Square::fromString(s.substr(2, 2));
        if (!from || !to) return std::nullopt;
        return Move(*from, *to);
    }
};

enum class GameResult : int16_t {
    ONGOING = 0,
    LIGHT_WIN = 32000,
    DARK_WIN = -32000,
    DRAW = 0
};

enum class SquareType : uint8_t {
    NORMAL = 0,
    WATER = 1,
    TRAP_LIGHT = 2,
    TRAP_DARK = 3,
    DEN_LIGHT = 4,
    DEN_DARK = 5
};

inline SquareType getSquareType(const Square& sq) {
    static const SquareType board[BOARD_ROWS][BOARD_COLS] = {
        {SquareType::NORMAL, SquareType::NORMAL, SquareType::TRAP_LIGHT, SquareType::DEN_LIGHT, SquareType::TRAP_LIGHT, SquareType::NORMAL, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::TRAP_LIGHT, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::WATER, SquareType::WATER, SquareType::NORMAL, SquareType::WATER, SquareType::WATER, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::WATER, SquareType::WATER, SquareType::NORMAL, SquareType::WATER, SquareType::WATER, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::WATER, SquareType::WATER, SquareType::NORMAL, SquareType::WATER, SquareType::WATER, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL, SquareType::TRAP_DARK, SquareType::NORMAL, SquareType::NORMAL, SquareType::NORMAL},
        {SquareType::NORMAL, SquareType::NORMAL, SquareType::TRAP_DARK, SquareType::DEN_DARK, SquareType::TRAP_DARK, SquareType::NORMAL, SquareType::NORMAL}
    };
    return board[sq.row][sq.col];
}

inline bool isWater(const Square& sq) {
    return getSquareType(sq) == SquareType::WATER;
}

inline bool isTrap(const Square& sq) {
    auto t = getSquareType(sq);
    return t == SquareType::TRAP_LIGHT || t == SquareType::TRAP_DARK;
}

inline bool isEnemyTrap(const Square& sq, Color c) {
    auto t = getSquareType(sq);
    return (c == Color::LIGHT && t == SquareType::TRAP_DARK) ||
           (c == Color::DARK && t == SquareType::TRAP_LIGHT);
}

inline bool isOwnDen(const Square& sq, Color c) {
    auto t = getSquareType(sq);
    return (c == Color::LIGHT && t == SquareType::DEN_LIGHT) ||
           (c == Color::DARK && t == SquareType::DEN_DARK);
}

inline bool isEnemyDen(const Square& sq, Color c) {
    auto t = getSquareType(sq);
    return (c == Color::LIGHT && t == SquareType::DEN_DARK) ||
           (c == Color::DARK && t == SquareType::DEN_LIGHT);
}

inline bool isDen(const Square& sq) {
    auto t = getSquareType(sq);
    return t == SquareType::DEN_LIGHT || t == SquareType::DEN_DARK;
}

using MoveList = std::vector<Move>;

constexpr int INF = 32767;
constexpr int MATE_SCORE = 32000;

}

#endif
