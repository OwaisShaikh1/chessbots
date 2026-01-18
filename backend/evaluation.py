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

# Tunable evaluation parameters (can be modified at runtime)
EVAL_PARAMS = {
    "mobility_weight": 5,          # Centipawns per attacked square
    "castling_bonus": 50,          # Bonus for castled king
    "king_exposure_penalty": 25,   # Penalty per attacked square near king
    "king_safety_penalty": 30,     # Penalty for enemy piece near king
    "rook_open_file": 15,          # Bonus for rook on open file
    "rook_semi_open": 10,          # Bonus for rook on semi-open file
    "passed_pawn_scale": 10,       # Passed pawn bonus multiplier per rank
    "threat_divisor": 5,           # Divide piece value by this for threat penalty
    "lpdo_divisor": 2,             # Loose piece danger divisor
    "neural_blend": 15,            # 0-100: percentage of neural vs heuristic (lowered from 50 for better tactics)
    "queen_early_penalty": 20,     # Penalty for queen moving early (before move 8)
    "queen_exposure_penalty": 40,  # Penalty if queen is attacked
    "pin_penalty": 50,             # Penalty for pinned piece (scaled by piece value)
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


def endgame_mate_bonus(board):
    """Bonus for driving the enemy king to edges in winning endgames (e.g., Queen vs King)"""
    white_material, black_material = get_detailed_material(board)
    
    bonus = 0
    
    # Check if White has mating material and Black only has king
    if white_material >= 900 and black_material == 0:  # White has queen or more, Black only king
        black_king_sq = board.king(chess.BLACK)
        if black_king_sq is not None:
            # Reward pushing king to edges (distance from center)
            file, rank = chess.square_file(black_king_sq), chess.square_rank(black_king_sq)
            center_dist = max(abs(file - 3.5), abs(rank - 3.5))
            bonus += int(center_dist * 30)  # Up to 105 bonus for corner
            
            # Reward reducing the distance between our king and enemy king
            white_king_sq = board.king(chess.WHITE)
            if white_king_sq is not None:
                king_distance = chess.square_distance(white_king_sq, black_king_sq)
                bonus += (7 - king_distance) * 10  # Reward being close
    
    # Check if Black has mating material and White only has king
    elif black_material >= 900 and white_material == 0:
        white_king_sq = board.king(chess.WHITE)
        if white_king_sq is not None:
            file, rank = chess.square_file(white_king_sq), chess.square_rank(white_king_sq)
            center_dist = max(abs(file - 3.5), abs(rank - 3.5))
            bonus -= int(center_dist * 30)
            
            black_king_sq = board.king(chess.BLACK)
            if black_king_sq is not None:
                king_distance = chess.square_distance(white_king_sq, black_king_sq)
                bonus -= (7 - king_distance) * 10
    
    return bonus


MATE_VALUE = 100000

def loose_piece_penalty(board, square, piece, attackers_fn=None):
    """Robust Loose Piece Detection (LPDO) - compares lowest attacker vs lowest defender"""
    # King should never get LPDO penalty - it can't be captured
    if piece.piece_type == chess.KING:
        return 0
    
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
        return PIECE_VALUES[piece.piece_type] // EVAL_PARAMS["lpdo_divisor"]
    return 0

def threat_penalty(board, color, attackers_fn=None):
    """Detect if any piece of 'color' is under attack by the opponent"""
    penalty = 0
    for square, piece in board.piece_map().items():
        # Skip king - attacks on king are handled by check detection
        if piece.color == color and piece.piece_type != chess.KING:
            is_attacked = attackers_fn(not color, square) if attackers_fn else board.attackers(not color, square)
            if is_attacked:
                penalty += PIECE_VALUES[piece.piece_type] // EVAL_PARAMS["threat_divisor"]
    return penalty

def detect_pins(board, color):
    """Detect pinned pieces for a given color - especially penalize queen pins"""
    penalty = 0
    king_sq = board.king(color)
    if king_sq is None:
        return 0
    
    king_file, king_rank = chess.square_file(king_sq), chess.square_rank(king_sq)
    enemy = not color
    
    # Check all enemy sliding pieces for potential pins
    for enemy_sq, enemy_piece in board.piece_map().items():
        if enemy_piece.color != enemy:
            continue
        
        # Only sliding pieces can pin
        if enemy_piece.piece_type not in [chess.ROOK, chess.QUEEN, chess.BISHOP]:
            continue
        
        e_file, e_rank = chess.square_file(enemy_sq), chess.square_rank(enemy_sq)
        
        # Check if enemy piece is on a ray from the king
        on_same_file = e_file == king_file
        on_same_rank = e_rank == king_rank
        on_same_diag = abs(e_file - king_file) == abs(e_rank - king_rank)
        
        # Rooks/queens pin along files/ranks, bishops/queens pin on diagonals
        is_ortho = on_same_file or on_same_rank
        is_diag = on_same_diag and not (on_same_file and on_same_rank)
        
        if enemy_piece.piece_type == chess.ROOK and not is_ortho:
            continue
        if enemy_piece.piece_type == chess.BISHOP and not is_diag:
            continue
        if enemy_piece.piece_type == chess.QUEEN and not (is_ortho or is_diag):
            continue
        
        # Find squares between enemy and king
        step_file = 0 if e_file == king_file else (1 if king_file > e_file else -1)
        step_rank = 0 if e_rank == king_rank else (1 if king_rank > e_rank else -1)
        
        squares_between = []
        f, r = e_file + step_file, e_rank + step_rank
        while (f, r) != (king_file, king_rank):
            squares_between.append(chess.square(f, r))
            f += step_file
            r += step_rank
        
        # Check if exactly one friendly piece is between enemy and king
        friendly_pieces_between = []
        enemy_pieces_between = 0
        for sq in squares_between:
            piece = board.piece_at(sq)
            if piece:
                if piece.color == color:
                    friendly_pieces_between.append((sq, piece))
                else:
                    enemy_pieces_between += 1
        
        # A pin exists if exactly one friendly piece is between, and no enemy pieces block
        if len(friendly_pieces_between) == 1 and enemy_pieces_between == 0:
            pinned_sq, pinned_piece = friendly_pieces_between[0]
            
            # Queen pinned by rook/queen is catastrophic
            if pinned_piece.piece_type == chess.QUEEN:
                if enemy_piece.piece_type in [chess.ROOK, chess.QUEEN]:
                    # Queen pinned by rook/queen: almost losing the queen
                    penalty += PIECE_VALUES[chess.QUEEN] - PIECE_VALUES[chess.ROOK]
                else:
                    # Queen pinned by bishop: still bad but less so
                    penalty += PIECE_VALUES[chess.QUEEN] - PIECE_VALUES[chess.BISHOP]
            else:
                # Standard pin penalty scaled by piece value
                penalty += (PIECE_VALUES[pinned_piece.piece_type] * EVAL_PARAMS["pin_penalty"]) // 100
    
    return penalty

def queen_safety_penalty(board, color, attackers_fn=None):
    """Evaluate queen safety - check if queen is attacked or moved too early"""
    penalty = 0
    
    for square, piece in board.piece_map().items():
        if piece.color == color and piece.piece_type == chess.QUEEN:
            # Check if queen is under attack
            is_attacked = attackers_fn(not color, square) if attackers_fn else board.attackers(not color, square)
            if is_attacked:
                penalty += EVAL_PARAMS["queen_exposure_penalty"]
            
            # Penalty for early queen development (only in opening)
            # Check if queen has moved from starting position
            starting_rank = 0 if color == chess.WHITE else 7
            if chess.square_rank(square) != starting_rank:
                # Count developed pieces (knights and bishops)
                developed = 0
                for sq, p in board.piece_map().items():
                    if p.color == color and p.piece_type in [chess.KNIGHT, chess.BISHOP]:
                        start_rank = 0 if color == chess.WHITE else 7
                        if chess.square_rank(sq) != start_rank:
                            developed += 1
                
                # Penalty if queen moved before developing 2+ minor pieces
                if developed < 2:
                    penalty += EVAL_PARAMS["queen_early_penalty"]
    
    return penalty


def evaluate_position_fast(board):
    """
    Fast evaluation using only material and PST - for quiescence and high-speed search.
    About 5-10x faster than full evaluate_position.
    """
    if board.is_checkmate():
        return -MATE_VALUE if board.turn == chess.WHITE else MATE_VALUE
    if board.is_stalemate() or board.is_insufficient_material():
        return 0
    
    # Calculate phase for tapering
    mg_phase = 0
    for pt, val in PHASE_VALUES.items():
        mg_phase += len(board.pieces(pt, chess.WHITE)) * val
        mg_phase += len(board.pieces(pt, chess.BLACK)) * val
    
    current_max_phase = MAX_PHASE * 2
    if mg_phase > current_max_phase: 
        mg_phase = current_max_phase
    eg_phase = current_max_phase - mg_phase
    
    mg_score = 0
    eg_score = 0
    
    # Material + PST only
    for square, piece in board.piece_map().items():
        pt = piece.piece_type
        if piece.color == chess.WHITE:
            mg_score += PIECE_VALUES[pt] + MG_PST[pt][chess.square_mirror(square)]
            eg_score += PIECE_VALUES[pt] + EG_PST[pt][chess.square_mirror(square)]
        else:
            mg_score -= PIECE_VALUES[pt] + MG_PST[pt][square]
            eg_score -= PIECE_VALUES[pt] + EG_PST[pt][square]
    
    # Tapered score
    return (mg_score * mg_phase + eg_score * eg_phase) // current_max_phase


def evaluate_position(board, model=None, fast_mode=False):
    """Highly advanced evaluation with Tapered PST, LPDO, Threat Detection, and Safety Tuning.
    
    Args:
        board: chess.Board to evaluate
        model: Optional neural model for blending
        fast_mode: If True, use simplified fast evaluation (material + PST only)
    """
    # Fast mode for speed-critical paths
    if fast_mode:
        return evaluate_position_fast(board)
        
    if board.is_checkmate():
        return -MATE_VALUE if board.turn == chess.WHITE else MATE_VALUE
    if board.is_stalemate() or board.is_insufficient_material() or board.is_fifty_moves():
        return 0
    
    # 0. Get Neural Value if model exists
    neural_score = 0
    if model:
        # get_value returns perspective-adjusted value [-1, 1]
        # We need absolute score from White's perspective
        val = model.get_value(board) # [-1, 1] for current turn
        # Scale to centipawns ([-1000, 1000])
        cp_val = int(val * 1000)
        # If it's Black's turn, the model's positive value is Black's advantage
        neural_score = cp_val if board.turn == chess.WHITE else -cp_val

    # 1. Attackers Cache
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
                        mg_score += EVAL_PARAMS["rook_open_file"] # Open file
                    else:
                        mg_score += EVAL_PARAMS["rook_semi_open"] # Semi-open
            
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
                    passed_bonus = (rank - 1) * EVAL_PARAMS["passed_pawn_scale"]
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
                        mg_score -= EVAL_PARAMS["rook_open_file"]
                    else:
                        mg_score -= EVAL_PARAMS["rook_semi_open"]
            
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
                    passed_bonus = (6 - rank) * EVAL_PARAMS["passed_pawn_scale"]
                    mg_score -= passed_bonus
                    eg_score -= passed_bonus * 2

    # 3. Mobility
    white_mobility = sum(int(board.attacks(sq)).bit_count() for sq, p in piece_map.items() if p.color == chess.WHITE and p.piece_type != chess.PAWN)
    black_mobility = sum(int(board.attacks(sq)).bit_count() for sq, p in piece_map.items() if p.color == chess.BLACK and p.piece_type != chess.PAWN)
    mg_score += (white_mobility - black_mobility) * EVAL_PARAMS["mobility_weight"]
    eg_score += (white_mobility - black_mobility) * EVAL_PARAMS["mobility_weight"]

    # 4. FIX 1: Castling Bonus & FIX 2: King Exposure (MG Only)
    # Castling Bonus
    if board.has_kingside_castling_rights(chess.WHITE) == False and white_king_sq in [chess.G1, chess.C1]:
        mg_score += EVAL_PARAMS["castling_bonus"]
    if board.has_kingside_castling_rights(chess.BLACK) == False and black_king_sq in [chess.G8, chess.C8]:
        mg_score -= EVAL_PARAMS["castling_bonus"]

    # FIX 2: King Exposure Penalty
    def tuned_king_exposure(board, color, attackers_fn):
        ksq = board.king(color)
        if ksq is None: return 0
        p = 0
        enemy = not color
        for sq in chess.SquareSet(chess.BB_KING_ATTACKS[ksq]):
            is_atkd = attackers_fn(enemy, sq) if attackers_fn else board.is_attacked_by(enemy, sq)
            if is_atkd: p += EVAL_PARAMS["king_exposure_penalty"]
        return p

    mg_score -= tuned_king_exposure(board, chess.WHITE, attackers_fn)
    mg_score += tuned_king_exposure(board, chess.BLACK, attackers_fn)

    # TODO 5: Threat Detection (Fork Awareness) - MG Only
    mg_score -= threat_penalty(board, chess.WHITE, attackers_fn)
    mg_score += threat_penalty(board, chess.BLACK, attackers_fn)
    
    # NEW: Queen Safety Evaluation (MG Only)
    mg_score -= queen_safety_penalty(board, chess.WHITE, attackers_fn)
    mg_score += queen_safety_penalty(board, chess.BLACK, attackers_fn)
    
    # NEW: Pin Detection (Both MG and EG)
    white_pin_penalty = detect_pins(board, chess.WHITE)
    black_pin_penalty = detect_pins(board, chess.BLACK)
    mg_score -= white_pin_penalty
    eg_score -= white_pin_penalty
    mg_score += black_pin_penalty
    eg_score += black_pin_penalty

    # Legacy King Safety
    if white_king_sq is not None:
        for sq in chess.SquareSet(chess.BB_KING_ATTACKS[white_king_sq]):
            if board.piece_at(sq) and board.piece_at(sq).color == chess.BLACK:
                mg_score -= EVAL_PARAMS["king_safety_penalty"]
    if black_king_sq is not None:
        for sq in chess.SquareSet(chess.BB_KING_ATTACKS[black_king_sq]):
            if board.piece_at(sq) and board.piece_at(sq).color == chess.WHITE:
                mg_score += EVAL_PARAMS["king_safety_penalty"]

    # 5. Tapered Result & Neural Blending
    heuristic_score = (mg_score * mg_phase + eg_score * eg_phase) // current_max_phase
    
    # Add endgame mating bonus
    heuristic_score += endgame_mate_bonus(board)
    
    if model:
        # Blend based on EVAL_PARAMS (0 = pure heuristic, 100 = pure neural)
        blend = EVAL_PARAMS["neural_blend"] / 100.0
        return int(heuristic_score * (1 - blend) + neural_score * blend)
        
    return heuristic_score


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
    """Fast move ordering using MVV-LVA, Checks, and Tactical Threats"""
    
    # Find enemy king and major piece positions
    enemy_color = not board.turn
    enemy_king_sq = board.king(enemy_color)
    enemy_queen_sq = None
    enemy_rook_sqs = []
    for sq, p in board.piece_map().items():
        if p.color == enemy_color:
            if p.piece_type == chess.QUEEN:
                enemy_queen_sq = sq
            elif p.piece_type == chess.ROOK:
                enemy_rook_sqs.append(sq)
    
    def move_score(move):
        score = 0
        
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
        
        piece = board.piece_at(move.from_square)
        if not piece:
            return 0
            
        dest = move.to_square
        
        # 4. Knight and bishop moves that attack major pieces
        if piece.piece_type in [chess.KNIGHT, chess.BISHOP]:
            if piece.piece_type == chess.KNIGHT:
                attacks = chess.BB_KNIGHT_ATTACKS[dest]
            else:
                # For bishops, we'd need to compute attacks, skip for now
                attacks = 0
                
            # Direct attack on queen
            if enemy_queen_sq and (attacks & chess.BB_SQUARES[enemy_queen_sq]):
                score = max(score, 750)  # Attacking queen is very valuable
                
            # Direct attack on king (creating threats)
            if enemy_king_sq and (attacks & chess.BB_SQUARES[enemy_king_sq]):
                score = max(score, 700)
                
            # Attack on rook
            for rook_sq in enemy_rook_sqs:
                if attacks & chess.BB_SQUARES[rook_sq]:
                    score = max(score, 650)
                    break
        
        # 5. Knight fork threats (knight move that can lead to fork next move)
        if piece.piece_type == chess.KNIGHT:
            knight_attacks = chess.BB_KNIGHT_ATTACKS[dest]
            
            # Check if from the destination, there's a square that forks king+queen
            for next_sq in chess.SquareSet(knight_attacks):
                # Can we actually move there (not blocked by own piece)?
                own_piece = board.piece_at(next_sq)
                if own_piece and own_piece.color == board.turn:
                    continue
                    
                future_attacks = chess.BB_KNIGHT_ATTACKS[next_sq]
                attacks_king = enemy_king_sq and (future_attacks & chess.BB_SQUARES[enemy_king_sq])
                attacks_queen = enemy_queen_sq and (future_attacks & chess.BB_SQUARES[enemy_queen_sq])
                
                if attacks_king and attacks_queen:
                    # This knight move sets up a fork next move!
                    # Check if the fork square is a capture (fork with tempo)
                    target = board.piece_at(next_sq)
                    if target and target.color == enemy_color:
                        score = max(score, 820)  # Fork with capture
                    else:
                        score = max(score, 720)  # Fork threat
                elif attacks_king:
                    # Can fork king + rook?
                    for rook_sq in enemy_rook_sqs:
                        if future_attacks & chess.BB_SQUARES[rook_sq]:
                            score = max(score, 700)
                            break
            
        return score
    
    return sorted(moves, key=move_score, reverse=True)
