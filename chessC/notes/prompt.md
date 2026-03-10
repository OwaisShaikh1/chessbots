
# ChessBot Engine Generation Roadmap for Claude

## Purpose

This document defines a **deterministic generation sequence** for building the ChessBot engine using an LLM.

The goal is to generate the engine **module-by-module**, ensuring:

* stable interfaces
* correct dependencies
* incremental testing
* compilable code after every step

This roadmap is optimized for **LLM code generation workflows**.

---

# Generation Rules

When generating code:

1. Generate **one module at a time**
2. Do not modify previous modules unless explicitly instructed
3. Always output **complete files**
4. Use consistent naming across modules
5. Ensure the engine **compiles after each step**
6. the location of all the work will be in chessC

---

# Target Language

Engine implementation language:

* C++17 or Rust

Bitboard architecture must be used.

---

# Engine Directory Layout

```id="x2"
engine/

include/
src/

board/
movegen/
search/
evaluation/
uci/
perft/
utils/
```

---

# Step 1 — Core Types

Generate core engine types.

Files:

```id="x3"
engine/include/types.h
```

Define:

* piece enums
* square enums
* color enums
* move flags
* basic type aliases

Example structures to define:

```id="x4"
enum Piece
enum Color
enum Square

struct Move
```

Requirements:

* compact move representation
* 32-bit move encoding if possible

---

# Step 2 — Bitboard Utilities

Generate bitboard helper functions.

Files:

```id="x5"
engine/include/bitboard.h
engine/src/bitboard.cpp
```

Functions required:

```id="x6"
popcount()
lsb()
msb()
set_bit()
clear_bit()
get_bit()
```

Use compiler intrinsics where available.

---

# Step 3 — Board Representation

Generate the board state implementation.

Files:

```id="x7"
engine/include/board.h
engine/src/board.cpp
```

Board structure must include:

```id="x8"
bitboards[12]
occupancy[3]

side_to_move
castling_rights
en_passant_square
halfmove_clock
zobrist_hash
```

Functions required:

```id="x9"
init_board()
load_fen()
print_board()
make_move()
undo_move()
```

---

# Step 4 — Attack Tables

Generate precomputed attack tables.

Files:

```id="x10"
engine/include/attacks.h
engine/src/attacks.cpp
```

Precompute attacks for:

```id="x11"
pawn
knight
king
bishop
rook
queen
```

Sliding attacks should use:

* magic bitboards or
* ray scanning

---

# Step 5 — Move Generation

Generate pseudo-legal move generator.

Files:

```id="x12"
engine/include/movegen.h
engine/src/movegen.cpp
```

Functions:

```id="x13"
generate_moves()
generate_captures()
generate_quiets()
```

Handle:

```id="x14"
pawn pushes
pawn captures
promotions
en passant
castling
```

---

# Step 6 — Legal Move Filtering

Generate legal move validation.

Files:

```id="x15"
engine/src/legal_moves.cpp
```

Functions:

```id="x16"
is_square_attacked()
in_check()
generate_legal_moves()
```

---

# Step 7 — Perft Testing

Generate perft module.

Files:

```id="x17"
engine/include/perft.h
engine/src/perft.cpp
```

Functions:

```id="x18"
perft(depth)
perft_divide(depth)
```

Test cases:

```id="x19"
startpos depth 1 = 20
startpos depth 2 = 400
startpos depth 3 = 8902
startpos depth 4 = 197281
```

The engine must pass these before continuing.

---

# Step 8 — Evaluation Function

Generate evaluation module.

Files:

```id="x20"
engine/include/evaluation.h
engine/src/evaluation.cpp
```

Evaluation components:

```id="x21"
material
piece_square_tables
mobility
pawn_structure
king_safety
```

Return score from perspective of side to move.

---

# Step 9 — Search Framework

Generate search module.

Files:

```id="x22"
engine/include/search.h
engine/src/search.cpp
```

Functions:

```id="x23"
search_position()
iterative_deepening()
alpha_beta()
quiescence()
```

Use negamax formulation.

---

# Step 10 — Move Ordering

Enhance search with move ordering.

Implement:

```id="x24"
MVV-LVA
killer moves
history heuristic
```

These improve pruning efficiency.

---

# Step 11 — Transposition Table

Generate hash table.

Files:

```id="x25"
engine/include/tt.h
engine/src/tt.cpp
```

Table entries:

```id="x26"
zobrist_key
depth
score
flag
best_move
```

Flags:

```id="x27"
exact
alpha
beta
```

---

# Step 12 — UCI Protocol

Implement **UCI**.

Files:

```id="x28"
engine/src/uci.cpp
```

Commands:

```id="x29"
uci
isready
position
go
stop
quit
```

Engine must output:

```id="x30"
bestmove <move>
```

---

# Step 13 — Engine Main

Generate engine entry point.

File:

```id="x31"
engine/src/main.cpp
```

Responsibilities:

```id="x32"
initialize engine
initialize attack tables
start UCI loop
```

---

# Step 14 — Benchmark Mode

Add benchmarking command.

Example:

```id="x33"
bench depth 10
```

Measure:

```id="x34"
nodes searched
nodes per second
search time
```

---

# Step 15 — Engine Testing

Test engine with:

* perft validation
* search sanity tests
* mate detection

Ensure no illegal moves are generated.

---

# Step 16 — Integration With Backend

Backend communicates with engine via:

* subprocess execution
* UCI command streams

Example:

```id="x35"
position startpos moves e2e4 e7e5
go depth 10
```

Engine returns:

```id="x36"
bestmove g1f3
```

---

# Step 17 — Self Play Mode

Add engine self-play support.

Used for:

* generating training data
* testing evaluation changes

---

# Step 18 — Future Improvements

Possible engine improvements:

```id="x37"
magic bitboards
late move reductions
null move pruning
aspiration windows
NNUE evaluation
```

---

# Final Goal

The completed engine should support:

* player vs bot
* bot vs bot tournaments
* engine analysis
* training data generation

And integrate cleanly with the ChessBot backend and frontend.

---

