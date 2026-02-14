#include "board.h"
#include <queue>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <cctype>

// ---- Global table storage ----
int  terrain[NUM_SQ];
bool isWater[NUM_SQ];
int  distLand[2][NUM_SQ];
int  distJumper[2][NUM_SQ];
int  distSwimmer[2][NUM_SQ];

JumpEntry jumpTable[MAX_JUMPS];
int numJumps = 0;
SqJumps sqJumpLookup[NUM_SQ];

uint64_t zobristPiece[NUM_SQ][NUM_PIECE_TYPES][2];
uint64_t zobristSide;

// ---- Piece-square tables (from Light's perspective, row 0 = Light's back rank) ----
// Index: [rank][sq]  (rank 0 unused)
static int PST[NUM_PIECE_TYPES][NUM_SQ];

// Base advancement + center values per square (Light perspective)
static int basePST[NUM_SQ];

// Distance tables for evaluation
static void computeBFS(int denSq, int type, int* dist);

// ========================================================================
//  initTables() - call once at startup
// ========================================================================
void initTables() {
    // ---- Terrain ----
    memset(terrain, TERRAIN_LAND, sizeof(terrain));
    memset(isWater, 0, sizeof(isWater));

    // Water squares
    int waterSqs[] = {22,23,29,30,36,37, 25,26,32,33,39,40};
    for (int sq : waterSqs) { terrain[sq] = TERRAIN_WATER; isWater[sq] = true; }

    // Dens
    terrain[DEN_LIGHT_SQ] = TERRAIN_DEN_LIGHT;
    terrain[DEN_DARK_SQ]  = TERRAIN_DEN_DARK;

    // Traps
    int lightTraps[] = { makeSq(0,2), makeSq(0,4), makeSq(1,3) }; // C1, E1, D2
    int darkTraps[]  = { makeSq(8,2), makeSq(8,4), makeSq(7,3) }; // C9, E9, D8
    for (int sq : lightTraps) terrain[sq] = TERRAIN_TRAP_LIGHT;
    for (int sq : darkTraps)  terrain[sq] = TERRAIN_TRAP_DARK;

    // ---- Jump table ----
    numJumps = 0;
    memset(sqJumpLookup, 0, sizeof(sqJumpLookup));

    auto addJump = [&](int from, int to, std::initializer_list<int> blocks) {
        JumpEntry& e = jumpTable[numJumps++];
        e.from = from; e.to = to;
        e.numBlocking = 0;
        for (int b : blocks) e.blocking[e.numBlocking++] = b;
    };

    // Left river horizontal jumps (cols B-C = 1,2;  rows 4-6 = index 3-5)
    for (int r = 3; r <= 5; r++) {
        int a = makeSq(r,0), b1 = makeSq(r,1), b2 = makeSq(r,2), d = makeSq(r,3);
        addJump(a, d, {b1, b2});   // A->D east
        addJump(d, a, {b2, b1});   // D->A west
    }
    // Right river horizontal jumps (cols E-F = 4,5;  rows 4-6)
    for (int r = 3; r <= 5; r++) {
        int d = makeSq(r,3), e1 = makeSq(r,4), e2 = makeSq(r,5), g = makeSq(r,6);
        addJump(d, g, {e1, e2});   // D->G east
        addJump(g, d, {e2, e1});   // G->D west
    }
    // Left river vertical jumps (cols B,C; rows 3->7 over rows 4,5,6)
    for (int c = 1; c <= 2; c++) {
        int bot = makeSq(2,c), top = makeSq(6,c);
        int w1 = makeSq(3,c), w2 = makeSq(4,c), w3 = makeSq(5,c);
        addJump(bot, top, {w1, w2, w3}); // row3 -> row7 north
        addJump(top, bot, {w3, w2, w1}); // row7 -> row3 south
    }
    // Right river vertical jumps (cols E,F)
    for (int c = 4; c <= 5; c++) {
        int bot = makeSq(2,c), top = makeSq(6,c);
        int w1 = makeSq(3,c), w2 = makeSq(4,c), w3 = makeSq(5,c);
        addJump(bot, top, {w1, w2, w3});
        addJump(top, bot, {w3, w2, w1});
    }

    // Build per-square jump lookup
    for (int i = 0; i < numJumps; i++) {
        const JumpEntry& e = jumpTable[i];
        SqJumps& sj = sqJumpLookup[e.from];
        int idx = sj.count;
        sj.dest[idx] = e.to;
        sj.blockStart[idx] = (idx > 0) ? sj.blockStart[idx-1] + sj.blockCount[idx-1] : 0;
        sj.blockCount[idx] = e.numBlocking;
        for (int j = 0; j < e.numBlocking; j++)
            sj.blockingSqs[sj.blockStart[idx] + j] = e.blocking[j];
        sj.count++;
    }

    // ---- Zobrist keys ----
    std::mt19937_64 rng(0xDEADBEEF42ULL);
    for (int sq = 0; sq < NUM_SQ; sq++)
        for (int rk = 1; rk < NUM_PIECE_TYPES; rk++)
            for (int c = 0; c < 2; c++)
                zobristPiece[sq][rk][c] = rng();
    zobristSide = rng();

    // ---- BFS distance tables ----
    // type 0 = land-only, type 1 = jumper (land + jumps), type 2 = swimmer (land + water)
    for (int den = 0; den < 2; den++) {
        int denSq = (den == 0) ? DEN_LIGHT_SQ : DEN_DARK_SQ;
        computeBFS(denSq, 0, distLand[den]);
        computeBFS(denSq, 1, distJumper[den]);
        computeBFS(denSq, 2, distSwimmer[den]);
    }

    // ---- Piece-square tables ----
    // Base PST: advancement + center gradient (from Light perspective)
    const int rowBonus[9]  = { -5, 0, 5, 15, 25, 35, 55, 85, 120 };
    const int colBonus[7]  = { 0, 5, 15, 30, 15, 5, 0 };

    for (int sq = 0; sq < NUM_SQ; sq++) {
        if (isWater[sq]) { basePST[sq] = 0; continue; }
        int r = sqRow(sq), c = sqCol(sq);
        basePST[sq] = rowBonus[r] + colBonus[c];
    }

    // Per-piece PSTs  (adjustments on top of base)
    for (int sq = 0; sq < NUM_SQ; sq++) {
        for (int rk = 1; rk <= 8; rk++) {
            PST[rk][sq] = basePST[sq];
        }
    }

    // Rats get bonus for water squares and advanced positions
    for (int sq = 0; sq < NUM_SQ; sq++) {
        if (isWater[sq]) PST[RAT][sq] = 20 + sqRow(sq) * 5;
    }

    // Lions/tigers get extra bonus for river-adjacent squares (can jump)
    for (int i = 0; i < numJumps; i++) {
        PST[LION][jumpTable[i].from]  += 15;
        PST[TIGER][jumpTable[i].from] += 15;
    }

    // Emphasise den-proximity for all pieces (beyond the row bonus)
    for (int sq = 0; sq < NUM_SQ; sq++) {
        if (isWater[sq]) continue;
        int dd = distLand[1][sq]; // distance to dark den (for Light)
        if (dd <= 8) {
            int bonus = std::max(0, 130 - dd * 15);
            for (int rk = 1; rk <= 8; rk++)
                PST[rk][sq] += bonus;
        }
    }
}

// BFS distance computation
static void computeBFS(int denSq, int type, int* dist) {
    for (int i = 0; i < NUM_SQ; i++) dist[i] = 99;
    std::queue<int> q;
    dist[denSq] = 0;
    q.push(denSq);

    while (!q.empty()) {
        int sq = q.front(); q.pop();
        int nd = dist[sq] + 1;

        // Normal 4-directional moves
        for (int d : DIRS) {
            if (!canStep(sq, d)) continue;
            int ns = sq + d;
            if (type == 0 && isWater[ns]) continue;         // land-only
            if (type == 1 && isWater[ns]) continue;         // jumper: land only for steps
            // type 2 (swimmer): can traverse water
            if (nd < dist[ns]) { dist[ns] = nd; q.push(ns); }
        }

        // Jumps (type 1 = jumper)
        if (type == 1) {
            const SqJumps& sj = sqJumpLookup[sq];
            for (int i = 0; i < sj.count; i++) {
                int ns = sj.dest[i];
                if (nd < dist[ns]) { dist[ns] = nd; q.push(ns); }
            }
        }
    }
}

// ========================================================================
//  Board::init() - starting position
// ========================================================================
void Board::init() {
    memset(squares, 0, sizeof(squares));
    memset(pieceSq, -1, sizeof(pieceSq));
    pieceCount[LIGHT] = 8;
    pieceCount[DARK]  = 8;
    sideToMove = LIGHT;
    ply = 0;
    halfmove = 0;
    histLen = 0;

    // Light pieces (positive)
    auto place = [&](int color, int rank, int sq) {
        squares[sq] = (int8_t)(color == LIGHT ? rank : -rank);
        pieceSq[color][rank] = (int8_t)sq;
    };

    // Bottom player (Light) - rows 1-3
    place(LIGHT, ELEPHANT, makeSq(2,0)); // A3
    place(LIGHT, LION,     makeSq(0,6)); // G1
    place(LIGHT, TIGER,    makeSq(0,0)); // A1
    place(LIGHT, LEOPARD,  makeSq(2,4)); // E3
    place(LIGHT, WOLF,     makeSq(2,2)); // C3
    place(LIGHT, DOG,      makeSq(1,5)); // F2
    place(LIGHT, CAT,      makeSq(1,1)); // B2
    place(LIGHT, RAT,      makeSq(2,6)); // G3

    // Top player (Dark) - rows 7-9
    place(DARK, ELEPHANT, makeSq(6,6)); // G7
    place(DARK, LION,     makeSq(8,0)); // A9
    place(DARK, TIGER,    makeSq(8,6)); // G9
    place(DARK, LEOPARD,  makeSq(6,2)); // C7
    place(DARK, WOLF,     makeSq(6,4)); // E7
    place(DARK, DOG,      makeSq(7,1)); // B8
    place(DARK, CAT,      makeSq(7,5)); // F8
    place(DARK, RAT,      makeSq(6,0)); // A7

    computeHash();
    posHistory[histLen++] = hash;
}

// ========================================================================
//  FEN parsing / output
// ========================================================================
// FEN: ranks 9..1 separated by '/', then space, then 'w' or 'b'
// Pieces: R=Rat C=Cat D=Dog W=Wolf P=Leopard T=Tiger L=Lion E=Elephant
// Uppercase=Light, lowercase=Dark.  Digits=empty squares.
bool Board::setFEN(const std::string& fen) {
    memset(squares, 0, sizeof(squares));
    memset(pieceSq, -1, sizeof(pieceSq));
    pieceCount[LIGHT] = 0;
    pieceCount[DARK]  = 0;
    ply = 0;
    halfmove = 0;
    histLen = 0;

    size_t idx = 0;
    int row = 8; // start from top row (row 9 = index 8)
    int col = 0;

    while (idx < fen.size() && row >= 0) {
        char ch = fen[idx++];
        if (ch == '/') { row--; col = 0; continue; }
        if (ch == ' ') break;
        if (ch >= '1' && ch <= '7') { col += (ch - '0'); continue; }

        int rk = charToRank(ch);
        if (rk == 0) return false;
        int color = isupper(ch) ? LIGHT : DARK;
        int sq = makeSq(row, col);
        squares[sq] = (int8_t)(color == LIGHT ? rk : -rk);
        pieceSq[color][rk] = (int8_t)sq;
        pieceCount[color]++;
        col++;
    }

    // Side to move
    while (idx < fen.size() && fen[idx] == ' ') idx++;
    if (idx < fen.size()) {
        sideToMove = (fen[idx] == 'b') ? DARK : LIGHT;
    } else {
        sideToMove = LIGHT;
    }

    computeHash();
    posHistory[histLen++] = hash;
    return true;
}

std::string Board::toFEN() const {
    std::string fen;
    for (int r = 8; r >= 0; r--) {
        int empty = 0;
        for (int c = 0; c < 7; c++) {
            int sq = makeSq(r, c);
            if (squares[sq] == 0) { empty++; continue; }
            if (empty > 0) { fen += (char)('0' + empty); empty = 0; }
            int rk = abs(squares[sq]);
            int color = squares[sq] > 0 ? LIGHT : DARK;
            fen += pieceChar(rk, color);
        }
        if (empty > 0) fen += (char)('0' + empty);
        if (r > 0) fen += '/';
    }
    fen += ' ';
    fen += (sideToMove == LIGHT) ? 'w' : 'b';
    return fen;
}

// ========================================================================
//  Zobrist hash
// ========================================================================
void Board::computeHash() {
    hash = 0;
    for (int sq = 0; sq < NUM_SQ; sq++) {
        if (squares[sq] != 0) {
            int rk = abs(squares[sq]);
            int col = squares[sq] > 0 ? LIGHT : DARK;
            hash ^= zobristPiece[sq][rk][col];
        }
    }
    if (sideToMove == DARK) hash ^= zobristSide;
}

// ========================================================================
//  Capture legality
// ========================================================================
bool Board::canCapture(int attackerRank, int defenderRank, int attackerColor,
                       int fromSq, int toSq) const {
    bool fromW = isWater[fromSq];
    bool toW   = isWater[toSq];

    // Can't capture across water/land boundary
    if (fromW != toW) return false;

    // Both in water: both must be rats => can capture
    if (fromW && toW) return true;

    // Both on land: check trap rule
    if (attackerColor == LIGHT && terrain[toSq] == TERRAIN_TRAP_LIGHT) return true;
    if (attackerColor == DARK  && terrain[toSq] == TERRAIN_TRAP_DARK)  return true;

    // Rat vs Elephant
    if (attackerRank == RAT && defenderRank == ELEPHANT) return true;
    if (attackerRank == ELEPHANT && defenderRank == RAT) return false;

    // Normal
    return attackerRank >= defenderRank;
}

// ========================================================================
//  Move generation
// ========================================================================
void Board::addNormalMoves(int sq, int rank, int color, Move* moves, int& count) const {
    for (int d : DIRS) {
        if (!canStep(sq, d)) continue;
        int to = sq + d;

        int ter = terrain[to];

        // Can't enter own den
        if (color == LIGHT && ter == TERRAIN_DEN_LIGHT) continue;
        if (color == DARK  && ter == TERRAIN_DEN_DARK)  continue;

        // Only rat can enter water
        if (ter == TERRAIN_WATER && rank != RAT) continue;

        int target = squares[to];
        if (target == 0) {
            // Empty square: always OK
            moves[count++] = encodeMove(sq, to);
        } else {
            // Occupied: check if it's an opponent piece and we can capture
            int tColor = target > 0 ? LIGHT : DARK;
            if (tColor == color) continue; // own piece
            int tRank = abs(target);
            if (canCapture(rank, tRank, color, sq, to))
                moves[count++] = encodeMove(sq, to);
        }
    }
}

void Board::addJumpMoves(int sq, int rank, int color, Move* moves, int& count) const {
    const SqJumps& sj = sqJumpLookup[sq];
    for (int i = 0; i < sj.count; i++) {
        int to = sj.dest[i];

        // Can't enter own den (unlikely from a jump but check anyway)
        if (color == LIGHT && terrain[to] == TERRAIN_DEN_LIGHT) continue;
        if (color == DARK  && terrain[to] == TERRAIN_DEN_DARK)  continue;

        // Check that no rat blocks the path
        bool blocked = false;
        int bStart = sj.blockStart[i];
        for (int j = 0; j < sj.blockCount[i]; j++) {
            if (squares[sj.blockingSqs[bStart + j]] != 0) {
                blocked = true;
                break;
            }
        }
        if (blocked) continue;

        int target = squares[to];
        if (target == 0) {
            moves[count++] = encodeMove(sq, to);
        } else {
            int tColor = target > 0 ? LIGHT : DARK;
            if (tColor == color) continue;
            int tRank = abs(target);
            // Jump lands on land, from land: normal capture rules apply
            if (canCapture(rank, tRank, color, sq, to))
                moves[count++] = encodeMove(sq, to);
        }
    }
}

void Board::generateMoves(Move* moves, int& count) const {
    count = 0;
    int color = sideToMove;
    for (int rk = 1; rk <= 8; rk++) {
        int sq = pieceSq[color][rk];
        if (sq < 0) continue; // captured

        // Normal 1-step moves
        addNormalMoves(sq, rk, color, moves, count);

        // Jump moves for lion and tiger
        if (rk == LION || rk == TIGER) {
            addJumpMoves(sq, rk, color, moves, count);
        }
    }
}

void Board::generateCaptures(Move* moves, int& count) const {
    count = 0;
    int color = sideToMove;
    for (int rk = 1; rk <= 8; rk++) {
        int sq = pieceSq[color][rk];
        if (sq < 0) continue;

        // Normal captures
        for (int d : DIRS) {
            if (!canStep(sq, d)) continue;
            int to = sq + d;
            int ter = terrain[to];
            if (color == LIGHT && ter == TERRAIN_DEN_LIGHT) continue;
            if (color == DARK  && ter == TERRAIN_DEN_DARK)  continue;
            if (ter == TERRAIN_WATER && rk != RAT) continue;

            int target = squares[to];
            if (target == 0) continue;
            int tColor = target > 0 ? LIGHT : DARK;
            if (tColor == color) continue;
            int tRank = abs(target);
            if (canCapture(rk, tRank, color, sq, to))
                moves[count++] = encodeMove(sq, to);
        }

        // Jump captures for lion/tiger
        if (rk == LION || rk == TIGER) {
            const SqJumps& sj = sqJumpLookup[sq];
            for (int i = 0; i < sj.count; i++) {
                int to = sj.dest[i];
                if (color == LIGHT && terrain[to] == TERRAIN_DEN_LIGHT) continue;
                if (color == DARK  && terrain[to] == TERRAIN_DEN_DARK)  continue;

                int target = squares[to];
                if (target == 0) continue;
                int tColor = target > 0 ? LIGHT : DARK;
                if (tColor == color) continue;

                bool blocked = false;
                int bStart = sj.blockStart[i];
                for (int j = 0; j < sj.blockCount[i]; j++) {
                    if (squares[sj.blockingSqs[bStart + j]] != 0) {
                        blocked = true; break;
                    }
                }
                if (blocked) continue;

                int tRank = abs(target);
                if (canCapture(rk, tRank, color, sq, to))
                    moves[count++] = encodeMove(sq, to);
            }
        }
    }
}

// ========================================================================
//  Make / unmake
// ========================================================================
void Board::makeMove(Move m) {
    int from = moveFrom(m);
    int to   = moveTo(m);
    int piece = squares[from];
    int rk    = abs(piece);
    int color = piece > 0 ? LIGHT : DARK;

    // Save undo info
    UndoInfo& u = undoStack[ply];
    u.captured = squares[to];
    u.hash     = hash;
    u.halfmove = halfmove;
    u.move     = m;

    // Handle capture
    if (u.captured != 0) {
        int cRk   = abs(u.captured);
        int cCol  = u.captured > 0 ? LIGHT : DARK;
        pieceSq[cCol][cRk] = -1;
        pieceCount[cCol]--;
        hash ^= zobristPiece[to][cRk][cCol];
        halfmove = 0;
    } else {
        halfmove++;
    }

    // Move piece
    hash ^= zobristPiece[from][rk][color];
    hash ^= zobristPiece[to][rk][color];
    squares[to]   = piece;
    squares[from] = 0;
    pieceSq[color][rk] = (int8_t)to;

    // Flip side
    sideToMove = 1 - sideToMove;
    hash ^= zobristSide;
    ply++;

    posHistory[histLen++] = hash;
}

void Board::unmakeMove() {
    ply--;
    histLen--;

    const UndoInfo& u = undoStack[ply];
    int from = moveFrom(u.move);
    int to   = moveTo(u.move);

    sideToMove = 1 - sideToMove;
    hash = u.hash;
    halfmove = u.halfmove;

    int piece = squares[to];
    int rk    = abs(piece);
    int color = piece > 0 ? LIGHT : DARK;

    // Move piece back
    squares[from] = piece;
    squares[to]   = u.captured;
    pieceSq[color][rk] = (int8_t)from;

    // Restore captured piece
    if (u.captured != 0) {
        int cRk  = abs(u.captured);
        int cCol = u.captured > 0 ? LIGHT : DARK;
        pieceSq[cCol][cRk] = (int8_t)to;
        pieceCount[cCol]++;
    }
}

void Board::makeNullMove() {
    UndoInfo& u = undoStack[ply];
    u.captured = 0;
    u.hash     = hash;
    u.halfmove = halfmove;
    u.move     = MOVE_NONE;

    sideToMove = 1 - sideToMove;
    hash ^= zobristSide;
    ply++;
    posHistory[histLen++] = hash;
}

void Board::unmakeNullMove() {
    ply--;
    histLen--;
    const UndoInfo& u = undoStack[ply];
    sideToMove = 1 - sideToMove;
    hash = u.hash;
    halfmove = u.halfmove;
}

// ========================================================================
//  Repetition detection
// ========================================================================
bool Board::isRepetition() const {
    if (histLen < 5) return false;
    int count = 0;
    for (int i = histLen - 3; i >= 0; i -= 2) {
        if (posHistory[i] == hash) {
            count++;
            if (count >= 2) return true; // 3-fold
        }
    }
    return false;
}

// ========================================================================
//  Game-over detection  (called at the top of search before generating moves)
// ========================================================================
int Board::checkGameOver() const {
    int opp = 1 - sideToMove;
    // Did the opponent (who just moved) reach our den?
    // If a piece of `opp` color is on our den, opp wins => we lost => return -1
    int ourDen = (sideToMove == LIGHT) ? DEN_LIGHT_SQ : DEN_DARK_SQ;
    if (squares[ourDen] != 0) {
        // A piece is on our den
        int pc = squares[ourDen];
        int pcColor = pc > 0 ? LIGHT : DARK;
        if (pcColor == opp) return -1; // we lost
    }

    // Do we have any pieces? If not, we lost.
    if (pieceCount[sideToMove] == 0) return -1;

    // Did opponent lose all pieces? If so, we win.
    if (pieceCount[opp] == 0) return 1;

    return 0; // game continues
}

// ========================================================================
//  Static evaluation (from side-to-move's perspective)
// ========================================================================
int Board::evaluate() const {
    int score = 0;
    int stm = sideToMove;
    int opp = 1 - stm;

    // ---- Material + PST ----
    for (int color = 0; color < 2; color++) {
        int sign = (color == stm) ? 1 : -1;
        for (int rk = 1; rk <= 8; rk++) {
            int sq = pieceSq[color][rk];
            if (sq < 0) continue;

            // Material
            score += sign * MATERIAL_VAL[rk];

            // PST (oriented for this color)
            int pstSq = (color == LIGHT) ? sq : (NUM_SQ - 1 - sq);
            score += sign * PST[rk][pstSq];
        }
    }

    // ---- Den proximity bonus (extra beyond PST) ----
    // For our pieces: bonus for being close to opponent den
    // For opponent pieces: penalty for being close to our den
    for (int color = 0; color < 2; color++) {
        int sign = (color == stm) ? 1 : -1;
        int targetDen = (color == LIGHT) ? 1 : 0; // which den we're attacking
        for (int rk = 1; rk <= 8; rk++) {
            int sq = pieceSq[color][rk];
            if (sq < 0) continue;

            // Pick appropriate distance table
            int d;
            if (rk == RAT) d = distSwimmer[targetDen][sq];
            else if (rk == LION || rk == TIGER) d = distJumper[targetDen][sq];
            else d = distLand[targetDen][sq];

            // Exponential bonus for proximity
            if (d <= 1) score += sign * 250;
            else if (d == 2) score += sign * 120;
            else if (d == 3) score += sign * 60;
            else if (d <= 5) score += sign * 20;
        }
    }

    // ---- Trap control ----
    // Opponent piece on our trap = good (it's weakened)
    // Our piece on opponent's trap = bad (we're weakened)
    for (int sq = 0; sq < NUM_SQ; sq++) {
        if (squares[sq] == 0) continue;
        int pc = squares[sq];
        int pcColor = pc > 0 ? LIGHT : DARK;
        int pcRank  = abs(pc);

        if (pcColor == stm) {
            // Our piece on opponent's trap: penalty
            if ((stm == LIGHT && terrain[sq] == TERRAIN_TRAP_DARK) ||
                (stm == DARK  && terrain[sq] == TERRAIN_TRAP_LIGHT)) {
                score -= MATERIAL_VAL[pcRank] / 3;
            }
        } else {
            // Opponent piece on our trap: bonus
            if ((opp == LIGHT && terrain[sq] == TERRAIN_TRAP_DARK) ||
                (opp == DARK  && terrain[sq] == TERRAIN_TRAP_LIGHT)) {
                score += MATERIAL_VAL[pcRank] / 3;
            }
        }
    }

    // ---- Rat-Elephant dynamics ----
    // If we have a rat and opponent has elephant, that's a threat we possess
    if (pieceSq[stm][RAT] >= 0 && pieceSq[opp][ELEPHANT] >= 0) {
        int ratSq = pieceSq[stm][RAT];
        int eleSq = pieceSq[opp][ELEPHANT];
        int dr = abs(sqRow(ratSq) - sqRow(eleSq));
        int dc = abs(sqCol(ratSq) - sqCol(eleSq));
        int dist = dr + dc;
        score += 40; // having the threat
        if (dist <= 2) score += 60; // close threat
        if (dist == 1) score += 80; // adjacent = very dangerous
    }
    // If opponent has rat and we have elephant, that's a threat against us
    if (pieceSq[opp][RAT] >= 0 && pieceSq[stm][ELEPHANT] >= 0) {
        int ratSq = pieceSq[opp][RAT];
        int eleSq = pieceSq[stm][ELEPHANT];
        int dr = abs(sqRow(ratSq) - sqRow(eleSq));
        int dc = abs(sqCol(ratSq) - sqCol(eleSq));
        int dist = dr + dc;
        score -= 30;
        if (dist <= 2) score -= 40;
        if (dist == 1) score -= 60;
    }

    // ---- Den safety ----
    // Penalty for opponent pieces near our den
    int ourDenSq = (stm == LIGHT) ? DEN_LIGHT_SQ : DEN_DARK_SQ;
    for (int rk = 1; rk <= 8; rk++) {
        int sq = pieceSq[opp][rk];
        if (sq < 0) continue;
        int dr = abs(sqRow(sq) - sqRow(ourDenSq));
        int dc = abs(sqCol(sq) - sqCol(ourDenSq));
        int dist = dr + dc;
        if (dist <= 1) score -= 300;
        else if (dist == 2) score -= 100;
        else if (dist == 3) score -= 30;
    }

    // ---- Piece count advantage bonus ----
    int pieceDiff = pieceCount[stm] - pieceCount[opp];
    score += pieceDiff * 30;

    // ---- Endgame adjustment: emphasise den proximity when few pieces ----
    int totalPieces = pieceCount[0] + pieceCount[1];
    if (totalPieces <= 6) {
        // Recompute den proximity with higher weight
        for (int rk = 1; rk <= 8; rk++) {
            int sq = pieceSq[stm][rk];
            if (sq < 0) continue;
            int d;
            int targetDen = (stm == LIGHT) ? 1 : 0;
            if (rk == RAT) d = distSwimmer[targetDen][sq];
            else if (rk == LION || rk == TIGER) d = distJumper[targetDen][sq];
            else d = distLand[targetDen][sq];
            if (d <= 3) score += (4 - d) * 80;
        }
    }

    return score;
}

// ========================================================================
//  Perft
// ========================================================================
uint64_t Board::perft(int depth) {
    if (depth == 0) return 1;
    if (checkGameOver() != 0) return 0;

    Move moves[MAX_MOVES];
    int count = 0;
    generateMoves(moves, count);

    uint64_t nodes = 0;
    for (int i = 0; i < count; i++) {
        makeMove(moves[i]);
        nodes += perft(depth - 1);
        unmakeMove();
    }
    return nodes;
}

// ========================================================================
//  Display
// ========================================================================
void Board::display() const {
    std::printf("\n");
    for (int r = 8; r >= 0; r--) {
        std::printf("  %d ", r + 1);
        for (int c = 0; c < 7; c++) {
            int sq = makeSq(r, c);
            if (squares[sq] != 0) {
                int rk = abs(squares[sq]);
                int col = squares[sq] > 0 ? LIGHT : DARK;
                std::printf(" %c", pieceChar(rk, col));
            } else {
                char tc = '.';
                if (isWater[sq]) tc = '~';
                else if (terrain[sq] == TERRAIN_TRAP_LIGHT) tc = '^';
                else if (terrain[sq] == TERRAIN_TRAP_DARK)  tc = 'v';
                else if (terrain[sq] == TERRAIN_DEN_LIGHT)  tc = '*';
                else if (terrain[sq] == TERRAIN_DEN_DARK)   tc = '#';
                std::printf(" %c", tc);
            }
        }
        std::printf("\n");
    }
    std::printf("    ");
    for (int c = 0; c < 7; c++) std::printf(" %c", 'a' + c);
    std::printf("\n\n  %s to move\n", sideToMove == LIGHT ? "Light" : "Dark");
    std::printf("  FEN: %s\n\n", toFEN().c_str());
}
