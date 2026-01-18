"""
Chess search algorithms: alpha-beta pruning and quiescence search
Optimized with: LMR, NMP, Futility Pruning, History Heuristic, PVS
"""
import chess
import chess.polyglot
from evaluation import evaluate_position, order_moves, MATE_VALUE, MVV_LVA, PIECE_VALUES

# Late Move Reduction table (precomputed)
# LMR_TABLE[depth][move_count] = reduction amount
LMR_TABLE = [[0] * 64 for _ in range(64)]
for d in range(1, 64):
    for m in range(1, 64):
        if d >= 3 and m >= 4:
            # Logarithmic reduction formula (similar to Stockfish)
            import math
            LMR_TABLE[d][m] = max(1, int(0.5 + math.log(d) * math.log(m) / 2.0))


class ChessSearch:
    """Handles alpha-beta search with TT, LMR, NMP, Futility, History, and PVS"""
    
    def __init__(self, tt_size=2000000):
        self.tt = {} # Transposition table: {hash: (depth, value, type, best_move)}
        self.tt_size = tt_size
        self.killer_moves = [[None] * 2 for _ in range(64)] # 2 killers per depth
        self.history = {}  # History heuristic: {(color, from, to): score}
        self.quiescence_depth = 2  # Back to 2 for speed
        self.nodes_searched = 0
    
    def clear_tt(self):
        self.tt.clear()
        self.killer_moves = [[None] * 2 for _ in range(64)]
        self.history.clear()
        self.nodes_searched = 0
    
    def _update_history(self, board, move, depth):
        """Update history score for a move that caused a beta cutoff"""
        key = (board.turn, move.from_square, move.to_square)
        bonus = depth * depth  # Quadratic bonus
        self.history[key] = self.history.get(key, 0) + bonus
        # Age/cap history scores to prevent overflow
        if self.history[key] > 10000:
            for k in self.history:
                self.history[k] //= 2
    
    def _get_history(self, board, move):
        """Get history score for a move"""
        key = (board.turn, move.from_square, move.to_square)
        return self.history.get(key, 0)

    def _critical_piece_in_danger(self, board):
        """Checks if King or Queen of side to move is under attack"""
        for square, piece in board.piece_map().items():
            if piece.color != board.turn:
                continue
            if piece.piece_type not in (chess.QUEEN, chess.KING):
                continue
            if board.attackers(not board.turn, square):
                return True
        return False

    
    def _has_hanging_piece(self, board):
        """Check if either side has a hanging (undefended attacked) major piece"""
        for square, piece in board.piece_map().items():
            if piece.piece_type in [chess.QUEEN, chess.ROOK]:
                # Is it attacked?
                if board.attackers(not piece.color, square):
                    # Is it defended?
                    if not board.attackers(piece.color, square):
                        return True
        return False
    
    def _has_knight_fork_threat(self, board):
        """Fast check if opponent's knight threatens a royal fork (attacks king from reachable square)"""
        enemy_color = not board.turn
        our_king_sq = board.king(board.turn)
        
        # Quick check: find enemy knights and see if any can reach a square attacking our king
        enemy_knights = board.pieces(chess.KNIGHT, enemy_color)
        if not enemy_knights:
            return False
        
        for knight_sq in enemy_knights:
            knight_attacks = chess.BB_KNIGHT_ATTACKS[knight_sq]
            # Check if any reachable square can check our king
            for reachable_sq in chess.SquareSet(knight_attacks):
                blocker = board.piece_at(reachable_sq)
                if blocker and blocker.color == enemy_color:
                    continue  # Blocked by own piece
                
                from_there_attacks = chess.BB_KNIGHT_ATTACKS[reachable_sq]
                if from_there_attacks & chess.BB_SQUARES[our_king_sq]:
                    return True
        
        return False

    def alphabeta(self, board, depth, alpha, beta, ply, eval_model=None, allow_nmp=True, is_pv=True):
        """Negamax with alpha-beta, TT, NMP, LMR, Futility, and PVS"""
        self.nodes_searched += 1
        alpha_orig = alpha
        in_check = board.is_check()
        
        # 1. Transposition Table Lookup
        board_hash = chess.polyglot.zobrist_hash(board)
        tt_move = None
        if board_hash in self.tt:
            tt_depth, tt_val, tt_type, tt_move = self.tt[board_hash]
            if tt_depth >= depth:
                # Correct mate score relative to current ply
                if tt_val > MATE_VALUE / 2: tt_val -= ply
                elif tt_val < -MATE_VALUE / 2: tt_val += ply
                
                if tt_type == 'exact':
                    return tt_val
                elif tt_type == 'lower':
                    alpha = max(alpha, tt_val)
                elif tt_type == 'upper':
                    beta = min(beta, tt_val)
                
                if alpha >= beta:
                    return tt_val

        # Check for checkmate/stalemate BEFORE anything else
        if board.is_checkmate():
            return -MATE_VALUE + ply
        
        if board.is_stalemate() or board.is_insufficient_material() or board.is_seventyfive_moves() or board.is_fivefold_repetition():
            return 0  # Draw
            
        if depth <= 0:
            return self.quiescence(board, alpha, beta, ply, eval_model=None, max_depth=self.quiescence_depth)
        
        # Mate Distance Pruning
        mate_alpha = -MATE_VALUE + ply
        mate_beta = MATE_VALUE - ply - 1
        if mate_alpha > alpha:
            alpha = mate_alpha
            if alpha >= beta:
                return alpha
        if mate_beta < beta:
            beta = mate_beta
            if alpha >= beta:
                return beta
        
        # Tactical position detection - disable aggressive pruning when pieces are hanging
        # Only check fork threat at shallow depths (expensive check)
        tactical_position = self._has_hanging_piece(board) or self._critical_piece_in_danger(board)
        if not tactical_position and depth <= 3:
            tactical_position = self._has_knight_fork_threat(board)
        
        # 2. Null Move Pruning (NMP) - only in quiet non-tactical positions
        if allow_nmp and not in_check and depth >= 3 and not is_pv and not tactical_position:
            # Check we have non-pawn material
            dominated_by_pawns = (board.occupied_co[board.turn] & ~board.pawns & ~board.kings) == 0
            if not dominated_by_pawns:
                R = 2 + (depth >= 6)  # Reduction: 2 or 3
                board.push(chess.Move.null())
                null_score = -self.alphabeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1, 
                                            eval_model=eval_model, allow_nmp=False, is_pv=False)
                board.pop()
                if null_score >= beta:
                    return beta
        
        # 3. Futility Pruning - DISABLED in tactical positions
        futility_margin = 200 * depth  # ~200cp per depth
        static_eval = None
        if depth <= 2 and not in_check and abs(beta) < MATE_VALUE / 2 and not tactical_position:
            static_eval = evaluate_position(board, model=None, fast_mode=True)
            static_eval = static_eval if board.turn == chess.WHITE else -static_eval
            if static_eval - futility_margin >= beta:
                return static_eval  # Fail high - position is already winning

        # 4. Move Ordering with History Heuristic
        legal_moves = list(board.legal_moves)
        killers = self.killer_moves[ply] if ply < 64 else [None, None]
        
        def move_score(move):
            score = 0
            
            # TT move is handled separately in the sort key
            # Captures get high priority via MVV-LVA
            if board.is_capture(move):
                victim = board.piece_at(move.to_square)
                attacker = board.piece_at(move.from_square)
                if victim and attacker:
                    # MVV-LVA score, but don't penalize ALL "losing" captures
                    # because the piece might be forking/threatening other pieces
                    mvv_lva = MVV_LVA[victim.piece_type][attacker.piece_type]
                    
                    # Check if after this capture, we attack major pieces
                    board.push(move)
                    attacks_major = False
                    for sq, p in board.piece_map().items():
                        if p.color != board.turn and p.piece_type in [chess.QUEEN, chess.ROOK]:
                            if board.is_attacked_by(board.turn, sq):
                                attacks_major = True
                                break
                    board.pop()
                    
                    if attacks_major:
                        # This capture creates threats - prioritize it!
                        return 11000 + mvv_lva
                    else:
                        return 10000 + mvv_lva
                return 10100  # En passant
            
            # Killer moves  
            if move == killers[0]: return 9000
            if move == killers[1]: return 8900
            
            # Promotions
            if move.promotion: return 8000
            
            # Check if this quiet move attacks major pieces (forks!)
            board.push(move)
            attacks_major = False
            for sq, p in board.piece_map().items():
                if p.color != board.turn and p.piece_type in [chess.QUEEN, chess.ROOK, chess.KING]:
                    if board.is_attacked_by(board.turn, sq):
                        attacks_major = True
                        break
            gives_check = board.is_check()
            board.pop()
            
            if attacks_major or gives_check:
                return 7500  # High priority for forking/attacking moves
            
            # History heuristic
            return self._get_history(board, move)

        moves = sorted(
            legal_moves,
            key=lambda m: (m == tt_move, move_score(m)),
            reverse=True
        )
        
        value = -float('inf')
        best_move_in_node = None
        moves_searched = 0
        
        for move in moves:
            is_capture = board.is_capture(move)
            is_promotion = move.promotion is not None
            gives_check = board.gives_check(move)
            
            board.push(move)
            child_in_check = board.is_check()
            
            # Check extension
            new_depth = depth - 1
            if child_in_check:
                new_depth += 1
            
            # 5. Late Move Reductions (LMR)
            # Reduce search depth for later quiet moves
            # BUT: Don't reduce in tactical positions or if this move attacks major pieces
            reduction = 0
            if (moves_searched >= 4 and depth >= 3 and 
                not in_check and not child_in_check and
                not is_capture and not is_promotion and
                not gives_check and not tactical_position):
                reduction = LMR_TABLE[min(depth, 63)][min(moves_searched, 63)]
                # Don't reduce into negative depth
                reduction = min(reduction, new_depth - 1)
            
            # 6. Principal Variation Search (PVS)
            if moves_searched == 0:
                # First move - search with full window
                res = -self.alphabeta(board, new_depth, -beta, -alpha, ply + 1, 
                                     eval_model=eval_model, allow_nmp=True, is_pv=True)
            else:
                # Search with reduced depth and null window first
                res = -self.alphabeta(board, new_depth - reduction, -alpha - 1, -alpha, ply + 1,
                                     eval_model=eval_model, allow_nmp=True, is_pv=False)
                
                # Re-search if it looks promising
                if res > alpha and (reduction > 0 or res < beta):
                    res = -self.alphabeta(board, new_depth, -beta, -alpha, ply + 1,
                                         eval_model=eval_model, allow_nmp=True, is_pv=True)
            
            board.pop()
            moves_searched += 1
            
            if res > value:
                value = res
                best_move_in_node = move
            
            alpha = max(alpha, value)
            if alpha >= beta:
                # Store Killer Move and update history
                if not is_capture and ply < 64:
                    if move != self.killer_moves[ply][0]:
                        self.killer_moves[ply][1] = self.killer_moves[ply][0]
                        self.killer_moves[ply][0] = move
                    self._update_history(board, move, depth)
                break
        
        # 7. Transposition Table Store
        tt_store_val = value
        if value > MATE_VALUE / 2: tt_store_val += ply
        elif value < -MATE_VALUE / 2: tt_store_val -= ply
            
        if len(self.tt) < self.tt_size:
            tt_type = 'exact'
            if value <= alpha_orig: tt_type = 'upper'
            elif value >= beta: tt_type = 'lower'
            self.tt[board_hash] = (depth, tt_store_val, tt_type, best_move_in_node)
            
        return value
    
    def quiescence(self, board, alpha, beta, ply, eval_model=None, max_depth=3):
        """Quiescence search - focus on winning captures to avoid tactical blindness"""
        # Check for immediate checkmate
        if board.is_checkmate():
            return -MATE_VALUE + ply
        
        if board.is_stalemate() or board.is_insufficient_material():
            return 0
        
        # Use fast evaluation in quiescence for speed (no neural, no complex heuristics)
        stand_pat = evaluate_position(board, model=None, fast_mode=True)
        stand_pat = stand_pat if board.turn == chess.WHITE else -stand_pat
        
        if stand_pat > MATE_VALUE / 2: stand_pat -= ply
        elif stand_pat < -MATE_VALUE / 2: stand_pat += ply

        if stand_pat >= beta:
            # TODO 2: DO NOT cutoff if captures exist
            if not any(board.is_capture(m) for m in board.legal_moves):
                return beta
                
        if alpha < stand_pat:
            alpha = stand_pat
        
        if max_depth == 0:
            return stand_pat
        
        # FORCE QUIET MOVE SEARCH WHEN MATERIAL (Q/K) IS HANGING
        in_danger = board.is_check() or self._critical_piece_in_danger(board)

        if in_danger:
            moves = board.legal_moves
        else:
            # FIX 5: Focus on captures, especially winning ones
            moves = [m for m in board.legal_moves if board.is_capture(m)]
        
        # Sort moves using MVV-LVA
        def quiescence_score(move):
            victim = board.piece_at(move.to_square)
            attacker = board.piece_at(move.from_square)
            if victim and attacker:
                # Prioritize MVV-LVA captures
                return 1000 + MVV_LVA[victim.piece_type][attacker.piece_type]
            return 1100 # En passant
            
        moves = sorted(list(moves), key=quiescence_score, reverse=True)
        
        for move in moves:
            # Simple Static Exchange Evaluation (SEE) proxy
            if not board.is_check():
                victim = board.piece_at(move.to_square)
                attacker = board.piece_at(move.from_square)
                if victim and attacker:
                    if PIECE_VALUES[victim.piece_type] < PIECE_VALUES[attacker.piece_type]:
                        continue

            board.push(move)
            score = -self.quiescence(board, -beta, -alpha, ply + 1, eval_model=eval_model, max_depth=max_depth - 1)
            board.pop()
            
            if score >= beta:
                return beta
            if score > alpha:
                alpha = score
        
        return alpha
