# Jungle Chess Engine

A strong AI engine for **Jungle Chess** (Dou Shou Qi / 鬥獸棋), a traditional Chinese strategy board game. The engine uses alpha-beta search with modern enhancements and communicates via a UCI-like text protocol. Includes a Python/tkinter GUI.

## Quick Start

```bash
# Build the engine
cd engine && make

# Play (launches GUI)
cd .. && ./play.sh
```

In the GUI, set Light/Dark to `human` or `engine`, adjust thinking time, and click pieces to move.

## Engine Protocol (JCEI)

The engine binary (`engine/jungle`) reads/writes stdin/stdout, similar to UCI:

```
jcei              → id + jceiok
isready           → readyok
position startpos [moves a1a2 ...]
position fen <fen> [moves ...]
go depth 20       → info ... + bestmove
go movetime 3000  → search for 3 seconds
stop / quit
d                 → display board
perft <n>         → node count validation
```

FEN format: ranks 9-1 top-to-bottom separated by `/`, pieces `RCDWPTLE` (upper=Light, lower=Dark), digits=empty squares, then `w`/`b`.

## Engine Internals

- Iterative deepening with aspiration windows
- PVS (Principal Variation Search) with correct 3-step re-search
- Transposition table (64MB default, Zobrist hashing)
- Null move pruning, LMR, futility/reverse futility pruning, razoring
- Killer moves + history heuristic move ordering
- Den-threat extensions, high-value capture extensions
- Quiescence search with delta pruning
- BFS-precomputed distance tables (land/swimmer/jumper) for evaluation
- Evaluation: material, piece-square tables, den proximity, trap control, rat-elephant dynamics, den safety

Reaches depth 18+ in ~2 seconds from the starting position on modern hardware.

## Requirements

- C++17 compiler (g++)
- Python 3 with tkinter (for GUI)
- Linux (tested on CachyOS)

## License

MIT License. See [LICENSE](LICENSE).
