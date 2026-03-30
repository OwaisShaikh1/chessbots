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
import datetime
import json
import logging
import platform
import subprocess
import time
import uuid
from typing import Any, Awaitable, Callable, Optional

import chess
import chess.pgn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

logger = logging.getLogger("chessbot.arena")

# ── Engine path ────────────────────────────────────────────────────────────
import pathlib

BASE = pathlib.Path(__file__).parent.parent
ENGINE_BUILD_DIR = BASE / "engine" / "build"
ENGINES_DIR = BASE / "engines"
BOTS_DIR = BASE / "bots"
ACTIVE_BOT_DIR = BOTS_DIR / "active"
PST_CONFIG_PATH = BASE / "backend" / "pst_config.json"
NOTES_DIR = BASE / "notes"
POSITION_SETS_DIR = NOTES_DIR / "position_sets"
ARENA_LOGS_DIR = NOTES_DIR / "arena_logs"

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
    movetime_ms: int = 0
    engine_id: Optional[str] = None

class MoveRequest(BaseModel):
    game_id: str
    move: str              # UCI e.g. 'e2e4'

class EvaluateRequest(BaseModel):
    fen: str
    depth: int = 10
    movetime_ms: int = 0
    engine_id: Optional[str] = None

class BotBattleRequest(BaseModel):
    depth_white: int = 6
    depth_black: int = 6
    movetime_white_ms: int = 0
    movetime_black_ms: int = 0
    games: int = 1
    engine_white_id: Optional[str] = None
    engine_black_id: Optional[str] = None
    position_set: Optional[str] = None
    mirror_positions: bool = True
    limit_positions: int = 500
    max_plies: int = 300
    return_games_in_response: bool = False


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


def _extract_search_summary(lines: list[str]) -> dict[str, Any]:
    bestmove: Optional[str] = None
    depth = 0
    score = 0
    pv = ""
    time_ms = 0

    for line in lines:
        if line.startswith("bestmove"):
            parts = line.split()
            bestmove = parts[1] if len(parts) > 1 and parts[1] != "(none)" else None
            continue

        info = parse_info(line)
        if not info:
            continue

        if "depth" in info:
            depth = info["depth"]
        if "score" in info:
            score = info["score"]
        if "pv" in info:
            pv = info["pv"]
        if "time" in info:
            time_ms = info["time"]

    return {
        "bestmove": bestmove,
        "depth": depth,
        "score": score,
        "pv": pv,
        "time_ms": time_ms,
    }


def _safe_set_id(raw: str) -> str:
    return "".join(ch for ch in raw if ch.isalnum() or ch in ("_", "-", "."))


def _position_set_file_path(set_id: str) -> pathlib.Path:
    clean = _safe_set_id(set_id)
    if not clean:
        raise HTTPException(400, "Invalid position_set id")

    candidates = [
        POSITION_SETS_DIR / f"{clean}.txt",
        POSITION_SETS_DIR / clean,
    ]
    for path in candidates:
        if path.exists() and path.is_file():
            return path

    raise HTTPException(404, f"Position set '{set_id}' not found")


def _load_position_set(set_id: str, limit_positions: int) -> list[str]:
    path = _position_set_file_path(set_id)
    fens: list[str] = []

    with path.open("r", encoding="utf-8") as f:
        for line in f:
            fen = line.strip()
            if not fen:
                continue
            try:
                chess.Board(fen)
            except Exception:
                continue
            fens.append(fen)
            if len(fens) >= max(1, int(limit_positions)):
                break

    return fens


def _list_position_sets() -> list[dict[str, Any]]:
    if not POSITION_SETS_DIR.exists():
        return []

    entries: list[dict[str, Any]] = []
    for path in sorted(POSITION_SETS_DIR.glob("*.txt"), key=lambda p: p.name.lower()):
        count = 0
        try:
            with path.open("r", encoding="utf-8") as f:
                for line in f:
                    if line.strip():
                        count += 1
        except Exception:
            count = 0

        entries.append({
            "id": path.stem,
            "filename": path.name,
            "count": count,
            "path": str(path),
        })
    return entries


def _winner_label(result: str) -> str:
    if result == "1-0":
        return "white"
    if result == "0-1":
        return "black"
    if result == "1/2-1/2":
        return "draw"
    return "unknown"


def _ensure_arena_dirs() -> None:
    POSITION_SETS_DIR.mkdir(parents=True, exist_ok=True)
    ARENA_LOGS_DIR.mkdir(parents=True, exist_ok=True)


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
        "movetime_ms": req.movetime_ms,
        "engine_id": req.engine_id,
    }
    response = {"game_id": game_id, "fen": board.fen()}

    # If player is black, engine plays first as white
    if req.color == "black":
        move_info = await _engine_move_info(board.fen(), req.depth, req.engine_id, req.movetime_ms)
        bm = move_info["bestmove"]
        if bm:
            board.push_uci(bm)
            games[game_id]["history"].append(bm)
            response["engine_move"] = bm
            response["engine_depth"] = move_info["depth"]
            response["engine_time_ms"] = move_info["time_ms"]
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
    move_info = await _engine_move_info(board.fen(), g["depth"], g.get("engine_id"), g.get("movetime_ms", 0))
    bm = move_info["bestmove"]
    if not _is_legal_uci(board, bm):
        bm = _first_legal_uci(board)
    if bm:
        board.push_uci(bm)
        g["history"].append(bm)

    return {
        "fen": board.fen(),
        "engine_move": bm,
        "engine_depth": move_info["depth"],
        "engine_time_ms": move_info["time_ms"],
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
    lines = await _engine_search_lines(req.fen, req.depth, req.engine_id, req.movetime_ms)
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
    lines = await _engine_search_lines(board.fen(), g["depth"], g.get("engine_id"), g.get("movetime_ms", 0))
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
    return await _run_bot_battle_session(req)


async def _run_bot_battle_session(
    req: BotBattleRequest,
    progress_cb: Optional[Callable[[dict[str, Any]], Awaitable[None]]] = None,
) -> dict[str, Any]:
    _resolve_engine_path(req.engine_white_id)
    _resolve_engine_path(req.engine_black_id)

    async def play_single_game(
        initial_fen: str,
        white_engine_id: Optional[str],
        black_engine_id: Optional[str],
        depth_white: int,
        depth_black: int,
        movetime_white_ms: int,
        movetime_black_ms: int,
        max_plies: int,
        pair_index: int,
        leg: int,
    ) -> dict[str, Any]:
        board = chess.Board(initial_fen)
        moves: list[str] = []
        move_log: list[dict[str, Any]] = []

        await emit({
            "type": "game_start",
            "pair_index": pair_index,
            "leg": leg,
            "initial_fen": initial_fen,
            "white_engine_id": white_engine_id,
            "black_engine_id": black_engine_id,
        })

        for ply in range(1, max(1, int(max_plies)) + 1):
            if board.is_game_over():
                break

            white_to_move = board.turn == chess.WHITE
            depth = depth_white if white_to_move else depth_black
            movetime_ms = movetime_white_ms if white_to_move else movetime_black_ms
            engine_id = white_engine_id if white_to_move else black_engine_id

            move_info = await _engine_move_info(board.fen(), depth, engine_id, movetime_ms)
            bm = move_info["bestmove"]
            used_fallback = False

            if not _is_legal_uci(board, bm):
                bm = _first_legal_uci(board)
                used_fallback = True

            if not bm:
                break

            board.push_uci(bm)
            moves.append(bm)
            move_entry = {
                "ply": ply,
                "turn": "white" if white_to_move else "black",
                "engine_id": engine_id,
                "depth_requested": depth,
                "movetime_ms_requested": movetime_ms,
                "engine_depth": move_info.get("depth", 0),
                "engine_time_ms": move_info.get("time_ms", 0),
                "score_cp": move_info.get("score", 0),
                "move": bm,
                "fallback_used": used_fallback,
            }
            move_log.append(move_entry)
            await emit({
                "type": "move",
                "pair_index": pair_index,
                "leg": leg,
                "ply": ply,
                "turn": move_entry["turn"],
                "move": bm,
                "fen": board.fen(),
                "engine_id": engine_id,
                "engine_depth": move_entry["engine_depth"],
                "engine_time_ms": move_entry["engine_time_ms"],
                "score_cp": move_entry["score_cp"],
            })

        result = board.result() if board.is_game_over() else "*"
        return {
            "initial_fen": initial_fen,
            "final_fen": board.fen(),
            "result": result,
            "winner": _winner_label(result),
            "plies": len(moves),
            "moves": moves,
            "move_log": move_log,
        }

    _ensure_arena_dirs()

    white_depth = max(1, int(req.depth_white))
    black_depth = max(1, int(req.depth_black))
    white_time = max(0, int(req.movetime_white_ms))
    black_time = max(0, int(req.movetime_black_ms))

    games_out: list[dict[str, Any]] = []
    session_id = uuid.uuid4().hex[:12]
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

    async def emit(payload: dict[str, Any]) -> None:
        if progress_cb:
            await progress_cb(payload)

    if req.position_set:
        base_fens = _load_position_set(req.position_set, req.limit_positions)
        if not base_fens:
            raise HTTPException(400, f"Position set '{req.position_set}' is empty")

        planned_total_games = len(base_fens) * (2 if req.mirror_positions else 1)
        await emit({
            "type": "started",
            "mode": "fixed_positions",
            "total_games": planned_total_games,
            "position_set": req.position_set,
        })

        for index, fen in enumerate(base_fens, start=1):
            g1 = await play_single_game(
                initial_fen=fen,
                white_engine_id=req.engine_white_id,
                black_engine_id=req.engine_black_id,
                depth_white=white_depth,
                depth_black=black_depth,
                movetime_white_ms=white_time,
                movetime_black_ms=black_time,
                max_plies=req.max_plies,
                pair_index=index,
                leg=1,
            )
            g1["pair_index"] = index
            g1["leg"] = 1
            g1["white_engine_id"] = req.engine_white_id
            g1["black_engine_id"] = req.engine_black_id
            games_out.append(g1)
            await emit({
                "type": "progress",
                "completed_games": len(games_out),
                "total_games": planned_total_games,
                "pair_index": index,
                "leg": 1,
                "result": g1["result"],
                "winner": g1["winner"],
                "white_engine_id": g1["white_engine_id"],
                "black_engine_id": g1["black_engine_id"],
                "winner_engine_id": (
                    g1["white_engine_id"] if g1["winner"] == "white"
                    else g1["black_engine_id"] if g1["winner"] == "black"
                    else None
                ),
            })

            if req.mirror_positions:
                g2 = await play_single_game(
                    initial_fen=fen,
                    white_engine_id=req.engine_black_id,
                    black_engine_id=req.engine_white_id,
                    depth_white=black_depth,
                    depth_black=white_depth,
                    movetime_white_ms=black_time,
                    movetime_black_ms=white_time,
                    max_plies=req.max_plies,
                    pair_index=index,
                    leg=2,
                )
                g2["pair_index"] = index
                g2["leg"] = 2
                g2["white_engine_id"] = req.engine_black_id
                g2["black_engine_id"] = req.engine_white_id
                games_out.append(g2)
                await emit({
                    "type": "progress",
                    "completed_games": len(games_out),
                    "total_games": planned_total_games,
                    "pair_index": index,
                    "leg": 2,
                    "result": g2["result"],
                    "winner": g2["winner"],
                    "white_engine_id": g2["white_engine_id"],
                    "black_engine_id": g2["black_engine_id"],
                    "winner_engine_id": (
                        g2["white_engine_id"] if g2["winner"] == "white"
                        else g2["black_engine_id"] if g2["winner"] == "black"
                        else None
                    ),
                })
    else:
        planned_total_games = max(1, int(req.games))
        await emit({
            "type": "started",
            "mode": "startpos_random",
            "total_games": planned_total_games,
        })

        for idx in range(planned_total_games):
            pair_index = idx + 1
            g = await play_single_game(
                initial_fen=chess.STARTING_FEN,
                white_engine_id=req.engine_white_id,
                black_engine_id=req.engine_black_id,
                depth_white=white_depth,
                depth_black=black_depth,
                movetime_white_ms=white_time,
                movetime_black_ms=black_time,
                max_plies=req.max_plies,
                pair_index=pair_index,
                leg=1,
            )
            g["pair_index"] = pair_index
            g["leg"] = 1
            g["white_engine_id"] = req.engine_white_id
            g["black_engine_id"] = req.engine_black_id
            games_out.append(g)
            await emit({
                "type": "progress",
                "completed_games": len(games_out),
                "total_games": planned_total_games,
                "pair_index": pair_index,
                "leg": 1,
                "result": g["result"],
                "winner": g["winner"],
                "white_engine_id": g["white_engine_id"],
                "black_engine_id": g["black_engine_id"],
                "winner_engine_id": (
                    g["white_engine_id"] if g["winner"] == "white"
                    else g["black_engine_id"] if g["winner"] == "black"
                    else None
                ),
            })

    summary = {
        "total_games": len(games_out),
        "white_wins": 0,
        "black_wins": 0,
        "draws": 0,
        "unfinished": 0,
        "by_engine": {},
    }

    tracked_engine_ids = [req.engine_white_id or "default", req.engine_black_id or "default"]
    for engine_id in tracked_engine_ids:
        if engine_id not in summary["by_engine"]:
            summary["by_engine"][engine_id] = {"wins": 0, "losses": 0, "draws": 0, "games": 0}

    for g in games_out:
        winner = g["winner"]
        white_id = g.get("white_engine_id") or "default"
        black_id = g.get("black_engine_id") or "default"

        summary["by_engine"].setdefault(white_id, {"wins": 0, "losses": 0, "draws": 0, "games": 0})
        summary["by_engine"].setdefault(black_id, {"wins": 0, "losses": 0, "draws": 0, "games": 0})
        summary["by_engine"][white_id]["games"] += 1
        summary["by_engine"][black_id]["games"] += 1

        if winner == "white":
            summary["white_wins"] += 1
            summary["by_engine"][white_id]["wins"] += 1
            summary["by_engine"][black_id]["losses"] += 1
        elif winner == "black":
            summary["black_wins"] += 1
            summary["by_engine"][black_id]["wins"] += 1
            summary["by_engine"][white_id]["losses"] += 1
        elif winner == "draw":
            summary["draws"] += 1
            summary["by_engine"][white_id]["draws"] += 1
            summary["by_engine"][black_id]["draws"] += 1
        else:
            summary["unfinished"] += 1

    session_log = {
        "schema": "arena_session_v1",
        "session_id": session_id,
        "timestamp": datetime.datetime.now().isoformat(timespec="seconds"),
        "mode": "fixed_positions" if req.position_set else "startpos_random",
        "request": req.model_dump(),
        "settings": {
            "engine_white_id": req.engine_white_id,
            "engine_black_id": req.engine_black_id,
            "depth_white": white_depth,
            "depth_black": black_depth,
            "movetime_white_ms": white_time,
            "movetime_black_ms": black_time,
            "position_set": req.position_set,
            "mirror_positions": req.mirror_positions,
            "limit_positions": req.limit_positions,
            "max_plies": req.max_plies,
        },
        "summary": summary,
        "games": games_out,
    }

    log_path = ARENA_LOGS_DIR / f"arena_{ts}_{session_id}.json"
    log_path.write_text(json.dumps(session_log, indent=2), encoding="utf-8")

    response: dict[str, Any] = {
        "session_id": session_id,
        "log_file": str(log_path),
        "summary": summary,
        "total_games": len(games_out),
    }
    if req.return_games_in_response:
        response["games"] = games_out
    return response


@app.websocket("/ws/bot-battle")
async def ws_bot_battle(ws: WebSocket):
    await ws.accept()
    try:
        await ws.send_json({"type": "ready"})
        payload = await asyncio.wait_for(ws.receive_json(), timeout=15)
        req = BotBattleRequest(**payload)
        logger.info(
            "ws-bot-battle payload received: position_set=%s mirror=%s limit=%s white=%s black=%s",
            req.position_set,
            req.mirror_positions,
            req.limit_positions,
            req.engine_white_id or "default",
            req.engine_black_id or "default",
        )

        event_counts: dict[str, int] = {}

        async def send_progress(event: dict[str, Any]) -> None:
            event_type = str(event.get("type", "unknown"))
            event_counts[event_type] = event_counts.get(event_type, 0) + 1
            if event_type != "move" or event_counts[event_type] % 100 == 1:
                logger.info(
                    "ws-bot-battle emit: type=%s pair=%s leg=%s completed=%s total=%s",
                    event_type,
                    event.get("pair_index"),
                    event.get("leg"),
                    event.get("completed_games"),
                    event.get("total_games"),
                )
            await ws.send_json(event)

        response = await _run_bot_battle_session(req, progress_cb=send_progress)
        logger.info(
            "ws-bot-battle done: total_games=%s counts=%s",
            response.get("total_games"),
            event_counts,
        )
        await ws.send_json({"type": "done", **response})
    except WebSocketDisconnect:
        logger.info("ws-bot-battle disconnected")
        pass
    except asyncio.TimeoutError:
        try:
            await ws.send_json({"type": "error", "message": "No battle payload received from client"})
        except Exception:
            pass
        logger.warning("ws-bot-battle timeout waiting for client payload")
    except Exception as e:
        logger.exception("ws-bot-battle failed: %s", e)
        try:
            await ws.send_json({"type": "error", "message": str(e)})
        except Exception:
            pass


@app.get("/position-sets")
async def get_position_sets():
    return {"sets": _list_position_sets()}


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
        movetime_ms = int(data.get("movetime_ms", 0))
        engine_id = data.get("engine_id")

        lines = await _engine_search_lines(fen, depth, engine_id, movetime_ms)
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

async def _engine_search_lines(
    fen: str,
    depth: int,
    engine_id: Optional[str] = None,
    movetime_ms: int = 0,
) -> list[str]:
    """Run engine synchronously in a thread pool to avoid blocking."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, _sync_engine_search, fen, depth, engine_id, movetime_ms)


def _sync_engine_search(
    fen: str,
    depth: int,
    engine_id: Optional[str] = None,
    movetime_ms: int = 0,
) -> list[str]:
    """Blocking call to engine binary via UCI."""
    proc: Optional[subprocess.Popen] = None
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
        safe_depth = max(1, int(depth))
        safe_movetime_ms = max(0, int(movetime_ms))
        go_cmd = f"go depth {safe_depth}"
        if safe_movetime_ms > 0:
            go_cmd += f" movetime {safe_movetime_ms}"
        cmds = (
            "uci\n"
            + f"setoption name PstFile value {pst_path}\n"
            + "setoption name ReloadPst\n"
            + "isready\n"
            + f"position fen {fen}\n"
            + go_cmd + "\n"
            + "quit\n"
        )
        timeout_s = max(1.0, (safe_movetime_ms / 1000.0) + 1.0)
        out, _ = proc.communicate(cmds, timeout=timeout_s)

        lines: list[str] = []
        for raw in out.splitlines():
            line = raw.strip()
            if not line:
                continue
            if line in ("uciok", "readyok"):
                continue
            lines.append(line)
            if line.startswith("bestmove"):
                break

        if not any(line.startswith("bestmove") for line in lines):
            lines.append("bestmove (none)")
        return lines
    except subprocess.TimeoutExpired:
        if proc is not None:
            try:
                proc.kill()
            except Exception:
                pass
        return ["bestmove (none)"]
    except Exception as e:
        return [f"bestmove (none)"]


async def _engine_move(
    fen: str,
    depth: int,
    engine_id: Optional[str] = None,
    movetime_ms: int = 0,
) -> Optional[str]:
    info = await _engine_move_info(fen, depth, engine_id, movetime_ms)
    return info["bestmove"]


async def _engine_move_info(
    fen: str,
    depth: int,
    engine_id: Optional[str] = None,
    movetime_ms: int = 0,
) -> dict[str, Any]:
    lines = await _engine_search_lines(fen, depth, engine_id, movetime_ms)
    return _extract_search_summary(lines)
