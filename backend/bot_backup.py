import chess
import random
import os
import torch
import torch.optim as optim
import torch.nn as nn
import json
import sqlite3
from model import ChessNet, board_to_tensor

def get_material_score(board):
    # Returns score from White's perspective
    score = 0
    values = {chess.PAWN: 1, chess.KNIGHT: 3, chess.BISHOP: 3, chess.ROOK: 5, chess.QUEEN: 9}
    for piece_type in values:
        score += len(board.pieces(piece_type, chess.WHITE)) * values[piece_type]
        score -= len(board.pieces(piece_type, chess.BLACK)) * values[piece_type]
    return score

def init_db():
    conn = sqlite3.connect("training_data.db")
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS training_moves (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            game_id TEXT,
            start_fen TEXT,
            step INTEGER,
            turn TEXT,
            material_score INTEGER,
            result INTEGER,
            fen TEXT
        )
    ''')
    try:
        cursor.execute("ALTER TABLE training_moves ADD COLUMN opponent TEXT DEFAULT 'Self'")
    except:
        pass # Column likely exists
    conn.commit()
    conn.close()

class NeuralLearner:
    # ... (existing code) ...

# ... (skipped unchanged parts) ...


    def __init__(self, filepath="brain.pth"):
        self.filepath = filepath
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model = ChessNet().to(self.device)
        self.optimizer = optim.Adam(self.model.parameters(), lr=0.001)
        self.criterion = nn.MSELoss()
        self.load()

    def get_value(self, board):
        """
        Returns a value between -1 and 1 for the current board state.
        Value is from the perspective of the player to move (active color).
        1.0 means the active player is winning, -1.0 means losing.
        """
        self.model.eval()
        with torch.no_grad():
            tensor = board_to_tensor(board).to(self.device)
            value = self.model(tensor).item()
        return value

    def train_batch(self, states, targets):
        """
        Trains the model on a batch of states and target values.
        states: List of chess.Board objects (or tensors)
        targets: List of floats [-1, 1]
        """
        self.model.train()
        
        # Convert all boards to a single batch tensor
        tensors = [board_to_tensor(s).squeeze(0) for s in states] # Remove batch dim to stack
        batch_tensor = torch.stack(tensors).to(self.device)
        target_tensor = torch.tensor(targets, dtype=torch.float32).unsqueeze(1).to(self.device)
        
        self.optimizer.zero_grad()
        outputs = self.model(batch_tensor)
        loss = self.criterion(outputs, target_tensor)
        loss.backward()
        self.optimizer.step()
        
        return loss.item()

    def save(self):
        torch.save(self.model.state_dict(), self.filepath)

    def load(self):
        if os.path.exists(self.filepath):
            try:
                self.model.load_state_dict(torch.load(self.filepath, map_location=self.device))
                print(f"Loaded model from {self.filepath}")
            except Exception as e:
                print(f"Failed to load model: {e}")
                
class ChessBot:
    def __init__(self):
        init_db() # Ensure DB exists on startup
        self.board = chess.Board()
        self.learner = NeuralLearner()
        self.training_status = {"active": False, "game": "0/0", "fen": ""}

    def get_legal_moves(self):
        return list(self.board.legal_moves)

    def make_move(self, move_uci: str):
        try:
            move = chess.Move.from_uci(move_uci)
        except ValueError:
            return False
            
        if move in self.board.legal_moves:
            self.board.push(move)
            return True
        return False

def evaluate_position(board):
    """Enhanced chess evaluation from White's perspective"""
    if board.is_checkmate():
        # Prefer faster checkmates
        return -10000 if board.turn == chess.WHITE else 10000
    if board.is_stalemate() or board.is_insufficient_material():
        return 0
    
    # Material score from White's perspective
    score = get_material_score(board)
    
    # King safety - penalize exposed kings
    white_king_square = board.king(chess.WHITE)
    black_king_square = board.king(chess.BLACK)
    
    if white_king_square:
        # Count attackers near white king
        white_king_attackers = 0
        for square in chess.SQUARES:
            if board.is_attacked_by(chess.BLACK, square):
                if chess.square_distance(square, white_king_square) <= 2:
                    white_king_attackers += 1
        score -= white_king_attackers * 0.5
    
    if black_king_square:
        # Count attackers near black king
        black_king_attackers = 0
        for square in chess.SQUARES:
            if board.is_attacked_by(chess.WHITE, square):
                if chess.square_distance(square, black_king_square) <= 2:
                    black_king_attackers += 1
        score += black_king_attackers * 0.5
    
    # Piece-square tables (center control)
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece:
            rank, file = divmod(square, 8)
            center_bonus = 0
            if 2 <= rank <= 5 and 2 <= file <= 5:
                center_bonus = 0.1
            
            if piece.color == chess.WHITE:
                score += center_bonus
            else:
                score -= center_bonus
    
    # Mobility bonus
    original_turn = board.turn
    board.turn = chess.WHITE
    white_mobility = len(list(board.legal_moves))
    board.turn = chess.BLACK
    black_mobility = len(list(board.legal_moves))
    board.turn = original_turn  # Restore original turn
    
    score += (white_mobility - black_mobility) * 0.05
    
    return score

def order_moves(board, moves):
    """Order moves for better alpha-beta pruning: captures first, then checks"""
    def move_priority(move):
        priority = 0
        # Captures are highest priority
        if board.is_capture(move):
            captured = board.piece_at(move.to_square)
            if captured:
                priority += 10 + captured.piece_type
        # Checks are second priority
        board.push(move)
        if board.is_check():
            priority += 5
        board.pop()
        return -priority  # Negative for descending sort
    
    return sorted(moves, key=move_priority)

class ChessBot:
    def __init__(self):
        init_db() # Ensure DB exists on startup
        self.board = chess.Board()
        self.learner = NeuralLearner()
        self.training_status = {"active": False, "game": "0/0", "fen": ""}

    def get_legal_moves(self):
        return list(self.board.legal_moves)

    def make_move(self, move_uci: str):
        try:
            move = chess.Move.from_uci(move_uci)
        except ValueError:
            return False
            
        if move in self.board.legal_moves:
            self.board.push(move)
            return True
        return False

    def get_best_move(self, board=None, randomness=0.1, depth=3):
        target_board = board if board else self.board
        moves = list(target_board.legal_moves)
        
        if not moves:
            return None
        
        if random.random() < randomness:
            return random.choice(moves).uci()
        
        best_move = None
        best_value = -float('inf')
        alpha = -float('inf')
        beta = float('inf')
        
        # Order moves for better pruning
        ordered_moves = order_moves(target_board, moves)
        
        for move in ordered_moves:
            target_board.push(move)
            # Negamax: negate the score from opponent's perspective
            value = -self._alphabeta(target_board, depth - 1, -beta, -alpha)
            target_board.pop()
            
            if value > best_value:
                best_value = value
                best_move = move
            alpha = max(alpha, value)
        
        if best_move:
            return best_move.uci()
        return random.choice(moves).uci()
    
    def _alphabeta(self, board, depth, alpha, beta):
        """Negamax with alpha-beta pruning"""
        if depth == 0:
            # Use quiescence search instead of static eval
            return self._quiescence(board, alpha, beta)
        
        if board.is_game_over():
            eval_score = evaluate_position(board)
            return eval_score if board.turn == chess.WHITE else -eval_score
        
        # Move ordering for better pruning
        moves = order_moves(board, list(board.legal_moves))
        
        value = -float('inf')
        for move in moves:
            board.push(move)
            value = max(value, -self._alphabeta(board, depth - 1, -beta, -alpha))
            board.pop()
            alpha = max(alpha, value)
            if alpha >= beta:
                break  # Beta cutoff
        return value
    
    def _quiescence(self, board, alpha, beta, max_depth=4):
        """Quiescence search - only search captures to avoid horizon effects"""
        stand_pat = evaluate_position(board)
        stand_pat = stand_pat if board.turn == chess.WHITE else -stand_pat
        
        if stand_pat >= beta:
            return beta
        if alpha < stand_pat:
            alpha = stand_pat
        
        if max_depth == 0:
            return stand_pat
        
        # Only search captures
        captures = [m for m in board.legal_moves if board.is_capture(m)]
        
        for move in captures:
            board.push(move)
            score = -self._quiescence(board, -beta, -alpha, max_depth - 1)
            board.pop()
            
            if score >= beta:
                return beta
            if score > alpha:
                alpha = score
        
        return alpha
    
    def train(self, iterations=100, start_fen=None):
        wins = 0
        losses = 0
        draws = 0
        total_loss = 0
        
        for _ in range(iterations):
            if start_fen:
                try:
                    temp_board = chess.Board(start_fen)
                except ValueError:
                    temp_board = chess.Board()
            else:
                temp_board = chess.Board()
            
            # Update status for UI
            self.training_status = {
                "active": True,
                "game": f"{_+1}/{iterations}",
                "fen": temp_board.fen()
            }
                
            game_history = [] # List of (board_state_copy, active_player)
            
            while not temp_board.is_game_over():
                moves = list(temp_board.legal_moves)
                if not moves: break
                
                # Update FEN for UI
                self.training_status["fen"] = temp_board.fen()
                
                # Use the model to select moves (Self-Play)
                # We use some exploration noise
                
                best_move = None
                best_val = -float('inf')
                
                # Simple policy: 80% best move, 20% random during training
                if random.random() < 0.2:
                    move = random.choice(moves)
                else:
                    # Look ahead 1 step
                    for m in moves:
                        temp_board.push(m)
                        if temp_board.is_checkmate():
                            val = 9999
                        elif temp_board.is_variant_draw(): 
                            val = 0
                        else:
                            val = -self.learner.get_value(temp_board)
                        temp_board.pop()
                        
                        if val > best_val:
                            best_val = val
                            best_move = m
                    move = best_move if best_move else random.choice(moves)

                # Store state BEFORE move
                # We want to map THIS state to the eventual outcome for the player whose turn it is.
                game_history.append((temp_board.copy(), temp_board.turn))
                
                temp_board.push(move)
                
                if len(game_history) > 100: # Max moves to prevent forever games
                    break
            
            # Determine result
            result = temp_board.result()
            
            # Reward: 1 for Win, -1 for Loss, 0 for Draw
            # White Win: White gets 1, Black gets -1
            if result == "1-0":
                rewards = {chess.WHITE: 1.0, chess.BLACK: -1.0}
                wins += 1
            elif result == "0-1":
                rewards = {chess.WHITE: -1.0, chess.BLACK: 1.0}
                losses += 1
            else:
                rewards = {chess.WHITE: 0.0, chess.BLACK: 0.0}
                draws += 1
                
            # Prepare training batch
            states = []
            targets = []
            
            # Q-Learning / Monte Carlo approach:
            # We assign the final game result as the target value for ALL positions visited.
            # (Discount factor gamma could be used, but for simplicity gamma=1)
            
            for board_state, turn in game_history:
                target_val = rewards[turn]
                states.append(board_state)
                targets.append(target_val)
                
            # Train model
            if states:
                batch_loss = self.learner.train_batch(states, targets)
                total_loss += batch_loss
                
        self.learner.save()
        stats = {
            "games": iterations, 
            "white_wins": wins, 
            "black_wins": losses, 
            "draws": draws,
            "avg_loss": total_loss / iterations if iterations > 0 else 0
        }
        self.training_status["active"] = False
        self.training_status["stats"] = stats
        return stats

    def _training_worker(self, iterations, start_fen, event_queue, draw_punishment, material_weight, depth):
        import json
        event_queue.put(json.dumps({"type": "start", "total_games": iterations}) + "\n")
        
        wins = 0
        losses = 0
        draws = 0
        total_loss = 0
        
        for i in range(iterations):
            if start_fen:
                try:
                    temp_board = chess.Board(start_fen)
                except ValueError:
                    temp_board = chess.Board()
            else:
                temp_board = chess.Board()
                
            game_history = []
            
            while not temp_board.is_game_over():
                moves = list(temp_board.legal_moves)
                if not moves: break
                
                best_move = None
                best_val = -float('inf')
                
                if random.random() < 0.2:
                    move = random.choice(moves)
                else:
                    for m in moves:
                        temp_board.push(m)
                        if temp_board.is_checkmate():
                            val = 9999
                        elif temp_board.is_variant_draw(): 
                            val = 0
                        else:
                            val = -self.learner.get_value(temp_board)
                        temp_board.pop()
                        
                        if val > best_val:
                            best_val = val
                            best_move = m
                    move = best_move if best_move else random.choice(moves)

                game_history.append((temp_board.copy(), temp_board.turn))
                temp_board.push(move)
                
                # yield move event
                event_queue.put(json.dumps({
                    "type": "move", 
                    "fen": temp_board.fen(), 
                    "game": i + 1
                }) + "\n")
                
                if len(game_history) > 100:
                    break
            
            result = temp_board.result()
            mat_score = get_material_score(temp_board) # Positive = White Advantage
            
            rewards = {chess.WHITE: 0.0, chess.BLACK: 0.0}

            if result == "1-0":
                rewards[chess.WHITE] = 1.0
                rewards[chess.BLACK] = -1.0
                wins += 1
            elif result == "0-1":
                rewards[chess.WHITE] = -1.0
                rewards[chess.BLACK] = 1.0
                losses += 1
            else:
                # DRAW
                rewards[chess.WHITE] = 0.0
                rewards[chess.BLACK] = 0.0
                
                # Punish if ahead
                if mat_score > 3: # White was winning
                     rewards[chess.WHITE] -= draw_punishment
                elif mat_score < -3: # Black was winning
                     rewards[chess.BLACK] -= draw_punishment
                     
                draws += 1
            # Log data to SQLite
            try:
                # Ensure DB is ready in case init_db wasn't called outside
                conn = sqlite3.connect("training_data.db")
                cursor = conn.cursor()
                
                safe_start_fen = start_fen if start_fen else chess.STARTING_FEN
                data_to_insert = []
                
                for step, (board_state, turn) in enumerate(game_history):
                    mat = get_material_score(board_state)
                    
                    pixel_result = 0
                    if result == "1-0":
                        pixel_result = 1 if turn == chess.WHITE else -1
                    elif result == "0-1":
                        pixel_result = 1 if turn == chess.BLACK else -1
                    else:
                        pixel_result = 0
                    
                    turn_str = "White" if turn == chess.WHITE else "Black"
                    
                    # Generate a simple unique ID for this game
                    import uuid
                    game_id = str(uuid.uuid4())
                    
                    data_to_insert.append((game_id, safe_start_fen, step, turn_str, mat, pixel_result, board_state.fen()))
                
                # We assume the table was created by init_db() called at thread start.
                # If not, this insert will fail. But init_db() is called in _training_worker entry.
                cursor.executemany("INSERT INTO training_moves (game_id, start_fen, step, turn, material_score, result, fen) VALUES (?, ?, ?, ?, ?, ?, ?)", data_to_insert)
                conn.commit()
                conn.close()
            except Exception as e:
                print(f"Logging failed: {e}")

            states = []
            targets = []
            for board_state, turn in game_history:
                # Base Reward
                r = rewards[turn]
                
                # Material Shaping
                curr_mat = get_material_score(board_state)
                # If turn is White, mat advantage is curr_mat. If Black, -curr_mat.
                my_mat = curr_mat if turn == chess.WHITE else -curr_mat
                
                # Add scaled material bonus (e.g. +1 pawn = +0.1 reward)
                # SCALING FACTOR: 0.05 * weight. 
                # Explanation: Material diff usually ranges -10 to 10.
                # 10 * 0.05 = 0.5. So a full queen advantage gives +0.5 reward bonus.
                r += (my_mat * 0.05 * material_weight)
                
                states.append(board_state)
                targets.append(r)
                
            if states:
                batch_loss = self.learner.train_batch(states, targets)
                total_loss += batch_loss
                
            event_queue.put(json.dumps({
                "type": "game_end",
                "game": i + 1,
                "result": result,
                "current_stats": {"w": wins, "l": losses, "d": draws}
            }) + "\n")
                
        self.learner.save()
        final_stats = {
            "games": iterations, 
            "white_wins": wins, 
            "black_wins": losses, 
            "draws": draws,
            "avg_loss": total_loss / iterations if iterations > 0 else 0
        }
        event_queue.put(json.dumps({"type": "complete", "stats": final_stats}) + "\n")
        print(f"Server: Training Finished. Stats: {final_stats}")
        event_queue.put(None) # Sentinel

    def train_gen(self, iterations=100, start_fen=None, draw_punishment=0.0, material_weight=0.0, depth=1):
        import queue
        import threading
        
        q = queue.Queue()
        t = threading.Thread(target=self._training_worker, args=(iterations, start_fen, q, draw_punishment, material_weight, depth))
        t.start()
        
        while True:
            item = q.get()
            if item is None:
                break
            yield item
        
        t.join()

    def get_fen(self):
        return self.board.fen()
    
    def reset_board(self):
        self.board.reset()

    def is_game_over(self):
        return self.board.is_game_over()

    def get_analytics_data(self):
        try:
            conn = sqlite3.connect("training_data.db")
            cursor = conn.cursor()
            
            # 1. Win Rate History (White's perspective)
            # We take one row per game where turn is White to get the result.
            cursor.execute('''
                SELECT result 
                FROM training_moves 
                WHERE turn = 'White' AND opponent = 'Self'
                GROUP BY game_id 
                ORDER BY id DESC
                LIMIT 5000
            ''')
            rows = cursor.fetchall()
            rows.reverse()
            
            history = []
            batch_size = 10
            current_batch = {"batch_index": 0, "wins": 0, "losses": 0, "draws": 0}
            count = 0
            batch_idx = 1
            
            for r in rows:
                res = r[0] # 1 (Win), -1 (Loss), 0 (Draw)
                if res == 1: current_batch["wins"] += 1
                elif res == -1: current_batch["losses"] += 1
                else: current_batch["draws"] += 1
                
                count += 1
                if count >= batch_size:
                    current_batch["batch_index"] = batch_idx
                    history.append(current_batch)
                    current_batch = {"batch_index": 0, "wins": 0, "losses": 0, "draws": 0}
                    count = 0
                    batch_idx += 1
            
            if count > 0: 
                current_batch["batch_index"] = batch_idx
                history.append(current_batch)
            
            # 2. Recent Games Tracker
            # Get last 50 games info: ID, Moves Count, Result (White's perspective), Opponent
            try:
                # Try to select opponent column, fallback if not exists (handled by init_db but safety check)
                cursor.execute('''
                    SELECT game_id, count(step) as moves, max(result) as res, opponent
                    FROM training_moves 
                    WHERE turn = 'White'
                    GROUP BY game_id 
                    ORDER BY id DESC 
                    LIMIT 50
                ''')
            except:
                 # Fallback for old schema if migration failed slightly
                 cursor.execute('''
                    SELECT game_id, count(step) as moves, max(result) as res, 'Self' as opponent
                    FROM training_moves 
                    WHERE turn = 'White'
                    GROUP BY game_id 
                    ORDER BY id DESC 
                    LIMIT 50
                ''')

            recent_rows = cursor.fetchall()
            recent_games = []
            for r in recent_rows:
                # r[2] is result for White: 1=Win, -1=Loss, 0=Draw
                res_str = "Draw"
                if r[2] == 1: res_str = "White Won"
                elif r[2] == -1: res_str = "Black Won"
                
                recent_games.append({
                    "id": r[0],
                    "moves": r[1],
                    "winner": res_str,
                    "opponent": r[3]
                })
                
            conn.close()
            return {"history": history, "recent": recent_games}
            
        except Exception as e:
            print(f"Analytics error: {e}")
            return {"history": [], "recent": []}
            
    def _battle_worker(self, iterations, engine_path, elo, event_queue, start_fen=None, depth=3, bot_side="alternate"):
        import json
        import chess.engine
        
        try:
            # Check if engine exists
            if not os.path.isfile(engine_path):
                event_queue.put(json.dumps({"type": "error", "message": f"Engine not found at {engine_path}"}) + "\n")
                event_queue.put(None)
                return

            event_queue.put(json.dumps({"type": "start", "total_games": iterations}) + "\n")
            
            engine = chess.engine.SimpleEngine.popen_uci(engine_path)
            
            # Configure Elo if possible (Stockfish supports 'UCI_LimitStrength' and 'UCI_Elo')
            # Stockfish minimum Elo is 1320, so for lower ratings we use time limits
            elo_configured = False
            stockfish_time = 0.1  # Default time per move
            
            if elo >= 1320:
                try:
                    engine.configure({"UCI_LimitStrength": True, "UCI_Elo": elo})
                    elo_configured = True
                    print(f"Stockfish configured to Elo {elo}")
                    event_queue.put(json.dumps({"type": "info", "message": f"Stockfish Elo set to {elo}"}) + "\n")
                except Exception as e:
                    print(f"Failed to configure Stockfish Elo: {e}")
                    event_queue.put(json.dumps({"type": "warning", "message": f"Stockfish Elo limiting failed: {e}"}) + "\n")
            else:
                # For Elo < 1320, weaken by limiting thinking time
                # Map Elo to time: 100 Elo = 0.001s, 1000 Elo = 0.05s, 1320 Elo = 0.1s
                stockfish_time = max(0.001, (elo / 1320) * 0.1)
                print(f"Weakening Stockfish to ~{elo} Elo using {stockfish_time:.4f}s per move")
                event_queue.put(json.dumps({"type": "info", "message": f"Stockfish weakened to ~{elo} Elo (time limit: {stockfish_time:.4f}s)"}) + "\n")
                
            wins = 0
            losses = 0
            draws = 0
            
            for i in range(iterations):
                if start_fen:
                    try:
                        temp_board = chess.Board(start_fen)
                    except ValueError:
                        temp_board = chess.Board()
                else:
                    temp_board = chess.Board()
                    
                game_history = []
                
                # Determine bot color based on bot_side setting
                if bot_side == "white":
                    bot_color = chess.WHITE
                elif bot_side == "black":
                    bot_color = chess.BLACK
                else:  # "alternate"
                    bot_color = chess.WHITE if i % 2 == 0 else chess.BLACK
                
                white_name = "Hero Bot" if bot_color == chess.WHITE else f"Stockfish (Elo {elo})"
                black_name = "Hero Bot" if bot_color == chess.BLACK else f"Stockfish (Elo {elo})"
                
                event_queue.put(json.dumps({
                    "type": "game_start", 
                    "game": i + 1,
                    "white": white_name,
                    "black": black_name
                }) + "\n")
                
                while not temp_board.is_game_over():
                    if temp_board.turn == bot_color:
                        # Bot Move
                        move_uci = self.get_best_move(board=temp_board, randomness=0.05, depth=depth)
                        move = chess.Move.from_uci(move_uci)
                    else:
                        # Engine Move (use configured time limit)
                        result = engine.play(temp_board, chess.engine.Limit(time=stockfish_time))
                        move = result.move
                        
                    game_history.append((temp_board.copy(), temp_board.turn))
                    temp_board.push(move)
                    
                    event_queue.put(json.dumps({
                        "type": "move", 
                        "fen": temp_board.fen(), 
                        "game": i + 1
                    }) + "\n")
                    
                    if len(game_history) > 150: break
                
                result = temp_board.result()
                
                # Determine Bot Result
                bot_result = 0 # Draw
                if result == "1-0":
                    if bot_color == chess.WHITE: 
                        bot_result = 1
                        wins += 1
                    else: 
                        bot_result = -1
                        losses += 1
                elif result == "0-1":
                    if bot_color == chess.BLACK: 
                        bot_result = 1
                        wins += 1
                    else: 
                        bot_result = -1
                        losses += 1
                else:
                    draws += 1
                    
                # Log to DB
                try:
                    conn = sqlite3.connect("training_data.db")
                    cursor = conn.cursor()
                    
                    data_to_insert = []
                    import uuid
                    game_id = str(uuid.uuid4())
                    
                    # Use provided start_fen for logging if available, otherwise standard
                    log_start_fen = start_fen if start_fen else "Standard" 
                    if not start_fen: log_start_fen = "Standard"

                    for step, (board_state, turn) in enumerate(game_history):
                        mat = get_material_score(board_state)
                        
                        # Result from PERSPECTIVE of the turn player
                        # If turn == bot_color, result = bot_result
                        # If turn != bot_color, result = -bot_result
                        
                        pixel_result = 0
                        if turn == bot_color: pixel_result = bot_result
                        else: pixel_result = -bot_result
                        
                        turn_str = "White" if turn == chess.WHITE else "Black"
                        
                        data_to_insert.append((game_id, log_start_fen, step, turn_str, mat, pixel_result, board_state.fen(), "Stockfish"))
                    
                    cursor.executemany("INSERT INTO training_moves (game_id, start_fen, step, turn, material_score, result, fen, opponent) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", data_to_insert)
                    conn.commit()
                    conn.close()
                except Exception as e:
                    print(f"Battle Logging failed: {e}")
                    
                event_queue.put(json.dumps({
                    "type": "game_end",
                    "game": i + 1,
                    "result": result,
                    "current_stats": {"w": wins, "l": losses, "d": draws}
                }) + "\n")
                
            engine.quit()
            
            final_stats = {"games": iterations, "wins": wins, "losses": losses, "draws": draws}
            event_queue.put(json.dumps({"type": "complete", "stats": final_stats}) + "\n")
            event_queue.put(None)
            
        except Exception as e:
            event_queue.put(json.dumps({"type": "error", "message": str(e)}) + "\n")
            event_queue.put(None)

    def battle_stockfish_gen(self, iterations=10, engine_path="stockfish.exe", elo=1350, start_fen=None, depth=3, bot_side="alternate"):
        import queue
        import threading
        
        q = queue.Queue()
        t = threading.Thread(target=self._battle_worker, args=(iterations, engine_path, elo, q, start_fen, depth, bot_side))
        t.start()
        
        while True:
            item = q.get()
            if item is None:
                break
            yield item
        
        t.join()
