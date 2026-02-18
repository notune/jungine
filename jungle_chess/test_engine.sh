#!/bin/bash
# Test script for Jungle Chess Engine

ENGINE_DIR="engine"
ENGINE="$ENGINE_DIR/jungle_engine"

echo "=== Jungle Chess Engine Test ==="
echo ""

cd "$(dirname "$0")"

# Test 1: Basic engine startup
echo "Test 1: Engine startup and UCI..."
echo -e "uci\nisready\nquit" | ./$ENGINE | head -5
echo ""

# Test 2: Move generation
echo "Test 2: Move generation from initial position..."
echo -e "position startpos\nmoves\nquit" | ./$ENGINE | head -3
echo ""

# Test 3: Perft test
echo "Test 3: Perft (move generation performance)..."
echo -e "position startpos\nperft 4\nquit" | ./$ENGINE
echo ""

# Test 4: Short search
echo "Test 4: Search at depth 5..."
echo -e "position startpos\ngo depth 5\nquit" | ./$ENGINE
echo ""

# Test 5: Midgame position
echo "Test 5: Midgame search..."
echo -e "position startpos moves G3G4 G7F7 G4G5 F7G7 A1A2\ngo movetime 200\nquit" | ./$ENGINE
echo ""

# Test 6: Position display
echo "Test 6: Board display..."
echo -e "position startpos\nprint\nquit" | ./$ENGINE
echo ""

echo "=== All tests completed ==="
