# Chess Engine Analysis Report

Generated: January 18, 2026

---

## Table of Contents
1. [Engine Blindspots](#engine-blindspots)
   - [Evaluation Blindspots](#evaluation-blindspots)
   - [Search Blindspots](#search-blindspots)
2. [Bot Parameters Reference](#bot-parameters-reference)
3. [Visualization Improvements](#visualization-improvements)
4. [Training Method Suggestions](#training-method-suggestions)

---

## Engine Blindspots

### Evaluation Blindspots

| # | Blindspot | Description | Severity | File Location |
|---|-----------|-------------|----------|---------------|
| 1 | **No Pawn Structure Analysis** | No detection of isolated pawns, doubled pawns, or backward pawns. These are critical positional weaknesses that engines must penalize. | 🔴 HIGH | `evaluation.py` |
| 2 | **No Bishop Pair Bonus** | Having both bishops (~+50 cp) is a significant advantage, especially in open positions. Not evaluated. | 🟡 MEDIUM | `evaluation.py` |
| 3 | **Weak King Safety** | Only checks squares around king for enemy pieces. No pawn shield evaluation, no attack count weighting, no storm detection. | 🔴 HIGH | `evaluation.py` |
| 4 | **No Outpost Detection** | Knights on outposts (protected by pawns, can't be attacked by enemy pawns) are extremely strong. Not rewarded. | 🟡 MEDIUM | `evaluation.py` |
| 5 | **No Space Advantage** | Engine doesn't evaluate territorial control/space advantage. | 🟢 LOW | `evaluation.py` |
| 6 | **Simplistic Mobility** | Counts all attacks equally. Should weight central squares higher, penalize pieces with few moves more. | 🟡 MEDIUM | `evaluation.py` |
| 7 | **No Blocked Pieces** | Doesn't detect pieces that are completely blocked/trapped (e.g., bad bishop behind own pawns). | 🟡 MEDIUM | `evaluation.py` |
| 8 | **No Connected Rooks** | Rooks on 7th rank and connected rooks are very powerful. Not evaluated. | 🟡 MEDIUM | `evaluation.py` |
| 9 | **No Weak Square Detection** | Holes in pawn structure (squares that can never be defended by pawns) not detected. | 🟡 MEDIUM | `evaluation.py` |
| 10 | **Neural Blend is Fixed** | Hard-coded blend ratio may be suboptimal. Now configurable via EVAL_PARAMS. | 🟢 LOW | `evaluation.py` |

### Search Blindspots

| # | Blindspot | Description | Severity | File Location |
|---|-----------|-------------|----------|---------------|
| 1 | **Null Move Pruning Disabled** | Comment shows NMP is disabled. This is a major speed optimization that's missing. | 🔴 HIGH | `search.py` |
| 2 | **No Late Move Reductions (LMR)** | Moves ordered late in the list should be searched at reduced depth. Major pruning technique missing. | 🔴 HIGH | `search.py` |
| 3 | **No Principal Variation Search (PVS)** | Uses standard alpha-beta instead of zero-window searches for non-PV moves. | 🟡 MEDIUM | `search.py` |
| 4 | **No Aspiration Windows** | Not using aspiration windows for iterative deepening. | 🟡 MEDIUM | `search.py` |
| 5 | **No History Heuristic** | Only uses killer moves. History tables would improve move ordering significantly. | 🟡 MEDIUM | `search.py` |
| 6 | **Quiescence Depth Limited** | `max_depth=3` may cause tactical blindness in complex positions. Now configurable. | 🟡 MEDIUM | `search.py` |
| 7 | **No Static Exchange Evaluation (SEE)** | Uses simplistic "attacker > victim" check. Real SEE would be more accurate. | 🟡 MEDIUM | `search.py` |
| 8 | **Check Extension Always +1** | No limit on check extensions can cause search explosion in some positions. | 🟡 MEDIUM | `search.py` |
| 9 | **TT Replacement Strategy** | Simple size-based replacement. Age/depth-based replacement is better. | 🟢 LOW | `search.py` |
| 10 | **No Futility Pruning** | At low depths, positions far below beta could be pruned. | 🟡 MEDIUM | `search.py` |
| 11 | **No Singular Extensions** | If one move is much better than others, it should be extended. | 🟢 LOW | `search.py` |

---

## Bot Parameters Reference

All parameters are now configurable at runtime via the Parameters screen in the app.

### Material Values (centipawns)

| Piece | Default | Range | Description |
|-------|---------|-------|-------------|
| Pawn | 100 cp | 50-200 | Base unit of value |
| Knight | 320 cp | 200-400 | ~3.2 pawns |
| Bishop | 330 cp | 200-400 | Slightly > Knight |
| Rook | 500 cp | 400-600 | ~5 pawns |
| Queen | 900 cp | 800-1200 | ~9 pawns |

### Positional Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| Mobility Weight | 5 cp/sq | 0-20 | Bonus per attacked square (non-pawn pieces) |
| Castling Bonus | 50 cp | 0-100 | Reward for having castled |
| King Exposure Penalty | 25 cp | 0-50 | Penalty per attacked square near king |
| King Safety Penalty | 30 cp | 0-60 | Penalty for enemy piece adjacent to king |
| Rook Open File | 15 cp | 0-40 | Bonus for rook on open file |
| Rook Semi-Open | 10 cp | 0-30 | Bonus for rook on semi-open file |
| Passed Pawn Scale | 10 cp/rank | 0-30 | Per-rank passed pawn bonus |
| Threat Divisor | 5 | 1-10 | Piece value ÷ this = threat penalty (higher = less penalty) |
| LPDO Divisor | 2 | 1-5 | Loose Piece Detection ratio (higher = less penalty) |

### Neural Network

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| Neural Blend | 50% | 0-100% | 0% = pure heuristic (classical), 100% = pure neural (AlphaZero-style) |

### Search Engine

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| Quiescence Depth | 3 ply | 1-8 | How deep to search captures after main search ends |

### Training

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| Learning Rate | 0.0002 | 0.00001-0.01 | Adam optimizer learning rate (lower = more stable, slower) |

---

## Visualization Improvements

The following visualization enhancements were added to the Arena screen:

### Live Game Metrics Card
- **Current Evaluation** - Shows eval in pawns with +/- indicator and color coding
- **Game Phase Badge** - Displays "Opening", "Middlegame", or "Endgame"
- **Move Quality Indicator** - Shows "blunder", "mistake", "good", or "excellent" for last move
- **Average Move Time** - Tracks bot's average thinking time in milliseconds
- **Mini Eval Chart** - Real-time graph of evaluation over the game

### Win Rate Display
- Live win rate percentage calculation during battle
- Color-coded based on performance (green if winning, red if losing)

### Enhanced Training Metrics
- **Best/Average Loss** - Shows best and average loss values during training
- **Training Trend Indicator** - Shows "↓ Improving", "→ Stable", or "↑ Overfitting"
- **Loss Trend Chart** - Visual graph of loss over training batches

---

## Training Method Suggestions

### Backend Data Enhancements
To fully utilize the new visualization, the backend should send additional fields in move events:

```python
# In battle stream, add to move events:
{
    "type": "move",
    "eval": current_eval,           # centipawns (int)
    "move_quality": "good",         # "blunder" | "mistake" | "good" | "excellent"
    "phase": "Middlegame",          # "Opening" | "Middlegame" | "Endgame"
    "time_ms": 150                  # move computation time (int)
}
```

### Recommended Training Improvements

| Technique | Description | Priority |
|-----------|-------------|----------|
| **Curriculum Learning** | Start with simpler positions (fewer pieces), gradually increase complexity | 🔴 HIGH |
| **Loss Weighting** | Weight critical positions (blunders, missed wins) higher in loss function | 🔴 HIGH |
| **Data Augmentation** | Mirror board positions horizontally to double training data | 🟡 MEDIUM |
| **Temporal Difference** | Use eval differences between positions, not just game outcomes | 🟡 MEDIUM |
| **Early Stopping** | Stop training when overfitting is detected (loss starts increasing) | 🟡 MEDIUM |
| **Separate Heads** | Train move prediction (policy) and position evaluation (value) separately | 🟢 LOW |

### Parameter Tuning Workflow

1. **Baseline**: Run 50+ games with default parameters, record win rate
2. **Hypothesis**: Identify weakness (e.g., "bot loses in endgame")
3. **Adjust**: Modify relevant parameter (e.g., increase passed pawn scale)
4. **Test**: Run another 50+ games with new parameters
5. **Compare**: If win rate improved, keep change; otherwise revert
6. **Iterate**: Repeat for other parameters

---

## Algorithm Design & Conditions

This section describes each algorithm implemented in the engine, the specific conditions under which it operates, and the reasoning behind the design.

### Evaluation Algorithms

#### 1. **Tapered Evaluation** (`evaluate_position`)
**Conditions:**
- Calculates game phase based on material: `mg_phase = Σ(pieces × phase_values)`
- Phase values: Knight=1, Bishop=1, Rook=2, Queen=4
- MAX_PHASE = 24 per side (48 total for starting position)

**Algorithm:**
```
current_max_phase = 48
eg_phase = current_max_phase - mg_phase
final_score = (mg_score × mg_phase + eg_score × eg_phase) / current_max_phase
```

**Reasoning:** Chess strategy differs drastically between opening/middlegame (material, king safety) and endgame (king activity, passed pawns). Tapered evaluation smoothly transitions between two separate evaluation functions, giving more accurate assessments throughout the game.

---

#### 2. **Loose Piece Detection (LPDO)** (`loose_piece_penalty`)
**Conditions:**
- Piece has NO attackers → 0 penalty
- Piece has attackers but NO defenders → full piece value penalty
- Piece has defenders AND attackers:
  - If `min(attacker_values) < min(defender_values)` → piece value ÷ lpdo_divisor penalty
  - Otherwise → 0 penalty

**Algorithm:**
```python
if not attackers: return 0
if not defenders: return piece_value

min_attacker = min([value for attacker in attackers])
min_defender = min([value for defender in defenders])

if min_attacker < min_defender:
    return piece_value / lpdo_divisor  # Default divisor = 2
return 0
```

**Reasoning:** A piece is "loose" when it can be captured by a lower-value piece. For example, a queen (900cp) attacked by a knight (320cp) and defended by a rook (500cp) is loose because the knight can safely capture it. This detects hanging pieces and tactical vulnerabilities.

---

#### 3. **Threat Detection** (`threat_penalty`)
**Conditions:**
- For each piece of the side being evaluated
- Check if opponent attacks that piece

**Algorithm:**
```python
penalty = 0
for each piece of our_color:
    if enemy_attacks(piece.square):
        penalty += piece_value / threat_divisor  # Default divisor = 5
return penalty
```

**Reasoning:** Pieces under attack are tactically weak even if defended. The threat divisor (default 5) scales the penalty - a threatened queen (900cp) causes 180cp penalty. This encourages the bot to remove threats and avoid leaving pieces under attack.

---

#### 4. **Pin Detection** (`detect_pins`)
**Conditions:**
- Piece is NOT the king itself
- Piece is on same file, rank, or diagonal as friendly king
- When piece is temporarily removed, enemy attacks king (x-ray attack through the piece)

**Algorithm:**
```python
for each friendly_piece (excluding king):
    king_file, king_rank = king_position
    piece_file, piece_rank = piece_position
    
    on_same_line = (king_file == piece_file) OR 
                   (king_rank == piece_rank) OR 
                   (abs(king_file - piece_file) == abs(king_rank - piece_rank))
    
    if on_same_line:
        test_board = board.copy()
        test_board.remove(piece)
        if test_board.is_attacked_by(enemy, king_square):
            penalty += (piece_value × pin_penalty%) / 100  # Default 50%
```

**Reasoning:** Pinned pieces cannot move without exposing the king to check, making them temporarily immobile and tactically weak. A pinned rook (500cp) with 50% pin penalty causes 250cp penalty. This algorithm uses x-ray detection - if removing the piece exposes the king to attack, it's pinned.

---

#### 5. **Queen Safety** (`queen_safety_penalty`)
**Conditions:**
- **Exposure Penalty:** Queen is attacked by opponent → penalty
- **Early Development Penalty:** Queen has moved from starting rank AND fewer than 2 minor pieces (N/B) have been developed → penalty

**Algorithm:**
```python
penalty = 0
for each friendly_queen:
    # Exposure check
    if enemy_attacks(queen_square):
        penalty += queen_exposure_penalty  # Default 40cp
    
    # Early development check (opening phase)
    starting_rank = 0 (white) or 7 (black)
    if queen_rank != starting_rank:
        developed_minor_pieces = count(N/B not on starting rank)
        if developed_minor_pieces < 2:
            penalty += queen_early_penalty  # Default 20cp
return penalty
```

**Reasoning:** Queens are powerful but vulnerable. Developing the queen too early (before knights/bishops) often leads to it being chased around by opponent's pieces, losing time. The exposure penalty discourages leaving the queen under attack. Total max penalty: 60cp for exposed queen moved too early.

---

#### 6. **King Safety** (`tuned_king_exposure`)
**Conditions:**
- Applies only in middlegame (MG phase)
- Checks all 8 squares adjacent to the king
- Counts how many are attacked by opponent

**Algorithm:**
```python
penalty = 0
king_square = board.king(color)
for each square in king_attack_squares (8 squares):
    if enemy_attacks(square):
        penalty += king_exposure_penalty  # Default 25cp per square
return penalty
```

**Reasoning:** A king surrounded by enemy-controlled squares is vulnerable to checkmate threats. Each attacked square near the king increases danger. This is MG-only because in endgames, active kings are beneficial. Maximum penalty: 8 × 25cp = 200cp for completely surrounded king.

---

#### 7. **Castling Bonus**
**Conditions:**
- Player has castled (detected by checking castling rights are lost AND king is on G1/C1 for white or G8/C8 for black)

**Algorithm:**
```python
if not board.has_castling_rights(color) and king in [G1, C1] (or G8, C8):
    score += castling_bonus  # Default 50cp
```

**Reasoning:** Castling achieves two goals: (1) moves king to safety, (2) activates rook. This bonus encourages the bot to castle early in the game. Applied in MG only since castling typically happens in opening/early middlegame.

---

#### 8. **Rook on Open/Semi-Open File**
**Conditions:**
- Piece is a rook
- **Open File:** No pawns of either color on the file → bonus
- **Semi-Open File:** No friendly pawns but opponent has pawns on the file → smaller bonus

**Algorithm:**
```python
if piece == ROOK:
    file_bitboard = all_squares_on_file(rook_file)
    has_friendly_pawns = file_bitboard & our_pawns
    has_enemy_pawns = file_bitboard & enemy_pawns
    
    if not has_friendly_pawns:
        if not has_enemy_pawns:
            score += rook_open_file  # Default 15cp (open file)
        else:
            score += rook_semi_open  # Default 10cp (semi-open)
```

**Reasoning:** Rooks are most effective on files without friendly pawns blocking them. Open files (no pawns) allow rooks to penetrate the 7th/8th rank. Semi-open files (only enemy pawns) allow pressure on enemy pawns. This encourages proper rook placement.

---

#### 9. **Passed Pawn Bonus**
**Conditions:**
- Piece is a pawn
- No enemy pawns exist on:
  - Same file as the pawn
  - Adjacent files (left and right)
  - In front of the pawn (closer to promotion)

**Algorithm:**
```python
if piece == PAWN:
    rank = pawn_rank (0-7)
    file = pawn_file (0-7)
    passed = True
    
    for check_file in [file-1, file, file+1]:
        enemy_pawns_on_file = board.pawns(enemy) & file_bitboard(check_file)
        for enemy_pawn in enemy_pawns_on_file:
            if enemy_pawn_rank > our_pawn_rank (white) or < (black):
                passed = False
    
    if passed:
        # White: rank 1-6 (promotion at rank 7)
        bonus = (rank - 1) × passed_pawn_scale  # Default 10cp/rank
        mg_score += bonus
        eg_score += bonus × 2  # Double bonus in endgame
```

**Reasoning:** Passed pawns (no enemy pawn can stop them) are extremely powerful, especially in endgames. Bonus scales with rank - a pawn on 7th rank is more valuable than one on 3rd. Endgame bonus is doubled because passed pawns often decide endgame outcomes.

---

#### 10. **Mobility Scoring**
**Conditions:**
- Applies to all non-pawn pieces (N, B, R, Q)
- Counts number of squares each piece attacks

**Algorithm:**
```python
white_mobility = Σ(squares_attacked for each white non-pawn piece)
black_mobility = Σ(squares_attacked for each black non-pawn piece)
score += (white_mobility - black_mobility) × mobility_weight  # Default 5cp/square
```

**Reasoning:** Pieces with more movement options are more effective. A knight in the center controls 8 squares, but one in a corner controls only 2. This rewards centralization and piece coordination while penalizing cramped positions.

---

#### 11. **Neural Network Blending**
**Conditions:**
- If neural model is provided
- Blend ratio controlled by `neural_blend` parameter (0-100%)

**Algorithm:**
```python
heuristic_score = tapered_eval(board)
neural_score = model.get_value(board) × 1000  # Scale [-1, 1] to centipawns

blend_ratio = neural_blend / 100.0  # Default 50% = 0.5
final_score = heuristic_score × (1 - blend_ratio) + neural_score × blend_ratio
```

**Reasoning:** Combines classical chess knowledge (heuristics) with learned patterns (neural network). At 0%, engine is pure classical (like Stockfish). At 100%, pure neural (like AlphaZero). 50% default balances both approaches - heuristics provide tactical accuracy while neural provides strategic understanding.

---

### Search Algorithms

#### 1. **Alpha-Beta Pruning with Negamax** (`alphabeta`)
**Conditions:**
- At each node, tries to maximize score for current player
- Uses fail-soft alpha-beta bounds
- Prunes branches when `alpha >= beta`

**Algorithm:**
```python
def alphabeta(depth, alpha, beta):
    if depth == 0 or game_over:
        return evaluate()
    
    value = -infinity
    for each move:
        make_move()
        score = -alphabeta(depth-1, -beta, -alpha)  # Negamax recursion
        undo_move()
        
        value = max(value, score)
        alpha = max(alpha, score)
        
        if alpha >= beta:  # Beta cutoff
            break  # Prune remaining moves
    
    return value
```

**Reasoning:** Alpha-beta eliminates branches that cannot affect the final decision. If we've found a move that guarantees score >= beta, opponent will avoid this position, so remaining moves don't need searching. This reduces search tree from O(b^d) to O(b^(d/2)) in best case.

---

#### 2. **Transposition Table (TT)**
**Conditions:**
- Before searching a position, check if already searched
- Store exact scores, lower bounds (alpha), upper bounds (beta)
- Only use TT entry if `stored_depth >= current_depth`

**Algorithm:**
```python
# Lookup
hash = zobrist_hash(board)
if hash in TT:
    stored_depth, score, type, best_move = TT[hash]
    if stored_depth >= depth:
        if type == 'exact': return score
        if type == 'lower': alpha = max(alpha, score)
        if type == 'upper': beta = min(beta, score)

# Store
if value <= alpha_orig:
    type = 'upper'  # All moves failed low
elif value >= beta:
    type = 'lower'  # Beta cutoff
else:
    type = 'exact'  # Exact value
TT[hash] = (depth, value, type, best_move)
```

**Reasoning:** Chess positions often repeat due to transpositions (same position reached via different move orders). TT caches evaluated positions to avoid re-searching them. Zobrist hashing provides fast, collision-resistant position identification. This can speed up search by 10-50x.

---

#### 3. **Iterative Deepening (ID)**
**Conditions:**
- Search depth 1, then 2, then 3, etc. until time limit
- Each iteration uses TT from previous iteration

**Algorithm:**
```python
for depth in [1, 2, 3, 4, ...]:
    score = alphabeta(depth, -infinity, +infinity)
    best_move = get_best_from_TT()
    if time_expired():
        return best_move_from_previous_depth
```

**Reasoning:** Seems wasteful to search shallow depths first, but TT and move ordering make deeper searches much faster. Searching depth 5 after searching depths 1-4 is faster than directly searching depth 5. Also allows time management - always have a best move available when time expires.

---

#### 4. **Quiescence Search** (`quiescence`)
**Conditions:**
- Called when main search reaches depth 0
- Only searches captures, checks, and promotions
- Limited to `quiescence_depth` plies (default 3)

**Algorithm:**
```python
def quiescence(alpha, beta, depth):
    stand_pat = evaluate()
    if stand_pat >= beta: return beta
    alpha = max(alpha, stand_pat)
    
    if depth == 0: return stand_pat
    
    for each capture/check/promotion:
        make_move()
        score = -quiescence(-beta, -alpha, depth-1)
        undo_move()
        
        alpha = max(alpha, score)
        if alpha >= beta: break
    
    return alpha
```

**Reasoning:** Stopping search at depth 0 causes horizon effect - bot can't see captures 1 ply away. Example: trading queen for rook looks good (+400) until opponent recaptures (+400-900=-500). Quiescence search continues until position is "quiet" (no immediate tactical threats), ensuring accurate evaluation.

---

#### 5. **Move Ordering**
**Conditions:**
- Prioritize moves likely to cause beta cutoffs
- Order: TT move > Good captures > Killer moves > Checks > Other moves

**Algorithm:**
```python
def move_score(move):
    if move == tt_move: return 3000  # Highest priority
    
    if is_capture:
        victim = captured_piece_value
        attacker = attacking_piece_value
        if victim < attacker: return 500  # Bad capture (QxP losing Q)
        return 2000 + MVV_LVA[victim][attacker]  # MVV-LVA ordering
    
    if move == killer[0]: return 1000  # Killer move 1
    if move == killer[1]: return 900   # Killer move 2
    if gives_check: return 800
    return 0  # Quiet moves
```

**Reasoning:** Better move ordering → more beta cutoffs → smaller search tree. Best moves are searched first, causing more pruning. MVV-LVA (Most Valuable Victim - Least Valuable Attacker) prioritizes QxP over PxP. Killer moves (non-captures that caused cutoffs) are likely good in similar positions.

---

#### 6. **Killer Move Heuristic**
**Conditions:**
- When a non-capture move causes beta cutoff
- Store 2 killer moves per ply (depth level)

**Algorithm:**
```python
# When beta cutoff occurs
if not is_capture and alpha >= beta:
    if move != killers[ply][0]:
        killers[ply][1] = killers[ply][0]  # Shift previous killer
        killers[ply][0] = move  # Store new killer

# In move ordering
if move == killers[ply][0]: score = 1000
if move == killers[ply][1]: score = 900
```

**Reasoning:** If a quiet move (non-capture) causes cutoff in one position, it's likely good in sibling positions at same ply. Example: if Nf3 refutes one line, it might refute similar lines in the search tree. Storing 2 killers per ply catches multiple strong moves.

---

#### 7. **Check Extension**
**Conditions:**
- If a move gives check to opponent king
- Extend search by 1 ply

**Algorithm:**
```python
for each move:
    make_move()
    new_depth = depth - 1
    
    if board.is_check():
        new_depth += 1  # Search one ply deeper
    
    score = -alphabeta(new_depth, -beta, -alpha)
    undo_move()
```

**Reasoning:** Checks are forcing moves (opponent must respond), so they're more important to analyze deeply. This prevents horizon effect in tactical sequences. Warning: Can cause search explosion if not limited (currently unlimited, which is a blindspot).

---

#### 8. **Mate Score Adjustment**
**Conditions:**
- When storing/retrieving mate scores from TT
- Adjust score based on distance from root

**Algorithm:**
```python
MATE_VALUE = 20000

# When retrieving from TT
if score > MATE_VALUE/2:
    score -= ply  # Mate is further away from root
elif score < -MATE_VALUE/2:
    score += ply  # Getting mated is closer to root

# When storing to TT
if score > MATE_VALUE/2:
    score += ply  # Store absolute distance to mate
elif score < -MATE_VALUE/2:
    score -= ply
```

**Reasoning:** Mate scores must be relative to current ply, not root. If we find mate-in-3 at ply 5, it's mate-in-8 from root. Without adjustment, TT would return incorrect mate distances. Closer mates are more valuable (prefer mate-in-2 over mate-in-5).

---

## Files Modified

| File | Changes |
|------|---------|
| `backend/evaluation.py` | Added `EVAL_PARAMS` dictionary, refactored all hardcoded values to use parameters |
| `backend/search.py` | Added configurable `quiescence_depth` attribute |
| `backend/main.py` | Added `/parameters` GET/POST endpoints for runtime parameter tuning |
| `frontend/lib/parameters_screen.dart` | New screen with sliders for all bot parameters |
| `frontend/lib/arena_screen.dart` | Added live game metrics, eval chart, win rate display |
| `frontend/lib/training_provider.dart` | Enhanced training state with trend analysis |
| `frontend/lib/main.dart` | Added navigation to Parameters screen |

---

*Report generated by GitHub Copilot*
