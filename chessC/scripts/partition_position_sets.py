import os
from pathlib import Path

SRC_DIR = Path(__file__).parent.parent / "notes" / "position_sets"
DST_DIR = Path(__file__).parent.parent / "test_position_sets"
DST_DIR.mkdir(exist_ok=True)

for src_file in SRC_DIR.glob("set_*.txt"):
    with open(src_file, "r", encoding="utf-8") as f:
        lines = [line for line in f if line.strip()]
    base = src_file.stem
    for i in range(0, len(lines), 100):
        chunk = lines[i:i+100]
        if not chunk:
            continue
        out_name = f"{base}_part_{i//100+1:03d}.txt"
        out_path = DST_DIR / out_name
        with open(out_path, "w", encoding="utf-8") as out:
            out.writelines(chunk)
print(f"Done. Partitioned sets written to {DST_DIR}")
