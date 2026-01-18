import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np
import chess

class ChessNet(nn.Module):
    def __init__(self):
        super(ChessNet, self).__init__()
        # Input: 12 channels (6 piece types * 2 colors), 8x8 board
        self.conv1 = nn.Conv2d(12, 64, kernel_size=3, padding=1)
        self.bn1 = nn.BatchNorm2d(64)
        self.conv2 = nn.Conv2d(64, 128, kernel_size=3, padding=1)
        self.bn2 = nn.BatchNorm2d(128)
        self.conv3 = nn.Conv2d(128, 128, kernel_size=3, padding=1)
        self.bn3 = nn.BatchNorm2d(128)
        
        self.fc1 = nn.Linear(128 * 8 * 8, 256)
        self.fc2 = nn.Linear(256, 1)

    def forward(self, x):
        x = F.relu(self.bn1(self.conv1(x)))
        x = F.relu(self.bn2(self.conv2(x)))
        x = F.relu(self.bn3(self.conv3(x)))
        
        x = x.view(-1, 128 * 8 * 8)
        x = F.relu(self.fc1(x))
        x = torch.tanh(self.fc2(x)) # Output between -1 (Loss) and 1 (Win)
        return x

def board_to_tensor(board: chess.Board):
    """
    Converts a chess.Board object to a 12x8x8 PyTorch tensor.
    Perspective is always from the side to move (they become White/active in tensor).
    """
    # 12 channels: P, N, B, R, Q, K for "Us", then P, N, B, R, Q, K for "Them"
    pieces = [chess.PAWN, chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN, chess.KING]
    
    # Create empty matrix
    matrix = np.zeros((12, 8, 8), dtype=np.float32)
    
    # We want to orient the board so that the current player is effective "White" (at bottom)
    # If it's Black's turn, we flip the board rank-wise and swap color roles.
    
    us = board.turn
    them = not us
    
    for i, piece_type in enumerate(pieces):
        # "Our" pieces -> channels 0-5
        for sq in board.pieces(piece_type, us):
            rank, file = chess.square_rank(sq), chess.square_file(sq)
            if us == chess.BLACK:
                rank = 7 - rank
                # file = 7 - file # Usually we mirror file too for true symmetry? 
                # Standard practice: Vertical flip is enough to keep pieces attacking forward.
                # But left/right swap might be needed if side-specific logic exists (Queenside/Kingside).
                # For simplicity, let's just flip ranks.
            matrix[i][rank][file] = 1.0

        # "Their" pieces -> channels 6-11
        for sq in board.pieces(piece_type, them):
            rank, file = chess.square_rank(sq), chess.square_file(sq)
            if us == chess.BLACK:
                rank = 7 - rank
            matrix[i + 6][rank][file] = 1.0
            
    return torch.tensor(matrix).unsqueeze(0) # Add batch dimension -> (1, 12, 8, 8)
