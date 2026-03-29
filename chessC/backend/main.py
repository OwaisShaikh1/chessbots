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
import json
import platform
import subprocess
import uuid
from typing import Any, Optional

import chess
import chess.pgn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

# ── Engine path ────────────────────────────────────────────────────────────
import pathlib

BASE = pathlib.Path(__file__).parent.parent
ENGINE_BUILD_DIR = BASE / "engine" / "build"
ENGINES_DIR = BASE / "engines"
BOTS_DIR = BASE / "bots"
ACTIVE_BOT_DIR = BOTS_DIR / "active"
PST_CONFIG_PATH = BASE / "backend" / "pst_config.json"

PST_KEYS = ("p", "n", "b", "r", "q", "k_open", "k_end")


def _default_pst_config() -> dict[str, list[int]]:
    return {
        "p": [
             0,  0,  0,  0,  0,  0,  0,  0,
            50, 50, 50, 50, 50, 50, 50, 50,
            10, 10, 20, 30, 30, 20, 10, 10,
             5,  5, 10, 25, 25, 10,  5,  5,
             0,  0,  0, 20, 20,  0,  0,  0,
             5, -5,-10,  0,  0,-10, -5,  5,
             5, 10, 10,-20,-20, 10, 10,  5,
             0,  0,  0,  0,  0,  0,  0,  0,
        ],
        "n": [
            -50,-40,-30,-30,-30,-30,-40,-50,
            -40,-20,  0,  0,  0,  0,-20,-40,
            -30,  0, 10, 15, 15, 10,  0,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  0, 15, 20, 20, 15,  0,-30,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -40,-20,  0,  5,  5,  0,-20,-40,
            -50,-40,-30,-30,-30,-30,-40,-50,
        ],
        "b": [
            -20,-10,-10,-10,-10,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5, 10, 10,  5,  0,-10,
            -10,  5,  5, 10, 10,  5,  5,-10,
            -10,  0, 10, 10, 10, 10,  0,-10,
            -10, 10, 10, 10, 10, 10, 10,-10,
            -10,  5,  0,  0,  0,  0,  5,-10,
            -20,-10,-10,-10,-10,-10,-10,-20,
        ],
        "r": [
             0,  0,  0,  0,  0,  0,  0,  0,
             5, 10, 10, 10, 10, 10, 10,  5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
             0,  0,  0,  5,  5,  0,  0,  0,
        ],
        "q": [
            -20,-10,-10, -5, -5,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5,  5,  5,  5,  0,-10,
             -5,  0,  5,  5,  5,  5,  0, -5,
              0,  0,  5,  5,  5,  5,  0, -5,
            -10,  5,  5,  5,  5,  5,  0,-10,
            -10,  0,  5,  0,  0,  0,  0,-10,
            -20,-10,-10, -5, -5,-10,-10,-20,
        ],
        "k_open": [
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -20,-30,-30,-40,-40,-30,-30,-20,
            -10,-20,-20,-20,-20,-20,-20,-10,
             20, 20,  0,  0,  0,  0, 20, 20,
             20, 30, 10,  0,  0, 10, 30, 20,
        ],
        "k_end": [
            -50,-40,-30,-20,-20,-30,-40,-50,
            -30,-20,-10,  0,  0,-10,-20,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-30,  0,  0,  0,  0,-30,-30,
            -50,-30,-30,-30,-30,-30,-30,-50,
        ],
    }


def _clamp_pst_value(value: Any) -> int:
    try:
        numeric = int(round(float(value)))
    except Exception:
        return 0
    return max(-5000, min(5000, numeric))


def _sanitize_pst_table(values: Any) -> list[int]:
    table = values if isinstance(values, list) else []
    out = [_clamp_pst_value(v) for v in table[:64]]
    while len(out) < 64:
        out.append(0)
    return out


def _sanitize_pst_config(payload: dict[str, Any]) -> dict[str, list[int]]:
    defaults = _default_pst_config()
    clean: dict[str, list[int]] = {}
    for key in PST_KEYS:
        source = payload.get(key, defaults[key])
        clean[key] = _sanitize_pst_table(source)
    return clean


def _load_pst_config() -> dict[str, list[int]]:
    if not PST_CONFIG_PATH.exists():
        return _default_pst_config()
    try:
        raw = json.loads(PST_CONFIG_PATH.read_text(encoding="utf-8"))
        if not isinstance(raw, dict):
            return _default_pst_config()
        return _sanitize_pst_config(raw)
    except Exception:
        return _default_pst_config()


def _save_pst_config(config: dict[str, Any]) -> dict[str, list[int]]:
    clean = _sanitize_pst_config(config)
    PST_CONFIG_PATH.write_text(json.dumps(clean, indent=2), encoding="utf-8")
    return clean

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
    engine_id: Optional[str] = None

class MoveRequest(BaseModel):
    game_id: str
    move: str              # UCI e.g. 'e2e4'

class EvaluateRequest(BaseModel):
    fen: str
    depth: int = 10
    engine_id: Optional[str] = None

class BotBattleRequest(BaseModel):
    depth_white: int = 6
    depth_black: int = 6
    games: int = 1
    engine_white_id: Optional[str] = None
    engine_black_id: Optional[str] = None


class PstConfigRequest(BaseModel):
    config: dict[str, list[int]]


def _engine_binary_names() -> list[str]:
    if platform.system().lower().startswith("win"):
        return ["chessbot.exe", "chessbot"]
    return ["chessbot", "chessbot.exe"]


def _find_engine_binary(directory: pathlib.Path) -> Optional[pathlib.Path]:
    for name in _engine_binary_names():
        candidate = directory / name
        if candidate.exists():
            return candidate
    return None


def _find_named_engine_binary(directory: pathlib.Path, engine_id: str) -> Optional[pathlib.Path]:
    candidates = [directory / f"{engine_id}.exe", directory / engine_id]
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate
    return None


def _collect_engine_entries() -> list[dict[str, Optional[str]]]:
    entries: list[dict[str, Optional[str]]] = []
    seen_ids: set[str] = set()

    default_engine = _find_engine_binary(ACTIVE_BOT_DIR)
    default_path = str(ACTIVE_BOT_DIR)
    if not default_engine:
        default_engine = _find_engine_binary(ENGINE_BUILD_DIR)
        default_path = str(ENGINE_BUILD_DIR)
    entries.append(
        {
            "id": "default",
            "engine_id": None,
            "exists": default_engine is not None,
            "path": str(default_engine) if default_engine else default_path,
            "source": "default",
        }
    )

    def add_entry(engine_id: str, engine_path: Optional[pathlib.Path], source: str, fallback_path: pathlib.Path):
        if engine_id in seen_ids:
            return
        seen_ids.add(engine_id)
        entries.append(
            {
                "id": engine_id,
                "engine_id": engine_id,
                "exists": engine_path is not None,
                "path": str(engine_path) if engine_path else str(fallback_path),
                "source": source,
            }
        )

    if BOTS_DIR.exists():
        for entry in sorted(BOTS_DIR.iterdir(), key=lambda p: p.name.lower()):
            if entry.name == "active":
                continue
            if entry.is_dir():
                engine_path = _find_engine_binary(entry)
                if engine_path:
                    add_entry(entry.name, engine_path, "bots", entry)

                for nested in sorted(entry.iterdir(), key=lambda p: p.name.lower()):
                    if nested.is_file() and nested.suffix.lower() == ".exe":
                        if engine_path and nested == engine_path:
                            continue
                        nested_id = f"{entry.name}.{nested.stem}"
                        add_entry(nested_id, nested, "bots", nested)
            elif entry.is_file() and entry.suffix.lower() == ".exe":
                add_entry(entry.stem, entry, "bots", entry)

    if ENGINES_DIR.exists():
        for entry in sorted(ENGINES_DIR.iterdir(), key=lambda p: p.name.lower()):
            if not entry.is_dir():
                continue
            engine_path = _find_engine_binary(entry)
            add_entry(entry.name, engine_path, "engines", entry)

    return entries


def _validate_engine_id(engine_id: str) -> str:
    if not engine_id:
        raise HTTPException(400, "Engine id cannot be empty")
    for ch in engine_id:
        if not (ch.isalnum() or ch in ("_", "-", ".")):
            raise HTTPException(400, "Engine id contains invalid characters")
    return engine_id


def _resolve_engine_path(engine_id: Optional[str]) -> pathlib.Path:
    if not engine_id:
        default_engine = _find_engine_binary(ACTIVE_BOT_DIR)
        if not default_engine:
            default_engine = _find_engine_binary(ENGINE_BUILD_DIR)
        if not default_engine:
            raise HTTPException(500, "Default engine binary not found in bots/active or engine/build")
        return default_engine

    safe_id = _validate_engine_id(engine_id)

    bots_dir_engine = _find_engine_binary(BOTS_DIR / safe_id)
    if bots_dir_engine:
        return bots_dir_engine

    bots_named_file = _find_named_engine_binary(BOTS_DIR, safe_id)
    if bots_named_file:
        return bots_named_file

    engines_dir_engine = _find_engine_binary(ENGINES_DIR / safe_id)
    if engines_dir_engine:
        return engines_dir_engine

    for entry in _collect_engine_entries():
        if entry.get("engine_id") == safe_id and entry.get("exists") and entry.get("path"):
            return pathlib.Path(str(entry["path"]))

    raise HTTPException(404, f"Engine '{safe_id}' not found in bots/ or engines/")


def _list_engines() -> list[dict]:
    return _collect_engine_entries()


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


def _is_legal_uci(board: chess.Board, move_uci: Optional[str]) -> bool:
    if not move_uci:
        return False
    try:
        move = chess.Move.from_uci(move_uci)
    except Exception:
        return False
    return move in board.legal_moves


def _first_legal_uci(board: chess.Board) -> Optional[str]:
    try:
        move = next(iter(board.legal_moves), None)
    except Exception:
        move = None
    return move.uci() if move else None


# ── Routes ───────────────────────────────────────────────────────────────────

@app.post("/game/start")
async def game_start(req: StartGameRequest):
    _resolve_engine_path(req.engine_id)
    game_id = str(uuid.uuid4())[:8]
    board = chess.Board()
    games[game_id] = {
        "board": board,
        "history": [],
        "color": req.color,
        "depth": req.depth,
        "engine_id": req.engine_id,
    }
    response = {"game_id": game_id, "fen": board.fen()}

    # If player is black, engine plays first as white
    if req.color == "black":
        bm = await _engine_move(board.fen(), req.depth, req.engine_id)
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
    bm = await _engine_move(board.fen(), g["depth"], g.get("engine_id"))
    if not _is_legal_uci(board, bm):
        bm = _first_legal_uci(board)
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
    lines = await _engine_search_lines(req.fen, req.depth, req.engine_id)
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
    try:
        board = chess.Board(req.fen)
        if not _is_legal_uci(board, bestmove):
            bestmove = _first_legal_uci(board)
    except Exception:
        pass
    return {"bestmove": bestmove, "score": score, "depth": depth, "pv": pv}


@app.get("/analysis/{game_id}")
async def get_analysis(game_id: str):
    if game_id not in games:
        raise HTTPException(404, "Game not found")
    g = games[game_id]
    board: chess.Board = g["board"]
    lines = await _engine_search_lines(board.fen(), g["depth"], g.get("engine_id"))
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
    _resolve_engine_path(req.engine_white_id)
    _resolve_engine_path(req.engine_black_id)
    results = []
    for _ in range(req.games):
        board = chess.Board()
        moves = []
        while not board.is_game_over():
            depth = req.depth_white if board.turn == chess.WHITE else req.depth_black
            engine_id = req.engine_white_id if board.turn == chess.WHITE else req.engine_black_id
            bm = await _engine_move(board.fen(), depth, engine_id)
            if not _is_legal_uci(board, bm):
                bm = _first_legal_uci(board)
            if not bm:
                break
            board.push_uci(bm)
            moves.append(bm)
        results.append({"result": board.result(), "moves": moves})
    return {"games": results}


@app.get("/engines")
async def get_engines():
    return {"engines": _list_engines()}


@app.get("/pst-config")
async def get_pst_config():
    return {"config": _load_pst_config()}


@app.post("/pst-config")
async def save_pst_config(req: PstConfigRequest):
    saved = _save_pst_config(req.config)
    return {"ok": True, "config": saved, "path": str(PST_CONFIG_PATH)}


# ── WebSocket analysis stream ─────────────────────────────────────────────

@app.websocket("/ws/analysis")
async def ws_analysis(ws: WebSocket):
    await ws.accept()
    try:
        data = await ws.receive_json()
        fen   = data.get("fen", chess.STARTING_FEN)
        depth = int(data.get("depth", 10))
        engine_id = data.get("engine_id")

        lines = await _engine_search_lines(fen, depth, engine_id)
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

async def _engine_search_lines(fen: str, depth: int, engine_id: Optional[str] = None) -> list[str]:
    """Run engine synchronously in a thread pool to avoid blocking."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, _sync_engine_search, fen, depth, engine_id)


def _sync_engine_search(fen: str, depth: int, engine_id: Optional[str] = None) -> list[str]:
    """Blocking call to engine binary via UCI."""
    try:
        engine_path = _resolve_engine_path(engine_id)
        proc = subprocess.Popen(
            [str(engine_path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        pst_path = str(PST_CONFIG_PATH).replace("\\", "/")
        cmds = (
            "uci\n"
            + f"setoption name PstFile value {pst_path}\n"
            + "setoption name ReloadPst\n"
            + "isready\n"
            + f"position fen {fen}\n"
            + f"go depth {depth}\n"
        )
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


async def _engine_move(fen: str, depth: int, engine_id: Optional[str] = None) -> Optional[str]:
    lines = await _engine_search_lines(fen, depth, engine_id)
    for line in lines:
        if line.startswith("bestmove"):
            parts = line.split()
            return parts[1] if len(parts) > 1 and parts[1] != "(none)" else None
    return None
