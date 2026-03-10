
# ChessBot Development Specification

## Overview

ChessBot is a modular chess experimentation platform designed for:

* chess engine development
* engine evaluation experimentation
* bot tournaments
* machine-learning tuning of evaluation heuristics
* visualization of engine analysis

The system is composed of five major components:

1. Chess Engine
2. Backend API
3. React Frontend
4. Training System
5. Database

The engine will be implemented in **C++ or Rust**, while the frontend will use **React**.

---

# System Architecture

```
React Frontend
      ↓
REST / WebSocket API
      ↓
Backend Service
      ↓
Chess Engine
      ↓
Training System
      ↓
Database
```

---

# Technology Stack

## Frontend

* React
* react-chessboard
* chess.js

Responsibilities:

* board UI
* PGN viewer
* engine evaluation visualization
* game control

---

## Backend

Possible frameworks:

* FastAPI
* Express.js

Responsibilities:

* game orchestration
* engine communication
* tournament execution
* training data storage

---

## Database

Recommended:

* PostgreSQL

Used to store:

* games
* engine analysis
* training datasets
* tournament results

---

## Engine Build System

Possible tools:

* CMake (C++)
* Cargo (Rust)

---

# Repository Structure

```
chessbot/

frontend/
backend/
engine/
training/
database/
docs/
```

---

# Engine Architecture

```
engine/

board/
movegen/
search/
evaluation/
uci/
perft/
utils/
```

---

# Board Representation

ChessBot uses **bitboards**.

Each piece type is stored as a 64-bit integer.

Example:

```
uint64 white_pawns
uint64 white_knights
uint64 white_bishops
uint64 white_rooks
uint64 white_queens
uint64 white_king

uint64 black_pawns
uint64 black_knights
uint64 black_bishops
uint64 black_rooks
uint64 black_queens
uint64 black_king
```

---

## Additional Game State

```
side_to_move
castling_rights
en_passant_square
halfmove_clock
fullmove_number
zobrist_hash
```

---

# Square Indexing

Squares are indexed from **0–63**.

```
A1 = 0
B1 = 1
...
H1 = 7

A8 = 56
...
H8 = 63
```

Board orientation:

```
63 62 61 60 59 58 57 56
55 54 53 52 51 50 49 48
...
7  6  5  4  3  2  1  0
```

---

# Bitboard Constants and Masks

## File Masks

```
FILE_A = 0x0101010101010101
FILE_B = 0x0202020202020202
FILE_C = 0x0404040404040404
FILE_D = 0x0808080808080808
FILE_E = 0x1010101010101010
FILE_F = 0x2020202020202020
FILE_G = 0x4040404040404040
FILE_H = 0x8080808080808080
```

---

## Rank Masks

```
RANK_1 = 0x00000000000000FF
RANK_2 = 0x000000000000FF00
RANK_3 = 0x0000000000FF0000
RANK_4 = 0x00000000FF000000
RANK_5 = 0x000000FF00000000
RANK_6 = 0x0000FF0000000000
RANK_7 = 0x00FF000000000000
RANK_8 = 0xFF00000000000000
```

---

## Useful Masks

```
CENTER = squares D4 E4 D5 E5

WHITE_PAWN_START = RANK_2
BLACK_PAWN_START = RANK_7
```

---

# Move Representation

Moves are stored as a compact structure.

Example:

```
struct Move {
    int from_square
    int to_square
    int promotion_piece
    int flags
}
```

Flags include:

```
CAPTURE
PROMOTION
CASTLE
EN_PASSANT
DOUBLE_PAWN_PUSH
```

---

## UCI Move Format

Moves are exported using **UCI notation**.

Example:

```
e2e4
g1f3
e7e8q
```

---

# Engine Module API Contracts

Each engine subsystem exposes explicit interfaces.

---

## Board Module

```
init_board()

load_fen(fen_string)

make_move(move)

undo_move(move)

is_square_attacked(square, side)

in_check(side)
```

---

## Move Generation Module

```
generate_pseudo_legal_moves()

generate_legal_moves()

generate_captures()

generate_quiet_moves()
```

---

## Search Module

```
search_position(depth)

iterative_deepening(max_depth)

alpha_beta(alpha, beta, depth)

quiescence(alpha, beta)
```

---

## Evaluation Module

```
evaluate_position(board)

evaluate_material()

evaluate_mobility()

evaluate_pawn_structure()

evaluate_king_safety()
```

---

# Move Generation

Generate **legal moves** for:

```
pawn
knight
bishop
rook
queen
king
```

Include special moves:

```
castling
en passant
promotion
```

Legal move validation requires:

```
king safety check
pinned piece detection
```

---

# Search System

Search includes:

```
iterative deepening
alpha beta pruning
quiescence search
transposition tables
killer moves
history heuristic
aspiration windows
null move pruning
late move reductions
```

---

# Alpha Beta Search Pseudocode

```
function alpha_beta(depth, alpha, beta):

    if depth == 0:
        return quiescence(alpha, beta)

    moves = generate_moves()

    if moves is empty:
        if in_check():
            return -CHECKMATE_SCORE
        else:
            return DRAW_SCORE

    for move in moves:

        make_move(move)

        score = -alpha_beta(depth - 1, -beta, -alpha)

        undo_move(move)

        if score >= beta:
            return beta

        if score > alpha:
            alpha = score

    return alpha
```

---

# Quiescence Search

```
function quiescence(alpha, beta):

    stand_pat = evaluate()

    if stand_pat >= beta:
        return beta

    if alpha < stand_pat:
        alpha = stand_pat

    capture_moves = generate_captures()

    for move in capture_moves:

        make_move(move)

        score = -quiescence(-beta, -alpha)

        undo_move(move)

        if score >= beta:
            return beta

        if score > alpha:
            alpha = score

    return alpha
```

---

# Evaluation Function

Evaluation combines multiple heuristics.

```
score =
material
+ mobility
+ king_safety
+ pawn_structure
+ piece_square_tables
+ passed_pawns
+ rook_open_file
+ bishop_pair
```

---

## Piece Values

```
pawn   = 100
knight = 320
bishop = 330
rook   = 500
queen  = 900
```

---

# Engine Protocol

Engine communicates using **UCI**.

Supported commands:

```
uci
isready
position
go depth N
stop
bestmove
```

---

# Backend API

Endpoints:

```
POST /game/start
POST /move
POST /engine/evaluate
POST /bot-battle
GET /game/{id}
GET /analysis/{id}
```

WebSocket:

```
/ws/analysis
```

Streams:

```
search depth
evaluation
principal variation
```

---

# Database Schema

Tables:

```
games
positions
engine_analysis
bot_results
training_positions
```

Example:

```
games

id
pgn
result
date
engine_white
engine_black
```

---

# Player vs Bot Flow

```
player move
→ frontend sends move
→ backend validates move
→ engine calculates response
→ backend returns bestmove
→ frontend updates board
```

---

# Bot vs Bot System

Supported tournament modes:

```
round robin
gauntlet
self play
```

Results are stored for **Elo rating calculation**.

---

# PGN Support

System supports **Portable Game Notation**.

Capabilities:

```
PGN import
PGN export
SAN parsing
metadata storage
```

---

# Evaluation Visualization

Frontend displays:

```
evaluation graph
principal variation
move list
depth progression
```

---

# Training System

Training pipeline collects:

```
self play games
human PGN databases
engine evaluation positions
```

Features are extracted from board states.

---

# Machine Learning Training

Evaluation weights are optimized using:

```
genetic algorithms
logistic regression
reinforcement learning
```

Parameters tuned:

```
piece values
mobility weights
pawn structure weights
king safety weights
```

---

# Engine Configuration

```
threads
hash_size
search_depth
pondering
```

---

# Testing

Testing includes:

```
perft testing
unit tests
search regression tests
```

Example:

```
perft depth 5
expected node count
```

---

# Deployment

System components:

```
engine service
backend API
frontend
database
training worker
```

Containerization recommended via Docker.

---

