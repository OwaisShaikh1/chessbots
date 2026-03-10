#include "../include/search.h"
#include "../include/perft.h"
#include "../include/legal_moves.h"
#include "../include/attacks.h"
#include "../include/zobrist.h"
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

// ─── Move to UCI string ───────────────────────────────────────────────────

static std::string move_to_uci(Move m) {
    if (m.is_null()) return "0000";
    std::string s;
    s += char('a' + file_of(m.from()));
    s += char('1' + rank_of(m.from()));
    s += char('a' + file_of(m.to()));
    s += char('1' + rank_of(m.to()));
    if (m.is_promotion()) {
        static const char pc[] = "  nbrq";
        s += pc[m.promo_piece()];
    }
    return s;
}

// ─── UCI string to Move ───────────────────────────────────────────────────

static Move uci_to_move(const Board& b, const std::string& uci) {
    if (uci.size() < 4) return NULL_MOVE;
    int from_file = uci[0] - 'a';
    int from_rank = uci[1] - '1';
    int to_file   = uci[2] - 'a';
    int to_rank   = uci[3] - '1';
    if (from_file < 0 || from_file > 7 || from_rank < 0 || from_rank > 7 ||
        to_file   < 0 || to_file   > 7 || to_rank   < 0 || to_rank   > 7)
        return NULL_MOVE;

    Square from = make_square(from_file, from_rank);
    Square to   = make_square(to_file,   to_rank);

    // Determine promo piece
    PieceType promo = NO_PIECE_TYPE;
    if (uci.size() >= 5) {
        switch (uci[4]) {
            case 'n': promo = KNIGHT; break;
            case 'b': promo = BISHOP; break;
            case 'r': promo = ROOK;   break;
            case 'q': promo = QUEEN;  break;
            default:  break;
        }
    }

    // Match against legal moves to get the exact move with correct flags
    MoveList legal;
    generate_legal_moves(const_cast<Board&>(b), legal);
    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        if (m.from() == from && m.to() == to) {
            if (promo != NO_PIECE_TYPE) {
                if (m.promo_piece() == promo) return m;
            } else {
                return m;
            }
        }
    }
    return NULL_MOVE;
}

// ─── UCI loop ─────────────────────────────────────────────────────────────

void uci_loop() {
    Board board;
    board.load_fen(START_FEN);

    std::cout << "id name ChessBot\n";
    std::cout << "id author ChessBot Engine\n";
    std::cout << "option name Hash type spin default 128 min 1 max 4096\n";
    std::cout << "uciok\n";
    std::cout.flush();

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "uci") {
            std::cout << "id name ChessBot\n";
            std::cout << "id author ChessBot Engine\n";
            std::cout << "uciok\n";
            std::cout.flush();
        }
        else if (token == "isready") {
            std::cout << "readyok\n";
            std::cout.flush();
        }
        else if (token == "setoption") {
            // option name Hash value N
            std::string name_tok, name, value_tok;
            int value = 128;
            ss >> name_tok >> name >> value_tok >> value;
            if (name == "Hash") {
                TT = TranspositionTable(static_cast<size_t>(value));
            }
        }
        else if (token == "ucinewgame") {
            board.load_fen(START_FEN);
            TT.clear();
        }
        else if (token == "position") {
            std::string type;
            ss >> type;
            if (type == "startpos") {
                board.load_fen(START_FEN);
                ss >> token; // consume optional "moves"
            } else if (type == "fen") {
                // Read up to 6 FEN tokens
                std::string fen;
                int fen_parts = 0;
                while (ss >> token && token != "moves" && fen_parts < 6) {
                    if (!fen.empty()) fen += ' ';
                    fen += token;
                    fen_parts++;
                }
                board.load_fen(fen);
                if (token == "moves") { /* already consumed */ }
                else ss >> token; // try to get "moves"
            }
            // Apply move list
            if (token == "moves") {
                std::string mv;
                while (ss >> mv) {
                    Move m = uci_to_move(board, mv);
                    if (!m.is_null()) board.make_move(m);
                }
            }
        }
        else if (token == "go") {
            SearchLimits limits;
            std::string opt;
            while (ss >> opt) {
                if (opt == "depth") {
                    ss >> limits.depth;
                } else if (opt == "movetime") {
                    ss >> limits.movetime_ms;
                } else if (opt == "infinite") {
                    limits.infinite = true;
                    limits.depth    = 64;
                } else if (opt == "wtime") {
                    int wtime;
                    ss >> wtime;
                    if (board.side_to_move == WHITE && limits.movetime_ms == 0)
                        limits.movetime_ms = wtime / 20; // simple time management
                } else if (opt == "btime") {
                    int btime;
                    ss >> btime;
                    if (board.side_to_move == BLACK && limits.movetime_ms == 0)
                        limits.movetime_ms = btime / 20;
                }
            }
            if (limits.infinite) limits.movetime_ms = 0;

            SearchInfo info;
            ENGINE.search_position(board, limits, info);

            std::cout << "bestmove " << move_to_uci(info.best_move) << "\n";
            std::cout.flush();
        }
        else if (token == "stop") {
            ENGINE.stop();
        }
        else if (token == "quit") {
            break;
        }
        // Non-UCI extensions
        else if (token == "d") {
            board.print_board();
        }
        else if (token == "perft") {
            int depth = 5;
            ss >> depth;
            Perft::perft_divide(board, depth);
        }
        else if (token == "perfttest") {
            Perft::run_tests();
        }
        else if (token == "bench") {
            int depth = 10;
            ss >> depth;
            Board bench_board;
            bench_board.load_fen(START_FEN);
            SearchLimits lim;
            lim.depth = depth;
            SearchInfo info;
            ENGINE.search_position(bench_board, lim, info);
            std::cout << "bench depth=" << depth
                      << " nodes=" << info.nodes
                      << " time=" << int(info.elapsed_ms) << "ms"
                      << " nps=" << int(info.nodes / (info.elapsed_ms / 1000.0 + 0.001)) << "\n";
        }
    }
}
