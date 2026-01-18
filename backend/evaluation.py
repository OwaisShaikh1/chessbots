"""
Chess position evaluation functions
"""
import chess


# Material values
PIECE_VALUES = {
    chess.PAWN: 100,
    chess.KNIGHT: 320,
    chess.BISHOP: 330,
    chess.ROOK: 500,
    chess.QUEEN: 900,
    chess.KING: 20000
}

# Piece-Square Tables (PST) - Middlegame (MG)
MG_PST = {
    chess.PAWN: [
        0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    ],
    chess.KNIGHT: [
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    ],
    chess.BISHOP: [
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    ],
    chess.ROOK: [
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        0,  0,  0,  5,  5,  0,  0,  0
    ],
    chess.QUEEN: [
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
        -5,  0,  5,  5,  5,  5,  0, -5,
        0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    ],
    chess.KING: [
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
        20, 20,  0,  0,  0,  0, 20, 20,
        20, 30, 10,  0,  0, 10, 30, 20
    ]
}

# Piece-Square Tables (PST) - Endgame (EG)
EG_PST = {
    chess.PAWN: [
        0,  0,  0,  0,  0,  0,  0,  0,
        170,170,170,170,170,170,170,170,
        145,145,145,145,145,145,145,145,
        120,120,120,120,120,120,120,120,
        95, 95, 95, 95, 95, 95, 95, 95,
        70, 70, 70, 70, 70, 70, 70, 70,
        45, 45, 45, 45, 45, 45, 45, 45,
        0,  0,  0,  0,  0,  0,  0,  0
    ],
    chess.KNIGHT: [
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -40,-20,-10,  0,  0,-10,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    ],
    chess.BISHOP: [
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    ],
    chess.ROOK: [
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        0,  0,  0,  5,  5,  0,  0,  0
    ],
    chess.QUEEN: [
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  5,  0,  0,  5,  0,-10,
        -10,  5,  5,  5,  5,  5,  5,-10,
        -5,  0,  5,  5,  5,  5,  0, -5,
        -5,  0,  5,  5,  5,  5,  0, -5,
        -10,  0,  5,  5,  5,  5,  0,-10,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    ],
    chess.KING: [
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,-10,  0,  0,-10,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    ]
}

# Piece phase values for tapered evaluation
PHASE_VALUES = {
    chess.PAWN: 0,
    chess.KNIGHT: 1,
    chess.BISHOP: 1,
    chess.ROOK: 2,
    chess.QUEEN: 4,
    chess.KING: 0
}
MAX_PHASE = 24 # for one side


def get_detailed_material(board):
    """Returns material values for (white, black)"""
    w_score = 0
    b_score = 0
    for piece_type, value in PIECE_VALUES.items():
        if piece_type == chess.KING:
            continue
        w_score += len(board.pieces(piece_type, chess.WHITE)) * value
        b_score += len(board.pieces(piece_type, chess.BLACK)) * value
    return w_score, b_score


def get_material_score(board):
    """Returns material score from White's perspective"""
    w, b = get_detailed_material(board)
    return w - b


MATE_VALUE = 100000

def loose_piece_penalty(board, square, piece, attackers_fn=None):
    """Robust Loose Piece Detection (LPDO) - compares lowest attacker vs lowest defender"""
    if attackers_fn:
        attackers = attackers_fn(not piece.color, square)
        defenders = attackers_fn(piece.color, square)
    else:
        attackers = board.attackers(not piece.color, square)
        defenders = board.attackers(piece.color, square)

    if not attackers:
        return 0
    if not defenders:
        return PIECE_VALUES[piece.piece_type]

    # Handle empty attacker/defender sets just in case although attackers is checked above
    atks_vals = [PIECE_VALUES[board.piece_at(a).piece_type] for a in attackers if board.piece_at(a)]
    defs_vals = [PIECE_VALUES[board.piece_at(d).piece_type] for d in defenders if board.piece_at(d)]
    
    if not atks_vals: return 0
    if not defs_vals: return PIECE_VALUES[piece.piece_type]

    min_attacker = min(atks_vals)
    min_defender = min(defs_vals)

    if min_attacker < min_defender:
        return PIECE_VALUES[piece.piece_type] // 2
    return 0

def threat_penalty(board, color, attackers_fn=None):
    """Detect if any piece of 'color' is under attack by the opponent"""
    penalty = 0
    for square, piece in board.piece_map().items():
        if piece.color == color:
            is_attacked = attackers_fn(not color, square) if attackers_fn else board.attackers(not color, square)
            if is_attacked:
                penalty += PIECE_VALUES[piece.piece_type] // 5
    return penalty

def evaluate_position(board):
    """Highly advanced evaluation with Tapered PST, LPDO, Threat Detection, and Safety Tuning"""
    if board.is_checkmate():
        return -MATE_VALUE if board.turn == chess.WHITE else MATE_VALUE
    if board.is_stalemate() or board.is_insufficient_material() or board.is_fifty_moves():
        return 0
    
    # 0. Attackers Cache
    attackers_cache = {}
    def attackers_fn(color, square):
        key = (color, square)
        if key not in attackers_cache:
            attackers_cache[key] = board.attackers(color, square)
        return attackers_cache[key]

    # 1. Calculate Phase (0 = EG, 24 = MG per side)
    mg_phase = 0
    for pt, val in PHASE_VALUES.items():
        mg_phase += len(board.pieces(pt, chess.WHITE)) * val
        mg_phase += len(board.pieces(pt, chess.BLACK)) * val
    
    current_max_phase = MAX_PHASE * 2
    if mg_phase > current_max_phase: mg_phase = current_max_phase
    eg_phase = current_max_phase - mg_phase
    
    mg_score = 0
    eg_score = 0
    
    piece_map = board.piece_map()
    white_king_sq = board.king(chess.WHITE)
    black_king_sq = board.king(chess.BLACK)
    
    white_pawns = int(board.pieces(chess.PAWN, chess.WHITE))
    black_pawns = int(board.pieces(chess.PAWN, chess.BLACK))

    # 2. Main Loop: Material, PST, and FIX 4: LPDO
    for square, piece in piece_map.items():
        pt = piece.piece_type
        if piece.color == chess.WHITE:
            mg_score += PIECE_VALUES[pt] + MG_PST[pt][chess.square_mirror(square)]
            eg_score += PIECE_VALUES[pt] + EG_PST[pt][chess.square_mirror(square)]
            
            # FIX 4: LPDO (Loose Piece Detection)
            penalty = loose_piece_penalty(board, square, piece, attackers_fn)
            mg_score -= penalty
            eg_score -= penalty

            # Rook on Open/Semi-Open File
            if pt == chess.ROOK:
                file_bb = chess.BB_FILES[chess.square_file(square)]
                if not (file_bb & white_pawns):
                    if not (file_bb & black_pawns):
                        mg_score += 15 # Open file
                    else:
                        mg_score += 10 # Semi-open
            
            # Passed Pawn detection
            if pt == chess.PAWN:
                file = chess.square_file(square)
                rank = chess.square_rank(square)
                passed = True
                for f in range(max(0, file-1), min(7, file+1) + 1):
                    enemy_pawns_in_front = black_pawns & chess.BB_FILES[f]
                    for ep_sq in chess.SquareSet(enemy_pawns_in_front):
                        if chess.square_rank(ep_sq) > rank:
                            passed = False
                            break
                    if not passed: break
                if passed:
                    passed_bonus = (rank - 1) * 10
                    mg_score += passed_bonus
                    eg_score += passed_bonus * 2
        else:
            mg_score -= PIECE_VALUES[pt] + MG_PST[pt][square]
            eg_score -= PIECE_VALUES[pt] + EG_PST[pt][square]
            
            # FIX 4: LPDO
            penalty = loose_piece_penalty(board, square, piece, attackers_fn)
            mg_score += penalty
            eg_score += penalty

            if pt == chess.ROOK:
                file_bb = chess.BB_FILES[chess.square_file(square)]
                if not (file_bb & black_pawns):
                    if not (file_bb & white_pawns):
                        mg_score -= 15
                    else:
                        mg_score -= 10
            
            if pt == chess.PAWN:
                file = chess.square_file(square)
                rank = chess.square_rank(square)
                passed = True
                for f in range(max(0, file-1), min(7, file+1) + 1):
                    enemy_pawns_in_front = white_pawns & chess.BB_FILES[f]
                    for ep_sq in chess.SquareSet(enemy_pawns_in_front):
                        if chess.square_rank(ep_sq) < rank:
                            passed = False
                            break
                    if not passed: break
                if passed:
                    passed_bonus = (6 - rank) * 10
                    mg_score -= passed_bonus
                    eg_score -= passed_bonus * 2

    # 3. Mobility
    white_mobility = sum(int(board.attacks(sq)).bit_count() for sq, p in piece_map.items() if p.color == chess.WHITE and p.piece_type != chess.PAWN)
    black_mobility = sum(int(board.attacks(sq)).bit_count() for sq, p in piece_map.items() if p.color == chess.BLACK and p.piece_type != chess.PAWN)
    mg_score += (white_mobility - black_mobility) * 5
    eg_score += (white_mobility - black_mobility) * 5

    # 4. FIX 1: Castling Bonus & FIX 2: King Exposure (MG Only)
    # Castling Bonus (Tuned: 50)
    if board.has_kingside_castling_rights(chess.WHITE) == False and white_king_sq in [chess.G1, chess.C1]:
        mg_score += 50
    if board.has_kingside_castling_rights(chess.BLACK) == False and black_king_sq in [chess.G8, chess.C8]:
        mg_score -= 50

    # FIX 2: King Exposure Penalty (Tuned: 25 per attacker)
    def tuned_king_exposure(board, color, attackers_fn):
        ksq = board.king(color)
        if ksq is None: return 0
        p = 0
        enemy = not color
        for sq in chess.SquareSet(chess.BB_KING_ATTACKS[ksq]):
            is_atkd = attackers_fn(enemy, sq) if attackers_fn else board.is_attacked_by(enemy, sq)
            if is_atkd: p += 25
        return p

    mg_score -= tuned_king_exposure(board, chess.WHITE, attackers_fn)
    mg_score += tuned_king_exposure(board, chess.BLACK, attackers_fn)

    # TODO 5: Threat Detection (Fork Awareness) - MG Only
    mg_score -= threat_penalty(board, chess.WHITE, attackers_fn)
    mg_score += threat_penalty(board, chess.BLACK, attackers_fn)

    # Legacy King Safety
    if white_king_sq is not None:
        for sq in chess.SquareSet(chess.BB_KING_ATTACKS[white_king_sq]):
            if board.piece_at(sq) and board.piece_at(sq).color == chess.BLACK:
                mg_score -= 30
    if black_king_sq is not None:
        for sq in chess.SquareSet(chess.BB_KING_ATTACKS[black_king_sq]):
            if board.piece_at(sq) and board.piece_at(sq).color == chess.WHITE:
                mg_score += 30

    # 5. Tapered Result
    score = (mg_score * mg_phase + eg_score * eg_phase) // current_max_phase
    return score


# MVV-LVA (Most Valuable Victim - Least Valuable Attacker) table
MVV_LVA = [
    [0, 0, 0, 0, 0, 0, 0],
    [105, 104, 103, 102, 101, 100, 100], # PAWN victim
    [205, 204, 203, 202, 201, 200, 200], # KNIGHT victim
    [305, 304, 303, 302, 301, 300, 300], # BISHOP victim
    [405, 404, 403, 402, 401, 400, 400], # ROOK victim
    [505, 504, 503, 502, 501, 500, 500], # QUEEN victim
    [605, 604, 603, 602, 601, 600, 600]  # KING victim
]

def order_moves(board, moves):
    """Fast move ordering using MVV-LVA and Checks"""
    def move_score(move):
        # 1. Captures (MVV-LVA)
        if board.is_capture(move):
            victim = board.piece_at(move.to_square)
            attacker = board.piece_at(move.from_square)
            if victim and attacker:
                return 1000 + MVV_LVA[victim.piece_type][attacker.piece_type]
            return 1100 # En passant
            
        # 2. Checks (High priority for finding mates)
        if board.gives_check(move):
            return 900
            
        # 3. Promotions
        if move.promotion:
            return 800
            
        return 0
    
    return sorted(moves, key=move_score, reverse=True)
