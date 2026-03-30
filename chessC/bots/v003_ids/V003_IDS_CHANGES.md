# v003_ids Changes (vs v002)

This document summarizes what was changed for the `v003_ids` engine compared to `v002`.

## Engine search updates

1. Added iterative deepening search in the engine move-selection path.
2. Search now starts from depth 1 and only advances when the current depth fully completes.
3. Added optional per-move time limit support via UCI `movetime`.
4. Depth and time now work as an OR stop condition:
- Stop when max depth is reached.
- Stop when time limit is reached.
5. On forced timeout, move selection behavior is:
- Return best move from the last fully completed depth.
- If current (deeper) depth already has a better fully evaluated move, return that better move.
6. Improved iterative deepening stability by ordering root moves with the previous iteration best move first.
7. This reduces noisy timeout behavior and makes extra search time more consistently useful.
8. Fixed PST square-index mapping symmetry between white and black.
9. This correction removes a side-bias bug that could make black consistently evaluate positions worse and lose material.

## UCI command handling

1. Updated `go` parsing to support both:
- `depth <n>`
- `movetime <ms>`
2. If both are present, the engine searches with both constraints active.

## Frontend/backend integration added after v003_ids build

These integration changes were made in source after creating the `v003_ids` snapshot so the UI can control time-per-move:

1. Backend API now accepts `movetime_ms` for game start and engine evaluation.
2. Backend UCI launcher now sends:
- `go depth <n> movetime <ms>` when `movetime_ms > 0`
- `go depth <n>` otherwise.
3. React Play page now includes a `Move time (ms)` setting.
4. React Arena page now includes per-side move time settings.

## Notes

- `v003_ids` binary was created and stored in `chessC/bots/v003_ids/chessbot.exe`.
- Historical versions remain untouched.
- `v003_ids` was later replaced with the newest build to include the root-move ordering stability fix.
