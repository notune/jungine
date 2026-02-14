#pragma once
#include "types.h"
#include <vector>
#include <iostream>

struct UndoInfo {
    int8_t captured;    // signed piece on target before move (0 if empty)
    uint64_t hash;
    int halfmove;       // half-move clock for repetition / 50-move
    Move move;
};

class Board {
public:
    // Board state: 0=empty, +1..+8=Light piece, -1..-8=Dark piece
    int8_t squares[NUM_SQ];
    int    sideToMove;      // LIGHT or DARK
    uint64_t hash;
    int    ply;             // game ply (0 = start)
    int    halfmove;        // half-moves since last capture

    // Piece tracking: pieceSq[color][rank] = square (-1 if captured)
    int8_t pieceSq[2][NUM_PIECE_TYPES];
    int    pieceCount[2];   // alive piece count per side

    // Undo stack
    UndoInfo undoStack[MAX_GAME_LEN];

    // Position history for repetition detection
    uint64_t posHistory[MAX_GAME_LEN];
    int histLen;

    // ---- Methods ----
    void init();                                // starting position
    bool setFEN(const std::string& fen);
    std::string toFEN() const;

    void generateMoves(Move* moves, int& count) const;
    void generateCaptures(Move* moves, int& count) const;

    void makeMove(Move m);
    void unmakeMove();
    void makeNullMove();
    void unmakeNullMove();

    bool isRepetition() const;
    // Returns 0 = game not over, +1 = side-to-move just won (should not happen mid-search),
    //         -1 = side-to-move just lost (opponent reached den or we have no pieces)
    int  checkGameOver() const;

    int evaluate() const;

    uint64_t perft(int depth);
    void display() const;

private:
    void addNormalMoves(int sq, int rank, int color, Move* moves, int& count) const;
    void addJumpMoves(int sq, int rank, int color, Move* moves, int& count) const;
    bool canCapture(int attackerRank, int defenderRank, int attackerColor,
                    int fromSq, int toSq) const;
    void computeHash();
};
