#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <random>

// ---- Board Geometry ----
constexpr int BOARD_W = 7;
constexpr int BOARD_H = 9;
constexpr int NUM_SQ  = 63;

// Square index = row * 7 + col.  row 0 = rank "1" (bottom), col 0 = file 'a' (left).
inline int sqRow(int sq) { return sq / BOARD_W; }
inline int sqCol(int sq) { return sq % BOARD_W; }
constexpr int makeSq(int r, int c) { return r * BOARD_W + c; }

// ---- Piece Ranks (1-8, 0 = none) ----
constexpr int NONE_PC  = 0;
constexpr int RAT      = 1;
constexpr int CAT      = 2;
constexpr int DOG      = 3;
constexpr int WOLF     = 4;
constexpr int LEOPARD  = 5;
constexpr int TIGER    = 6;
constexpr int LION     = 7;
constexpr int ELEPHANT = 8;
constexpr int NUM_PIECE_TYPES = 9; // index 0 unused

// ---- Colours ----
constexpr int LIGHT = 0;
constexpr int DARK  = 1;

// ---- Terrain ----
constexpr int TERRAIN_LAND       = 0;
constexpr int TERRAIN_WATER      = 1;
constexpr int TERRAIN_TRAP_LIGHT = 2; // Light's trap (weakens Dark pieces)
constexpr int TERRAIN_TRAP_DARK  = 3; // Dark's trap  (weakens Light pieces)
constexpr int TERRAIN_DEN_LIGHT  = 4; // D1
constexpr int TERRAIN_DEN_DARK   = 5; // D9

// Named squares
constexpr int DEN_LIGHT_SQ = makeSq(0, 3); // D1 = 3
constexpr int DEN_DARK_SQ  = makeSq(8, 3); // D9 = 59

// ---- Directions ----
constexpr int DIR_N =  7;
constexpr int DIR_S = -7;
constexpr int DIR_E =  1;
constexpr int DIR_W = -1;
constexpr int DIRS[4] = { DIR_N, DIR_S, DIR_E, DIR_W };

// ---- Move encoding: from (bits 0-5), to (bits 6-11) ----
using Move = uint16_t;
constexpr Move MOVE_NONE = 0xFFFF;

inline int  moveFrom(Move m) { return m & 0x3F; }
inline int  moveTo  (Move m) { return (m >> 6) & 0x3F; }
inline Move encodeMove(int from, int to) { return (uint16_t)(from | (to << 6)); }

// ---- Score constants ----
constexpr int SCORE_INF   = 30000;
constexpr int SCORE_MATE  = 29000;
constexpr int SCORE_DRAW  = 0;
constexpr int MAX_PLY     = 128;
constexpr int MAX_MOVES   = 80;
constexpr int MAX_GAME_LEN = 2048;

// ---- String conversion ----
inline std::string sqToStr(int sq) {
    return std::string(1, 'a' + sqCol(sq)) + std::string(1, '1' + sqRow(sq));
}
inline int strToSq(const std::string& s) {
    if (s.size() < 2) return -1;
    int c = s[0] - 'a', r = s[1] - '1';
    if (c < 0 || c >= BOARD_W || r < 0 || r >= BOARD_H) return -1;
    return makeSq(r, c);
}
inline std::string moveToStr(Move m) {
    if (m == MOVE_NONE) return "0000";
    return sqToStr(moveFrom(m)) + sqToStr(moveTo(m));
}
inline Move strToMove(const std::string& s) {
    if (s.size() < 4) return MOVE_NONE;
    int f = strToSq(s.substr(0,2)), t = strToSq(s.substr(2,2));
    if (f < 0 || t < 0) return MOVE_NONE;
    return encodeMove(f, t);
}

// ---- Piece char (upper=Light, lower=Dark) ----
//   R=Rat C=Cat D=Dog W=Wolf P=Leopard T=Tiger L=Lion E=Elephant
inline char rankToChar(int rank) {
    constexpr char tbl[] = " RCDWPTLE";
    return (rank >= 1 && rank <= 8) ? tbl[rank] : '?';
}
inline int charToRank(char ch) {
    ch = (char)toupper(ch);
    constexpr char tbl[] = "RCDWPTLE";
    for (int i = 0; i < 8; i++) if (tbl[i] == ch) return i + 1;
    return 0;
}
inline char pieceChar(int rank, int color) {
    char c = rankToChar(rank);
    return color == DARK ? (char)tolower(c) : c;
}

// ---- Global tables (initialised once) ----
extern int  terrain[NUM_SQ];
extern bool isWater[NUM_SQ];
extern int  distLand[2][NUM_SQ];   // [den_side][sq] shortest distance on land
extern int  distJumper[2][NUM_SQ]; // [den_side][sq] shortest distance with jumps
extern int  distSwimmer[2][NUM_SQ];// [den_side][sq] shortest distance through water

// Jump table for lion/tiger
struct JumpEntry {
    int from, to;
    int blocking[3];   // water squares to check for rats
    int numBlocking;
};
constexpr int MAX_JUMPS = 40;
extern JumpEntry jumpTable[MAX_JUMPS];
extern int numJumps;

// Jump lookup per square
constexpr int MAX_JUMPS_PER_SQ = 4;
struct SqJumps {
    int dest[MAX_JUMPS_PER_SQ];
    int blockStart[MAX_JUMPS_PER_SQ]; // index into blocking squares
    int blockCount[MAX_JUMPS_PER_SQ];
    int blockingSqs[MAX_JUMPS_PER_SQ * 3]; // flattened
    int count;
};
extern SqJumps sqJumpLookup[NUM_SQ];

// Zobrist keys
extern uint64_t zobristPiece[NUM_SQ][NUM_PIECE_TYPES][2]; // [sq][rank][color]
extern uint64_t zobristSide;

// Material values (index = rank)
constexpr int MATERIAL_VAL[9] = {
    0,     // NONE
    400,   // RAT   - high strategic value (kills elephant, enters water)
    250,   // CAT
    300,   // DOG
    450,   // WOLF
    650,   // LEOPARD
    950,   // TIGER  - river jump
    1050,  // LION   - river jump, strongest practical piece
    1000   // ELEPHANT - strongest rank but vulnerable to rat
};

// Initialise all global tables
void initTables();

// Direction validity
inline bool canStep(int from, int dir) {
    int to = from + dir;
    if (to < 0 || to >= NUM_SQ) return false;
    if (dir == DIR_E && sqCol(from) == BOARD_W - 1) return false;
    if (dir == DIR_W && sqCol(from) == 0) return false;
    return true;
}
