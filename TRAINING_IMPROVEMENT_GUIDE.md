# Training Improvement Guide

## Current Issues with "Learn from History"

### Problem 1: Final Result Labeling
**Current:** Every position in a won game gets +1.0, every position in a lost game gets -1.0

**Why it's bad:** 
- Blunders in won games get labeled as "good positions"
- Brilliant moves in lost games get labeled as "bad positions"  
- Model learns "these positions happened in wins" not "these positions are objectively strong"

**Example:**
```
Game: Bot (White) vs Stockfish (1500 Elo) → White wins

Move 1: e4 (opening, eval +0.3) → labeled +1.0 ✗
Move 15: Qxg7+ (brilliant sacrifice, eval +3.5) → labeled +1.0 ✗
Move 20: Ra1?? (blunder, eval -2.1) → labeled +1.0 ✗ WRONG!
Move 30: Checkmate → labeled +1.0 ✓

All positions get same label despite vastly different quality!
```

### Problem 2: No Position Quality Assessment
The model needs to learn what makes a position strong, not just which positions occurred in victories.

### Problem 3: Overfitting
- Training on same positions 5 times (epochs) without validation
- No train/validation split to detect overfitting
- Loss decreases but model performance doesn't improve on new positions

---

## Solution: Better Training Methods

### Method 1: Temporal Difference (TD) Learning
Train on differences between consecutive positions:

```python
# Instead of: position → final_result
# Use: position → next_position_value

# Example:
# Position A (eval +0.5) → after move → Position B (eval +1.2)
# Train: eval(A) should be close to eval(B)

for i in range(len(game_history) - 1):
    current_board, current_turn = game_history[i]
    next_board, next_turn = game_history[i + 1]
    
    # Target: current position value = next position value (from opponent perspective)
    next_value = learner.get_value(next_board)
    target = -next_value  # Flip sign because turns alternate
    
    states.append(current_board)
    targets.append(target)

# Last position uses actual game result
states.append(game_history[-1][0])
targets.append(game_result)
```

**Benefits:**
- Positions are evaluated relative to their consequences
- Blunders are penalized (position value drops)
- Good moves are rewarded (position value improves)
- More accurate than final result labeling

---

### Method 2: Stockfish Evaluation as Ground Truth
Use Stockfish's evaluation as the training target:

```python
import chess.engine

# During battle, store Stockfish's eval for each position
engine = chess.engine.SimpleEngine.popen_uci("stockfish.exe")

for position in game_history:
    info = engine.analyse(position, chess.engine.Limit(depth=15))
    stockfish_eval_cp = info["score"].relative.score()  # Centipawns
    
    # Convert to [-1, 1] range for neural network
    # Sigmoid-like scaling: eval=+300cp → 0.76, eval=-300cp → -0.76
    target = math.tanh(stockfish_eval_cp / 400.0)
    
    states.append(position)
    targets.append(target)

engine.quit()
```

**Benefits:**
- Ground truth comes from strong engine (2500+ Elo)
- Every position gets accurate evaluation
- No win/loss/draw noise
- Model learns to approximate Stockfish

**Drawbacks:**
- Slow (Stockfish analysis takes time)
- Requires Stockfish to be running

---

### Method 3: Position Quality Filtering
Only train on "decisive" positions where the result is clear:

```python
# During data collection, tag positions by material advantage
for board_state, turn in game_history:
    material = get_material_score(board_state)
    
    # Only train on positions with clear material advantage
    if result == "1-0" and material > 3:  # White won and was ahead
        states.append(board_state)
        targets.append(1.0 if turn == chess.WHITE else -1.0)
    elif result == "0-1" and material < -3:  # Black won and was ahead
        states.append(board_state)
        targets.append(1.0 if turn == chess.BLACK else -1.0)
    # Skip positions with unclear advantage
```

**Benefits:**
- Reduces label noise
- Focuses on positions where outcome is clear
- Prevents contradictory training signals

---

### Method 4: Validation Split + Early Stopping
Split data into train/validation to detect overfitting:

```python
from sklearn.model_selection import train_test_split

# Split 80% train, 20% validation
train_data, val_data = train_test_split(rows, test_size=0.2, random_state=42)

best_val_loss = float('inf')
patience = 3
no_improve_count = 0

for epoch in range(epochs):
    # Train on training data
    train_loss = train_epoch(train_data)
    
    # Evaluate on validation data
    val_loss = evaluate(val_data)
    
    if val_loss < best_val_loss:
        best_val_loss = val_loss
        learner.save()  # Save best model
        no_improve_count = 0
    else:
        no_improve_count += 1
    
    # Early stopping if no improvement
    if no_improve_count >= patience:
        print(f"Early stopping at epoch {epoch}")
        break
```

**Benefits:**
- Detects when model starts overfitting
- Prevents wasted training time
- Saves best model (lowest validation loss)

---

### Method 5: Data Augmentation
Double training data by mirroring positions:

```python
def mirror_board(board):
    """Flip board horizontally (a-file ↔ h-file)"""
    mirrored = chess.Board(None)  # Empty board
    for square, piece in board.piece_map().items():
        rank = chess.square_rank(square)
        file = chess.square_file(square)
        mirrored_file = 7 - file
        mirrored_square = chess.square(mirrored_file, rank)
        mirrored.set_piece_at(mirrored_square, piece)
    return mirrored

# During training:
for fen, result in batch:
    board = chess.Board(fen)
    states.append(board)
    targets.append(result)
    
    # Add mirrored version
    states.append(mirror_board(board))
    targets.append(result)  # Same result, mirrored position
```

**Benefits:**
- 2x more training data
- Helps model learn symmetry
- Reduces overfitting

---

## Recommended Implementation Priority

1. **HIGH PRIORITY:** Implement TD Learning (Method 1)
   - Biggest improvement for effort
   - Fixes fundamental labeling problem
   - No external dependencies

2. **HIGH PRIORITY:** Add Train/Val Split + Early Stopping (Method 4)
   - Prevents overfitting
   - Easy to implement
   - Essential for measuring real improvement

3. **MEDIUM PRIORITY:** Stockfish Ground Truth (Method 2)
   - Very accurate labels
   - Slow but worth it for small datasets
   - Use depth=10-15 for speed/accuracy balance

4. **MEDIUM PRIORITY:** Position Quality Filtering (Method 3)
   - Quick win for reducing noise
   - Complement to other methods

5. **LOW PRIORITY:** Data Augmentation (Method 5)
   - Nice-to-have
   - Helps when data is scarce
   - Easy to add later

---

## Current Training Flow (What's Actually Happening)

```
1. Battle vs Stockfish
   ├─ Store: (FEN, result) for every position
   └─ result = +1 if turn_player wins, -1 if loses, 0 if draw

2. Learn from History (CURRENT IMPLEMENTATION)
   ├─ Load all (FEN, result) pairs
   ├─ For each epoch:
   │  ├─ Shuffle data
   │  ├─ For each batch:
   │  │  ├─ states = [Board(fen) for fen in batch]
   │  │  ├─ targets = [result for result in batch]
   │  │  └─ loss = train_batch(states, targets)
   │  └─ Save model
   └─ Problem: targets are noisy final results, not position quality!

3. Why loss doesn't improve:
   ├─ Model tries to learn: "Position X → +1.0"
   ├─ But Position X might be a blunder that happened in a won game
   ├─ Model can't generalize because labels are contradictory
   └─ Loss plateaus or even increases on new positions
```

---

## Example Code: Implementing TD Learning

Add this to `bot.py`:

```python
def _history_training_worker_TD(self, event_queue, batch_size=32, epochs=5):
    """
    Improved training using Temporal Difference learning
    """
    import json
    import time
    
    try:
        conn = sqlite3.connect("training_data.db")
        cursor = conn.cursor()
        
        # Fetch moves grouped by game_id to reconstruct game sequences
        cursor.execute("""
            SELECT game_id, fen, result, step 
            FROM training_moves 
            WHERE opponent = 'Stockfish'
            ORDER BY game_id, step
        """)
        rows = cursor.fetchall()
        conn.close()
        
        if not rows:
            event_queue.put(json.dumps({"type": "error", "message": "No battle history found."}) + "\n")
            event_queue.put(None)
            return
        
        # Group positions by game
        games = {}
        for game_id, fen, result, step in rows:
            if game_id not in games:
                games[game_id] = []
            games[game_id].append((fen, result, step))
        
        # Sort each game by step
        for game_id in games:
            games[game_id].sort(key=lambda x: x[2])  # Sort by step
        
        # Build training pairs using TD learning
        training_pairs = []
        
        for game_id, positions in games.items():
            for i in range(len(positions) - 1):
                current_fen, _, _ = positions[i]
                next_fen, next_result, _ = positions[i + 1]
                
                try:
                    current_board = chess.Board(current_fen)
                    next_board = chess.Board(next_fen)
                    
                    # TD target: value of next position (from opponent's perspective)
                    next_value = self.learner.get_value(next_board)
                    target = -next_value  # Flip sign for alternating turns
                    
                    training_pairs.append((current_board, target))
                except:
                    continue
            
            # Last position uses actual game result
            final_fen, final_result, _ = positions[-1]
            try:
                final_board = chess.Board(final_fen)
                training_pairs.append((final_board, float(final_result)))
            except:
                continue
        
        # Train/Val split
        random.shuffle(training_pairs)
        split_idx = int(len(training_pairs) * 0.8)
        train_pairs = training_pairs[:split_idx]
        val_pairs = training_pairs[split_idx:]
        
        event_queue.put(json.dumps({
            "type": "start",
            "total_positions": len(training_pairs),
            "train_size": len(train_pairs),
            "val_size": len(val_pairs),
            "epochs": epochs
        }) + "\n")
        
        best_val_loss = float('inf')
        
        for epoch in range(epochs):
            random.shuffle(train_pairs)
            epoch_train_loss = 0
            batches = 0
            
            # Training phase
            for i in range(0, len(train_pairs), batch_size):
                batch = train_pairs[i:i+batch_size]
                states = [state for state, _ in batch]
                targets = [target for _, target in batch]
                
                loss = self.learner.train_batch(states, targets)
                epoch_train_loss += loss
                batches += 1
            
            avg_train_loss = epoch_train_loss / batches if batches > 0 else 0
            
            # Validation phase
            val_loss = 0
            val_batches = 0
            for i in range(0, len(val_pairs), batch_size):
                batch = val_pairs[i:i+batch_size]
                states = [state for state, _ in batch]
                targets = [target for _, target in batch]
                
                # Evaluate without training
                self.model.eval()
                with torch.no_grad():
                    tensors = [board_to_tensor(s).squeeze(0) for s in states]
                    batch_tensor = torch.stack(tensors).to(self.device)
                    target_tensor = torch.tensor(targets, dtype=torch.float32).unsqueeze(1).to(self.device)
                    outputs = self.model(batch_tensor)
                    loss = self.criterion(outputs, target_tensor)
                    val_loss += loss.item()
                    val_batches += 1
            
            avg_val_loss = val_loss / val_batches if val_batches > 0 else 0
            
            # Save best model
            if avg_val_loss < best_val_loss:
                best_val_loss = avg_val_loss
                self.learner.save()
                improvement = "✓ Best model saved"
            else:
                improvement = "No improvement"
            
            event_queue.put(json.dumps({
                "type": "epoch_end",
                "epoch": epoch + 1,
                "train_loss": avg_train_loss,
                "val_loss": avg_val_loss,
                "improvement": improvement
            }) + "\n")
        
        self.learner.save()
        event_queue.put(json.dumps({
            "type": "complete",
            "message": f"TD Learning complete. Best val loss: {best_val_loss:.6f}"
        }) + "\n")
        event_queue.put(None)
        
    except Exception as e:
        import traceback
        traceback.print_exc()
        event_queue.put(json.dumps({"type": "error", "message": str(e)}) + "\n")
        event_queue.put(None)
```

---

## Next Steps

1. Run battles vs Stockfish to collect data (you likely already have this)
2. Implement TD Learning method above
3. Add validation split to monitor overfitting
4. Compare results:
   - Before: Loss decreases but bot doesn't improve
   - After: Loss decreases AND bot performance improves

The key insight: **Loss going down ≠ Model getting better**. You need proper labeling (TD learning) and validation monitoring to ensure real improvement.
