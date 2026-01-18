# ♟️ Experimental Chess Engine: Technical Report

A state-of-the-art chess engine project combining classical **Alpha-Beta Search** with **Deep Learning (PyTorch)** and a modern **Flutter web UI**. Developed to explore the synergy between tactical search and positional evaluation.

---

## 🏗️ System Architecture

The project is split into a **High-Performance Python Backend** and a **Reactive Flutter Frontend**.

### Backend Layer
- **Framework**: FastAPI (Asynchronous Python)
- **Deep Learning**: PyTorch (Neural Network evaluation)
- **Search Logic**: Python-Chess (Game rules & search orchestration)
- **Database**: SQLite (Training move persistence & analytics)
- **External Engine**: Stockfish (Benchmark & training opponent)

### Frontend Layer
- **Framework**: Flutter (Web/Desktop)
- **UI Components**: Material 3, Custom Canvas Chessboard
- **Charts**: fl_chart (Real-time analytics)

---

## 🧠 Brain & Evaluation

### 1. Neural Network (`model.py`)
- **Architecture**: `ChessNet` - 3-layer fully connected network (832 -> 512 -> 256 -> 1).
- **Transformation**: Board states are bitmapped into a 13-channel tensor (Piece types for White/Black + Active layer).
- **Output**: Evaluation score normalized between `[-1.0, 1.0]`.

### 2. Tactical Evaluation (`evaluation.py`)
- **Tapered PST**: Piece-Square Tables that scale from Middlegame to Endgame weights.
- **LPDO (Loose Piece Detection)**: Robust detection comparing lowest attacker vs lowest defender.
- **Threat Detection**: Proactive penalty for undefended pieces and fork awareness.
- **King Safety**: Penalties for exposure and proximity to enemy pieces.
- **Mobility**: Scoring based on available legal moves per piece type.

---

## 🔎 Search Algorithm (`search.py`)

The engine uses a highly optimized **Alpha-Beta Search** with the following stabilizers:

- **Iterative Deepening**: Progressively deeper search to manage time effectively.
- **Quiescence Search**: Extends search during captures/checks to prevent the "Horizon Effect."
- **Check Extensions**: Adds +1 depth when responding to checks.
- **Killer Move Heuristic**: Prioritizes moves that caused cut-offs in sibling branches.
- **TT (Transposition Table)**: Caches evaluated positions using Zobrist Hashing (Partial impl).
- **MVV-LVA**: Most Valuable Victim - Least Valuable Attacker move ordering.

---

## 📟 API Endpoints (`main.py`)

### 🛰️ Core Operations
- `GET /`: API Health check.
- `GET /fen`: Get the current board state.
- `POST /move`: Submit a user move (UCI format).
- `GET /bot-move`: Trigger the bot to calculate and make its move.
- `POST /reset`: Reset the board to standard starting position.

### 🏋️ Training & Battles
- `POST /train-stream`: Standard self-play training (Stream).
- `POST /battle-stream`: Orchestrate games vs Stockfish with ELO matching (Stream).
- `POST /train-history-stream`: Retrain the Neural Network using historical games (Stream).
- `GET /analytics`: Retrieve win-rates and recent game history.
- `GET /metrics`: Technical info on the active model and hardware (CUDA/CPU).

---

## 📺 User Interface Screens

### 1. Arena Screen
- **Material Bar**: Live visual balance of numerical material values.
- **Game Controller**: Media-player style controls (⏮️, ◀️, ▶️, ⏭️) for move history.
- **PGN Export**: Generates and copies standard PGN transcripts for external analysis.
- **Interactive Move List**: Clickable SAN moves (e.g., 1. e4 e5) for quick navigation.

### 2. Training Screen
- **Self-Play UI**: Watch the bot play against variations of itself.
- **Reward Tuning**: Sliders for `Draw Punishment` and `Material Weight`.

### 3. Analytics Screen
- **Intelligence Evolution**: Line charts tracking win-rate and draw-rate over time.
- **Historical Learning**: Button to trigger "Learn from battles" from SQLite history.
- **Recent Games**: Detailed log of past opponents (Stockfish vs Self).

---

## 🗄️ Database Schema (`database.py`)

**Table**: `training_moves`
- `game_id`: UUID for the session.
- `start_fen`: The FEN the game started with.
- `step`: Ply index.
- `turn`: Active player (White/Black).
- `material_score`: Evaluated material balance at that point.
- `result`: Game outcome from player perspective (1=Win, 0=Draw, -1=Loss).
- `fen`: Exact position at that step.
- `opponent`: "Self" or "Stockfish (Elo X)".

---
*Report generated on: 2026-01-18*
