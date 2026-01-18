"""
Chess search algorithms: alpha-beta pruning and quiescence search
"""
import chess
import chess.polyglot
from evaluation import evaluate_position, order_moves, MATE_VALUE, MVV_LVA, PIECE_VALUES


class ChessSearch:
    """Handles alpha-beta search with TT, ID, and Null Move Pruning"""
    
    def __init__(self, tt_size=1000000):
        self.tt = {} # Simple transposition table: {hash: (depth, value, type, best_move)}
        self.tt_size = tt_size
        self.killer_moves = [[None] * 2 for _ in range(64)] # 2 killers per depth
    
    def clear_tt(self):
        self.tt.clear()
        self.killer_moves = [[None] * 2 for _ in range(64)]

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

    
    def alphabeta(self, board, depth, alpha, beta, ply, allow_nmp=False):
        """Negamax with alpha-beta pruning, TT, and Check Extensions"""
        alpha_orig = alpha

        
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

        if board.is_game_over():
            score = evaluate_position(board)
            score = score if board.turn == chess.WHITE else -score
            # Store absolute mate score (distance from root)
            if score > MATE_VALUE / 2: return score - ply
            if score < -MATE_VALUE / 2: return score + ply
            return score
            
        if depth == 0:
            return self.quiescence(board, alpha, beta, ply)
        
        # 2. Null Move Pruning (NMP) - DISABLED per TODO 1
        # if allow_nmp and not board.is_check() and depth >= 4:
        #     pieces = board.occupied_co[board.turn] & ~board.pawns
        #     if pieces:
        #         board.push(chess.Move.null())
        #         res = -self.alphabeta(board, depth - 1 - 2, -beta, -beta + 1, ply + 1, allow_nmp=False)
        #         board.pop()
        #         if res >= beta:
        #             return beta

        # 3. Move Ordering with Killer Moves
        legal_moves = list(board.legal_moves)
        killers = self.killer_moves[ply] if ply < 64 else [None, None]
        
        def move_score(move):
            if board.is_capture(move):
                victim = board.piece_at(move.to_square)
                attacker = board.piece_at(move.from_square)
                if victim and attacker:
                    if PIECE_VALUES[victim.piece_type] < PIECE_VALUES[attacker.piece_type]:
                        return 500   # BAD CAPTURE
                    return 2000 + MVV_LVA[victim.piece_type][attacker.piece_type]
                return 2100 
            if move == killers[0]: return 1000
            if move == killers[1]: return 900
            if board.gives_check(move): return 800 
            return 0

        moves = sorted(
            legal_moves,
            key=lambda m: (m == tt_move, move_score(m)),
            reverse=True
        )
        
        value = -float('inf')
        best_move_in_node = None
        
        for move in moves:
            board.push(move)
            
            # TODO 3: CHECK EXTENSION
            new_depth = depth - 1
            if board.is_check():
                new_depth += 1
                
            res = -self.alphabeta(board, new_depth, -beta, -alpha, ply + 1)
            board.pop()
            
            if res > value:
                value = res
                best_move_in_node = move
            
            alpha = max(alpha, value)
            if alpha >= beta:
                # 4. Store Killer Move
                if not board.is_capture(move) and ply < 64:
                    if move != self.killer_moves[ply][0]:
                        self.killer_moves[ply][1] = self.killer_moves[ply][0]
                        self.killer_moves[ply][0] = move
                break
        
        # 5. Transposition Table Store (Correct mate score for storage)
        tt_store_val = value
        if value > MATE_VALUE / 2: tt_store_val += ply
        elif value < -MATE_VALUE / 2: tt_store_val -= ply
            
        if len(self.tt) < self.tt_size:
            tt_type = 'exact'
            if value <= alpha_orig: tt_type = 'upper'
            elif value >= beta: tt_type = 'lower'
            self.tt[board_hash] = (depth, tt_store_val, tt_type, best_move_in_node)
            
        return value
    
    def quiescence(self, board, alpha, beta, ply, max_depth=3):
        """Quiescence search - focus on winning captures to avoid tactical blindness"""
        stand_pat = evaluate_position(board)
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
            score = -self.quiescence(board, -beta, -alpha, ply + 1, max_depth - 1)
            board.pop()
            
            if score >= beta:
                return beta
            if score > alpha:
                alpha = score
        
        return alpha
