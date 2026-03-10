"""Test tactical vision - royal fork detection"""
import chess
from bot import ChessBot
from search import ChessSearch
from evaluation import evaluate_position

# Create a fresh bot to clear any cached values
bot = ChessBot()

# Debug the Nf7 fork position
board = chess.Board("r2qk2r/ppp2ppp/8/4N3/8/8/PPPPPPPP/R2QKB1R w KQkq - 0 1")
print(f"\n{board}\n")
print("Position: Knight on e5 can play Nxf7 - a royal fork!")
print("After Nxf7, the knight attacks Queen@d8 AND Rook@h8 (and King@e8)")

# CRITICAL: Let's compare evaluations of the positions after each move
print("\n" + "="*60)
print("LEAF NODE EVALUATION COMPARISON")
print("="*60)

# After 1. Nxf7 Kxf7 - the expected line  
board_nxf7 = board.copy()
board_nxf7.push_san("Nxf7")
board_nxf7.push_san("Kxf7")
eval_nxf7 = evaluate_position(board_nxf7, model=None, fast_mode=True)
print(f"\n1. Nxf7 Kxf7 (White to move):")
print(board_nxf7)
print(f"  Material: White captured pawn (+100 cp), Black captured nothing")
print(f"  Fast eval (White's view) = {eval_nxf7}")

# After 1. e4 (any Black move)
board_e4 = board.copy()
board_e4.push_san("e4")
board_e4.push_san("Rg8")  # random Black move
eval_e4 = evaluate_position(board_e4, model=None, fast_mode=True)
print(f"\n1. e4 Rg8 (White to move):")
print(board_e4)
print(f"  Material: No captures")
print(f"  Fast eval (White's view) = {eval_e4}")

print(f"\nNxf7 should be BETTER because White captured a pawn!")
print(f"But: Nxf7 eval = {eval_nxf7}, e4 eval = {eval_e4}")

if eval_nxf7 < eval_e4:
    print("BUG: e4 has higher eval than Nxf7 - check PST values!")
    
    # Debug: Check what the PST contributes
    from evaluation import MG_PST, PIECE_VALUES
    
    # Position after Nxf7 Kxf7
    print("\n--- PST BREAKDOWN after Nxf7 Kxf7 ---")
    for sq, piece in board_nxf7.piece_map().items():
        pt = piece.piece_type
        pst_val = MG_PST[pt][chess.square_mirror(sq) if piece.color == chess.WHITE else sq]
        mat_val = PIECE_VALUES[pt]
        sign = 1 if piece.color == chess.WHITE else -1
        print(f"  {chess.piece_name(pt)} @ {chess.square_name(sq)}: mat={sign*mat_val}, pst={sign*pst_val}")
    
    # Position after e4 Rg8
    print("\n--- PST BREAKDOWN after e4 Rg8 ---")
    for sq, piece in board_e4.piece_map().items():
        pt = piece.piece_type
        pst_val = MG_PST[pt][chess.square_mirror(sq) if piece.color == chess.WHITE else sq]
        mat_val = PIECE_VALUES[pt]
        sign = 1 if piece.color == chess.WHITE else -1
        print(f"  {chess.piece_name(pt)} @ {chess.square_name(sq)}: mat={sign*mat_val}, pst={sign*pst_val}")

# Let's trace what happens at each depth
print("\n" + "="*60)
print("SEARCH TRACE AT EACH DEPTH")
print("="*60)

for depth in range(1, 5):
    # Create fresh search engine for each test
    bot.search_engine.clear_tt()
    bot.learner.clear_cache()
    
    best_move_uci = bot.get_best_move(board.copy(), depth=depth, randomness=0)
    best_move = chess.Move.from_uci(best_move_uci)
    print(f"Depth {depth}: Best move = {board.san(best_move):6s}")

# Now let's trace EXACTLY what alphabeta returns for Nxf7 vs e4
print("\n" + "="*60)
print("DETAILED ALPHABETA TRACE FOR Nxf7 vs e4")
print("="*60)

for move_san in ["Nxf7", "e4"]:
    print(f"\n--- {move_san} ---")
    
    for depth in [2, 3]:
        board_copy = board.copy()
        board_copy.push_san(move_san)
        
        # Clear TT for fresh search
        bot.search_engine.clear_tt()
        bot.learner.clear_cache()
        
        # After playing move_san, it's Black's turn
        # alphabeta returns score from Black's perspective
        # So we negate to get White's perspective
        black_score = bot.search_engine.alphabeta(
            board_copy, depth, -float('inf'), float('inf'), 1,
            eval_model=bot.learner, allow_nmp=True, is_pv=True
        )
        white_score = -black_score
        
        print(f"  Depth {depth}: White's score after {move_san} = {white_score}")

# Let's also check what happens after Nxf7 with different Black replies
print("\n" + "="*60)
print("AFTER Nxf7, BLACK MOVES - WHAT DOES WHITE GET?")
print("="*60)

board.push_san("Nxf7")
print(f"\nAfter 1. Nxf7 (Black to move):")
print(board)

for black_move in ["Rg8", "Rf8", "Kf8", "Kxf7"]:
    board_copy = board.copy()
    try:
        board_copy.push_san(black_move)
    except:
        continue
    
    # Now White to move - search at depth 1
    bot.search_engine.clear_tt()
    score = bot.search_engine.alphabeta(
        board_copy, 1, -float('inf'), float('inf'), 2,
        eval_model=bot.learner, allow_nmp=True, is_pv=True
    )
    
    # Get best white move
    best_white = None
    best_score = -float('inf')
    for wm in board_copy.legal_moves:
        board_copy.push(wm)
        ev = bot.search_engine.alphabeta(
            board_copy, 0, -float('inf'), float('inf'), 3,
            eval_model=bot.learner, allow_nmp=False, is_pv=True
        )
        board_copy.pop()
        if -ev > best_score:
            best_score = -ev
            best_white = wm
    
    if best_white:
        print(f"  After 1...{black_move}: White's best = {board_copy.san(best_white)}, score = {score}")

board.pop()

# Final check: does the neural network evaluation make sense?
print("\n" + "="*60)
print("NEURAL NETWORK EVALUATION CHECK")
print("="*60)

for move_san in ["Nxf7", "e4"]:
    board_copy = board.copy()
    board_copy.push_san(move_san)
    
    # Get neural eval
    neural_val = bot.learner.get_value(board_copy)
    # Get heuristic eval (fast mode - material + PST)
    heuristic_fast = evaluate_position(board_copy, model=None, fast_mode=True)
    # Get heuristic eval (full mode with neural blend)
    heuristic_full = evaluate_position(board_copy, model=bot.learner, fast_mode=False)
    
    # Adjust for side to move
    neural_white = -neural_val  # Negate because Black to move after White's move
    heuristic_fast_white = heuristic_fast if board_copy.turn == chess.WHITE else -heuristic_fast
    heuristic_full_white = heuristic_full if board_copy.turn == chess.WHITE else -heuristic_full
    
    print(f"After {move_san}:")
    print(f"  Neural (White view) = {neural_white:.3f}")
    print(f"  Heuristic FAST (White view) = {heuristic_fast_white}")
    print(f"  Heuristic FULL (White view) = {heuristic_full_white}")

# Let's check if Black plays Kxf7, what is the evaluation?
print("\n" + "="*60)
print("AFTER Nxf7 Kxf7 - FULL EVAL")
print("="*60)
board_fork = board.copy()
board_fork.push_san("Nxf7")
board_fork.push_san("Kxf7")
print(board_fork)
print(f"Turn: {'White' if board_fork.turn == chess.WHITE else 'Black'}")

# Fast eval
fast_eval = evaluate_position(board_fork, model=None, fast_mode=True)
print(f"Fast eval (from side to move's perspective): {fast_eval}")

# Full eval with neural
full_eval = evaluate_position(board_fork, model=bot.learner, fast_mode=False)
print(f"Full eval with neural blend (from side to move's perspective): {full_eval}")

# Neural only
neural_val = bot.learner.get_value(board_fork)
print(f"Neural value (from side to move's perspective): {neural_val:.3f}")
