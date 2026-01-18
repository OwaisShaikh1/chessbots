import chess
import chess.polyglot
import chess.engine
import random
import os
import torch
import torch.optim as optim
import torch.nn as nn
import json
import sqlite3

from model import ChessNet, board_to_tensor
from evaluation import get_material_score, evaluate_position, order_moves
from search import ChessSearch
from database import init_db, get_analytics_data


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
        self.search_engine = ChessSearch()
        self.training_status = {"active": False, "game": "0/0", "fen": ""}
        self.battle_paused = False

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
        """Find the best move using iterative deepening and alpha-beta search"""
        target_board = board if board else self.board
        moves = list(target_board.legal_moves)
        
        if not moves:
            return None
        
        if random.random() < randomness:
            return random.choice(moves).uci()
        
        # Clear Transposition Table for a fresh search
        self.search_engine.clear_tt()
        
        best_move = moves[0]
        
        # Iterative Deepening: start from depth 1 up to target depth
        # This improves pruning significantly by populating the TT
        for current_depth in range(1, depth + 1):
            alpha = -float('inf')
            beta = float('inf')
            
            # Use previously found best move if available (from TT)
            ordered_moves = order_moves(target_board, moves)
            board_hash = chess.polyglot.zobrist_hash(target_board)
            if board_hash in self.search_engine.tt:
                _, _, _, tt_move = self.search_engine.tt[board_hash]
                if tt_move in ordered_moves:
                    ordered_moves.remove(tt_move)
                    ordered_moves.insert(0, tt_move)

            best_value = -float('inf')
            current_best_move = best_move
            
            for move in ordered_moves:
                target_board.push(move)
                # Ply=1 here for the children of the root
                value = -self.search_engine.alphabeta(target_board, current_depth - 1, -beta, -alpha, 1)
                target_board.pop()
                
                if value > best_value:
                    best_value = value
                    current_best_move = move
                alpha = max(alpha, value)
            
            best_move = current_best_move
            
        return best_move.uci()
    
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

    def get_analytics(self):
        """Get training analytics from database"""
        return get_analytics_data()
            
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
                    # Handle pause
                    while self.battle_paused:
                        import time
                        time.sleep(0.1)
                        if not self.battle_paused: break # Double check for exit

                    if temp_board.turn == bot_color:
                        # Bot Move
                        move_uci = self.get_best_move(board=temp_board, randomness=0.0, depth=depth)
                        move = chess.Move.from_uci(move_uci)
                    else:
                        # Engine Move (use configured time limit)
                        result = engine.play(temp_board, chess.engine.Limit(time=stockfish_time))
                        move = result.move
                        
                    move_san = temp_board.san(move)
                    game_history.append((temp_board.copy(), temp_board.turn))
                    temp_board.push(move)
                    
                    event_queue.put(json.dumps({
                        "type": "move", 
                        "fen": temp_board.fen(), 
                        "move_san": move_san,
                        "game": i + 1,
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
    def _history_training_worker(self, event_queue, batch_size=32, epochs=5):
        import json
        try:
            conn = sqlite3.connect("training_data.db")
            cursor = conn.cursor()
            
            # Fetch all moves from Stockfish battles
            cursor.execute("SELECT fen, result FROM training_moves WHERE opponent = 'Stockfish'")
            rows = cursor.fetchall()
            conn.close()
            
            if not rows:
                event_queue.put(json.dumps({"type": "error", "message": "No battle history found in database."}) + "\n")
                event_queue.put(None)
                return

            event_queue.put(json.dumps({"type": "start", "total_moves": len(rows), "epochs": epochs}) + "\n")
            
            for epoch in range(epochs):
                random.shuffle(rows)
                total_loss = 0
                batches = 0
                
                for i in range(0, len(rows), batch_size):
                    batch = rows[i:i+batch_size]
                    states = []
                    targets = []
                    
                    for fen, result in batch:
                        try:
                            states.append(chess.Board(fen))
                            targets.append(float(result))
                        except Exception:
                            continue
                            
                    if states:
                        loss = self.learner.train_batch(states, targets)
                        total_loss += loss
                        batches += 1
                        
                        event_queue.put(json.dumps({
                            "type": "progress", 
                            "epoch": epoch + 1, 
                            "batch": batches, 
                            "loss": loss,
                            "total_batches": (len(rows) + batch_size - 1) // batch_size
                        }) + "\n")
                
                print(f"Epoch {epoch+1} complete. Avg Loss: {total_loss/batches if batches > 0 else 0}")
            
            self.learner.save()
            event_queue.put(json.dumps({"type": "complete", "message": "Training from history finished."}) + "\n")
            event_queue.put(None)
            
        except Exception as e:
            event_queue.put(json.dumps({"type": "error", "message": str(e)}) + "\n")
            event_queue.put(None)

    def train_from_history_gen(self, batch_size=32, epochs=5):
        import queue
        import threading
        
        q = queue.Queue()
        t = threading.Thread(target=self._history_training_worker, args=(q, batch_size, epochs))
        t.start()
        
        while True:
            item = q.get()
            if item is None:
                break
            yield item
        t.join()
