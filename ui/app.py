import tkinter as tk
from tkinter import ttk, messagebox
import subprocess
import threading
import queue
import os

# Board Dimensions
ROWS = 9
COLS = 7
CELL_SIZE = 80

# Colors
COLOR_LAND = "#e6d5ac"
COLOR_WATER = "#85c1e9"
COLOR_TRAP = "#d98880"
COLOR_DEN = "#5dade2"
COLOR_DARK_DEN = "#af7ac5"

COLOR_LIGHT_PIECE = "#fcf3cf"
COLOR_DARK_PIECE = "#d5d8dc"

COLOR_HIGHLIGHT_SELECTED = "#f4d03f"
COLOR_HIGHLIGHT_LAST = "#58d68d"
COLOR_HIGHLIGHT_MOVE = "#82e0aa" # Greenish tint for legal moves

PIECES_INFO = {
    'e': ('Elephant', '🐘', 8),
    'i': ('Lion', '🦁', 7),
    't': ('Tiger', '🐅', 6),
    'l': ('Leopard', '🐆', 5),
    'w': ('Wolf', '🐺', 4),
    'd': ('Dog', '🐕', 3),
    'c': ('Cat', '🐈', 2),
    'r': ('Rat', '🐀', 1)
}

class JungleGame:
    def __init__(self, root):
        self.root = root
        self.root.title("Jungle Chess - UI vs Engine")
        self.root.resizable(False, False)
        
        self.board = [["" for _ in range(COLS)] for _ in range(ROWS)]
        self.turn = "w" # 'w' for light (bottom), 'b' for dark (top)
        self.selected_sq = None
        self.last_move = None # ((fc, fr), (tc, tr))
        self.flipped = False
        self.game_over = False
        self.move_history = []
        self.current_legal_moves = []
        self.is_engine_thinking = False
        
        self.engine_process = None
        self.engine_queue = queue.Queue()
        
        self.init_ui()
        self.start_engine()
        self.init_board()
        self.draw_board()
        
        self.light_type.trace_add("write", lambda *args: self.root.after(100, self.check_turn))
        self.dark_type.trace_add("write", lambda *args: self.root.after(100, self.check_turn))
        
    def init_ui(self):
        # Left frame for board
        self.board_frame = tk.Frame(self.root)
        self.board_frame.pack(side=tk.LEFT, padx=10, pady=10)
        
        self.canvas = tk.Canvas(self.board_frame, width=COLS * CELL_SIZE, height=ROWS * CELL_SIZE)
        self.canvas.pack()
        self.canvas.bind("<Button-1>", self.on_click)

        # Right frame for controls
        self.control_frame = tk.Frame(self.root)
        self.control_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)

        # Legend
        legend_frame = tk.LabelFrame(self.control_frame, text="Piece Ranks")
        legend_frame.pack(fill=tk.X, pady=5)
        legend_text = "\n".join([f"{data[2]}. {data[1]} {data[0]}" for k, data in sorted(PIECES_INFO.items(), key=lambda x: -x[1][2])])
        tk.Label(legend_frame, text=legend_text, justify=tk.LEFT, font=("Arial", 12)).pack(anchor=tk.W, padx=5, pady=5)

        # Players
        players_frame = tk.LabelFrame(self.control_frame, text="Players")
        players_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(players_frame, text="Light (Bottom):").grid(row=0, column=0, sticky="e", pady=2)
        self.light_type = tk.StringVar(value="Human")
        ttk.Combobox(players_frame, textvariable=self.light_type, values=["Human", "Bot"], state="readonly", width=8).grid(row=0, column=1, pady=2)

        tk.Label(players_frame, text="Dark (Top):").grid(row=1, column=0, sticky="e", pady=2)
        self.dark_type = tk.StringVar(value="Bot")
        ttk.Combobox(players_frame, textvariable=self.dark_type, values=["Human", "Bot"], state="readonly", width=8).grid(row=1, column=1, pady=2)

        # Settings
        settings_frame = tk.LabelFrame(self.control_frame, text="Engine Settings")
        settings_frame.pack(fill=tk.X, pady=5)

        tk.Label(settings_frame, text="Time (ms):").grid(row=0, column=0, sticky="e", pady=2)
        self.time_var = tk.StringVar(value="1000")
        tk.Entry(settings_frame, textvariable=self.time_var, width=8).grid(row=0, column=1, pady=2)

        tk.Label(settings_frame, text="Max Depth:").grid(row=1, column=0, sticky="e", pady=2)
        self.depth_var = tk.StringVar(value="64")
        tk.Entry(settings_frame, textvariable=self.depth_var, width=8).grid(row=1, column=1, pady=2)

        # Engine Info
        info_frame = tk.LabelFrame(self.control_frame, text="Engine Info")
        info_frame.pack(fill=tk.X, pady=5)
        self.info_label = tk.Label(info_frame, text="Depth: -\nNodes: -\nScore: -", justify=tk.LEFT, font=("Courier", 10))
        self.info_label.pack(anchor=tk.W, padx=5, pady=5)

        # Actions
        actions_frame = tk.Frame(self.control_frame)
        actions_frame.pack(fill=tk.X, pady=5)
        tk.Button(actions_frame, text="Flip Board", command=self.do_flip_board).pack(side=tk.LEFT, padx=2)
        tk.Button(actions_frame, text="Restart", command=self.restart_game).pack(side=tk.LEFT, padx=2)
        tk.Button(actions_frame, text="Force Bot", command=self.force_bot).pack(side=tk.LEFT, padx=2)

        # Move List
        move_list_frame = tk.LabelFrame(self.control_frame, text="Move List")
        move_list_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        self.move_text = tk.Text(move_list_frame, width=20, height=10, state=tk.DISABLED, font=("Courier", 10))
        self.move_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar = ttk.Scrollbar(move_list_frame, command=self.move_text.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.move_text.config(yscrollcommand=scrollbar.set)

    def do_flip_board(self):
        self.flipped = not self.flipped
        self.draw_board()

    def restart_game(self):
        self.move_history = []
        self.turn = 'w'
        self.selected_sq = None
        self.last_move = None
        self.game_over = False
        self.current_legal_moves = []
        self.info_label.config(text="Depth: -\nNodes: -\nScore: -")
        self.init_board()
        self.draw_board()
        self.update_move_list()
        self.request_legal_moves()
        self.root.after(100, self.check_turn)

    def force_bot(self):
        if self.game_over: return
        if not self.is_engine_thinking:
            self.ask_engine()

    def start_engine(self):
        engine_path = os.path.join(os.path.dirname(__file__), "..", "engine", "jungleai")
        if not os.path.exists(engine_path):
            messagebox.showerror("Error", "Engine not found. Please compile it first.")
            return
            
        self.engine_process = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        
        self.engine_process.stdin.write("uji\n")
        self.engine_process.stdin.write("isready\n")
        self.engine_process.stdin.flush()
        
        def read_engine():
            for line in self.engine_process.stdout:
                line = line.strip()
                if line:
                    self.engine_queue.put(line)
        
        threading.Thread(target=read_engine, daemon=True).start()
        self.root.after(100, self.process_engine_messages)

    def process_engine_messages(self):
        try:
            while True:
                msg = self.engine_queue.get_nowait()
                if msg.startswith("info"):
                    parts = msg.split()
                    depth = "?"
                    nodes = "?"
                    score = "?"
                    for i in range(len(parts)):
                        if parts[i] == "depth" and i+1 < len(parts): depth = parts[i+1]
                        if parts[i] == "nodes" and i+1 < len(parts): nodes = parts[i+1]
                        if parts[i] == "cp" and i+1 < len(parts): score = parts[i+1]
                    self.info_label.config(text=f"Depth: {depth}\nNodes: {nodes}\nScore: {score}")

                elif msg.startswith("legalmoves"):
                    parts = msg.split()
                    self.current_legal_moves = parts[1:] if len(parts) > 1 else []
                    self.draw_board() # Redraw to show move indicators if piece is selected

                elif msg.startswith("bestmove"):
                    parts = msg.split()
                    if len(parts) >= 2:
                        move = parts[1]
                        self.make_engine_move(move)
        except queue.Empty:
            pass
        self.root.after(100, self.process_engine_messages)

    def init_board(self):
        self.board = [["" for _ in range(COLS)] for _ in range(ROWS)]
        
        self.board[8][0] = ("t", "w")
        self.board[8][6] = ("i", "w")
        self.board[7][1] = ("c", "w")
        self.board[7][5] = ("d", "w")
        self.board[6][2] = ("w", "w")
        self.board[6][4] = ("l", "w")
        self.board[6][0] = ("e", "w")
        self.board[6][6] = ("r", "w")
        
        self.board[0][6] = ("t", "b")
        self.board[0][0] = ("i", "b")
        self.board[1][5] = ("c", "b")
        self.board[1][1] = ("d", "b")
        self.board[2][4] = ("w", "b")
        self.board[2][2] = ("l", "b")
        self.board[2][6] = ("e", "b")
        self.board[2][0] = ("r", "b")
        
        self.request_legal_moves()

    def get_draw_coords(self, c, r):
        if self.flipped:
            return (6 - c, 8 - r)
        return (c, r)

    def get_logical_coords(self, cx, cy):
        if self.flipped:
            return (6 - cx, 8 - cy)
        return (cx, cy)

    def draw_board(self):
        self.canvas.delete("all")
        
        # Pre-calculate target squares for the selected piece
        target_squares = set()
        if self.selected_sq:
            sel_str = self.sq_to_str(self.selected_sq[0], self.selected_sq[1])
            for move in self.current_legal_moves:
                if move.startswith(sel_str):
                    target_squares.add(self.str_to_sq(move[2:4]))
        
        for r in range(ROWS):
            for c in range(COLS):
                dc, dr = self.get_draw_coords(c, r)
                
                color = COLOR_LAND
                rank = 8 - r
                file = c
                
                if file in [1, 2, 4, 5] and rank in [3, 4, 5]:
                    color = COLOR_WATER
                
                if file == 3 and rank == 0: color = COLOR_DEN
                if file == 3 and rank == 8: color = COLOR_DARK_DEN
                
                if (file, rank) in [(2,0), (4,0), (3,1), (2,8), (4,8), (3,7)]:
                    color = COLOR_TRAP
                    
                x1 = dc * CELL_SIZE
                y1 = dr * CELL_SIZE
                x2 = x1 + CELL_SIZE
                y2 = y1 + CELL_SIZE
                
                self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline="black")
                
                # Highlight last move
                if self.last_move:
                    (fc, fr), (tc, tr) = self.last_move
                    if (c, r) == (fc, fr) or (c, r) == (tc, tr):
                        self.canvas.create_rectangle(x1, y1, x2, y2, fill=COLOR_HIGHLIGHT_LAST, outline="green", width=2)
                        
                # Highlight selected
                if self.selected_sq == (c, r):
                    self.canvas.create_rectangle(x1, y1, x2, y2, fill=COLOR_HIGHLIGHT_SELECTED, outline="red", width=3)
                
                piece = self.board[r][c]
                if piece:
                    ptype, pcol = piece
                    fill_color = COLOR_LIGHT_PIECE if pcol == 'w' else COLOR_DARK_PIECE
                    text_color = "blue" if pcol == 'w' else "red"
                    
                    pname, emoji, _ = PIECES_INFO.get(ptype, ("?", "?", 0))
                    
                    self.canvas.create_oval(x1+8, y1+8, x2-8, y2-8, fill=fill_color, outline="black", width=2)
                    self.canvas.create_text(x1+CELL_SIZE//2, y1+CELL_SIZE//2 - 8, text=emoji, font=("Arial", 24))
                    self.canvas.create_text(x1+CELL_SIZE//2, y1+CELL_SIZE//2 + 18, text=pname, fill=text_color, font=("Arial", 9, "bold"))
                
                # Draw legal move indicator dot
                if (c, r) in target_squares:
                    # Draw a semi-transparent dot or a circle
                    r_dot = 10
                    cx, cy = x1 + CELL_SIZE//2, y1 + CELL_SIZE//2
                    self.canvas.create_oval(cx - r_dot, cy - r_dot, cx + r_dot, cy + r_dot, fill=COLOR_HIGHLIGHT_MOVE, outline="black")
                    
    def sq_to_str(self, c, r):
        file_c = chr(ord('a') + c)
        rank_c = str(9 - r)
        return file_c + rank_c
        
    def str_to_sq(self, s):
        c = ord(s[0]) - ord('a')
        r = 9 - int(s[1])
        return (c, r)

    def request_legal_moves(self):
        moves = " ".join(self.move_history)
        if moves:
            self.engine_process.stdin.write(f"position startpos moves {moves}\n")
        else:
            self.engine_process.stdin.write(f"position startpos\n")
        self.engine_process.stdin.write("legalmoves\n")
        self.engine_process.stdin.flush()

    def on_click(self, event):
        if self.game_over: return
        if self.is_engine_thinking: return
        
        current_type = self.light_type.get() if self.turn == 'w' else self.dark_type.get()
        if current_type == "Bot": return # Ignore clicks if it's bot's turn
        
        cx = event.x // CELL_SIZE
        cy = event.y // CELL_SIZE
        
        c, r = self.get_logical_coords(cx, cy)
        
        if c < 0 or c >= COLS or r < 0 or r >= ROWS: return
        
        if self.selected_sq:
            move_str = self.sq_to_str(self.selected_sq[0], self.selected_sq[1]) + self.sq_to_str(c, r)
            
            if self.selected_sq == (c, r):
                self.selected_sq = None
            elif move_str in self.current_legal_moves:
                # Execute valid move
                self.board[r][c] = self.board[self.selected_sq[1]][self.selected_sq[0]]
                self.board[self.selected_sq[1]][self.selected_sq[0]] = ""
                
                self.move_history.append(move_str)
                self.last_move = (self.selected_sq, (c, r))
                self.selected_sq = None
                self.turn = 'b' if self.turn == 'w' else 'w'
                self.current_legal_moves = []
                
                self.update_move_list()
                self.draw_board()
                self.request_legal_moves()
                self.root.after(100, self.check_turn)
            else:
                # Clicked an invalid square, try to select another friendly piece
                piece = self.board[r][c]
                if piece and piece[1] == self.turn:
                    self.selected_sq = (c, r)
                else:
                    self.selected_sq = None
        else:
            piece = self.board[r][c]
            if piece and piece[1] == self.turn:
                self.selected_sq = (c, r)
        self.draw_board()
        
    def update_move_list(self):
        self.move_text.config(state=tk.NORMAL)
        self.move_text.delete(1.0, tk.END)
        for i, move in enumerate(self.move_history):
            if i % 2 == 0:
                self.move_text.insert(tk.END, f"{(i//2)+1}. {move} ")
            else:
                self.move_text.insert(tk.END, f"{move}\n")
        self.move_text.config(state=tk.DISABLED)
        self.move_text.see(tk.END)

    def check_game_over(self):
        if self.board[0][3] and self.board[0][3][1] == 'w':
            self.game_over = True
            messagebox.showinfo("Game Over", "Light wins by reaching the den!")
            return True
        if self.board[8][3] and self.board[8][3][1] == 'b':
            self.game_over = True
            messagebox.showinfo("Game Over", "Dark wins by reaching the den!")
            return True
            
        light_alive = False
        dark_alive = False
        for r in range(ROWS):
            for c in range(COLS):
                if self.board[r][c]:
                    if self.board[r][c][1] == 'w': light_alive = True
                    if self.board[r][c][1] == 'b': dark_alive = True
                    
        if not light_alive:
            self.game_over = True
            messagebox.showinfo("Game Over", "Dark wins by capturing all pieces!")
            return True
        if not dark_alive:
            self.game_over = True
            messagebox.showinfo("Game Over", "Light wins by capturing all pieces!")
            return True
            
        return False

    def check_turn(self):
        if self.game_over: return
        if self.check_game_over(): return
        if self.is_engine_thinking: return
        current_type = self.light_type.get() if self.turn == 'w' else self.dark_type.get()
        if current_type == "Bot":
            self.ask_engine()

    def ask_engine(self):
        self.is_engine_thinking = True
        
        moves = " ".join(self.move_history)
        if moves:
            self.engine_process.stdin.write(f"position startpos moves {moves}\n")
        else:
            self.engine_process.stdin.write(f"position startpos\n")
            
        t = self.time_var.get()
        d = self.depth_var.get()
        if not t.isdigit(): t = "1000"
        if not d.isdigit(): d = "64"
            
        self.engine_process.stdin.write(f"go movetime {t} depth {d}\n")
        self.engine_process.stdin.flush()

    def make_engine_move(self, move_str):
        self.is_engine_thinking = False
        if len(move_str) == 4:
            f_c, f_r = self.str_to_sq(move_str[0:2])
            t_c, t_r = self.str_to_sq(move_str[2:4])
            
            self.board[t_r][t_c] = self.board[f_r][f_c]
            self.board[f_r][f_c] = ""
            
            self.move_history.append(move_str)
            self.last_move = ((f_c, f_r), (t_c, t_r))
            self.turn = 'b' if self.turn == 'w' else 'w'
            self.current_legal_moves = []
            
            self.update_move_list()
            self.draw_board()
            self.request_legal_moves()
            self.root.after(100, self.check_turn)

if __name__ == "__main__":
    root = tk.Tk()
    app = JungleGame(root)
    root.mainloop()
