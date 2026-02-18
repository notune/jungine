#!/usr/bin/env python3
"""
Jungle Chess GUI - A graphical interface for the Jungle Chess engine
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import subprocess
import threading
import queue
import os
import sys


class JungleChessEngine:
    def __init__(self, engine_path):
        self.process = None
        self.engine_path = engine_path
        self.queue = queue.Queue()
        self.reader_thread = None
        self.running = False

    def start(self):
        try:
            self.process = subprocess.Popen(
                [self.engine_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
            )
            self.running = True
            self.reader_thread = threading.Thread(target=self._reader, daemon=True)
            self.reader_thread.start()
            self.send("uci")
            self.send("isready")
            return True
        except Exception as e:
            print(f"Failed to start engine: {e}")
            return False

    def _reader(self):
        while self.running:
            try:
                line = self.process.stdout.readline()
                if not line:
                    break
                self.queue.put(line.strip())
            except:
                break

    def send(self, cmd):
        if self.process and self.process.poll() is None:
            self.process.stdin.write(cmd + "\n")
            self.process.stdin.flush()

    def get_output(self, timeout=0.1):
        lines = []
        try:
            while True:
                line = self.queue.get(timeout=timeout)
                lines.append(line)
        except queue.Empty:
            pass
        return lines

    def stop(self):
        self.running = False
        if self.process:
            try:
                self.send("quit")
                self.process.wait(timeout=2)
            except:
                self.process.kill()


class JungleChessBoard(tk.Canvas):
    COLS = 7
    ROWS = 9
    CELL_SIZE = 60

    LIGHT_COLOR = "#F5DEB3"
    DARK_COLOR = "#DEB887"
    WATER_COLOR = "#87CEEB"
    TRAP_COLOR = "#FFB6C1"
    DEN_COLOR = "#FFD700"
    HIGHLIGHT_COLOR = "#90EE90"
    SELECTED_COLOR = "#FFA500"

    PIECE_CHARS = {
        ("r", True): ("🐀", "Rat"),
        ("r", False): ("🐀", "RAT"),
        ("c", True): ("🐱", "Cat"),
        ("c", False): ("🐱", "CAT"),
        ("d", True): ("🐕", "Dog"),
        ("d", False): ("🐕", "DOG"),
        ("w", True): ("🐺", "Wolf"),
        ("w", False): ("🐺", "WOLF"),
        ("p", True): ("🐆", "Leopard"),
        ("p", False): ("🐆", "LEOPARD"),
        ("t", True): ("🐅", "Tiger"),
        ("t", False): ("🐅", "TIGER"),
        ("l", True): ("🦁", "Lion"),
        ("l", False): ("🦁", "LION"),
        ("e", True): ("🐘", "Elephant"),
        ("e", False): ("🐘", "ELEPHANT"),
    }

    WATER_SQUARES = [
        (1, 3),
        (2, 3),
        (1, 4),
        (2, 4),
        (1, 5),
        (2, 5),
        (4, 3),
        (5, 3),
        (4, 4),
        (5, 4),
        (4, 5),
        (5, 5),
    ]

    TRAPS = {
        (2, 0): "light",
        (4, 0): "light",
        (3, 1): "light",
        (2, 8): "dark",
        (4, 8): "dark",
        (3, 7): "dark",
    }

    DENS = {(3, 0): "light", (3, 8): "dark"}

    def __init__(self, parent, on_square_click=None, **kwargs):
        width = self.COLS * self.CELL_SIZE + 20
        height = self.ROWS * self.CELL_SIZE + 20
        super().__init__(parent, width=width, height=height, **kwargs)

        self.on_square_click = on_square_click
        self.selected = None
        self.legal_moves = []
        self.last_move = None
        self.board = {}

        self.bind("<Button-1>", self._on_click)
        self._draw_board()

    def _draw_board(self):
        self.delete("all")

        for row in range(self.ROWS):
            for col in range(self.COLS):
                x1 = col * self.CELL_SIZE + 10
                y1 = (self.ROWS - 1 - row) * self.CELL_SIZE + 10
                x2 = x1 + self.CELL_SIZE
                y2 = y1 + self.CELL_SIZE

                if (col, row) in self.WATER_SQUARES:
                    color = self.WATER_COLOR
                elif (col, row) in self.TRAPS:
                    color = self.TRAP_COLOR
                elif (col, row) in self.DENS:
                    color = self.DEN_COLOR
                elif (col + row) % 2 == 0:
                    color = self.LIGHT_COLOR
                else:
                    color = self.DARK_COLOR

                if self.last_move:
                    if (col, row) == self.last_move[:2] or (col, row) == self.last_move[
                        2:
                    ]:
                        color = self.HIGHLIGHT_COLOR

                if self.selected == (col, row):
                    color = self.SELECTED_COLOR
                elif self.legal_moves and (col, row) in self.legal_moves:
                    color = self.HIGHLIGHT_COLOR

                self.create_rectangle(x1, y1, x2, y2, fill=color, outline="black")

                if (col, row) in self.DENS:
                    self.create_text(
                        (x1 + x2) / 2, (y1 + y2) / 2, text="🏠", font=("Arial", 16)
                    )
                elif (col, row) in self.TRAPS:
                    self.create_text(
                        (x1 + x2) / 2, (y1 + y2) / 2, text="⚠", font=("Arial", 12)
                    )

        for (col, row), piece in self.board.items():
            self._draw_piece(col, row, piece)

        for col in range(self.COLS):
            x = col * self.CELL_SIZE + self.CELL_SIZE / 2 + 10
            self.create_text(x, 5, text=chr(ord("A") + col), font=("Arial", 10))
            self.create_text(
                x,
                self.ROWS * self.CELL_SIZE + 15,
                text=chr(ord("A") + col),
                font=("Arial", 10),
            )

        for row in range(self.ROWS):
            y = (self.ROWS - 1 - row) * self.CELL_SIZE + self.CELL_SIZE / 2 + 10
            self.create_text(5, y, text=str(row + 1), font=("Arial", 10))
            self.create_text(
                self.COLS * self.CELL_SIZE + 15,
                y,
                text=str(row + 1),
                font=("Arial", 10),
            )

    def _draw_piece(self, col, row, piece):
        x1 = col * self.CELL_SIZE + 10
        y1 = (self.ROWS - 1 - row) * self.CELL_SIZE + 10
        x2 = x1 + self.CELL_SIZE
        y2 = y1 + self.CELL_SIZE

        is_light = piece.islower()
        key = (piece.lower(), is_light)
        emoji, name = self.PIECE_CHARS.get(key, ("?", "Unknown"))

        bg_color = "#FFFFFF" if is_light else "#333333"
        text_color = "#000000" if is_light else "#FFFFFF"

        cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
        r = self.CELL_SIZE * 0.4

        self.create_oval(
            cx - r, cy - r, cx + r, cy + r, fill=bg_color, outline=text_color, width=2
        )
        self.create_text(cx, cy, text=emoji, font=("Segoe UI Emoji", 20))

    def _on_click(self, event):
        col = (event.x - 10) // self.CELL_SIZE
        row = self.ROWS - 1 - (event.y - 10) // self.CELL_SIZE

        if 0 <= col < self.COLS and 0 <= row < self.ROWS:
            if self.on_square_click:
                self.on_square_click(col, row)

    def set_piece(self, col, row, piece):
        if piece:
            self.board[(col, row)] = piece
        elif (col, row) in self.board:
            del self.board[(col, row)]

    def set_position(self, pieces):
        self.board = {}
        for (col, row), piece in pieces.items():
            self.board[(col, row)] = piece
        self._draw_board()

    def set_selected(self, col, row):
        self.selected = (col, row) if col is not None else None
        self._draw_board()

    def set_legal_moves(self, moves):
        self.legal_moves = moves
        self._draw_board()

    def set_last_move(self, move):
        self.last_move = move
        self._draw_board()

    def clear_highlights(self):
        self.selected = None
        self.legal_moves = []
        self._draw_board()


class JungleChessApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Jungle Chess")
        self.root.resizable(False, False)

        engine_path = self._find_engine()
        if not engine_path:
            messagebox.showerror("Error", "Could not find jungle_engine binary!")
            sys.exit(1)

        self.engine = JungleChessEngine(engine_path)
        if not self.engine.start():
            messagebox.showerror("Error", "Failed to start chess engine!")
            sys.exit(1)

        self.moves = []
        self.selected_square = None
        self.player_color = "light"
        self.current_turn = "light"
        self.ai_thinking = False
        self.vs_ai = True
        self.ai_time = 1000

        self._setup_ui()
        self._new_game()
        self._poll_engine()

    def _find_engine(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        paths = [
            os.path.join(script_dir, "engine", "jungle_engine"),
            os.path.join(script_dir, "..", "engine", "jungle_engine"),
            os.path.join(os.path.dirname(script_dir), "engine", "jungle_engine"),
            "./jungle_engine",
        ]
        for path in paths:
            if os.path.isfile(path):
                return os.path.abspath(path)
        return None

    def _setup_ui(self):
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky="nsew")

        self.board = JungleChessBoard(main_frame, on_square_click=self._on_square_click)
        self.board.grid(row=0, column=0, rowspan=10)

        info_frame = ttk.LabelFrame(main_frame, text="Game Info", padding="10")
        info_frame.grid(row=0, column=1, sticky="new", padx=(10, 0))

        self.turn_label = ttk.Label(info_frame, text="Turn: Light", font=("Arial", 12))
        self.turn_label.grid(row=0, column=0, sticky="w")

        self.status_label = ttk.Label(
            info_frame, text="Status: Ready", font=("Arial", 10)
        )
        self.status_label.grid(row=1, column=0, sticky="w")

        self.eval_label = ttk.Label(info_frame, text="Eval: 0", font=("Arial", 10))
        self.eval_label.grid(row=2, column=0, sticky="w")

        control_frame = ttk.LabelFrame(main_frame, text="Controls", padding="10")
        control_frame.grid(row=1, column=1, sticky="new", padx=(10, 0), pady=(10, 0))

        ttk.Button(control_frame, text="New Game", command=self._new_game).grid(
            row=0, column=0, sticky="ew", pady=2
        )
        ttk.Button(control_frame, text="Flip Board", command=self._flip_board).grid(
            row=1, column=0, sticky="ew", pady=2
        )
        ttk.Button(control_frame, text="Undo Move", command=self._undo_move).grid(
            row=2, column=0, sticky="ew", pady=2
        )

        self.ai_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            control_frame,
            text="Play vs AI",
            variable=self.ai_var,
            command=self._toggle_ai,
        ).grid(row=3, column=0, sticky="w", pady=5)

        time_frame = ttk.Frame(control_frame)
        time_frame.grid(row=4, column=0, sticky="ew", pady=5)
        ttk.Label(time_frame, text="AI Time (ms):").grid(row=0, column=0, sticky="w")
        self.time_spinbox = ttk.Spinbox(
            time_frame, from_=100, to=10000, increment=100, width=8
        )
        self.time_spinbox.set(str(self.ai_time))
        self.time_spinbox.grid(row=0, column=1, sticky="e")

        moves_frame = ttk.LabelFrame(main_frame, text="Move History", padding="10")
        moves_frame.grid(row=2, column=1, sticky="new", padx=(10, 0), pady=(10, 0))

        self.moves_text = tk.Text(
            moves_frame, width=20, height=15, font=("Courier", 10)
        )
        self.moves_text.grid(row=0, column=0, sticky="nsew")

        scrollbar = ttk.Scrollbar(
            moves_frame, orient="vertical", command=self.moves_text.yview
        )
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.moves_text.configure(yscrollcommand=scrollbar.set)

        legend_frame = ttk.LabelFrame(main_frame, text="Piece Legend", padding="10")
        legend_frame.grid(row=3, column=1, sticky="new", padx=(10, 0), pady=(10, 0))

        pieces = [
            ("🐘 Elephant", "8 (strongest)"),
            ("🦁 Lion", "7 (jumps)"),
            ("🐅 Tiger", "6 (jumps)"),
            ("🐆 Leopard", "5"),
            ("🐺 Wolf", "4"),
            ("🐕 Dog", "3"),
            ("🐱 Cat", "2"),
            ("🐀 Rat", "1 (swims, beats elephant)"),
        ]

        for i, (name, strength) in enumerate(pieces):
            ttk.Label(legend_frame, text=f"{name}: {strength}", font=("Arial", 9)).grid(
                row=i, column=0, sticky="w"
            )

        rules_frame = ttk.LabelFrame(main_frame, text="Quick Rules", padding="10")
        rules_frame.grid(row=4, column=1, sticky="new", padx=(10, 0), pady=(10, 0))

        rules = [
            "• Move one square orthogonally",
            "• Capture ≤ strength pieces",
            "• Rat beats Elephant!",
            "• Trapped pieces lose strength",
            "• Reach enemy den to win!",
            "• Lion/Tiger jump rivers",
        ]

        for i, rule in enumerate(rules):
            ttk.Label(rules_frame, text=rule, font=("Arial", 9)).grid(
                row=i, column=0, sticky="w"
            )

    def _parse_board(self, lines):
        pieces = {}
        for line in lines:
            if "info" in line.lower() and "score" in line.lower():
                parts = line.split()
                try:
                    idx = parts.index("score")
                    if idx + 2 < len(parts):
                        score_type = parts[idx + 1]
                        score_val = parts[idx + 2]
                        if score_type == "cp":
                            self.eval_label.config(text=f"Eval: {score_val}")
                        elif score_type == "mate":
                            self.eval_label.config(text=f"Mate in: {score_val}")
                except:
                    pass

        return pieces

    def _parse_position_command(self, cmd):
        pieces = {}
        tokens = cmd.split()
        if "startpos" in cmd:
            pieces = self._get_initial_position()
        return pieces

    def _get_initial_position(self):
        pieces = {
            (0, 0): "t",
            (6, 0): "l",
            (1, 1): "c",
            (5, 1): "d",
            (0, 2): "e",
            (2, 2): "w",
            (4, 2): "p",
            (6, 2): "r",
            (0, 6): "R",
            (2, 6): "P",
            (4, 6): "W",
            (6, 6): "E",
            (1, 7): "D",
            (5, 7): "C",
            (0, 8): "L",
            (6, 8): "T",
        }
        return pieces

    def _square_to_str(self, col, row):
        return chr(ord("A") + col) + str(row + 1)

    def _str_to_square(self, s):
        if len(s) >= 2:
            col = ord(s[0].upper()) - ord("A")
            row = int(s[1]) - 1
            if 0 <= col < 7 and 0 <= row < 9:
                return (col, row)
        return None

    def _new_game(self):
        self.moves = []
        self.selected_square = None
        self.current_turn = "light"
        self.moves_text.delete(1.0, tk.END)

        self.engine.send("ucinewgame")

        pieces = self._get_initial_position()
        self.board.set_position(pieces)
        self.board.clear_highlights()

        self._update_turn_display()
        self.status_label.config(text="Status: Your turn")

        if self.vs_ai and self.player_color == "dark":
            self._make_ai_move()

    def _flip_board(self):
        pass

    def _undo_move(self):
        if len(self.moves) < 2:
            return

        self.moves.pop()
        self.moves.pop()

        self.engine.send("ucinewgame")

        if self.moves:
            move_str = " ".join(self.moves)
            self.engine.send(f"position startpos moves {move_str}")
        else:
            self.engine.send("position startpos")

        self.board.set_position(self._get_initial_position())

        for move in self.moves:
            self._apply_move_to_board(move)

        self.current_turn = "light" if len(self.moves) % 2 == 0 else "dark"
        self._update_turn_display()
        self.status_label.config(text="Status: Your turn")

        self.moves_text.delete(1.0, tk.END)
        for i, move in enumerate(self.moves):
            num = (i // 2) + 1
            if i % 2 == 0:
                self.moves_text.insert(tk.END, f"{num}. {move}")
            else:
                self.moves_text.insert(tk.END, f" {move}\n")

    def _toggle_ai(self):
        self.vs_ai = self.ai_var.get()

    def _update_turn_display(self):
        turn = "Light" if self.current_turn == "light" else "Dark"
        self.turn_label.config(text=f"Turn: {turn}")

    def _on_square_click(self, col, row):
        if self.ai_thinking:
            return

        is_player_turn = (self.current_turn == self.player_color) or not self.vs_ai

        if not is_player_turn:
            return

        piece = self.board.board.get((col, row))
        piece_is_light = piece and piece.islower()
        current_is_light = self.current_turn == "light"

        if self.selected_square:
            from_col, from_row = self.selected_square

            if (col, row) == self.selected_square:
                self.board.clear_highlights()
                self.selected_square = None
                return

            move_str = self._square_to_str(from_col, from_row) + self._square_to_str(
                col, row
            )

            if self._try_make_move(move_str):
                self.board.clear_highlights()
                self.selected_square = None
            else:
                if piece and piece_is_light == current_is_light:
                    self.selected_square = (col, row)
                    self.board.set_selected(col, row)
                    self._update_legal_moves(col, row)
                else:
                    self.board.clear_highlights()
                    self.selected_square = None
        else:
            if piece and piece_is_light == current_is_light:
                self.selected_square = (col, row)
                self.board.set_selected(col, row)
                self._update_legal_moves(col, row)

    def _update_legal_moves(self, col, row):
        legal = []
        directions = [(0, 1), (0, -1), (1, 0), (-1, 0)]

        for dc, dr in directions:
            new_col, new_row = col + dc, row + dr
            if 0 <= new_col < 7 and 0 <= new_row < 9:
                target = self.board.board.get((new_col, new_row))
                can_move = False

                if not target:
                    can_move = True
                else:
                    target_is_light = target.islower()
                    current_is_light = self.current_turn == "light"
                    if target_is_light != current_is_light:
                        can_move = True

                if can_move:
                    legal.append((new_col, new_row))

        self.board.set_legal_moves(legal)

    def _try_make_move(self, move_str):
        self.engine.send(f"position startpos moves {' '.join(self.moves + [move_str])}")

        import time

        time.sleep(0.05)

        self.engine.send("d")
        time.sleep(0.05)
        output = self.engine.get_output()

        self.moves.append(move_str)
        self._apply_move_to_board(move_str)

        move_num = (len(self.moves) - 1) // 2 + 1
        if (len(self.moves) - 1) % 2 == 0:
            self.moves_text.insert(tk.END, f"{move_num}. {move_str}")
        else:
            self.moves_text.insert(tk.END, f" {move_str}\n")
        self.moves_text.see(tk.END)

        self.board.set_last_move(
            self._str_to_square(move_str[:2]) + self._str_to_square(move_str[2:4])
        )

        self.current_turn = "dark" if self.current_turn == "light" else "light"
        self._update_turn_display()

        result = self._check_game_result()
        if result:
            self.status_label.config(text=f"Status: {result}")
            return True

        if self.vs_ai:
            self.status_label.config(text="Status: AI thinking...")
            self.root.after(100, self._make_ai_move)
        else:
            self.status_label.config(text="Status: Your turn")

        return True

    def _apply_move_to_board(self, move_str):
        from_sq = self._str_to_square(move_str[:2])
        to_sq = self._str_to_square(move_str[2:4])

        if from_sq and to_sq:
            piece = self.board.board.get(from_sq)
            if piece:
                del self.board.board[from_sq]
                self.board.board[to_sq] = piece
                self.board._draw_board()

    def _make_ai_move(self):
        if not self.vs_ai:
            return

        self.ai_thinking = True

        try:
            ai_time = int(self.time_spinbox.get())
        except:
            ai_time = 1000

        self.engine.send(f"go movetime {ai_time}")

    def _check_game_result(self):
        light_den = (3, 0)
        dark_den = (3, 8)

        if dark_den in self.board.board:
            piece = self.board.board[dark_den]
            if piece and piece.islower():
                return "Light wins! (Den captured)"

        if light_den in self.board.board:
            piece = self.board.board[light_den]
            if piece and piece.isupper():
                return "Dark wins! (Den captured)"

        light_pieces = sum(1 for p in self.board.board.values() if p.islower())
        dark_pieces = sum(1 for p in self.board.board.values() if p.isupper())

        if light_pieces == 0:
            return "Dark wins! (All pieces captured)"
        if dark_pieces == 0:
            return "Light wins! (All pieces captured)"

        return None

    def _poll_engine(self):
        lines = self.engine.get_output()

        for line in lines:
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    move = parts[1]
                    self._handle_ai_move(move)
            elif "score" in line:
                try:
                    parts = line.split()
                    idx = parts.index("score")
                    if idx + 2 < len(parts):
                        score_type = parts[idx + 1]
                        score_val = parts[idx + 2]
                        if score_type == "cp":
                            self.eval_label.config(text=f"Eval: {score_val}")
                        elif score_type == "mate":
                            self.eval_label.config(text=f"Mate in: {score_val}")
                except:
                    pass

        self.root.after(50, self._poll_engine)

    def _handle_ai_move(self, move):
        self.ai_thinking = False

        self.moves.append(move)
        self._apply_move_to_board(move)

        move_num = (len(self.moves) - 1) // 2 + 1
        if (len(self.moves) - 1) % 2 == 0:
            self.moves_text.insert(tk.END, f"{move_num}. {move}")
        else:
            self.moves_text.insert(tk.END, f" {move}\n")
        self.moves_text.see(tk.END)

        self.board.set_last_move(
            self._str_to_square(move[:2]) + self._str_to_square(move[2:4])
        )

        self.current_turn = "dark" if self.current_turn == "light" else "light"
        self._update_turn_display()

        result = self._check_game_result()
        if result:
            self.status_label.config(text=f"Status: {result}")
        else:
            self.status_label.config(text="Status: Your turn")

    def cleanup(self):
        self.engine.stop()


def main():
    root = tk.Tk()
    app = JungleChessApp(root)

    def on_closing():
        app.cleanup()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()
