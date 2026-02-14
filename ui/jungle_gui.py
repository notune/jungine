#!/usr/bin/env python3
"""
Jungle Chess (Dou Shou Qi) GUI
Communicates with the JungleEngine binary via stdin/stdout pipe.
"""

import tkinter as tk
from tkinter import messagebox, ttk
import subprocess
import threading
import os
import sys
import time

# ---- Constants ----
BOARD_COLS = 7
BOARD_ROWS = 9
SQ_SIZE = 72
MARGIN = 40
CANVAS_W = MARGIN * 2 + BOARD_COLS * SQ_SIZE
CANVAS_H = MARGIN * 2 + BOARD_ROWS * SQ_SIZE

# Terrain colours
COL_LAND       = "#c8b878"
COL_WATER      = "#4488cc"
COL_TRAP_LIGHT = "#d4a050"
COL_TRAP_DARK  = "#d4a050"
COL_DEN_LIGHT  = "#e8c840"
COL_DEN_DARK   = "#e8c840"
COL_SELECTED   = "#80ff80"
COL_HIGHLIGHT  = "#ffff80"
COL_LAST_MOVE  = "#aaddaa"

# Piece display
PIECE_NAMES = {
    1: "Rat", 2: "Cat", 3: "Dog", 4: "Wolf",
    5: "Leopard", 6: "Tiger", 7: "Lion", 8: "Elephant"
}
PIECE_SYMBOLS = {
    1: "\U0001F400", 2: "\U0001F408", 3: "\U0001F415", 4: "\U0001F43A",
    5: "\U0001F406", 6: "\U0001F405", 7: "\U0001F981", 8: "\U0001F418"
}
PIECE_LETTERS = {1: "R", 2: "C", 3: "D", 4: "W", 5: "P", 6: "T", 7: "L", 8: "E"}
PIECE_CHAR_SHORT = " RCDWPTLE"

# Terrain map
WATER_SQUARES = {
    (3,1),(3,2),(4,1),(4,2),(5,1),(5,2),  # left river
    (3,4),(3,5),(4,4),(4,5),(5,4),(5,5),  # right river
}
TRAP_LIGHT = {(0,2),(0,4),(1,3)}  # near D1
TRAP_DARK  = {(8,2),(8,4),(7,3)}  # near D9
DEN_LIGHT  = (0,3)
DEN_DARK   = (8,3)


def sq_name(row, col):
    """Convert (row,col) to algebraic notation like 'a1'."""
    return chr(ord('a') + col) + str(row + 1)


class EngineProcess:
    """Manages communication with the engine binary."""

    def __init__(self, engine_path):
        self.engine_path = engine_path
        self.proc = None
        self.lock = threading.Lock()

    def start(self):
        self.proc = subprocess.Popen(
            [self.engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        # Send init
        self.send("jcei")
        self.read_until("jceiok")
        self.send("isready")
        self.read_until("readyok")

    def send(self, cmd):
        if self.proc and self.proc.poll() is None:
            self.proc.stdin.write(cmd + "\n")
            self.proc.stdin.flush()

    def read_line(self):
        if self.proc and self.proc.poll() is None:
            return self.proc.stdout.readline().strip()
        return ""

    def read_until(self, token):
        """Read lines until one contains token. Return all lines."""
        lines = []
        while True:
            line = self.read_line()
            if not line and (self.proc is None or self.proc.poll() is not None):
                break
            lines.append(line)
            if token in line:
                break
        return lines

    def quit(self):
        if self.proc and self.proc.poll() is None:
            self.send("quit")
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    def set_position(self, moves_list):
        if moves_list:
            cmd = "position startpos moves " + " ".join(moves_list)
        else:
            cmd = "position startpos"
        self.send(cmd)

    def go(self, movetime_ms=3000, depth=0):
        if depth > 0:
            self.send(f"go depth {depth}")
        else:
            self.send(f"go movetime {movetime_ms}")

    def stop_search(self):
        self.send("stop")


class JungleGUI:
    def __init__(self, root, engine_path):
        self.root = root
        self.root.title("Jungle Chess - Dou Shou Qi")
        self.root.resizable(False, False)

        # Engine
        self.engine = EngineProcess(engine_path)
        self.engine.start()

        # Game state
        self.board = [[0]*BOARD_COLS for _ in range(BOARD_ROWS)]
        self.side_to_move = 0  # 0=Light, 1=Dark
        self.moves_list = []   # list of move strings for position command
        self.selected = None   # (row, col) of selected piece
        self.legal_targets = []  # list of (row, col) this piece can move to
        self.last_move = None  # ((from_r, from_c), (to_r, to_c))
        self.game_over = False
        self.engine_thinking = False

        # Settings
        self.player_light = "human"  # "human" or "engine"
        self.player_dark = "engine"
        self.engine_time_ms = 3000

        self._build_ui()
        self._init_board()
        self._draw_board()

        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        # If engine plays first
        if self.player_light == "engine":
            self.root.after(100, self._engine_move)

    def _build_ui(self):
        # Top frame with controls
        top = tk.Frame(self.root)
        top.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)

        tk.Label(top, text="Light:").pack(side=tk.LEFT)
        self.light_var = tk.StringVar(value="human")
        ttk.Combobox(top, textvariable=self.light_var, values=["human","engine"],
                     width=7, state="readonly").pack(side=tk.LEFT, padx=2)

        tk.Label(top, text="  Dark:").pack(side=tk.LEFT)
        self.dark_var = tk.StringVar(value="engine")
        ttk.Combobox(top, textvariable=self.dark_var, values=["human","engine"],
                     width=7, state="readonly").pack(side=tk.LEFT, padx=2)

        tk.Label(top, text="  Time(s):").pack(side=tk.LEFT)
        self.time_var = tk.StringVar(value="3")
        tk.Entry(top, textvariable=self.time_var, width=4).pack(side=tk.LEFT, padx=2)

        tk.Button(top, text="New Game", command=self._new_game).pack(side=tk.LEFT, padx=10)
        tk.Button(top, text="Undo", command=self._undo_move).pack(side=tk.LEFT, padx=2)

        # Canvas
        self.canvas = tk.Canvas(self.root, width=CANVAS_W, height=CANVAS_H, bg="#333")
        self.canvas.pack(side=tk.TOP, padx=5, pady=5)
        self.canvas.bind("<Button-1>", self._on_click)

        # Info panel
        bottom = tk.Frame(self.root)
        bottom.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)

        self.status_var = tk.StringVar(value="Light to move")
        tk.Label(bottom, textvariable=self.status_var, font=("monospace", 11),
                 anchor="w").pack(side=tk.LEFT, fill=tk.X, expand=True)

        # Engine info panel
        self.info_var = tk.StringVar(value="")
        tk.Label(bottom, textvariable=self.info_var, font=("monospace", 9),
                 anchor="e", fg="#555").pack(side=tk.RIGHT)

    def _init_board(self):
        """Set up initial piece positions."""
        self.board = [[0]*BOARD_COLS for _ in range(BOARD_ROWS)]
        self.side_to_move = 0
        self.moves_list = []
        self.selected = None
        self.legal_targets = []
        self.last_move = None
        self.game_over = False

        # Light (positive values)
        placements_light = [
            (2,0,8),(0,6,7),(0,0,6),(2,4,5),(2,2,4),(1,5,3),(1,1,2),(2,6,1)
        ]
        for r,c,rank in placements_light:
            self.board[r][c] = rank

        # Dark (negative values)
        placements_dark = [
            (6,6,-8),(8,0,-7),(8,6,-6),(6,2,-5),(6,4,-4),(7,1,-3),(7,5,-2),(6,0,-1)
        ]
        for r,c,rank in placements_dark:
            self.board[r][c] = rank

    def _draw_board(self):
        self.canvas.delete("all")

        for r in range(BOARD_ROWS):
            for c in range(BOARD_COLS):
                x = MARGIN + c * SQ_SIZE
                # Flip: row 0 at bottom
                y = MARGIN + (BOARD_ROWS - 1 - r) * SQ_SIZE

                # Determine square colour
                if (r,c) in WATER_SQUARES:
                    fill = COL_WATER
                elif (r,c) == DEN_LIGHT or (r,c) == DEN_DARK:
                    fill = COL_DEN_DARK if (r,c) == DEN_DARK else COL_DEN_LIGHT
                elif (r,c) in TRAP_LIGHT:
                    fill = COL_TRAP_LIGHT
                elif (r,c) in TRAP_DARK:
                    fill = COL_TRAP_DARK
                else:
                    fill = COL_LAND

                # Last move highlight
                outline_col = "#666"
                outline_w = 1
                if self.last_move:
                    if (r,c) == self.last_move[0] or (r,c) == self.last_move[1]:
                        fill = COL_LAST_MOVE

                # Selected highlight
                if self.selected and (r,c) == self.selected:
                    fill = COL_SELECTED

                # Legal target highlight
                if (r,c) in self.legal_targets:
                    outline_col = "#00cc00"
                    outline_w = 3

                self.canvas.create_rectangle(
                    x, y, x + SQ_SIZE, y + SQ_SIZE,
                    fill=fill, outline=outline_col, width=outline_w
                )

                # Terrain labels
                if (r,c) in WATER_SQUARES:
                    self.canvas.create_text(x + SQ_SIZE//2, y + SQ_SIZE//2,
                                            text="~", fill="#2266aa",
                                            font=("sans-serif", 20))
                elif (r,c) == DEN_LIGHT:
                    self.canvas.create_text(x + SQ_SIZE//2, y + SQ_SIZE - 12,
                                            text="DEN", fill="#996600",
                                            font=("sans-serif", 9, "bold"))
                elif (r,c) == DEN_DARK:
                    self.canvas.create_text(x + SQ_SIZE//2, y + SQ_SIZE - 12,
                                            text="DEN", fill="#996600",
                                            font=("sans-serif", 9, "bold"))
                elif (r,c) in TRAP_LIGHT or (r,c) in TRAP_DARK:
                    self.canvas.create_text(x + SQ_SIZE//2, y + SQ_SIZE - 10,
                                            text="trap", fill="#886633",
                                            font=("sans-serif", 8))

                # Piece
                piece = self.board[r][c]
                if piece != 0:
                    rank = abs(piece)
                    color = "Light" if piece > 0 else "Dark"
                    # Background circle
                    cx, cy = x + SQ_SIZE//2, y + SQ_SIZE//2
                    pr = SQ_SIZE // 2 - 6
                    circle_fill = "#f0e8d0" if piece > 0 else "#404040"
                    circle_outline = "#aa8800" if piece > 0 else "#222222"
                    self.canvas.create_oval(
                        cx - pr, cy - pr, cx + pr, cy + pr,
                        fill=circle_fill, outline=circle_outline, width=2
                    )
                    # Rank number
                    txt_color = "#333" if piece > 0 else "#eee"
                    # Animal letter + rank
                    letter = PIECE_LETTERS.get(rank, "?")
                    self.canvas.create_text(cx, cy - 8,
                                            text=letter, fill=txt_color,
                                            font=("sans-serif", 18, "bold"))
                    self.canvas.create_text(cx, cy + 14,
                                            text=PIECE_NAMES.get(rank, "?"),
                                            fill=txt_color,
                                            font=("sans-serif", 8))

        # Row/column labels
        for c in range(BOARD_COLS):
            x = MARGIN + c * SQ_SIZE + SQ_SIZE // 2
            self.canvas.create_text(x, MARGIN - 15,
                                    text=chr(ord('a') + c), fill="#ccc",
                                    font=("monospace", 12))
            self.canvas.create_text(x, MARGIN + BOARD_ROWS * SQ_SIZE + 15,
                                    text=chr(ord('a') + c), fill="#ccc",
                                    font=("monospace", 12))
        for r in range(BOARD_ROWS):
            y = MARGIN + (BOARD_ROWS - 1 - r) * SQ_SIZE + SQ_SIZE // 2
            self.canvas.create_text(MARGIN - 15, y,
                                    text=str(r + 1), fill="#ccc",
                                    font=("monospace", 12))
            self.canvas.create_text(MARGIN + BOARD_COLS * SQ_SIZE + 15, y,
                                    text=str(r + 1), fill="#ccc",
                                    font=("monospace", 12))

    def _coords_to_rc(self, x, y):
        """Convert canvas click coords to (row, col) or None."""
        c = (x - MARGIN) // SQ_SIZE
        r = BOARD_ROWS - 1 - (y - MARGIN) // SQ_SIZE
        if 0 <= r < BOARD_ROWS and 0 <= c < BOARD_COLS:
            return (r, c)
        return None

    def _on_click(self, event):
        if self.game_over or self.engine_thinking:
            return
        current_player = "human" if self.side_to_move == 0 else "human"
        if self.side_to_move == 0:
            current_player = self.light_var.get()
        else:
            current_player = self.dark_var.get()
        if current_player != "human":
            return

        rc = self._coords_to_rc(event.x, event.y)
        if rc is None:
            return
        r, c = rc

        if self.selected is None:
            # Select a piece
            piece = self.board[r][c]
            if piece == 0:
                return
            if self.side_to_move == 0 and piece < 0:
                return  # Light's turn, clicked Dark piece
            if self.side_to_move == 1 and piece > 0:
                return  # Dark's turn, clicked Light piece
            self.selected = (r, c)
            self.legal_targets = self._get_legal_targets(r, c)
            self._draw_board()
        else:
            # Try to make a move
            if (r, c) == self.selected:
                # Deselect
                self.selected = None
                self.legal_targets = []
                self._draw_board()
                return

            if (r, c) in self.legal_targets:
                # Make the move
                fr, fc = self.selected
                self._make_ui_move(fr, fc, r, c)
                self.selected = None
                self.legal_targets = []
            else:
                # Try selecting a different piece
                piece = self.board[r][c]
                if piece != 0:
                    if (self.side_to_move == 0 and piece > 0) or \
                       (self.side_to_move == 1 and piece < 0):
                        self.selected = (r, c)
                        self.legal_targets = self._get_legal_targets(r, c)
                        self._draw_board()
                        return
                self.selected = None
                self.legal_targets = []
                self._draw_board()

    def _get_legal_targets(self, r, c):
        """Get legal target squares for piece at (r,c) using the engine."""
        # Ask engine for legal moves
        self.engine.set_position(self.moves_list)
        self.engine.send("moves")
        line = self.engine.read_line()
        # Parse "Legal moves (N): a1a2 b3b4 ..."
        targets = []
        if ":" in line:
            parts = line.split(":", 1)[1].strip().split()
            from_str = sq_name(r, c)
            for mv in parts:
                if mv[:2] == from_str:
                    tr = int(mv[3]) - 1
                    tc = ord(mv[2]) - ord('a')
                    targets.append((tr, tc))
        return targets

    def _make_ui_move(self, fr, fc, tr, tc):
        """Apply a move on the UI board and send to engine."""
        move_str = sq_name(fr, fc) + sq_name(tr, tc)
        self.moves_list.append(move_str)

        # Update board
        piece = self.board[fr][fc]
        self.board[tr][tc] = piece
        self.board[fr][fc] = 0
        self.last_move = ((fr, fc), (tr, tc))
        self.side_to_move = 1 - self.side_to_move

        # Check game over
        if self._check_game_over(tr, tc, piece):
            self._draw_board()
            return

        self._draw_board()
        self._update_status()

        # If next player is engine, trigger engine move
        if self.side_to_move == 0:
            if self.light_var.get() == "engine":
                self.root.after(50, self._engine_move)
        else:
            if self.dark_var.get() == "engine":
                self.root.after(50, self._engine_move)

    def _check_game_over(self, tr, tc, piece):
        """Check if the game is over after a move to (tr,tc)."""
        # Den entry
        if piece > 0 and (tr, tc) == DEN_DARK:
            self.game_over = True
            self.status_var.set("LIGHT WINS - Den captured!")
            messagebox.showinfo("Game Over", "Light wins by entering Dark's den!")
            return True
        if piece < 0 and (tr, tc) == DEN_LIGHT:
            self.game_over = True
            self.status_var.set("DARK WINS - Den captured!")
            messagebox.showinfo("Game Over", "Dark wins by entering Light's den!")
            return True

        # All pieces captured
        light_count = sum(1 for r in range(BOARD_ROWS) for c in range(BOARD_COLS)
                          if self.board[r][c] > 0)
        dark_count = sum(1 for r in range(BOARD_ROWS) for c in range(BOARD_COLS)
                         if self.board[r][c] < 0)
        if light_count == 0:
            self.game_over = True
            self.status_var.set("DARK WINS - All Light pieces captured!")
            messagebox.showinfo("Game Over", "Dark wins - all Light pieces captured!")
            return True
        if dark_count == 0:
            self.game_over = True
            self.status_var.set("LIGHT WINS - All Dark pieces captured!")
            messagebox.showinfo("Game Over", "Light wins - all Dark pieces captured!")
            return True

        return False

    def _engine_move(self):
        """Ask the engine to make a move."""
        if self.game_over:
            return
        self.engine_thinking = True
        self.status_var.set("Engine thinking...")
        self._draw_board()
        self.root.update()

        def run():
            try:
                time_ms = int(float(self.time_var.get()) * 1000)
            except ValueError:
                time_ms = 3000

            self.engine.set_position(self.moves_list)
            self.engine.go(movetime_ms=time_ms)

            best_move = None
            last_info = ""
            while True:
                line = self.engine.read_line()
                if not line:
                    break
                if line.startswith("info"):
                    last_info = line
                    # Update info display
                    self.root.after(0, lambda l=line: self._parse_info(l))
                if line.startswith("bestmove"):
                    parts = line.split()
                    if len(parts) >= 2:
                        best_move = parts[1]
                    break

            self.root.after(0, lambda: self._apply_engine_move(best_move))

        t = threading.Thread(target=run, daemon=True)
        t.start()

    def _parse_info(self, line):
        """Parse engine info line and update display."""
        parts = line.split()
        info = {}
        i = 1
        while i < len(parts):
            if parts[i] == "depth" and i+1 < len(parts):
                info["depth"] = parts[i+1]; i += 2
            elif parts[i] == "score" and i+2 < len(parts):
                info["score"] = parts[i+1] + " " + parts[i+2]; i += 3
            elif parts[i] == "nodes" and i+1 < len(parts):
                info["nodes"] = parts[i+1]; i += 2
            elif parts[i] == "nps" and i+1 < len(parts):
                info["nps"] = parts[i+1]; i += 2
            elif parts[i] == "time" and i+1 < len(parts):
                info["time"] = parts[i+1]; i += 2
            elif parts[i] == "pv":
                info["pv"] = " ".join(parts[i+1:]); i = len(parts)
            else:
                i += 1

        depth = info.get("depth", "?")
        score = info.get("score", "?")
        nodes = info.get("nodes", "?")
        try:
            n = int(nodes)
            if n > 1000000:
                nodes = f"{n/1000000:.1f}M"
            elif n > 1000:
                nodes = f"{n/1000:.0f}K"
        except (ValueError, TypeError):
            pass
        self.info_var.set(f"d={depth}  {score}  n={nodes}")

    def _apply_engine_move(self, move_str):
        """Apply the engine's move to the board."""
        self.engine_thinking = False
        if not move_str or move_str == "0000" or len(move_str) < 4:
            self.status_var.set("Engine returned no move!")
            return

        fc = ord(move_str[0]) - ord('a')
        fr = int(move_str[1]) - 1
        tc = ord(move_str[2]) - ord('a')
        tr = int(move_str[3]) - 1

        self._make_ui_move(fr, fc, tr, tc)

    def _update_status(self):
        if self.game_over:
            return
        who = "Light" if self.side_to_move == 0 else "Dark"
        mode = self.light_var.get() if self.side_to_move == 0 else self.dark_var.get()
        label = f"({mode})" if mode == "engine" else ""
        self.status_var.set(f"{who} to move {label}")

    def _new_game(self):
        self.engine.send("newgame")
        self._init_board()
        self._draw_board()
        self._update_status()
        self.info_var.set("")

        # If engine plays first
        if self.light_var.get() == "engine":
            self.root.after(100, self._engine_move)

    def _undo_move(self):
        """Undo the last move (or two if playing vs engine)."""
        if not self.moves_list:
            return
        # If playing vs engine, undo two moves (engine + player)
        current_opp = self.dark_var.get() if self.side_to_move == 0 else self.light_var.get()
        undo_count = 2 if current_opp == "engine" and len(self.moves_list) >= 2 else 1

        for _ in range(undo_count):
            if self.moves_list:
                self.moves_list.pop()

        # Rebuild board from scratch
        self._init_board()
        # Replay all moves
        replay = list(self.moves_list)
        self.moves_list = []
        for mv in replay:
            fc = ord(mv[0]) - ord('a')
            fr = int(mv[1]) - 1
            tc = ord(mv[2]) - ord('a')
            tr = int(mv[3]) - 1
            piece = self.board[fr][fc]
            self.board[tr][tc] = piece
            self.board[fr][fc] = 0
            self.side_to_move = 1 - self.side_to_move
            self.moves_list.append(mv)

        self.selected = None
        self.legal_targets = []
        self.last_move = None
        self.game_over = False
        self._draw_board()
        self._update_status()

    def _on_close(self):
        self.engine.quit()
        self.root.destroy()


def main():
    # Find engine binary
    script_dir = os.path.dirname(os.path.abspath(__file__))
    engine_path = os.path.join(script_dir, "..", "engine", "jungle")
    if not os.path.isfile(engine_path):
        # Try current dir
        engine_path = os.path.join(script_dir, "jungle")
    if not os.path.isfile(engine_path):
        print(f"Error: engine binary not found at {engine_path}")
        print("Build the engine first: cd engine && make")
        sys.exit(1)

    root = tk.Tk()
    app = JungleGUI(root, engine_path)
    root.mainloop()


if __name__ == "__main__":
    main()
