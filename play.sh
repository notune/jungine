#!/bin/bash
# Launch the Jungle Chess GUI
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"
python3 ui/jungle_gui.py "$@"
