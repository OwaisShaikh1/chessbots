#include <iostream>
#include <sstream>
#include <string>

#include "../include/board.h"
#include "../include/move_generator.h"
#include "../include/position_heuristics.h"

static Move parse_uci_move(const std::string& uci) {
    Move m;
    if (uci.size() < 4) return m;

    m.from_col = uci[0] - 'a';
    m.from_row = uci[1] - '1';
    m.to_col = uci[2] - 'a';
    m.to_row = uci[3] - '1';
    m.is_capture = false;
    return m;
}

static std::string to_uci(const Move& m) {
    return to_algebraic(m.from_row, m.from_col) + to_algebraic(m.to_row, m.to_col);
}

int main() {
    Board board;
    Color side_to_move = Color::White;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name ChessBot\n";
            std::cout << "id author chessC\n";
            std::cout << "option name PstFile type string default backend/pst_config.json\n";
            std::cout << "option name ReloadPst type button\n";
            std::cout << "uciok\n";
            std::cout.flush();
        } else if (cmd == "isready") {
            std::cout << "readyok\n";
            std::cout.flush();
        } else if (cmd == "setoption") {
            std::string token;
            ss >> token; // name

            std::string option_name;
            while (ss >> token && token != "value") {
                if (!option_name.empty()) option_name += " ";
                option_name += token;
            }

            std::string option_value;
            if (token == "value") {
                std::getline(ss, option_value);
                if (!option_value.empty() && option_value[0] == ' ') {
                    option_value.erase(0, 1);
                }
            }

            if (option_name == "PstFile") {
                if (!option_value.empty()) {
                    PositionHeuristics::set_pst_file(option_value);
                }
            } else if (option_name == "ReloadPst") {
                PositionHeuristics::reload_pst();
            }
        } else if (cmd == "ucinewgame") {
            board = Board{};
            side_to_move = Color::White;
        } else if (cmd == "position") {
            std::string token;
            ss >> token;

            if (token == "startpos") {
                board = Board{};
                side_to_move = Color::White;
                ss >> token;
            } else if (token == "fen") {
                std::string fen_fields[6];
                for (int i = 0; i < 6; ++i) {
                    if (!(ss >> fen_fields[i])) fen_fields[i].clear();
                }

                std::string fen = fen_fields[0];
                for (int i = 1; i < 6; ++i) {
                    if (!fen_fields[i].empty()) {
                        fen += " ";
                        fen += fen_fields[i];
                    }
                }

                board.load_from_fen(fen);
                side_to_move = (fen_fields[1] == "b") ? Color::Black : Color::White;
                ss >> token;
            }

            if (token == "moves") {
                std::string mv;
                while (ss >> mv) {
                    Move m = parse_uci_move(mv);
                    board.apply_move(m);
                    side_to_move = (side_to_move == Color::White) ? Color::Black : Color::White;
                }
            }
        } else if (cmd == "go") {
            int depth = 1;
            std::string token;
            while (ss >> token) {
                if (token == "depth") {
                    int parsed = 1;
                    if (ss >> parsed) {
                        depth = parsed;
                    }
                    break;
                }
            }

            MoveGenerator::ScoredMove best = MoveGenerator::choose_best_move(board, side_to_move, depth);
            if (best.valid) {
                std::cout << "bestmove " << to_uci(best.move) << "\n";
            } else {
                std::cout << "bestmove 0000\n";
            }
            std::cout.flush();
        } else if (cmd == "quit") {
            break;
        }
    }

    return 0;
}
