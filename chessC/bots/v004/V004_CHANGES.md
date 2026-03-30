# v004 Changes (vs v003_ids)

This version focuses on improving iterative deepening behavior under time pressure and correcting side-evaluation bias.

## Search / IDS changes

1. Kept iterative deepening with root move ordering from previous best move.
2. Added partial-depth trust rule for time cutoffs:
- If the current deeper iteration completes at least 30% of root moves, prefer current-depth best move.
- Otherwise fallback logic remains: use previous completed depth unless current depth is clearly better.
3. This reduces over-reliance on shallow evaluations when deeper search has meaningful coverage.

## Evaluation fix

1. Fixed piece-square table (PST) row mapping symmetry between white and black.
2. This removes a side-bias issue that could cause black to evaluate positions incorrectly and lose material in practice.

## Time/depth behavior summary

1. Search starts at depth 1 and deepens only after full completion of each depth.
2. Stop condition is depth OR time.
3. On timeout, move selection now has three tiers:
- Completed deeper depth: use it.
- Interrupted deeper depth with >=30% root coverage: trust and use it.
- Otherwise: fallback to previous completed depth unless interrupted depth score is better.

## Build artifact

- Bot binary: `chessC/bots/v004/chessbot.exe`
