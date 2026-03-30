import argparse
import json
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Split FEN dataset into deduplicated 500-position set files.")
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("../notes/equal_opening_fens_depth10.txt"),
        help="Input FEN file path.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("../notes/position_sets"),
        help="Output directory for set_XXX.txt files.",
    )
    parser.add_argument(
        "--chunk-size",
        type=int,
        default=500,
        help="Number of unique FENs per output set.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    input_path = args.input.resolve()
    output_dir = args.output_dir.resolve()
    chunk_size = max(1, int(args.chunk_size))

    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")

    output_dir.mkdir(parents=True, exist_ok=True)

    unique_fens: list[str] = []
    seen: set[str] = set()

    with input_path.open("r", encoding="utf-8") as f:
        for line in f:
            fen = line.strip()
            if not fen or fen in seen:
                continue
            seen.add(fen)
            unique_fens.append(fen)

    # Clean existing generated set files to avoid stale leftovers.
    for old in output_dir.glob("set_*.txt"):
        old.unlink(missing_ok=True)

    sets = []
    for idx in range(0, len(unique_fens), chunk_size):
        chunk = unique_fens[idx:idx + chunk_size]
        set_num = idx // chunk_size + 1
        file_name = f"set_{set_num:03d}.txt"
        out_path = output_dir / file_name
        with out_path.open("w", encoding="utf-8") as out:
            for fen in chunk:
                out.write(fen + "\n")

        sets.append({
            "id": out_path.stem,
            "filename": out_path.name,
            "count": len(chunk),
            "path": str(out_path),
        })

    manifest = {
        "source": str(input_path),
        "total_unique": len(unique_fens),
        "chunk_size": chunk_size,
        "set_count": len(sets),
        "sets": sets,
    }
    manifest_path = output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"Input unique FENs: {len(unique_fens)}")
    print(f"Created set files: {len(sets)}")
    print(f"Manifest: {manifest_path}")


if __name__ == "__main__":
    main()
