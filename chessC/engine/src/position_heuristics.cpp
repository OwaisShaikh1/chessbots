#include "../include/position_heuristics.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace {

constexpr int kPawnValue = 100;
constexpr int kKnightValue = 320;
constexpr int kBishopValue = 330;
constexpr int kRookValue = 500;
constexpr int kQueenValue = 900;
constexpr int kKingValue = 20000;

constexpr int kDefaultPawnTable[8][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {5, 10, 10, -20, -20, 10, 10, 5},
    {5, -5, -10, 0, 0, -10, -5, 5},
    {0, 0, 0, 20, 20, 0, 0, 0},
    {5, 5, 10, 25, 25, 10, 5, 5},
    {10, 10, 20, 30, 30, 20, 10, 10},
    {50, 50, 50, 50, 50, 50, 50, 50},
    {0, 0, 0, 0, 0, 0, 0, 0}
};

constexpr int kDefaultKnightTable[8][8] = {
    {-50, -40, -30, -30, -30, -30, -40, -50},
    {-40, -20, 0, 0, 0, 0, -20, -40},
    {-30, 0, 10, 15, 15, 10, 0, -30},
    {-30, 5, 15, 20, 20, 15, 5, -30},
    {-30, 0, 15, 20, 20, 15, 0, -30},
    {-30, 5, 10, 15, 15, 10, 5, -30},
    {-40, -20, 0, 5, 5, 0, -20, -40},
    {-50, -40, -30, -30, -30, -30, -40, -50}
};

constexpr int kDefaultBishopTable[8][8] = {
    {-20, -10, -10, -10, -10, -10, -10, -20},
    {-10, 5, 0, 0, 0, 0, 5, -10},
    {-10, 10, 10, 10, 10, 10, 10, -10},
    {-10, 0, 10, 10, 10, 10, 0, -10},
    {-10, 5, 5, 10, 10, 5, 5, -10},
    {-10, 0, 5, 10, 10, 5, 0, -10},
    {-10, 0, 0, 0, 0, 0, 0, -10},
    {-20, -10, -10, -10, -10, -10, -10, -20}
};

constexpr int kDefaultRookTable[8][8] = {
    {0, 0, 0, 5, 5, 0, 0, 0},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {5, 10, 10, 10, 10, 10, 10, 5},
    {0, 0, 0, 0, 0, 0, 0, 0}
};

constexpr int kDefaultQueenTable[8][8] = {
    {-20, -10, -10, -5, -5, -10, -10, -20},
    {-10, 0, 0, 0, 0, 0, 0, -10},
    {-10, 0, 5, 5, 5, 5, 0, -10},
    {-5, 0, 5, 5, 5, 5, 0, -5},
    {0, 0, 5, 5, 5, 5, 0, -5},
    {-10, 5, 5, 5, 5, 5, 0, -10},
    {-10, 0, 5, 0, 0, 0, 0, -10},
    {-20, -10, -10, -5, -5, -10, -10, -20}
};

constexpr int kDefaultKingOpenTable[8][8] = {
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-20, -30, -30, -40, -40, -30, -30, -20},
    {-10, -20, -20, -20, -20, -20, -20, -10},
    {20, 20, 0, 0, 0, 0, 20, 20},
    {20, 30, 10, 0, 0, 10, 30, 20}
};

constexpr int kDefaultKingEndTable[8][8] = {
    {-50, -40, -30, -20, -20, -30, -40, -50},
    {-30, -20, -10, 0, 0, -10, -20, -30},
    {-30, -10, 20, 30, 30, 20, -10, -30},
    {-30, -10, 30, 40, 40, 30, -10, -30},
    {-30, -10, 30, 40, 40, 30, -10, -30},
    {-30, -10, 20, 30, 30, 20, -10, -30},
    {-30, -30, 0, 0, 0, 0, -30, -30},
    {-50, -30, -30, -30, -30, -30, -30, -50}
};

std::array<int, 64> g_pawn_table{};
std::array<int, 64> g_knight_table{};
std::array<int, 64> g_bishop_table{};
std::array<int, 64> g_rook_table{};
std::array<int, 64> g_queen_table{};
std::array<int, 64> g_king_open_table{};
std::array<int, 64> g_king_end_table{};

std::string g_pst_file = "backend/pst_config.json";

int base_value(PieceType type);

void fill_from_8x8(const int src[8][8], std::array<int, 64>& dst) {
    int idx = 0;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            dst[idx++] = src[row][col];
        }
    }
}

void load_default_tables() {
    fill_from_8x8(kDefaultPawnTable, g_pawn_table);
    fill_from_8x8(kDefaultKnightTable, g_knight_table);
    fill_from_8x8(kDefaultBishopTable, g_bishop_table);
    fill_from_8x8(kDefaultRookTable, g_rook_table);
    fill_from_8x8(kDefaultQueenTable, g_queen_table);
    fill_from_8x8(kDefaultKingOpenTable, g_king_open_table);
    fill_from_8x8(kDefaultKingEndTable, g_king_end_table);
}

bool parse_array64(const std::string& content, const std::string& key, std::array<int, 64>& out) {
    const std::string quoted = "\"" + key + "\"";
    size_t key_pos = content.find(quoted);
    if (key_pos == std::string::npos) {
        return false;
    }

    size_t open = content.find('[', key_pos);
    size_t close = content.find(']', open);
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return false;
    }

    std::string array_text = content.substr(open + 1, close - open - 1);
    std::array<int, 64> parsed{};
    int count = 0;
    size_t i = 0;

    while (i < array_text.size() && count < 64) {
        while (i < array_text.size() &&
               (std::isspace(static_cast<unsigned char>(array_text[i])) || array_text[i] == ',')) {
            ++i;
        }
        if (i >= array_text.size()) break;

        int sign = 1;
        if (array_text[i] == '-') {
            sign = -1;
            ++i;
        } else if (array_text[i] == '+') {
            ++i;
        }

        if (i >= array_text.size() || !std::isdigit(static_cast<unsigned char>(array_text[i]))) {
            return false;
        }

        int value = 0;
        while (i < array_text.size() && std::isdigit(static_cast<unsigned char>(array_text[i]))) {
            value = value * 10 + (array_text[i] - '0');
            ++i;
        }

        parsed[count++] = sign * value;
    }

    if (count != 64) {
        return false;
    }

    out = parsed;
    return true;
}

bool load_tables_from_json_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string content = ss.str();

    std::array<int, 64> p;
    std::array<int, 64> n;
    std::array<int, 64> b;
    std::array<int, 64> r;
    std::array<int, 64> q;
    std::array<int, 64> k_open;
    std::array<int, 64> k_end;

    if (!parse_array64(content, "p", p) ||
        !parse_array64(content, "n", n) ||
        !parse_array64(content, "b", b) ||
        !parse_array64(content, "r", r) ||
        !parse_array64(content, "q", q) ||
        !parse_array64(content, "k_open", k_open) ||
        !parse_array64(content, "k_end", k_end)) {
        return false;
    }

    g_pawn_table = p;
    g_knight_table = n;
    g_bishop_table = b;
    g_rook_table = r;
    g_queen_table = q;
    g_king_open_table = k_open;
    g_king_end_table = k_end;
    return true;
}

void ensure_tables_initialized() {
    static bool initialized = false;
    if (initialized) return;
    load_default_tables();
    load_tables_from_json_file(g_pst_file);
    initialized = true;
}

int game_phase_opening_weight(const Board& board) {
    int non_pawn_material = 0;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const Piece piece = board.at(row, col);
            if (piece.type == PieceType::None || piece.type == PieceType::Pawn || piece.type == PieceType::King) {
                continue;
            }
            non_pawn_material += base_value(piece.type);
        }
    }

    const int max_non_pawn_material = 4100;
    return std::clamp((non_pawn_material * 100) / max_non_pawn_material, 0, 100);
}

int base_value(PieceType type) {
    switch (type) {
        case PieceType::Pawn:
            return kPawnValue;
        case PieceType::Knight:
            return kKnightValue;
        case PieceType::Bishop:
            return kBishopValue;
        case PieceType::Rook:
            return kRookValue;
        case PieceType::Queen:
            return kQueenValue;
        case PieceType::King:
            return kKingValue;
        case PieceType::None:
            return 0;
    }

    return 0;
}

int square_bonus(PieceType type, Color color, int row, int col, int opening_weight) {
    // Board rows are rank-1..rank-8 (0..7), while PSTs are rank-8..rank-1.
    // Mirror white pieces and keep black rows to preserve side symmetry.
    const int table_row = (color == Color::White) ? (7 - row) : row;
    const int idx = table_row * 8 + col;

    switch (type) {
        case PieceType::Pawn:
            return g_pawn_table[idx];
        case PieceType::Knight:
            return g_knight_table[idx];
        case PieceType::Bishop:
            return g_bishop_table[idx];
        case PieceType::Rook:
            return g_rook_table[idx];
        case PieceType::Queen:
            return g_queen_table[idx];
        case PieceType::King: {
            const int open = g_king_open_table[idx];
            const int end = g_king_end_table[idx];
            return (open * opening_weight + end * (100 - opening_weight)) / 100;
        }
        case PieceType::None:
            return 0;
    }

    return 0;
}

}  // namespace

namespace PositionHeuristics {

bool set_pst_file(const std::string& path) {
    ensure_tables_initialized();
    g_pst_file = path;
    return reload_pst();
}

bool reload_pst() {
    ensure_tables_initialized();
    load_default_tables();
    if (g_pst_file.empty()) {
        return false;
    }
    return load_tables_from_json_file(g_pst_file);
}

std::string current_pst_file() {
    return g_pst_file;
}

int evaluate_board(const Board& board, Color perspective) {
    ensure_tables_initialized();
    int score = 0;
    const int opening_weight = game_phase_opening_weight(board);

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const Piece piece = board.at(row, col);
            if (piece.type == PieceType::None || piece.color == Color::None) {
                continue;
            }

            const int piece_score = base_value(piece.type) + square_bonus(piece.type, piece.color, row, col, opening_weight);
            if (piece.color == perspective) {
                score += piece_score;
            } else {
                score -= piece_score;
            }
        }
    }

    return score;
}

int evaluate_move(const Board& board, const Move& move, Color perspective) {
    ensure_tables_initialized();
    const Board moved = board.simulate_move(move);
    return evaluate_board(moved, perspective);
}

}  // namespace PositionHeuristics
