#include "types.h"
#include "board.h"
#include "movegen.h"
#include "evaluate.h"
#include "search.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>

namespace jungle {

class Engine {
public:
    Engine() : board_(), search_() {
        board_.setInitialPosition();
    }
    
    void run() {
        std::string line;
        while (std::getline(std::cin, line)) {
            processCommand(line);
        }
    }
    
private:
    Board board_;
    Search search_;
    
    void processCommand(const std::string& line) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        
        if (cmd == "uci") {
            handleUci();
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            board_.setInitialPosition();
            search_.clearTT();
        } else if (cmd == "position") {
            handlePosition(iss);
        } else if (cmd == "go") {
            handleGo(iss);
        } else if (cmd == "stop") {
            search_.stop();
        } else if (cmd == "print" || cmd == "d") {
            board_.print();
        } else if (cmd == "eval") {
            int score = Evaluator::evaluate(board_);
            std::cout << "eval: " << score << std::endl;
        } else if (cmd == "moves") {
            MoveList moves;
            MoveGenerator::generateMoves(board_, moves);
            std::cout << "legal moves:";
            for (const auto& m : moves) {
                std::cout << " " << m.toString();
            }
            std::cout << std::endl;
        } else if (cmd == "perft") {
            int depth = 1;
            iss >> depth;
            handlePerft(depth);
        } else if (cmd == "quit") {
            exit(0);
        }
    }
    
    void handleUci() {
        std::cout << "id name JungleEngine 1.0" << std::endl;
        std::cout << "id author JungleChess" << std::endl;
        std::cout << "option name Hash type spin default 256 min 1 max 4096" << std::endl;
        std::cout << "uciok" << std::endl;
    }
    
    void handlePosition(std::istringstream& iss) {
        std::string token;
        iss >> token;
        
        if (token == "startpos") {
            board_.setInitialPosition();
        } else if (token == "fen") {
            handleFen(iss);
        }
        
        iss >> token;
        if (token == "moves") {
            while (iss >> token) {
                auto move = Move::fromString(token);
                if (move) {
                    applyMove(*move);
                }
            }
        }
    }
    
    void handleFen(std::istringstream& iss) {
        board_ = Board();
        
        std::string row;
        for (int r = 8; r >= 0; r--) {
            iss >> row;
            
            int c = 0;
            for (char ch : row) {
                if (ch >= '1' && ch <= '7') {
                    c += (ch - '0');
                } else {
                    Piece p = charToPiece(ch);
                    if (p.type != PieceType::RAT || ch == 'r' || ch == 'R') {
                        board_.setPiece(Square(c, r), p);
                    }
                    c++;
                }
            }
        }
        
        std::string side;
        iss >> side;
        board_.sideToMove = (side == "w" || side == "l") ? Color::LIGHT : Color::DARK;
        
        board_.pieceCount = {0, 0};
        for (int idx = 0; idx < BOARD_COLS * BOARD_ROWS; idx++) {
            auto p = board_.squares[idx];
            if (p) {
                board_.pieceCount[static_cast<int>(p->color)]++;
            }
        }
        
        board_.computeHash();
    }
    
    Piece charToPiece(char ch) {
        Color c = (ch >= 'a' && ch <= 'z') ? Color::LIGHT : Color::DARK;
        ch = tolower(ch);
        
        PieceType pt = PieceType::RAT;
        switch (ch) {
            case 'r': pt = PieceType::RAT; break;
            case 'c': pt = PieceType::CAT; break;
            case 'd': pt = PieceType::DOG; break;
            case 'w': pt = PieceType::WOLF; break;
            case 'p': pt = PieceType::LEOPARD; break;
            case 't': pt = PieceType::TIGER; break;
            case 'l': pt = PieceType::LION; break;
            case 'e': pt = PieceType::ELEPHANT; break;
        }
        
        return Piece(pt, c);
    }
    
    void applyMove(const Move& move) {
        auto target = board_.getPiece(move.to);
        if (target) {
            Move captureMove(move.from, move.to, target->type);
            board_.makeMove(captureMove);
        } else {
            board_.makeMove(move);
        }
    }
    
    void handleGo(std::istringstream& iss) {
        int64_t timeMs = 5000;
        int maxDepth = 20;
        
        std::string token;
        while (iss >> token) {
            if (token == "depth") {
                iss >> maxDepth;
            } else if (token == "movetime") {
                iss >> timeMs;
            } else if (token == "wtime" || token == "ltime") {
                int64_t t;
                iss >> t;
                timeMs = t / 30;
                timeMs = std::min(timeMs, int64_t(10000));
                timeMs = std::max(timeMs, int64_t(100));
            }
        }
        
        auto result = search_.search(board_, maxDepth, timeMs);
        
        std::cout << "bestmove " << result.bestMove.toString() << std::endl;
    }
    
    void handlePerft(int depth) {
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t nodes = perft(board_, depth);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        std::cout << "perft " << depth << ": " << nodes << " nodes (" << ms << " ms)" << std::endl;
        if (ms > 0) {
            std::cout << "nps: " << (nodes * 1000 / ms) << std::endl;
        }
    }
    
    uint64_t perft(Board& board, int depth) {
        if (depth == 0) return 1;
        
        MoveList moves;
        MoveGenerator::generateMoves(board, moves);
        
        if (depth == 1) return moves.size();
        
        uint64_t nodes = 0;
        for (const auto& m : moves) {
            board.makeMove(m);
            nodes += perft(board, depth - 1);
            board.unmakeMove(m);
        }
        
        return nodes;
    }
};

}

int main() {
    jungle::Engine engine;
    engine.run();
    return 0;
}
