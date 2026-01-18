from typing import Optional
from fastapi import FastAPI, HTTPException, BackgroundTasks
from fastapi.responses import StreamingResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from bot import ChessBot

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"], # Allow all for development
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

bot = ChessBot()

class MoveRequest(BaseModel):
    uci: str

@app.get("/")
def read_root():
    return {"message": "Chess Bot API is running", "fen": bot.get_fen()}

@app.get("/fen")
def get_fen():
    return {"fen": bot.get_fen(), "game_over": bot.is_game_over()}

@app.post("/move")
def make_move(move: MoveRequest):
    success = bot.make_move(move.uci)
    if not success:
        raise HTTPException(status_code=400, detail="Invalid move")
    return {"fen": bot.get_fen(), "game_over": bot.is_game_over()}

@app.get("/bot-move")
def make_bot_move():
    if bot.is_game_over():
        return {"message": "Game Over", "fen": bot.get_fen()}
    
    move = bot.get_best_move()
    if move:
        bot.make_move(move)
        return {"move": move, "fen": bot.get_fen(), "game_over": bot.is_game_over()}
    return {"message": "No moves available"}

@app.post("/reset")
def reset_game():
    bot.reset_board()
    return {"message": "Game reset", "fen": bot.get_fen()}

class TrainRequest(BaseModel):
    iterations: int = 100
    fen: str = None
    draw_punishment: float = 0.0
    material_weight: float = 0.0
    depth: int = 1

@app.post("/train-stream")
def train_stream(request: TrainRequest):
    return StreamingResponse(
        bot.train_gen(
            request.iterations, 
            request.fen, 
            request.draw_punishment, 
            request.material_weight, 
            request.depth
        ), 
        media_type="application/x-ndjson"
    )

@app.post("/train")
def train_bot(request: TrainRequest, background_tasks: BackgroundTasks):
    # Run training in background so we can poll status
    background_tasks.add_task(bot.train, request.iterations, request.fen)
    return {"message": "Training started", "iterations": request.iterations}

@app.get("/training-status")
def get_training_status():
    return bot.training_status

class BattleRequest(BaseModel):
    iterations: int = 10
    engine_path: str = "stockfish.exe"
    elo: int = 1350
    fen: Optional[str] = None
    depth: int = 3
    bot_side: str = "alternate"
    threads: int = 1
    hash_size: int = 16

@app.post("/battle-stream")
def battle_stream(request: BattleRequest):
    return StreamingResponse(
        bot.battle_stockfish_gen(
            request.iterations, 
            request.engine_path, 
            request.elo,
            request.fen,
            request.depth,
            request.bot_side,
            request.threads,
            request.hash_size
        ), 
        media_type="application/x-ndjson"
    )

@app.post("/battle-pause")
def toggle_battle_pause():
    bot.battle_paused = not bot.battle_paused
    return {"paused": bot.battle_paused}

class HistoryTrainRequest(BaseModel):
    batch_size: int = 32
    epochs: int = 5

@app.post("/train-history-stream")
def train_history_stream(request: HistoryTrainRequest):
    return StreamingResponse(
        bot.train_from_history_gen(
            request.batch_size,
            request.epochs
        ),
        media_type="application/x-ndjson"
    )

@app.post("/train_from_history")
def train_from_history(request: HistoryTrainRequest = None):
    """Simple endpoint to trigger history training"""
    try:
        batch_size = request.batch_size if request else 32
        epochs = request.epochs if request else 5
        
        # Run training synchronously and get the results
        results = []
        for update in bot.train_from_history_gen(batch_size, epochs):
            results.append(update)
        
        # Return the final result
        if results:
            final = results[-1]
            return {"message": f"Training complete. Final loss: {final.get('train_loss', 'N/A')}", "details": final}
        return {"message": "Training completed with no data"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/metrics")
def get_metrics():
    # Return model info
    return {
        "model_type": "Neural Network (ChessNet)",
        "file_path": bot.learner.filepath,
        "device": str(bot.learner.device)
    }

@app.get("/analytics")
def get_analytics():
    return bot.get_analytics()

# ============ PARAMETER TUNING API ============
from evaluation import PIECE_VALUES, EVAL_PARAMS

class ParametersRequest(BaseModel):
    # Material values
    pawn_value: Optional[int] = None
    knight_value: Optional[int] = None
    bishop_value: Optional[int] = None
    rook_value: Optional[int] = None
    queen_value: Optional[int] = None
    
    # Positional parameters
    mobility_weight: Optional[int] = None
    castling_bonus: Optional[int] = None
    king_exposure_penalty: Optional[int] = None
    king_safety_penalty: Optional[int] = None
    rook_open_file: Optional[int] = None
    rook_semi_open: Optional[int] = None
    passed_pawn_scale: Optional[int] = None
    threat_divisor: Optional[int] = None
    lpdo_divisor: Optional[int] = None
    
    # Queen safety and pins
    queen_early_penalty: Optional[int] = None
    queen_exposure_penalty: Optional[int] = None
    pin_penalty: Optional[int] = None
    
    # Neural blend (0-100)
    neural_blend: Optional[int] = None
    
    # Search parameters
    quiescence_depth: Optional[int] = None
    
    # Training
    learning_rate: Optional[float] = None

@app.get("/parameters")
def get_parameters():
    """Get all tunable bot parameters"""
    import chess
    return {
        "material": {
            "pawn": PIECE_VALUES[chess.PAWN],
            "knight": PIECE_VALUES[chess.KNIGHT],
            "bishop": PIECE_VALUES[chess.BISHOP],
            "rook": PIECE_VALUES[chess.ROOK],
            "queen": PIECE_VALUES[chess.QUEEN],
        },
        "positional": {
            "mobility_weight": EVAL_PARAMS["mobility_weight"],
            "castling_bonus": EVAL_PARAMS["castling_bonus"],
            "king_exposure_penalty": EVAL_PARAMS["king_exposure_penalty"],
            "king_safety_penalty": EVAL_PARAMS["king_safety_penalty"],
            "rook_open_file": EVAL_PARAMS["rook_open_file"],
            "rook_semi_open": EVAL_PARAMS["rook_semi_open"],
            "passed_pawn_scale": EVAL_PARAMS["passed_pawn_scale"],
            "threat_divisor": EVAL_PARAMS["threat_divisor"],
            "lpdo_divisor": EVAL_PARAMS["lpdo_divisor"],
            "queen_early_penalty": EVAL_PARAMS["queen_early_penalty"],
            "queen_exposure_penalty": EVAL_PARAMS["queen_exposure_penalty"],
            "pin_penalty": EVAL_PARAMS["pin_penalty"],
        },
        "neural": {
            "neural_blend": EVAL_PARAMS["neural_blend"],
        },
        "search": {
            "quiescence_depth": bot.search_engine.quiescence_depth if hasattr(bot.search_engine, 'quiescence_depth') else 3,
        },
        "training": {
            "learning_rate": bot.learner.optimizer.param_groups[0]['lr'],
        }
    }

@app.post("/parameters")
def set_parameters(request: ParametersRequest):
    """Update bot parameters in real-time"""
    import chess
    updated = []
    
    # Material values
    if request.pawn_value is not None:
        PIECE_VALUES[chess.PAWN] = request.pawn_value
        updated.append("pawn_value")
    if request.knight_value is not None:
        PIECE_VALUES[chess.KNIGHT] = request.knight_value
        updated.append("knight_value")
    if request.bishop_value is not None:
        PIECE_VALUES[chess.BISHOP] = request.bishop_value
        updated.append("bishop_value")
    if request.rook_value is not None:
        PIECE_VALUES[chess.ROOK] = request.rook_value
        updated.append("rook_value")
    if request.queen_value is not None:
        PIECE_VALUES[chess.QUEEN] = request.queen_value
        updated.append("queen_value")
    
    # Positional
    if request.mobility_weight is not None:
        EVAL_PARAMS["mobility_weight"] = request.mobility_weight
        updated.append("mobility_weight")
    if request.castling_bonus is not None:
        EVAL_PARAMS["castling_bonus"] = request.castling_bonus
        updated.append("castling_bonus")
    if request.king_exposure_penalty is not None:
        EVAL_PARAMS["king_exposure_penalty"] = request.king_exposure_penalty
        updated.append("king_exposure_penalty")
    if request.king_safety_penalty is not None:
        EVAL_PARAMS["king_safety_penalty"] = request.king_safety_penalty
        updated.append("king_safety_penalty")
    if request.rook_open_file is not None:
        EVAL_PARAMS["rook_open_file"] = request.rook_open_file
        updated.append("rook_open_file")
    if request.rook_semi_open is not None:
        EVAL_PARAMS["rook_semi_open"] = request.rook_semi_open
        updated.append("rook_semi_open")
    if request.passed_pawn_scale is not None:
        EVAL_PARAMS["passed_pawn_scale"] = request.passed_pawn_scale
        updated.append("passed_pawn_scale")
    if request.threat_divisor is not None:
        EVAL_PARAMS["threat_divisor"] = request.threat_divisor
        updated.append("threat_divisor")
    if request.lpdo_divisor is not None:
        EVAL_PARAMS["lpdo_divisor"] = request.lpdo_divisor
        updated.append("lpdo_divisor")
    
    # Queen safety and pins
    if request.queen_early_penalty is not None:
        EVAL_PARAMS["queen_early_penalty"] = request.queen_early_penalty
        updated.append("queen_early_penalty")
    if request.queen_exposure_penalty is not None:
        EVAL_PARAMS["queen_exposure_penalty"] = request.queen_exposure_penalty
        updated.append("queen_exposure_penalty")
    if request.pin_penalty is not None:
        EVAL_PARAMS["pin_penalty"] = request.pin_penalty
        updated.append("pin_penalty")
    
    # Neural blend
    if request.neural_blend is not None:
        EVAL_PARAMS["neural_blend"] = max(0, min(100, request.neural_blend))
        updated.append("neural_blend")
    
    # Search
    if request.quiescence_depth is not None:
        bot.search_engine.quiescence_depth = request.quiescence_depth
        updated.append("quiescence_depth")
    
    # Training
    if request.learning_rate is not None:
        for param_group in bot.learner.optimizer.param_groups:
            param_group['lr'] = request.learning_rate
        updated.append("learning_rate")
    
    return {"updated": updated, "count": len(updated)}

