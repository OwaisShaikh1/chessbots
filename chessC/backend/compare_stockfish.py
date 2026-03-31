import csv
import os
import queue
import re
import subprocess
import threading
import time
from dataclasses import dataclass


SCORE_RE = re.compile(r"\bscore\s+(cp|mate)\s+(-?\d+)")
BESTMOVE_RE = re.compile(r"^bestmove\s+(\S+)")


@dataclass
class AnalysisResult:
    bestmove: str
    score_type: str | None
    score_value: int | None


class UCIEngine:
    def __init__(self, path: str):
        self.path = path
        self.proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        self._q: queue.Queue[str] = queue.Queue()
        self._reader = threading.Thread(target=self._read_stdout, daemon=True)
        self._reader.start()

    def _read_stdout(self) -> None:
        assert self.proc.stdout is not None
        for line in self.proc.stdout:
            self._q.put(line.rstrip("\n"))

    def _send(self, cmd: str) -> None:
        assert self.proc.stdin is not None
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _wait_for(self, token: str, timeout: float = 10.0) -> None:
        end = time.time() + timeout
        while time.time() < end:
            try:
                line = self._q.get(timeout=0.1)
            except queue.Empty:
                continue
            if token in line:
                return
        raise TimeoutError(f"Timed out waiting for {token!r} from {self.path}")

    def init(self, threads: int = 1, hash_mb: int = 64, multipv: int = 1, skill: int = 20) -> None:
        self._send("uci")
        self._wait_for("uciok", timeout=15.0)
        self._send(f"setoption name Threads value {threads}")
        self._send(f"setoption name Hash value {hash_mb}")
        self._send(f"setoption name MultiPV value {multipv}")
        self._send(f"setoption name Skill Level value {skill}")
        self._send("isready")
        self._wait_for("readyok", timeout=15.0)

    def analyze_fen(self, fen: str, depth: int = 12, timeout: float = 30.0) -> AnalysisResult:
        self._send(f"position fen {fen}")
        self._send(f"go depth {depth}")

        bestmove = "(none)"
        score_type: str | None = None
        score_value: int | None = None
        end = time.time() + timeout

        while time.time() < end:
            try:
                line = self._q.get(timeout=0.1)
            except queue.Empty:
                continue

            m = SCORE_RE.search(line)
            if m:
                score_type = m.group(1)
                score_value = int(m.group(2))

            bm = BESTMOVE_RE.match(line)
            if bm:
                bestmove = bm.group(1)
                return AnalysisResult(bestmove, score_type, score_value)

        raise TimeoutError(f"Timed out waiting for bestmove from {self.path}")

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                self._send("quit")
            except Exception:
                pass
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def main() -> None:
    root = r"c:\Users\OWAIS\MyStuff\Projects\AAAA My projects\Games\chess\chessC"
    custom = os.path.join(root, "bots", "stockfish.exe")
    official = os.path.join(
        root,
        "bots",
        "official_stockfish",
        "x86_64",
        "stockfish",
        "stockfish-windows-x86-64.exe",
    )
    fen_file = os.path.join(root, "notes", "equal_opening_fens_depth10.txt")
    out_csv = os.path.join(root, "notes", "stockfish_compare_interactive_depth12_50.csv")

    depth = 12
    max_positions = 50

    with open(fen_file, "r", encoding="utf-8") as f:
        fens = [ln.strip() for ln in f if ln.strip()][:max_positions]

    c_eng = UCIEngine(custom)
    o_eng = UCIEngine(official)

    try:
        c_eng.init(threads=1, hash_mb=64, multipv=1, skill=20)
        o_eng.init(threads=1, hash_mb=64, multipv=1, skill=20)

        rows = []
        for idx, fen in enumerate(fens, start=1):
            c = c_eng.analyze_fen(fen, depth=depth, timeout=40.0)
            o = o_eng.analyze_fen(fen, depth=depth, timeout=40.0)

            cp_abs_diff = ""
            if c.score_type == "cp" and o.score_type == "cp" and c.score_value is not None and o.score_value is not None:
                cp_abs_diff = abs(c.score_value - o.score_value)

            rows.append(
                {
                    "Idx": idx,
                    "CustomBest": c.bestmove,
                    "OfficialBest": o.bestmove,
                    "SameMove": c.bestmove == o.bestmove,
                    "CustomScore": f"{c.score_type} {c.score_value}" if c.score_type else "na",
                    "OfficialScore": f"{o.score_type} {o.score_value}" if o.score_type else "na",
                    "CpAbsDiff": cp_abs_diff,
                }
            )

        with open(out_csv, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(
                f,
                fieldnames=["Idx", "CustomBest", "OfficialBest", "SameMove", "CustomScore", "OfficialScore", "CpAbsDiff"],
            )
            writer.writeheader()
            writer.writerows(rows)

        total = len(rows)
        same = sum(1 for r in rows if r["SameMove"])
        diffs = [int(r["CpAbsDiff"]) for r in rows if r["CpAbsDiff"] != ""]
        avg_cp = sum(diffs) / len(diffs) if diffs else None
        max_cp = max(diffs) if diffs else None

        print(f"Compared {total} positions at depth {depth}")
        print(f"Same best move: {same}")
        print(f"Different best move: {total - same}")
        print(f"Avg abs cp diff (cp-only): {avg_cp}")
        print(f"Max abs cp diff (cp-only): {max_cp}")
        print(f"CSV: {out_csv}")

    finally:
        c_eng.close()
        o_eng.close()


if __name__ == "__main__":
    main()
