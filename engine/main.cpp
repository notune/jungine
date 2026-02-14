#include "search.h"
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>

static Search engine;
static std::atomic<bool> searching(false);
static std::thread searchThread;

static void doSearch(int depth, int64_t movetime, bool infinite) {
    searching = true;
    Move best = engine.think(depth, movetime, infinite);
    std::printf("bestmove %s\n", moveToStr(best).c_str());
    fflush(stdout);
    searching = false;
}

static void cmdPosition(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "startpos") {
        engine.board.init();
        iss >> token; // consume "moves" if present
    } else if (token == "fen") {
        std::string fen;
        // Read FEN tokens until "moves" or end
        while (iss >> token) {
            if (token == "moves") break;
            if (!fen.empty()) fen += ' ';
            fen += token;
        }
        engine.board.setFEN(fen);
    }

    // Apply moves
    if (token == "moves") {
        while (iss >> token) {
            Move m = strToMove(token);
            if (m != MOVE_NONE) {
                engine.board.makeMove(m);
            }
        }
    }
}

static void cmdGo(std::istringstream& iss) {
    int depth = 0;
    int64_t movetime = 0;
    bool infinite = false;
    int64_t wtime = 0, btime = 0;

    std::string token;
    while (iss >> token) {
        if (token == "depth")    { iss >> depth; }
        else if (token == "movetime") { iss >> movetime; }
        else if (token == "infinite") { infinite = true; }
        else if (token == "wtime")    { iss >> wtime; }
        else if (token == "btime")    { iss >> btime; }
    }

    // Time management: if wtime/btime given, compute movetime
    if (wtime > 0 || btime > 0) {
        int64_t ourTime = (engine.board.sideToMove == LIGHT) ? wtime : btime;
        // Simple: use 1/30 of remaining time, min 100ms
        movetime = std::max((int64_t)100, ourTime / 30);
    }

    if (depth == 0 && movetime == 0 && !infinite) {
        movetime = 5000; // default 5 seconds
    }

    // Launch search in a separate thread so we can process "stop"
    if (searchThread.joinable()) searchThread.join();
    searchThread = std::thread(doSearch, depth, movetime, infinite);
}

int main() {
    initTables();
    engine.init(64); // 64 MB TT
    engine.board.init();

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "jcei" || cmd == "uci") {
            std::printf("id name JungleEngine 0.1\n");
            std::printf("id author Claude\n");
            std::printf("option name Hash type spin default 128 min 1 max 4096\n");
            std::printf("jceiok\n");
            fflush(stdout);
        }
        else if (cmd == "isready") {
            std::printf("readyok\n");
            fflush(stdout);
        }
        else if (cmd == "position") {
            cmdPosition(iss);
        }
        else if (cmd == "go") {
            cmdGo(iss);
        }
        else if (cmd == "stop") {
            engine.stop();
            if (searchThread.joinable()) searchThread.join();
        }
        else if (cmd == "quit" || cmd == "exit") {
            engine.stop();
            if (searchThread.joinable()) searchThread.join();
            break;
        }
        else if (cmd == "display" || cmd == "d") {
            engine.board.display();
            fflush(stdout);
        }
        else if (cmd == "perft") {
            int d = 1;
            iss >> d;
            auto t0 = std::chrono::steady_clock::now();
            uint64_t n = engine.board.perft(d);
            auto t1 = std::chrono::steady_clock::now();
            int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            std::printf("perft(%d) = %llu  (%lld ms)\n", d, (unsigned long long)n, (long long)ms);
            fflush(stdout);
        }
        else if (cmd == "eval") {
            int s = engine.board.evaluate();
            std::printf("eval = %d cp (from %s perspective)\n", s,
                        engine.board.sideToMove == LIGHT ? "Light" : "Dark");
            fflush(stdout);
        }
        else if (cmd == "moves") {
            Move moves[MAX_MOVES];
            int count = 0;
            engine.board.generateMoves(moves, count);
            std::printf("Legal moves (%d):", count);
            for (int i = 0; i < count; i++)
                std::printf(" %s", moveToStr(moves[i]).c_str());
            std::printf("\n");
            fflush(stdout);
        }
        else if (cmd == "setoption") {
            // Parse "setoption name Hash value 256"
            std::string tok;
            std::string name, value;
            while (iss >> tok) {
                if (tok == "name") { iss >> name; }
                else if (tok == "value") { iss >> value; }
            }
            if (name == "Hash") {
                int mb = std::stoi(value);
                engine.destroy();
                engine.init((size_t)mb);
            }
        }
        else if (cmd == "newgame" || cmd == "ucinewgame") {
            engine.destroy();
            engine.init(64);
            engine.board.init();
            engine.clearHistory();
        }
    }

    engine.destroy();
    return 0;
}
