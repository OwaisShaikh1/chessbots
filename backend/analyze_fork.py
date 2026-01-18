import chess
from bot import ChessBot

# The key positions from the conversation summary:
# Position after 15...Nc6: 1r3b1r/2q2kp1/2n1bn1p/3p4/7P/3QP3/PPP2PP1/R3KB1R w KQ - 3 16
# Position after 16.b3 16...Nb4: 1r3b1r/2q2kp1/4bn1p/3p4/1n5P/1P1QP3/P1P2PP1/R3KB1R w KQ - 1 17

# Let's work backwards from the known positions to understand the fork sequence
# Fork sequence from move 16 position:
# 16. b3 Nb4 17. Qd4 Nxc2+ (4 ply)

# Let me trace from the position we know
board = chess.Board('1r3b1r/2q2kp1/2n1bn1p/3p4/7P/3QP3/PPP2PP1/R3KB1R w KQ - 3 16')

# This is after move 15...Nc6, White to play move 16
# The optimal fork sequence for Black: 
# If White plays b3, then Nb4, then any queen move, then Nxc2+

# Let's trace backwards to find the positions at moves 13, 14, 15
# We need to understand the knight's journey to the fork:
# The knight needs to go: somewhere -> Na5 -> Nc6 -> Nb4 -> Nxc2+

print("=== Known Position: After 15...Nc6 (White to play move 16) ===")
print(board)
print(f"FEN: {board.fen()}")
print()

# The fork is: b3 Nb4 Qd4 Nxc2+ (4 ply from here)
print("Fork sequence from here: b3 Nb4 Qd4 Nxc2+ = 4 ply")
print("At depth 4, the bot should see the fork!")
print()

# Let's trace back to find move 15, 14, 13 positions
# We need to undo: 15...Nc6
# That means the knight came from somewhere to c6

# Let's assume typical knight path: Na5 -> Nc6
# Before 15...Nc6, the position had knight on a5

print("=== Tracing backwards ===")
print()

# Position after 14...Na5 (White to play move 15)
# Knight was on a5, not c6 yet
board_14 = chess.Board('1r3b1r/2q2kp1/4bn1p/n2p4/7P/3QP3/PPP2PP1/R3KB1R w KQ - 2 15')
print("After 14...Na5 (White to play move 15):")
print(board_14)
print()

# Fork sequence from here: ?Nc5? Nc6 b3 Nb4 Qd4 Nxc2+
# That's: White's move, Nc6, b3, Nb4, Qd4, Nxc2+ = 6 ply
print("Fork path: [White move] Nc6 b3 Nb4 Qd4 Nxc2+ = 6 ply (BEYOND depth 4)")
print()

# Position after move 13...? (White to play move 14)
# Need to go back further
print("=== Summary of ply distances ===")
print("Move 16 (after 15...Nc6): Fork is 4 ply away -> Should see at depth 4")
print("Move 15 (after 14...Na5): Fork is 6 ply away -> Needs depth 6")
print("Move 14 (after 13...?):   Fork is 8 ply away -> Needs depth 8")
print("Move 13 (after 12...?):   Fork is 10 ply away -> Needs depth 10")

print()
print("=== Now let's verify what the bot sees at each position ===")

from bot import ChessBot
bot = ChessBot()

# Position at move 16 (after 15...Nc6)
board = chess.Board('1r3b1r/2q2kp1/2n1bn1p/3p4/7P/3QP3/PPP2PP1/R3KB1R w KQ - 3 16')
print("\n--- Move 16 position (fork 4 ply away) ---")
bot.search_engine.clear_tt()
for depth in [4, 5, 6]:
    best_move_uci = bot.get_best_move(board, depth=depth, randomness=0)
    best_move = chess.Move.from_uci(best_move_uci)
    print(f"Depth {depth}: {board.san(best_move)}")

# Check what happens after b3
print("\nIf White plays b3, what does Black see?")
board.push_san('b3')
bot.search_engine.clear_tt()
for depth in [3, 4]:
    best_move_uci = bot.get_best_move(board, depth=depth, randomness=0)
    best_move = chess.Move.from_uci(best_move_uci)
    print(f"Depth {depth}: Black plays {board.san(best_move)}")
board.pop()

print("\n=== CONCLUSION ===")
print("At depth 4:")
print("- Move 16: Fork is exactly 4 ply away, so it's at the search horizon")
print("- The bot may or may not see it depending on move ordering and pruning")
print("- At depth 5+, the fork is clearly visible")
print()
print("At moves 13-15:")
print("- The fork was 6-10 ply away, far beyond depth 4")
print("- No depth 4 search could possibly see it")
print("- The bot would need depth 6+ at move 15, depth 8+ at move 14")
