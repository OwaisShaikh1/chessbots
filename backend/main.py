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

@app.post("/battle-stream")
def battle_stream(request: BattleRequest):
    return StreamingResponse(
        bot.battle_stockfish_gen(
            request.iterations, 
            request.engine_path, 
            request.elo,
            request.fen,
            request.depth,
            request.bot_side
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
