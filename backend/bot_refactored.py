"""
Main Chess Bot - Refactored to use modular components
"""
import chess
import random
import os
import torch
import torch.optim as optim
import torch.nn as nn
import json
import sqlite3

from model import ChessNet, board_to_tensor
from evaluation import get_material_score, order_moves
from search import ChessSearch
from database import init_db, get_analytics_data


class NeuralLearner:
    """Neural network for chess position evaluation"""
    
    def __init__(self, filepath="brain.pth"):
        self.filepath = filepath
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model = ChessNet().to(self.device)
        self.optimizer = optim.Adam(self.model.parameters(), lr=0.001)
        self.criterion = nn.MSELoss()
        self.load()

    def get_value(self, board):
        """
        Returns a value between -1 and 1 for the current board state.
        Value is from the perspective of the player to move (active color).
        1.0 means the active player is winning, -1.0 means losing.
        """
        self.model.eval()
        with torch.no_grad():
            tensor = board_to_tensor(board).to(self.device)
            value = self.model(tensor).item()
        return value

    def train_batch(self, states, targets):
        """
        Trains the model on a batch of states and target values.
        states: List of chess.Board objects (or tensors)
        targets: List of floats [-1, 1]
        """
        self.model.train()
        
        # Convert all boards to a single batch tensor
        tensors = [board_to_tensor(s).squeeze(0) for s in states]  # Remove batch dim to stack
        batch_tensor = torch.stack(tensors).to(self.device)
        target_tensor = torch.tensor(targets, dtype=torch.float32).unsqueeze(1).to(self.device)
        
        self.optimizer.zero_grad()
        outputs = self.model(batch_tensor)
        loss = self.criterion(outputs, target_tensor)
        loss.backward()
        self.optimizer.step()
        
        return loss.item()

    def save(self):
        torch.save(self.model.state_dict(), self.filepath)

    def load(self):
        if os.path.exists(self.filepath):
            try:
                self.model.load_state_dict(torch.load(self.filepath, map_location=self.device))
                print(f"Loaded model from {self.filepath}")
            except Exception as e:
                print(f"Failed to load model: {e}")


class ChessBot:
    """Main chess bot with search, evaluation, and training capabilities"""
    
    def __init__(self):
        init_db()  # Ensure DB exists on startup
        self.board = chess.Board()
        self.learner = NeuralLearner()
        self.search_engine = ChessSearch()
        self.training_status = {"active": False, "game": "0/0", "fen": ""}

    def get_fen(self):
        return self.board.fen()

    def is_game_over(self):
        return self.board.is_game_over()

    def reset_board(self):
        self.board = chess.Board()

    def get_legal_moves(self):
        return list(self.board.legal_moves)

    def make_move(self, move_uci: str):
        try:
            move = chess.Move.from_uci(move_uci)
        except ValueError:
            return False
            
        if move in self.board.legal_moves:
            self.board.push(move)
            return True
        return False

    def get_best_move(self, board=None, randomness=0.1, depth=3):
        """Find the best move using alpha-beta search"""
        target_board = board if board else self.board
        moves = list(target_board.legal_moves)
        
        if not moves:
            return None
        
        if random.random() < randomness:
            return random.choice(moves).uci()
        
        best_move = None
        best_value = -float('inf')
        alpha = -float('inf')
        beta = float('inf')
        
        # Order moves for better pruning
        ordered_moves = order_moves(target_board, moves)
        
        for move in ordered_moves:
            target_board.push(move)
            # Negamax: negate the score from opponent's perspective
            value = -self.search_engine.alphabeta(target_board, depth - 1, -beta, -alpha)
            target_board.pop()
            
            if value > best_value:
                best_value = value
                best_move = move
            alpha = max(alpha, value)
        
        if best_move:
            return best_move.uci()
        return random.choice(moves).uci()
    
    def get_analytics(self):
        """Get training analytics from database"""
        return get_analytics_data()
