#include "bot/bot.hpp"
#include "evaluation/evaluation.hpp"

#define CROW_MAIN
#include <crow.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Global bot instance
ChessBot bot(4);

int main(int argc, char* argv[]) {
    // Initialize attack tables
    chess::init_attacks();
    
    int port = 8000;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    crow::SimpleApp app;
    
    // Enable CORS
    auto& cors = app.get_middleware<crow::CORSHandler>();
    // Not using middleware directly, we'll add headers manually
    
    // Helper to add CORS headers
    auto add_cors = [](crow::response& res) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
    };
    
    // Health check
    CROW_ROUTE(app, "/")([]() {
        json response;
        response["message"] = "Chess Bot API is running (C++)";
        response["fen"] = bot.get_fen();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Get FEN
    CROW_ROUTE(app, "/fen")([]() {
        json response;
        response["fen"] = bot.get_fen();
        response["game_over"] = bot.is_game_over();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Make a move
    CROW_ROUTE(app, "/move").methods("POST"_method, "OPTIONS"_method)
    ([](const crow::request& req) {
        if (req.method == "OPTIONS"_method) {
            crow::response res(200);
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
            return res;
        }
        
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            crow::response res(400, R"({"error": "Invalid JSON"})");
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        std::string uci = body.value("uci", "");
        if (uci.empty()) {
            crow::response res(400, R"({"error": "Missing 'uci' field"})");
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        bool success = bot.make_move(uci);
        if (!success) {
            crow::response res(400, R"({"detail": "Invalid move"})");
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        json response;
        response["fen"] = bot.get_fen();
        response["game_over"] = bot.is_game_over();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Bot move
    CROW_ROUTE(app, "/bot-move")([]() {
        if (bot.is_game_over()) {
            json response;
            response["message"] = "Game Over";
            response["fen"] = bot.get_fen();
            response["result"] = bot.get_result();
            
            crow::response res(response.dump());
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        std::string move = bot.get_best_move();
        if (move.empty()) {
            json response;
            response["message"] = "No moves available";
            
            crow::response res(response.dump());
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        bot.make_move(move);
        
        json response;
        response["move"] = move;
        response["fen"] = bot.get_fen();
        response["game_over"] = bot.is_game_over();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Reset game
    CROW_ROUTE(app, "/reset").methods("POST"_method, "OPTIONS"_method)
    ([](const crow::request& req) {
        if (req.method == "OPTIONS"_method) {
            crow::response res(200);
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
            return res;
        }
        
        bot.reset();
        
        json response;
        response["message"] = "Game reset";
        response["fen"] = bot.get_fen();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Get parameters
    CROW_ROUTE(app, "/parameters")([]() {
        json response;
        
        response["material"] = {
            {"pawn", eval::PIECE_VALUES.pawn},
            {"knight", eval::PIECE_VALUES.knight},
            {"bishop", eval::PIECE_VALUES.bishop},
            {"rook", eval::PIECE_VALUES.rook},
            {"queen", eval::PIECE_VALUES.queen}
        };
        
        response["positional"] = {
            {"mobility_weight", eval::EVAL_PARAMS.mobility_weight},
            {"castling_bonus", eval::EVAL_PARAMS.castling_bonus},
            {"king_exposure_penalty", eval::EVAL_PARAMS.king_exposure_penalty},
            {"king_safety_penalty", eval::EVAL_PARAMS.king_safety_penalty},
            {"rook_open_file", eval::EVAL_PARAMS.rook_open_file},
            {"rook_semi_open", eval::EVAL_PARAMS.rook_semi_open},
            {"passed_pawn_scale", eval::EVAL_PARAMS.passed_pawn_scale},
            {"threat_divisor", eval::EVAL_PARAMS.threat_divisor},
            {"lpdo_divisor", eval::EVAL_PARAMS.lpdo_divisor},
            {"queen_early_penalty", eval::EVAL_PARAMS.queen_early_penalty},
            {"queen_exposure_penalty", eval::EVAL_PARAMS.queen_exposure_penalty},
            {"pin_penalty", eval::EVAL_PARAMS.pin_penalty}
        };
        
        response["search"] = {
            {"depth", bot.get_search_depth()}
        };
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Set parameters
    CROW_ROUTE(app, "/parameters").methods("POST"_method, "OPTIONS"_method)
    ([](const crow::request& req) {
        if (req.method == "OPTIONS"_method) {
            crow::response res(200);
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
            return res;
        }
        
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            crow::response res(400, R"({"error": "Invalid JSON"})");
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        std::vector<std::string> updated;
        
        // Material values
        if (body.contains("pawn_value")) {
            eval::PIECE_VALUES.pawn = body["pawn_value"];
            updated.push_back("pawn_value");
        }
        if (body.contains("knight_value")) {
            eval::PIECE_VALUES.knight = body["knight_value"];
            updated.push_back("knight_value");
        }
        if (body.contains("bishop_value")) {
            eval::PIECE_VALUES.bishop = body["bishop_value"];
            updated.push_back("bishop_value");
        }
        if (body.contains("rook_value")) {
            eval::PIECE_VALUES.rook = body["rook_value"];
            updated.push_back("rook_value");
        }
        if (body.contains("queen_value")) {
            eval::PIECE_VALUES.queen = body["queen_value"];
            updated.push_back("queen_value");
        }
        
        // Positional parameters
        if (body.contains("mobility_weight")) {
            eval::EVAL_PARAMS.mobility_weight = body["mobility_weight"];
            updated.push_back("mobility_weight");
        }
        if (body.contains("castling_bonus")) {
            eval::EVAL_PARAMS.castling_bonus = body["castling_bonus"];
            updated.push_back("castling_bonus");
        }
        if (body.contains("king_exposure_penalty")) {
            eval::EVAL_PARAMS.king_exposure_penalty = body["king_exposure_penalty"];
            updated.push_back("king_exposure_penalty");
        }
        if (body.contains("rook_open_file")) {
            eval::EVAL_PARAMS.rook_open_file = body["rook_open_file"];
            updated.push_back("rook_open_file");
        }
        
        // Search depth
        if (body.contains("depth")) {
            bot.set_search_depth(body["depth"]);
            updated.push_back("depth");
        }
        
        json response;
        response["updated"] = updated;
        response["count"] = updated.size();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Get metrics
    CROW_ROUTE(app, "/metrics")([]() {
        json response;
        response["model_type"] = "C++ Chess Engine";
        response["search_depth"] = bot.get_search_depth();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Analytics (simplified - no database in C++ version)
    CROW_ROUTE(app, "/analytics")([]() {
        json response;
        response["message"] = "Analytics not available in C++ backend";
        response["games_played"] = 0;
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Set FEN (for testing)
    CROW_ROUTE(app, "/set-fen").methods("POST"_method, "OPTIONS"_method)
    ([](const crow::request& req) {
        if (req.method == "OPTIONS"_method) {
            crow::response res(200);
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
            return res;
        }
        
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            crow::response res(400, R"({"error": "Invalid JSON"})");
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        std::string fen = body.value("fen", "");
        if (fen.empty()) {
            crow::response res(400, R"({"error": "Missing 'fen' field"})");
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        
        bot.set_fen(fen);
        
        json response;
        response["message"] = "FEN set successfully";
        response["fen"] = bot.get_fen();
        
        crow::response res(response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    std::cout << "Chess C++ Backend starting on port " << port << std::endl;
    std::cout << "API compatible with Python backend" << std::endl;
    
    app.port(port).multithreaded().run();
    
    return 0;
}
