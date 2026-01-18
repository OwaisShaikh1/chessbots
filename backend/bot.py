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
import torch.autograd
import threading
import queue
from concurrent.futures import ThreadPoolExecutor

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
        
        # Fast caching for search evaluation
        self._eval_cache = {}
        self._max_cache_size = 2048
        
        self.load()

    def get_value(self, board):
        """
        Returns a value between -1 and 1 for the current board state.
        Value is from the perspective of the player to move (active color).
        1.0 means the active player is winning, -1.0 means losing.
        """
        fen = board.fen()
        # Fast cache lookup
        if fen in self._eval_cache:
            return self._eval_cache[fen]

        self.model.eval()
        with torch.no_grad():
            tensor = board_to_tensor(board).to(self.device)
            value = self.model(tensor).item()
            
        # Update cache (simple FIFO-ish eviction if too full)
        if len(self._eval_cache) >= self._max_cache_size:
            # Clear half the cache if it overflows
            keys = list(self._eval_cache.keys())
            for k in keys[:len(keys)//2]:
                del self._eval_cache[k]
        
        self._eval_cache[fen] = value
        return value

    def clear_cache(self):
        """Should be called at the start of a new move search"""
        self._eval_cache.clear()

    def train_batch(self, states, targets):
        """
        Trains the model on a batch of states and target values.
        states: List of chess.Board objects (or tensors)
        targets: List of floats [-1, 1]
        """
        self.model.train()

        # Convert all boards to a single batch tensor
        tensors = [board_to_tensor(s).squeeze(0) for s in states]  # Remove batch dim to stack
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


# Async Stockfish evaluation queue - runs in background thread
class StockfishEvalQueue:
    """Background queue for Stockfish evaluations to avoid blocking game play"""
    
    def __init__(self, engine_path=None, max_queue_size=1000):
        self.queue = queue.Queue(maxsize=max_queue_size)
        self.engine_path = engine_path
        self.engine = None
        self.running = False
        self.thread = None
        self._lock = threading.Lock()
        
    def start(self, engine_path):
        """Start the background evaluation thread"""
        with self._lock:
            if self.running:
                return
            self.engine_path = engine_path
            self.running = True
            self.thread = threading.Thread(target=self._worker, daemon=True)
            self.thread.start()
            print(f"Stockfish eval queue started (engine: {engine_path})")
    
    def stop(self):
        """Stop the background thread"""
        with self._lock:
            self.running = False
            # Add sentinel to unblock queue
            try:
                self.queue.put_nowait(None)
            except queue.Full:
                pass
                
    def add_game(self, game_id, positions, result, bot_color, log_start_fen):
        """Add a game's positions to the evaluation queue"""
        try:
            self.queue.put_nowait({
                "game_id": game_id,
                "positions": positions,  # List of (board_copy, turn, step)
                "result": result,
                "bot_color": bot_color,
                "log_start_fen": log_start_fen
            })
            print(f"Queued game {game_id} for Stockfish analysis ({len(positions)} positions)")
        except queue.Full:
            print(f"Eval queue full, skipping game {game_id}")
    
    def _worker(self):
        """Background worker that processes evaluation requests"""
        try:
            self.engine = chess.engine.SimpleEngine.popen_uci(self.engine_path)
            self.engine.configure({"Threads": 1, "Hash": 16})
            print("Stockfish eval worker engine initialized")
        except Exception as e:
            print(f"Failed to start eval engine: {e}")
            self.running = False
            return
            
        while self.running:
            try:
                item = self.queue.get(timeout=1.0)
                if item is None:
                    continue
                    
                self._process_game(item)
                self.queue.task_done()
                
            except queue.Empty:
                continue
            except Exception as e:
                print(f"Eval worker error: {e}")
                
        if self.engine:
            try:
                self.engine.quit()
            except:
                pass
        print("Stockfish eval worker stopped")
    
    def _process_game(self, item):
        """Process a single game's positions"""
        game_id = item["game_id"]
        positions = item["positions"]
        result = item["result"]
        bot_color = item["bot_color"]
        log_start_fen = item["log_start_fen"]
        
        try:
            conn = sqlite3.connect("training_data.db")
            cursor = conn.cursor()
            data_to_insert = []
            
            for board_state, turn, step in positions:
                mat = get_material_score(board_state)
                
                # Result from PERSPECTIVE of the turn player
                bot_result = 0
                if result == "1-0":
                    bot_result = 1 if bot_color == chess.WHITE else -1
                elif result == "0-1":
                    bot_result = 1 if bot_color == chess.BLACK else -1
                    
                pixel_result = 0
                if turn == bot_color:
                    pixel_result = bot_result
                else:
                    pixel_result = -bot_result
                
                turn_str = "White" if turn == chess.WHITE else "Black"
                
                # Get Stockfish evaluation
                try:
                    info = self.engine.analyse(board_state, chess.engine.Limit(depth=15))
                    score = info["score"].white()
                    
                    if score.is_mate():
                        mate_in = score.mate()
                        stockfish_eval = 10000 if mate_in > 0 else -10000
                    else:
                        stockfish_eval = score.score()
                except:
                    stockfish_eval = None
                
                data_to_insert.append((
                    game_id, log_start_fen, step, turn_str, mat, 
                    pixel_result, board_state.fen(), "Stockfish", stockfish_eval
                ))
            
            cursor.executemany(
                "INSERT INTO training_moves (game_id, start_fen, step, turn, material_score, result, fen, opponent, stockfish_eval) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                data_to_insert
            )
            conn.commit()
            conn.close()
            print(f"Completed analysis of game {game_id} ({len(positions)} positions)")
            
        except Exception as e:
            print(f"Failed to save game {game_id}: {e}")
                
                
class ChessBot:
    def __init__(self):
        init_db() # Ensure DB exists on startup
        self.board = chess.Board()
        self.learner = NeuralLearner()
        self.search_engine = ChessSearch()
        self.training_status = {"active": False, "game": "0/0", "fen": ""}
        self.battle_paused = False
        self.eval_queue = StockfishEvalQueue()  # Background eval queue

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
        """Find the best move using iterative deepening with aspiration windows"""
        target_board = board if board else self.board
        moves = list(target_board.legal_moves)
        
        if not moves:
            return None
        
        if random.random() < randomness:
            return random.choice(moves).uci()
        
        # Clear Transposition Table and Neural Cache for a fresh search
        self.search_engine.clear_tt()
        self.learner.clear_cache()
        
        best_move = moves[0]
        best_value = 0
        
        # Aspiration window parameters
        ASPIRATION_DELTA = 50  # Initial window size in centipawns
        
        # Iterative Deepening: start from depth 1 up to target depth
        for current_depth in range(1, depth + 1):
            # Use aspiration windows after depth 1
            if current_depth > 1:
                delta = ASPIRATION_DELTA
                alpha = best_value - delta
                beta = best_value + delta
            else:
                alpha = -float('inf')
                beta = float('inf')
            
            # Use previously found best move first (from TT or previous iteration)
            ordered_moves = order_moves(target_board, moves)
            board_hash = chess.polyglot.zobrist_hash(target_board)
            if board_hash in self.search_engine.tt:
                _, _, _, tt_move = self.search_engine.tt[board_hash]
                if tt_move in ordered_moves:
                    ordered_moves.remove(tt_move)
                    ordered_moves.insert(0, tt_move)
            elif best_move in ordered_moves:
                ordered_moves.remove(best_move)
                ordered_moves.insert(0, best_move)

            # Aspiration window loop - widen if we fail high/low
            while True:
                current_best_value = -float('inf')
                current_best_move = best_move
                
                # Save original window bounds for fail-low/high detection
                original_alpha = alpha
                original_beta = beta
                
                for i, move in enumerate(ordered_moves):
                    target_board.push(move)
                    
                    # Full window search at root for correctness
                    # PVS at root can cause issues with aspiration windows
                    value = -self.search_engine.alphabeta(
                        target_board, current_depth - 1, -beta, -alpha, 1, 
                        eval_model=self.learner, allow_nmp=True, is_pv=True
                    )
                    
                    target_board.pop()
                    
                    if value > current_best_value:
                        current_best_value = value
                        current_best_move = move
                    alpha = max(alpha, value)
                    
                    if alpha >= beta:
                        break
                
                # Check if we need to widen the aspiration window
                if current_depth > 1:
                    if current_best_value <= original_alpha:
                        # Fail low - widen lower bound and re-search
                        alpha = -float('inf')
                        beta = original_beta
                        continue
                    elif current_best_value >= original_beta:
                        # Fail high - widen upper bound and re-search
                        alpha = original_alpha  
                        beta = float('inf')
                        continue
                
                # Search complete for this depth
                best_value = current_best_value
                best_move = current_best_move
                break
            
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
                
                if len(game_history) > 400: # Max moves to prevent forever games
                    break
            
            # Determine result with adjudication if needed
            result = temp_board.result()
            
            # Adjudicate if game didn't end naturally
            if result == "*" and len(game_history) > 200:
                mat_advantage = get_material_score(temp_board)
                if mat_advantage > 3:
                    result = "1-0"
                elif mat_advantage < -3:
                    result = "0-1"
                else:
                    result = "1/2-1/2"
            
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
                
                if len(game_history) > 200:
                    break
            
            result = temp_board.result()
            
            # Adjudicate if needed
            if result == "*" and len(game_history) > 200:
                mat_advantage = get_material_score(temp_board)
                if mat_advantage > 3:
                    result = "1-0"
                elif mat_advantage < -3:
                    result = "0-1"
                else:
                    result = "1/2-1/2"
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
            
    def _battle_worker(self, iterations, engine_path, elo, event_queue, start_fen=None, depth=3, bot_side="alternate", threads=1, hash_size=16):
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
            
            # Configure engine parameters
            engine_configs = {
                "Threads": threads,
                "Hash": hash_size
            }
            
            # Configure Elo if possible (Stockfish supports 'UCI_LimitStrength' and 'UCI_Elo')
            # Stockfish minimum Elo is 1320, so for lower ratings we use time limits
            elo_configured = False
            stockfish_time = 0.1  # Default time per move
            
            if elo >= 1320:
                engine_configs["UCI_LimitStrength"] = True
                engine_configs["UCI_Elo"] = elo
                elo_configured = True
                print(f"Stockfish configured: Elo={elo}, Threads={threads}, Hash={hash_size}")
                event_queue.put(json.dumps({"type": "info", "message": f"Stockfish configured: Elo={elo}, Threads={threads}, Hash={hash_size}"}) + "\n")
            else:
                # For Elo < 1320, weaken by limiting thinking time
                stockfish_time = max(0.001, (elo / 1320) * 0.1)
                print(f"Weakening Stockfish to ~{elo} Elo (Threads={threads}, Hash={hash_size})")
                event_queue.put(json.dumps({"type": "info", "message": f"Stockfish weakened: ~{elo} Elo, Threads={threads}, Hash={hash_size}"}) + "\n")
            
            engine.configure(engine_configs)
                
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
                    
                    # Check if game ended after this move (checkmate, stalemate, etc.)
                    if temp_board.is_game_over():
                        break
                    
                    # Longer move limit for endgames, but adjudicate if reached
                    # if len(game_history) > 300:
                    #     print(f"Game {i+1} reached move limit. Adjudicating...")
                    #     break
                
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
                    
                # Queue game for async Stockfish evaluation (non-blocking)
                import uuid
                game_id = str(uuid.uuid4())
                log_start_fen = start_fen if start_fen else "Standard"
                
                # Prepare positions for async eval
                positions = [(board_state.copy(), turn, step) for step, (board_state, turn) in enumerate(game_history)]
                
                # Start eval queue if not running
                if not self.eval_queue.running:
                    self.eval_queue.start(engine_path)
                
                # Queue the game for background analysis
                self.eval_queue.add_game(game_id, positions, result, bot_color, log_start_fen)
                    
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

    def battle_stockfish_gen(self, iterations=10, engine_path="stockfish.exe", elo=1350, start_fen=None, depth=3, bot_side="alternate", threads=1, hash_size=16):
        import queue
        import threading
        
        q = queue.Queue()
        t = threading.Thread(target=self._battle_worker, args=(iterations, engine_path, elo, q, start_fen, depth, bot_side, threads, hash_size))
        t.start()
        
        while True:
            item = q.get()
            if item is None:
                break
            yield item
        
        t.join()

    def _history_training_worker(self, event_queue, batch_size=32, epochs=5):
        import json
        import time
        import math
        try:
            conn = sqlite3.connect("training_data.db")
            cursor = conn.cursor()
            
            # Fetch all moves from Stockfish battles WITH Stockfish evaluations
            # IMPORTANT: Filter out extreme mate scores (±10000) which cause saturation
            cursor.execute("SELECT fen, stockfish_eval FROM training_moves WHERE opponent = 'Stockfish' AND stockfish_eval IS NOT NULL AND stockfish_eval > -5000 AND stockfish_eval < 5000")
            rows = cursor.fetchall()
            conn.close()
            
            # Check if model is saturated (always outputs same value)
            # If so, reinitialize the model to allow learning
            test_boards = [chess.Board(r[0]) for r in rows[:10] if r[0]]
            if test_boards:
                self.learner.model.eval()
                with torch.no_grad():
                    test_tensors = [board_to_tensor(b).squeeze(0) for b in test_boards]
                    test_batch = torch.stack(test_tensors).to(self.learner.device)
                    test_preds = self.learner.model(test_batch)
                    pred_std = test_preds.std().item()
                    pred_mean = test_preds.mean().item()
                    print(f"Model check: mean={pred_mean:.4f}, std={pred_std:.4f}")
                    
                    # If std is near zero (all same prediction) or all saturated (±1), reinitialize
                    if pred_std < 0.01 or abs(pred_mean) > 0.95:
                        print("⚠️ Model is saturated! Reinitializing weights...")
                        self.learner.model = ChessNet().to(self.learner.device)
                        self.learner.optimizer = optim.Adam(self.learner.model.parameters(), lr=0.001)
                        event_queue.put(json.dumps({"type": "warning", "message": "Model was saturated - reinitialized with fresh weights"}) + "\n")
            
            if not rows:
                event_queue.put(json.dumps({"type": "error", "message": "No battle history with Stockfish evaluations found. Please run battles first."}) + "\n")
                event_queue.put(None)
                return

            total_moves = len(rows)
            event_queue.put(json.dumps({
                "type": "start", 
                "total_moves": total_moves, 
                "epochs": epochs,
                "batch_size": batch_size,
                "method": "Stockfish Ground Truth (depth 15)"
            }) + "\n")
            
            # Split into train/validation (80/20)
            random.shuffle(rows)
            split_idx = int(len(rows) * 0.8)
            train_rows = rows[:split_idx]
            val_rows = rows[split_idx:]
            
            event_queue.put(json.dumps({
                "type": "info",
                "train_size": len(train_rows),
                "val_size": len(val_rows)
            }) + "\n")
            
            start_time = time.time()
            best_val_loss = float('inf')
            
            # Track first layer weights to verify model is updating
            first_weight_sum = None
            
            for epoch in range(epochs):
                random.shuffle(train_rows)
                epoch_loss = 0
                batches = 0
                total_batches = (len(train_rows) + batch_size - 1) // batch_size
                
                # Training phase
                for i in range(0, len(train_rows), batch_size):
                    batch = train_rows[i:i+batch_size]
                    states = []
                    targets = []
                    
                    for fen, stockfish_eval in batch:
                        try:
                            board = chess.Board(fen)
                            # Clamp eval to ±2000cp before scaling to prevent saturation
                            clamped_eval = max(-2000, min(2000, stockfish_eval))
                            # Convert Stockfish eval (centipawns) to [-1, 1] range
                            # Use tanh scaling with larger divisor: ±500cp → ±0.46, ±1000cp → ±0.76
                            target = math.tanh(clamped_eval / 800.0)
                            
                            # Flip sign if it's Black's turn (model outputs from turn player perspective)
                            if board.turn == chess.BLACK:
                                target = -target
                            
                            states.append(board)
                            targets.append(target)
                        except Exception:
                            continue
                            
                    if states:
                        loss = self.learner.train_batch(states, targets)
                        epoch_loss += loss
                        batches += 1
                        
                        # Only stream every few batches or if it's the last one to avoid overhead
                        if batches % 10 == 0 or batches == total_batches:
                            event_queue.put(json.dumps({
                                "type": "progress", 
                                "epoch": epoch + 1, 
                                "batch": batches, 
                                "loss": loss,
                                "avg_epoch_loss": epoch_loss / batches,
                                "total_batches": total_batches
                            }) + "\n")
                
                avg_train_loss = epoch_loss / batches if batches > 0 else 0
                
                # Validation phase
                val_loss = 0
                val_batches = 0
                for i in range(0, len(val_rows), batch_size):
                    batch = val_rows[i:i+batch_size]
                    states = []
                    targets = []
                    
                    for fen, stockfish_eval in batch:
                        try:
                            board = chess.Board(fen)
                            # Use same clamping and scaling as training
                            clamped_eval = max(-2000, min(2000, stockfish_eval))
                            target = math.tanh(clamped_eval / 800.0)
                            if board.turn == chess.BLACK:
                                target = -target
                            states.append(board)
                            targets.append(target)
                        except Exception:
                            continue
                    
                    if states:
                        # Evaluate without training
                        self.learner.model.eval()
                        with torch.no_grad():
                            tensors = [board_to_tensor(s).squeeze(0) for s in states]
                            batch_tensor = torch.stack(tensors).to(self.learner.device)
                            target_tensor = torch.tensor(targets, dtype=torch.float32).unsqueeze(1).to(self.learner.device)
                            outputs = self.learner.model(batch_tensor)
                            batch_val_loss = self.learner.criterion(outputs, target_tensor)
                            val_loss += batch_val_loss.item()
                            val_batches += 1
                
                avg_val_loss = val_loss / val_batches if val_batches > 0 else 0
                
                # Check if model weights actually changed
                current_weight_sum = sum(p.sum().item() for p in self.learner.model.parameters())
                weight_changed = "N/A"
                if first_weight_sum is not None:
                    weight_changed = f"Δ{abs(current_weight_sum - first_weight_sum):.2e}"
                first_weight_sum = current_weight_sum
                
                # Save best model based on validation loss
                improvement = ""
                if avg_val_loss < best_val_loss:
                    best_val_loss = avg_val_loss
                    self.learner.save()
                    improvement = "✓ Best model saved"
                else:
                    improvement = "No improvement"
                
                print(f"Epoch {epoch+1}/{epochs} - Train: {avg_train_loss:.6f}, Val: {avg_val_loss:.6f} {improvement} (Weights: {weight_changed})")
                
                event_queue.put(json.dumps({
                    "type": "epoch_end",
                    "epoch": epoch + 1,
                    "train_loss": avg_train_loss,
                    "val_loss": avg_val_loss,
                    "improvement": improvement,
                    "weight_change": weight_changed
                }) + "\n")
            
            duration = time.time() - start_time
            event_queue.put(json.dumps({
                "type": "complete", 
                "message": f"Successfully learned from {total_moves} positions using Stockfish ground truth.",
                "duration": f"{duration:.1f}s",
                "best_val_loss": best_val_loss,
                "final_train_loss": avg_train_loss if 'avg_train_loss' in locals() else 0
            }) + "\n")
            event_queue.put(None)
            
        except Exception as e:
            import traceback
            traceback.print_exc()
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
