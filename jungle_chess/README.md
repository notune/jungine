# Jungle Chess (Dou Shou Qi) Engine

A strong AI engine for Jungle Chess (also known as Dou Shou Qi or Animal Chess).

## Features

- **Alpha-Beta Search** with iterative deepening
- **Transposition Table** for position caching (256MB by default)
- **Advanced Evaluation** considering:
  - Material balance with piece-square tables
  - Positional factors (distance to enemy den, center control)
  - Trap control and den threat assessment
  - Piece coordination and king safety
  - Special piece bonuses (rat near elephant, jumper advancement)
- **Move Ordering** with:
  - Transposition table moves
  - MVV-LVA for captures
  - Killer moves heuristic
  - History heuristic
- **Late Move Reduction (LMR)** for faster deep searches
- **Aspiration Windows** for efficient search
- **Quiescence Search** to avoid horizon effects

## Performance

- Perft speed: ~130M nodes/second
- Search speed: ~2.7M nodes/second
- Reaches depth 8-10 in a few seconds on modern hardware

## Building

```bash
cd engine
make
```

Requires: g++ with C++17 support

## Usage

### Command Line Interface

The engine uses a UCI-like protocol:

```bash
./jungle_engine
```

Commands:
- `uci` - Initialize UCI mode
- `isready` - Check if engine is ready
- `ucinewgame` - Start a new game
- `position startpos` - Set initial position
- `position startpos moves <move1> <move2> ...` - Set position with moves
- `go depth <n>` - Search to depth n
- `go movetime <ms>` - Search for specified milliseconds
- `stop` - Stop searching
- `print` or `d` - Display current position
- `moves` - Show legal moves
- `eval` - Show position evaluation
- `perft <depth>` - Run perft test
- `quit` - Exit engine

### Example Session

```
> uci
id name JungleEngine 1.0
id author JungleChess
uciok

> isready
readyok

> position startpos
> go movetime 2000
info depth 1 score cp 86 nodes 51 time 0 nps 0 pv F2E2
...
bestmove G3G4

> quit
```

### Move Format

Moves are specified in coordinate notation: `<from><to>`

Example: `G3G4` means move from G3 to G4

Columns: A-G (left to right)
Rows: 1-9 (bottom to top)

## Running the GUI

```bash
cd jungle_chess
./run_gui.sh
```

Or directly:
```bash
python3 ui/jungle_gui.py
```

Requires: Python 3 with tkinter

## Game Rules

### Board
- 7x9 grid
- Two Dens (D1 for Light, D9 for Dark)
- Six Traps (surrounding each den)
- Two River areas (3x2 each in the center)

### Pieces (from strongest to weakest)
| Piece | Rank | Special Ability |
|-------|------|-----------------|
| Elephant | 8 | Strongest piece |
| Lion | 7 | Can jump over rivers |
| Tiger | 6 | Can jump over rivers |
| Leopard | 5 | - |
| Wolf | 4 | - |
| Dog | 3 | - |
| Cat | 2 | - |
| Rat | 1 | Can swim, captures Elephant |

### Movement
- All pieces move one square orthogonally (not diagonally)
- Pieces cannot move to their own den

### Capturing
- A piece can capture an opponent's piece of equal or lower rank
- **Special**: Rat can capture Elephant (but Elephant cannot capture Rat)
- A piece in an enemy trap can be captured by any piece
- Rat in water cannot capture land pieces (except enemy rat in water)
- Rat on land cannot capture rat in water

### River Rules
- Only Rat can enter water squares
- Lion and Tiger can jump over the river horizontally or vertically
- Jumps are blocked if any Rat (of either color) is in the path

### Win Conditions
1. Enter the opponent's den
2. Capture all opponent's pieces

## Initial Position

```
   A  B  C  D  E  F  G 
9  L  . ( )[ ]( ) .  T  9    (Dark pieces - uppercase)
8  .  D  . ( ) .  C  .  8
7  R  .  P  .  W  .  E  7
6  . {~}{~} . {~}{~} .  6    {~} = Water
5  . {~}{~} . {~}{~} .  5
4  . {~}{~} . {~}{~} .  4
3  e  .  w  .  p  .  r  3    (Light pieces - lowercase)
2  .  c  . ( ) .  d  .  2
1  t  . ( )[ ]( ) .  l  1
   A  B  C  D  E  F  G 
```

Legend:
- `[ ]` = Den
- `( )` = Trap
- `{~}` = Water
- Lowercase = Light pieces
- Uppercase = Dark pieces

## Project Structure

```
jungle_chess/
├── engine/
│   ├── types.h        - Core types and constants
│   ├── board.h        - Board representation and state
│   ├── movegen.h      - Move generation
│   ├── evaluate.h     - Position evaluation
│   ├── search.h       - Alpha-beta search
│   ├── tt.h           - Transposition table
│   ├── main.cpp       - Entry point and UCI handler
│   └── Makefile       - Build configuration
├── ui/
│   └── jungle_gui.py  - Tkinter GUI
├── run_gui.sh         - Script to launch GUI
└── test_engine.sh     - Engine test script
```

## License

MIT License - Feel free to use and modify.
