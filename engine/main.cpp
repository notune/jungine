#include "jungle.hpp"
#include "search.hpp"
#include <iostream>
#include <string>
#include <sstream>

std::string sqToStr(int sq) {
    char f = 'a' + FILE_OF(sq);
    char r = '1' + RANK_OF(sq);
    std::string s;
    s += f;
    s += r;
    return s;
}

int strToSq(const std::string& str) {
    if (str.length() < 2) return -1;
    int f = str[0] - 'a';
    int r = str[1] - '1';
    if (f < 0 || f > 6 || r < 0 || r > 8) return -1;
    return SQ(f, r);
}

int main() {
    Board board;
    Search search;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "uji") {
            std::cout << "id name JungleAI" << std::endl;
            std::cout << "id author AI" << std::endl;
            std::cout << "ujiok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "position") {
            std::string subcmd;
            ss >> subcmd;
            if (subcmd == "startpos") {
                board.loadStartPos();
            } else if (subcmd == "fen") {
                // Not fully implemented yet
                board.loadStartPos();
            }
            std::string movesKeyword;
            ss >> movesKeyword;
            if (movesKeyword == "moves") {
                std::string moveStr;
                while (ss >> moveStr) {
                    int from = strToSq(moveStr.substr(0, 2));
                    int to = strToSq(moveStr.substr(2, 2));
                    if (from == -1 || to == -1) continue;
                    
                    std::vector<Move> legal;
                    board.generateMoves(legal);
                    Move m(from, to);
                    bool isLegal = false;
                    for(auto l : legal) {
                        if(l == m) { isLegal = true; break; }
                    }
                    if(isLegal) {
                        UndoInfo undo;
                        board.doMove(m, undo);
                    }
                }
            }
        } else if (cmd == "legalmoves") {
            std::vector<Move> moves;
            board.generateMoves(moves);
            std::cout << "legalmoves ";
            for (Move m : moves) {
                std::cout << sqToStr(m.from()) << sqToStr(m.to()) << " ";
            }
            std::cout << std::endl;
        } else if (cmd == "go") {
            std::string subcmd;
            int depth = 6;
            long long movetime = 3000; // default 3 seconds
            
            while (ss >> subcmd) {
                if (subcmd == "depth") {
                    ss >> depth;
                } else if (subcmd == "movetime") {
                    ss >> movetime;
                }
            }
            
            // Temporary simple search call
            // We want an iterative deepening search
            // The search class handles it. Let's call it with high depth and rely on time limit
            SearchResult res = search.search(board, 20, movetime);
            
            std::cout << "bestmove " << sqToStr(res.bestMove.from()) << sqToStr(res.bestMove.to()) << std::endl;
            
        } else if (cmd == "print") {
            board.print();
        } else if (cmd == "quit") {
            break;
        }
    }

    return 0;
}
