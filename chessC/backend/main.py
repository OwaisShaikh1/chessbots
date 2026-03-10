"""
ChessBot Backend — FastAPI
Endpoints:
  POST /game/start
  POST /move
  POST /engine/evaluate
  POST /bot-battle
  GET  /game/{id}
  GET  /analysis/{id}
  WS   /ws/analysis
"""
import asyncio
import subprocess
import uuid
from typing import Optional

import chess
import chess.pgn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

# ── Engine path ────────────────────────────────────────────────────────────
import pathlib

BASE = pathlib.Path(__file__).parent.parent
ENGINE_PATH = BASE / "engine" / "build" / "chessbot"

app = FastAPI(title="ChessBot API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ── In-memory game store ────────────────────────────────────────────────────
games: dict[str, dict] = {}
# Each game: { board: chess.Board, history: [UCI str], color: str, depth: int }


# ── Pydantic models ─────────────────────────────────────────────────────────
class StartGameRequest(BaseModel):
    color: str = "white"   # 'white' | 'black'
    depth: int = 10

class MoveRequest(BaseModel):
    game_id: str
    move: str              # UCI e.g. 'e2e4'

class EvaluateRequest(BaseModel):
    fen: str
    depth: int = 10

class BotBattleRequest(BaseModel):
    depth_white: int = 6
    depth_black: int = 6
    games: int = 1


# ── UCI engine helper ────────────────────────────────────────────────────────

class UCIEngine:
    """Thin async wrapper around a UCI chess engine subprocess."""

    def __init__(self, path: str):
        self.path = path
        self._proc: Optional[asyncio.subprocess.Process] = None

    async def start(self):
        self._proc = await asyncio.create_subprocess_exec(
            self.path,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL,
        )
        await self._send("uci")
        await self._wait_for("uciok")
        await self._send("isready")
        await self._wait_for("readyok")

    async def _send(self, cmd: str):
        self._proc.stdin.write((cmd + "\n").encode())
        await self._proc.stdin.drain()

    async def _wait_for(self, token: str) -> list[str]:
        lines = []
        while True:
            line = (await self._proc.stdout.readline()).decode().strip()
            lines.append(line)
            if line.startswith(token):
                return lines

    async def search(self, fen: str, depth: int):
        """Return list of 'info' strings + final 'bestmove' string."""
        await self._send(f"position fen {fen}")
        await self._send(f"go depth {depth}")
        lines = []
        while True:
            line = (await self._proc.stdout.readline()).decode().strip()
            lines.append(line)
            if line.startswith("bestmove"):
                return lines

    async def stop(self):
        if self._proc and self._proc.returncode is None:
            self._proc.stdin.write(b"quit\n")
            await self._proc.stdin.drain()
            await self._proc.wait()


def parse_info(line: str) -> Optional[dict]:
    """Parse a UCI info line into a dict."""
    if not line.startswith("info"):
        return None
    tokens = line.split()
    info = {}
    i = 1
    while i < len(tokens):
        key = tokens[i]
        if key == "depth" and i + 1 < len(tokens):
            info["depth"] = int(tokens[i + 1]); i += 2
        elif key == "score" and i + 2 < len(tokens):
            kind = tokens[i + 1]
            val  = int(tokens[i + 2])
            info["score"] = val if kind == "cp" else (30000 if val > 0 else -30000)
            i += 3
        elif key == "nodes" and i + 1 < len(tokens):
            info["nodes"] = int(tokens[i + 1]); i += 2
        elif key == "nps" and i + 1 < len(tokens):
            info["nps"] = int(tokens[i + 1]); i += 2
        elif key == "pv":
            info["pv"] = " ".join(tokens[i + 1:]); i = len(tokens)
        elif key == "time" and i + 1 < len(tokens):
            info["time"] = int(tokens[i + 1]); i += 2
        else:
            i += 1
    return info


# ── Routes ───────────────────────────────────────────────────────────────────

@app.post("/game/start")
async def game_start(req: StartGameRequest):
    game_id = str(uuid.uuid4())[:8]
    board = chess.Board()
    games[game_id] = {
        "board": board,
        "history": [],
        "color": req.color,
        "depth": req.depth,
    }
    response = {"game_id": game_id, "fen": board.fen()}

    # If player is black, engine plays first as white
    if req.color == "black":
        bm = await _engine_move(board.fen(), req.depth)
        if bm:
            board.push_uci(bm)
            games[game_id]["history"].append(bm)
            response["engine_move"] = bm
            response["fen"] = board.fen()

    return response


@app.post("/move")
async def move(req: MoveRequest):
    if req.game_id not in games:
        raise HTTPException(404, "Game not found")
    g = games[req.game_id]
    board: chess.Board = g["board"]

    try:
        board.push_uci(req.move)
    except Exception:
        raise HTTPException(400, f"Illegal move: {req.move}")
    g["history"].append(req.move)

    if board.is_game_over():
        return {"fen": board.fen(), "game_over": True, "result": board.result()}

    # Engine replies
    bm = await _engine_move(board.fen(), g["depth"])
    if bm:
        board.push_uci(bm)
        g["history"].append(bm)

    return {
        "fen": board.fen(),
        "engine_move": bm,
        "game_over": board.is_game_over(),
        "result": board.result() if board.is_game_over() else None,
    }


@app.get("/game/{game_id}")
async def get_game(game_id: str):
    if game_id not in games:
        raise HTTPException(404, "Game not found")
    g = games[game_id]
    board: chess.Board = g["board"]
    game = chess.pgn.Game.from_board(board)
    return {
        "game_id": game_id,
        "fen": board.fen(),
        "pgn": str(game),
        "history": g["history"],
        "game_over": board.is_game_over(),
        "result": board.result() if board.is_game_over() else None,
    }


@app.post("/engine/evaluate")
async def engine_evaluate(req: EvaluateRequest):
    lines = await _engine_search_lines(req.fen, req.depth)
    bestmove = None
    score = 0
    depth = 0
    pv = ""
    for line in lines:
        if line.startswith("bestmove"):
            parts = line.split()
            bestmove = parts[1] if len(parts) > 1 else None
        else:
            info = parse_info(line)
            if info:
                if "score" in info: score = info["score"]
                if "depth" in info: depth = info["depth"]
                if "pv"    in info: pv    = info["pv"]
    return {"bestmove": bestmove, "score": score, "depth": depth, "pv": pv}


@app.get("/analysis/{game_id}")
async def get_analysis(game_id: str):
    if game_id not in games:
        raise HTTPException(404, "Game not found")
    g = games[game_id]
    board: chess.Board = g["board"]
    lines = await _engine_search_lines(board.fen(), g["depth"])
    bestmove, score, depth, pv = None, 0, 0, ""
    for line in lines:
        if line.startswith("bestmove"):
            bestmove = line.split()[1] if len(line.split()) > 1 else None
        else:
            info = parse_info(line)
            if info:
                if "score" in info: score = info["score"]
                if "depth" in info: depth = info["depth"]
                if "pv"    in info: pv    = info["pv"]
    return {"game_id": game_id, "bestmove": bestmove, "score": score, "depth": depth, "pv": pv}


@app.post("/bot-battle")
async def bot_battle(req: BotBattleRequest):
    results = []
    for _ in range(req.games):
        board = chess.Board()
        moves = []
        while not board.is_game_over():
            depth = req.depth_white if board.turn == chess.WHITE else req.depth_black
            bm = await _engine_move(board.fen(), depth)
            if not bm:
                break
            board.push_uci(bm)
            moves.append(bm)
        results.append({"result": board.result(), "moves": moves})
    return {"games": results}


# ── WebSocket analysis stream ─────────────────────────────────────────────

@app.websocket("/ws/analysis")
async def ws_analysis(ws: WebSocket):
    await ws.accept()
    try:
        data = await ws.receive_json()
        fen   = data.get("fen", chess.STARTING_FEN)
        depth = int(data.get("depth", 10))

        lines = await _engine_search_lines(fen, depth)
        for line in lines:
            if line.startswith("bestmove"):
                parts = line.split()
                await ws.send_json({"type": "bestmove", "move": parts[1] if len(parts) > 1 else None})
            else:
                info = parse_info(line)
                if info:
                    await ws.send_json({"type": "info", **info})
    except WebSocketDisconnect:
        pass
    except Exception as e:
        try:
            await ws.send_json({"type": "error", "message": str(e)})
        except Exception:
            pass


# ── Engine subprocess helpers ─────────────────────────────────────────────

async def _engine_search_lines(fen: str, depth: int) -> list[str]:
    """Run engine synchronously in a thread pool to avoid blocking."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, _sync_engine_search, fen, depth)


def _sync_engine_search(fen: str, depth: int) -> list[str]:
    """Blocking call to engine binary via UCI."""
    if not ENGINE_PATH.exists():
        return [f"bestmove (none)"]
    try:
        proc = subprocess.Popen(
            [str(ENGINE_PATH)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        cmds = "uci\nisready\n" + f"position fen {fen}\ngo depth {depth}\n"
        proc.stdin.write(cmds)
        proc.stdin.flush()

        lines = []
        while True:
            line = proc.stdout.readline().strip()
            if not line:
                continue
            if line in ("uciok", "readyok"):
                continue
            lines.append(line)
            if line.startswith("bestmove"):
                break

        proc.stdin.write("quit\n")
        proc.stdin.flush()
        proc.wait(timeout=2)
        return lines
    except Exception as e:
        return [f"bestmove (none)"]


async def _engine_move(fen: str, depth: int) -> Optional[str]:
    lines = await _engine_search_lines(fen, depth)
    for line in lines:
        if line.startswith("bestmove"):
            parts = line.split()
            return parts[1] if len(parts) > 1 and parts[1] != "(none)" else None
    return None
