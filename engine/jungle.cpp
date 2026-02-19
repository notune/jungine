#include "jungle.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>

const int DIRS[4] = { -7, 1, 7, -1 };

static SquareType initSquareTypes[63];
uint64_t zobristPieces[2][9][63]; // [color][pieceType][square]
uint64_t zobristSide;

static void initSquares() {
    for (int i=0; i<63; ++i) initSquareTypes[i] = LAND;
    
    int waters[] = {
        SQ(1,3), SQ(1,4), SQ(1,5),
        SQ(2,3), SQ(2,4), SQ(2,5),
        SQ(4,3), SQ(4,4), SQ(4,5),
        SQ(5,3), SQ(5,4), SQ(5,5)
    };
    for(int sq : waters) initSquareTypes[sq] = WATER;

    initSquareTypes[SQ(3,0)] = DEN_LIGHT;
    initSquareTypes[SQ(3,8)] = DEN_DARK;

    initSquareTypes[SQ(2,0)] = TRAP_LIGHT;
    initSquareTypes[SQ(4,0)] = TRAP_LIGHT;
    initSquareTypes[SQ(3,1)] = TRAP_LIGHT;

    initSquareTypes[SQ(2,8)] = TRAP_DARK;
    initSquareTypes[SQ(4,8)] = TRAP_DARK;
    initSquareTypes[SQ(3,7)] = TRAP_DARK;
}

void initZobrist() {
    std::mt19937_64 rng(0x123456789ABCDEFULL);
    for(int c=0; c<2; ++c) {
        for(int p=1; p<=8; ++p) {
            for(int s=0; s<63; ++s) {
                zobristPieces[c][p][s] = rng();
            }
        }
    }
    zobristSide = rng();
}

const SquareType* getSquares() {
    static bool init = false;
    if(!init) { 
        initSquares(); 
        initZobrist();
        init=true; 
    }
    return initSquareTypes;
}

Board::Board() {
    loadStartPos();
}

void Board::loadStartPos() {
    getSquares();
    for (int i=0; i<63; ++i) pieces[i] = NO_PIECE;
    sideToMove = LIGHT;
    halfMoveClock = 0;
    fullMoveNumber = 1;

    pieces[SQ(0,0)] = {TIGER, LIGHT};
    pieces[SQ(6,0)] = {LION, LIGHT};
    pieces[SQ(1,1)] = {CAT, LIGHT};
    pieces[SQ(5,1)] = {DOG, LIGHT};
    pieces[SQ(2,2)] = {WOLF, LIGHT};
    pieces[SQ(4,2)] = {LEOPARD, LIGHT};
    pieces[SQ(0,2)] = {ELEPHANT, LIGHT};
    pieces[SQ(6,2)] = {RAT, LIGHT};

    pieces[SQ(6,8)] = {TIGER, DARK};
    pieces[SQ(0,8)] = {LION, DARK};
    pieces[SQ(5,7)] = {CAT, DARK};
    pieces[SQ(1,7)] = {DOG, DARK};
    pieces[SQ(4,6)] = {WOLF, DARK};
    pieces[SQ(2,6)] = {LEOPARD, DARK};
    pieces[SQ(6,6)] = {ELEPHANT, DARK};
    pieces[SQ(0,6)] = {RAT, DARK};

    currentHash = computeHash();
}

uint64_t Board::computeHash() const {
    uint64_t h = 0;
    for(int i=0; i<63; ++i) {
        if (pieces[i] != NO_PIECE) {
            h ^= zobristPieces[pieces[i].color][pieces[i].type][i];
        }
    }
    if (sideToMove == DARK) h ^= zobristSide;
    return h;
}

uint64_t Board::hash() const {
    return currentHash;
}

SquareType Board::getSquareType(int sq) const {
    return getSquares()[sq];
}

bool Board::canCapture(Piece p1, int sq1, Piece p2, int sq2) const {
    if (p1.color == p2.color) return false;
    if (p2 == NO_PIECE) return true;

    SquareType t2 = getSquareType(sq2);
    
    if (p1.color == LIGHT && t2 == TRAP_LIGHT) return true;
    if (p1.color == DARK && t2 == TRAP_DARK) return true;

    if (p1.type == RAT && p2.type == ELEPHANT) {
        SquareType t1 = getSquareType(sq1);
        if (t1 == WATER && t2 == LAND) return false;
        return true;
    }
    if (p1.type == ELEPHANT && p2.type == RAT) {
        return false;
    }

    if (p1.type == RAT && p2.type == RAT) {
        SquareType t1 = getSquareType(sq1);
        if (t1 == WATER && t2 == LAND) return false;
        if (t1 == LAND && t2 == WATER) return false;
        return true;
    }

    if (t2 == WATER) return false;

    return p1.type >= p2.type;
}

void Board::generateMoves(std::vector<Move>& moves) const {
    for (int sq = 0; sq < 63; ++sq) {
        Piece p = pieces[sq];
        if (p == NO_PIECE || p.color != sideToMove) continue;

        int f = FILE_OF(sq);
        int r = RANK_OF(sq);

        for (int i=0; i<4; ++i) {
            int nf = f + (i==1 ? 1 : (i==3 ? -1 : 0));
            int nr = r + (i==2 ? 1 : (i==0 ? -1 : 0));

            if (nf < 0 || nf > 6 || nr < 0 || nr > 8) continue;
            int nsq = SQ(nf, nr);
            SquareType nt = getSquareType(nsq);

            if (p.color == LIGHT && nt == DEN_LIGHT) continue;
            if (p.color == DARK && nt == DEN_DARK) continue;

            if (nt == WATER) {
                if (p.type == RAT) {
                    if (canCapture(p, sq, pieces[nsq], nsq)) moves.push_back(Move(sq, nsq));
                } else if (p.type == LION || p.type == TIGER) {
                    int jumpf = nf, jumpr = nr;
                    bool ratInPath = false;
                    while (jumpf >= 0 && jumpf <= 6 && jumpr >= 0 && jumpr <= 8) {
                        int jsq = SQ(jumpf, jumpr);
                        SquareType jt = getSquareType(jsq);
                        if (jt == WATER) {
                            if (pieces[jsq].type == RAT) {
                                ratInPath = true;
                                break;
                            }
                            jumpf += (i==1 ? 1 : (i==3 ? -1 : 0));
                            jumpr += (i==2 ? 1 : (i==0 ? -1 : 0));
                        } else {
                            if (!ratInPath) {
                                if (canCapture(p, sq, pieces[jsq], jsq)) {
                                    moves.push_back(Move(sq, jsq));
                                }
                            }
                            break;
                        }
                    }
                }
            } else {
                if (canCapture(p, sq, pieces[nsq], nsq)) {
                    moves.push_back(Move(sq, nsq));
                }
            }
        }
    }
}

void Board::doMove(Move m, UndoInfo& undo) {
    undo.move = m;
    undo.captured = pieces[m.to()];
    undo.halfMoveClock = halfMoveClock;
    undo.hash = currentHash;
    
    Piece p = pieces[m.from()];
    pieces[m.to()] = p;
    pieces[m.from()] = NO_PIECE;

    currentHash ^= zobristPieces[p.color][p.type][m.from()];
    currentHash ^= zobristPieces[p.color][p.type][m.to()];
    if (undo.captured != NO_PIECE) {
        currentHash ^= zobristPieces[undo.captured.color][undo.captured.type][m.to()];
        halfMoveClock = 0;
    } else {
        halfMoveClock++;
    }

    if (sideToMove == DARK) fullMoveNumber++;
    sideToMove = sideToMove == LIGHT ? DARK : LIGHT;
    currentHash ^= zobristSide;
}

void Board::undoMove(const UndoInfo& undo) {
    sideToMove = sideToMove == LIGHT ? DARK : LIGHT;
    if (sideToMove == DARK) fullMoveNumber--;

    pieces[undo.move.from()] = pieces[undo.move.to()];
    pieces[undo.move.to()] = undo.captured;
    halfMoveClock = undo.halfMoveClock;
    currentHash = undo.hash;
}

bool Board::isGameOver(int& winner) const {
    if (pieces[SQ(3,8)] != NO_PIECE && pieces[SQ(3,8)].color == LIGHT) { winner = LIGHT; return true; }
    if (pieces[SQ(3,0)] != NO_PIECE && pieces[SQ(3,0)].color == DARK) { winner = DARK; return true; }
    
    bool hasLight = false, hasDark = false;
    for(int i=0; i<63; ++i) {
        if(pieces[i].color == LIGHT) hasLight = true;
        if(pieces[i].color == DARK) hasDark = true;
    }
    if(!hasLight && !hasDark) { winner = NO_COLOR; return true; }
    if(!hasLight) { winner = DARK; return true; }
    if(!hasDark) { winner = LIGHT; return true; }
    
    if (halfMoveClock >= 100) { winner = NO_COLOR; return true; }
    
    return false;
}

int Board::evaluate() const {
    int w = 0;
    
    const int pieceValues[] = { 0, 100, 200, 300, 400, 500, 600, 700, 800 };

    for(int sq=0; sq<63; ++sq) {
        if (pieces[sq] == NO_PIECE) continue;
        int val = pieceValues[pieces[sq].type];
        
        int rank = RANK_OF(sq);
        int file = FILE_OF(sq);
        if (pieces[sq].color == LIGHT) {
            val += (rank * 10);
            val += (3 - std::abs(file - 3)) * 2;
            w += val;
        } else {
            val += ((8 - rank) * 10);
            val += (3 - std::abs(file - 3)) * 2;
            w -= val;
        }
    }
    return sideToMove == LIGHT ? w : -w;
}

std::string Board::getFen() const {
    std::stringstream ss;
    for(int r=8; r>=0; --r) {
        int empty = 0;
        for(int f=0; f<7; ++f) {
            Piece p = pieces[SQ(f,r)];
            if(p == NO_PIECE) {
                empty++;
            } else {
                if(empty > 0) { ss << empty; empty=0; }
                char c;
                switch(p.type) {
                    case RAT: c='r'; break;
                    case CAT: c='c'; break;
                    case DOG: c='d'; break;
                    case WOLF: c='w'; break;
                    case LEOPARD: c='l'; break;
                    case TIGER: c='t'; break;
                    case LION: c='i'; break;
                    case ELEPHANT: c='e'; break;
                    default: c='?'; break;
                }
                if(p.color == LIGHT) c = std::toupper(c);
                ss << c;
            }
        }
        if(empty > 0) ss << empty;
        if(r > 0) ss << '/';
    }
    ss << " " << (sideToMove == LIGHT ? "w" : "b") << " " << halfMoveClock << " " << fullMoveNumber;
    return ss.str();
}

void Board::print() const {
    for(int r=8; r>=0; --r) {
        std::cout << r+1 << " ";
        for(int f=0; f<7; ++f) {
            Piece p = pieces[SQ(f,r)];
            SquareType st = getSquareType(SQ(f,r));
            if(p == NO_PIECE) {
                if (st == WATER) std::cout << "~ ";
                else if (st == TRAP_LIGHT || st == TRAP_DARK) std::cout << "x ";
                else if (st == DEN_LIGHT || st == DEN_DARK) std::cout << "O ";
                else std::cout << ". ";
            } else {
                char c;
                switch(p.type) {
                    case RAT: c='r'; break;
                    case CAT: c='c'; break;
                    case DOG: c='d'; break;
                    case WOLF: c='w'; break;
                    case LEOPARD: c='l'; break;
                    case TIGER: c='t'; break;
                    case LION: c='i'; break;
                    case ELEPHANT: c='e'; break;
                    default: c='?'; break;
                }
                if(p.color == LIGHT) std::cout << (char)std::toupper(c) << " ";
                else std::cout << c << " ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "  A B C D E F G\n";
    std::cout << "Side to move: " << (sideToMove == LIGHT ? "Light" : "Dark") << "\n";
}
