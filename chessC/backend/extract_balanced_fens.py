import argparse
from io import StringIO
from pathlib import Path

import chess
import chess.engine
import chess.pgn


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Extract roughly equal opening FENs from game records using Stockfish."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("../notes/Games.txt"),
        help="Path to games text file (one game per line as SAN movetext).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("../notes/equal_opening_fens_depth10.txt"),
        help="Output txt path for filtered FEN positions.",
    )
    parser.add_argument(
        "--stockfish",
        type=Path,
        default=Path("../bots/stockfish.exe"),
        help="Path to Stockfish binary.",
    )
    parser.add_argument(
        "--depth",
        type=int,
        default=10,
        help="Stockfish search depth.",
    )
    parser.add_argument(
        "--min-ply",
        type=int,
        default=10,
        help="Minimum ply to evaluate (5 full moves = 10 ply).",
    )
    parser.add_argument(
        "--max-ply",
        type=int,
        default=12,
        help="Maximum ply to evaluate (6 full moves = 12 ply).",
    )
    parser.add_argument(
        "--cp-threshold",
        type=int,
        default=200,
        help="Absolute centipawn threshold for roughly equal positions. 200 = [-2, +2].",
    )
    parser.add_argument(
        "--keep-duplicates",
        action="store_true",
        help="If set, keep duplicate FENs instead of unique set.",
    )
    return parser.parse_args()


def parse_game_line(line: str) -> chess.pgn.Game | None:
    line = line.strip()
    if not line:
        return None
    return chess.pgn.read_game(StringIO(line))


def board_after_ply(moves: list[chess.Move], ply: int) -> chess.Board:
    board = chess.Board()
    for move in moves[:ply]:
        board.push(move)
    return board


def evaluate_cp(engine: chess.engine.SimpleEngine, board: chess.Board, depth: int) -> int:
    info = engine.analyse(board, chess.engine.Limit(depth=depth))
    score = info["score"].white().score(mate_score=100000)
    return int(score) if score is not None else 100000


def best_opening_candidate(
    engine: chess.engine.SimpleEngine,
    moves: list[chess.Move],
    depth: int,
    min_ply: int,
    max_ply: int,
) -> tuple[str, int, int] | None:
    if len(moves) < min_ply:
        return None

    end_ply = min(max_ply, len(moves))
    candidate = None

    for ply in range(min_ply, end_ply + 1):
        board = board_after_ply(moves, ply)
        cp = evaluate_cp(engine, board, depth)
        fen = board.fen()
        if candidate is None or abs(cp) < abs(candidate[1]):
            candidate = (fen, cp, ply)

    return candidate


def main() -> None:
    args = parse_args()

    input_path = args.input.resolve()
    output_path = args.output.resolve()
    stockfish_path = args.stockfish.resolve()

    if not input_path.exists():
        raise FileNotFoundError(f"Input games file not found: {input_path}")
    if not stockfish_path.exists():
        raise FileNotFoundError(f"Stockfish binary not found: {stockfish_path}")

    output_path.parent.mkdir(parents=True, exist_ok=True)

    total_lines = 0
    parsed_games = 0
    evaluated_games = 0
    selected_games = 0

    selected_fens: list[str] = []
    seen_fens: set[str] = set()

    with stockfish_path.open("rb"):
        pass

    with chess.engine.SimpleEngine.popen_uci(str(stockfish_path)) as engine:
        with input_path.open("r", encoding="utf-8") as f:
            for raw_line in f:
                total_lines += 1
                game = parse_game_line(raw_line)
                if game is None:
                    continue

                parsed_games += 1
                moves = list(game.mainline_moves())

                candidate = best_opening_candidate(
                    engine=engine,
                    moves=moves,
                    depth=args.depth,
                    min_ply=args.min_ply,
                    max_ply=args.max_ply,
                )
                if candidate is None:
                    continue

                evaluated_games += 1
                fen, cp, _ply = candidate
                if abs(cp) > args.cp_threshold:
                    continue

                if args.keep_duplicates:
                    selected_fens.append(fen)
                    selected_games += 1
                    continue

                if fen not in seen_fens:
                    seen_fens.add(fen)
                    selected_fens.append(fen)
                    selected_games += 1

    with output_path.open("w", encoding="utf-8") as out:
        for fen in selected_fens:
            out.write(fen + "\n")

    print(f"Total lines read: {total_lines}")
    print(f"PGN lines parsed: {parsed_games}")
    print(f"Games evaluated: {evaluated_games}")
    print(f"Balanced positions written: {selected_games}")
    print(f"Output file: {output_path}")


if __name__ == "__main__":
    main()
