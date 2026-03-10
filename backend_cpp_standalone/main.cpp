/*
 * Chess Backend C++ - HTTP Server (Standalone)
 * 
 * Full Python backend API replication with Stockfish support
 * 
 * Build (Windows with MinGW):
 *   g++ -O2 -std=c++17 main.cpp chess.cpp -o chess_backend.exe -lws2_32 -lpthread
 * 
 * Run: ./chess_backend (starts on port 8000)
 */

#include "chess.hpp"
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

// ============= Stockfish UCI Engine =============
#ifdef _WIN32
#include <windows.h>

class StockfishEngine {
public:
    StockfishEngine() : running_(false), process_(nullptr), stdin_write_(nullptr), stdout_read_(nullptr) {}
    
    ~StockfishEngine() {
        stop();
    }
    
    bool start(const std::string& path, int elo = 1350, int threads = 1, int hash_mb = 16) {
        if (running_) stop();
        
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;
        
        HANDLE stdin_read, stdout_write;
        
        if (!CreatePipe(&stdin_read, &stdin_write_, &sa, 0)) return false;
        if (!CreatePipe(&stdout_read_, &stdout_write, &sa, 0)) {
            CloseHandle(stdin_read);
            CloseHandle(stdin_write_);
            return false;
        }
        
        SetHandleInformation(stdin_write_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stdout_read_, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdInput = stdin_read;
        si.hStdOutput = stdout_write;
        si.hStdError = stdout_write;
        si.dwFlags |= STARTF_USESTDHANDLES;
        
        ZeroMemory(&pi, sizeof(pi));
        
        std::string cmd = path;
        if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE, 
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(stdin_read);
            CloseHandle(stdin_write_);
            CloseHandle(stdout_read_);
            CloseHandle(stdout_write);
            return false;
        }
        
        CloseHandle(stdin_read);
        CloseHandle(stdout_write);
        CloseHandle(pi.hThread);
        process_ = pi.hProcess;
        running_ = true;
        
        // Initialize UCI
        send("uci");
        wait_for("uciok");
        
        // Set options
        send("setoption name UCI_LimitStrength value true");
        send("setoption name UCI_Elo value " + std::to_string(elo));
        send("setoption name Threads value " + std::to_string(threads));
        send("setoption name Hash value " + std::to_string(hash_mb));
        send("isready");
        wait_for("readyok");
        
        return true;
    }
    
    void stop() {
        if (running_) {
            send("quit");
            if (process_) {
                WaitForSingleObject(process_, 1000);
                TerminateProcess(process_, 0);
                CloseHandle(process_);
            }
            if (stdin_write_) CloseHandle(stdin_write_);
            if (stdout_read_) CloseHandle(stdout_read_);
            process_ = nullptr;
            stdin_write_ = nullptr;
            stdout_read_ = nullptr;
            running_ = false;
        }
    }
    
    std::string get_best_move(const std::string& fen, int depth = 10) {
        if (!running_) return "";
        
        send("position fen " + fen);
        send("go depth " + std::to_string(depth));
        
        std::string line;
        while (read_line(line)) {
            if (line.substr(0, 8) == "bestmove") {
                size_t pos = line.find(' ', 9);
                if (pos == std::string::npos) pos = line.length();
                return line.substr(9, pos - 9);
            }
        }
        return "";
    }
    
    bool is_running() const { return running_; }
    
private:
    void send(const std::string& cmd) {
        if (!stdin_write_) return;
        std::string msg = cmd + "\n";
        DWORD written;
        WriteFile(stdin_write_, msg.c_str(), (DWORD)msg.length(), &written, nullptr);
    }
    
    bool read_line(std::string& line, int timeout_ms = 30000) {
        line.clear();
        char c;
        DWORD read;
        auto start = std::chrono::steady_clock::now();
        
        while (true) {
            DWORD available;
            if (!PeekNamedPipe(stdout_read_, nullptr, 0, nullptr, &available, nullptr)) {
                return false;
            }
            
            if (available > 0) {
                if (ReadFile(stdout_read_, &c, 1, &read, nullptr) && read > 0) {
                    if (c == '\n') return true;
                    if (c != '\r') line += c;
                }
            } else {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeout_ms) {
                    return false;
                }
                Sleep(10);
            }
        }
    }
    
    void wait_for(const std::string& expected) {
        std::string line;
        while (read_line(line)) {
            if (line.find(expected) != std::string::npos) return;
        }
    }
    
    std::atomic<bool> running_;
    HANDLE process_;
    HANDLE stdin_write_;
    HANDLE stdout_read_;
};

#else
// Linux/macOS stub
class StockfishEngine {
public:
    bool start(const std::string&, int = 1350, int = 1, int = 16) { return false; }
    void stop() {}
    std::string get_best_move(const std::string&, int = 10) { return ""; }
    bool is_running() const { return false; }
};
#endif

// Default Stockfish path
const std::string DEFAULT_STOCKFISH_PATH = 
    R"(C:\Users\OWAIS\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2.exe)";

// ============= Simple HTTP Server Implementation =============
// Minimal HTTP server - no external dependencies needed

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifndef _WINSOCKAPI_
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #endif
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

#include <thread>
#include <chrono>

class SimpleHttpServer {
public:
    struct Request {
        std::string method;
        std::string path;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
    };
    
    struct Response {
        int status = 200;
        std::string body;
        std::string content_type = "application/json";
        std::unordered_map<std::string, std::string> headers;
    };
    
    using Handler = std::function<Response(const Request&)>;
    
    void Get(const std::string& path, Handler h) { 
        std::lock_guard<std::mutex> lock(routes_mutex_);
        routes_["GET " + path] = h; 
    }
    void Post(const std::string& path, Handler h) { 
        std::lock_guard<std::mutex> lock(routes_mutex_);
        routes_["POST " + path] = h; 
    }
    void Options(const std::string& path, Handler h) { 
        std::lock_guard<std::mutex> lock(routes_mutex_);
        routes_["OPTIONS " + path] = h; 
    }
    
    bool listen(const std::string& host, int port) {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }
#endif
        
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ == INVALID_SOCKET) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to bind to port " << port << std::endl;
            return false;
        }
        if (::listen(server_fd_, 10) == SOCKET_ERROR) {
            std::cerr << "Failed to listen" << std::endl;
            return false;
        }
        
        std::cout << "Chess C++ Backend running on http://" << host << ":" << port << std::endl;
        std::cout.flush();
        
        running_ = true;
        while (running_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            SOCKET client = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
            if (client == INVALID_SOCKET) continue;
            
            std::thread([this, client]() { handle_client(client); }).detach();
        }
        
        return true;
    }
    
    void stop() {
        running_ = false;
        closesocket(server_fd_);
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
private:
    void handle_client(SOCKET client) {
        char buffer[8192];
        int len = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) { closesocket(client); return; }
        buffer[len] = '\0';
        
        Request req = parse_request(buffer);
        Response res;
        
        // Add CORS headers to all responses
        res.headers["Access-Control-Allow-Origin"] = "*";
        res.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        res.headers["Access-Control-Allow-Headers"] = "Content-Type";
        
        // Handle OPTIONS (preflight)
        if (req.method == "OPTIONS") {
            res.status = 200;
            res.body = "";
        } else {
            std::string key = req.method + " " + req.path;
            Handler handler;
            {
                std::lock_guard<std::mutex> lock(routes_mutex_);
                auto it = routes_.find(key);
                if (it != routes_.end()) {
                    handler = it->second;
                }
            }
            if (handler) {
                try {
                    res = handler(req);
                } catch (const std::exception& e) {
                    res.status = 500;
                    res.body = std::string("{\"error\":\"") + e.what() + "\"}";
                }
                res.headers["Access-Control-Allow-Origin"] = "*";
                res.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
                res.headers["Access-Control-Allow-Headers"] = "Content-Type";
            } else {
                res.status = 404;
                res.body = R"({"error": "Not found"})";
            }
        }
        
        std::string response = build_response(res);
        send(client, response.c_str(), (int)response.length(), 0);
        closesocket(client);
    }
    
    Request parse_request(const std::string& raw) {
        Request req;
        std::istringstream iss(raw);
        iss >> req.method >> req.path;
        
        std::string line;
        std::getline(iss, line); // Skip rest of first line
        
        // Parse headers
        while (std::getline(iss, line) && line != "\r" && !line.empty()) {
            if (line.back() == '\r') line.pop_back();
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string val = line.substr(pos + 1);
                while (!val.empty() && val[0] == ' ') val.erase(0, 1);
                req.headers[key] = val;
            }
        }
        
        // Find body
        size_t body_start = raw.find("\r\n\r\n");
        if (body_start != std::string::npos) {
            req.body = raw.substr(body_start + 4);
        }
        
        return req;
    }
    
    std::string build_response(const Response& res) {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << res.status << " ";
        switch (res.status) {
            case 200: oss << "OK"; break;
            case 400: oss << "Bad Request"; break;
            case 404: oss << "Not Found"; break;
            case 500: oss << "Internal Server Error"; break;
            default: oss << "Unknown"; break;
        }
        oss << "\r\n";
        oss << "Content-Type: " << res.content_type << "\r\n";
        oss << "Content-Length: " << res.body.length() << "\r\n";
        for (const auto& h : res.headers) {
            oss << h.first << ": " << h.second << "\r\n";
        }
        oss << "\r\n" << res.body;
        return oss.str();
    }
    
    std::unordered_map<std::string, Handler> routes_;
    mutable std::mutex routes_mutex_;
    SOCKET server_fd_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
};

// ============= Simple JSON Parser/Builder =============
class Json {
public:
    enum Type { Null, Bool, Number, String, Array, Object };
    
    Json() : type_(Null) {}
    Json(bool b) : type_(Bool), bool_(b) {}
    Json(int n) : type_(Number), num_(n) {}
    Json(double n) : type_(Number), num_(n) {}
    Json(const std::string& s) : type_(String), str_(s) {}
    Json(const char* s) : type_(String), str_(s) {}
    
    static Json object() { Json j; j.type_ = Object; return j; }
    static Json array() { Json j; j.type_ = Array; return j; }
    
    Json& operator[](const std::string& key) {
        type_ = Object;
        return obj_[key];
    }
    
    void push_back(const Json& val) {
        type_ = Array;
        arr_.push_back(val);
    }
    
    std::string dump() const {
        switch (type_) {
            case Null: return "null";
            case Bool: return bool_ ? "true" : "false";
            case Number: {
                std::ostringstream oss;
                oss << num_;
                return oss.str();
            }
            case String: return "\"" + escape(str_) + "\"";
            case Array: {
                std::string s = "[";
                for (size_t i = 0; i < arr_.size(); i++) {
                    if (i > 0) s += ",";
                    s += arr_[i].dump();
                }
                return s + "]";
            }
            case Object: {
                std::string s = "{";
                bool first = true;
                for (const auto& kv : obj_) {
                    if (!first) s += ",";
                    s += "\"" + kv.first + "\":" + kv.second.dump();
                    first = false;
                }
                return s + "}";
            }
        }
        return "null";
    }
    
    static Json parse(const std::string& s) {
        size_t pos = 0;
        return parse_value(s, pos);
    }
    
    std::string get_string(const std::string& key, const std::string& def = "") const {
        auto it = obj_.find(key);
        if (it != obj_.end() && it->second.type_ == String) return it->second.str_;
        return def;
    }
    
    int get_int(const std::string& key, int def = 0) const {
        auto it = obj_.find(key);
        if (it != obj_.end() && it->second.type_ == Number) return (int)it->second.num_;
        return def;
    }
    
private:
    static std::string escape(const std::string& s) {
        std::string r;
        for (char c : s) {
            switch (c) {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default: r += c; break;
            }
        }
        return r;
    }
    
    static void skip_ws(const std::string& s, size_t& pos) {
        while (pos < s.size() && std::isspace(s[pos])) pos++;
    }
    
    static Json parse_value(const std::string& s, size_t& pos) {
        skip_ws(s, pos);
        if (pos >= s.size()) return Json();
        
        char c = s[pos];
        if (c == '"') return parse_string(s, pos);
        if (c == '{') return parse_object(s, pos);
        if (c == '[') return parse_array(s, pos);
        if (c == 't' || c == 'f') return parse_bool(s, pos);
        if (c == 'n') { pos += 4; return Json(); }
        if (c == '-' || std::isdigit(c)) return parse_number(s, pos);
        return Json();
    }
    
    static Json parse_string(const std::string& s, size_t& pos) {
        pos++; // skip "
        std::string result;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos++;
                switch (s[pos]) {
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += s[pos]; break;
                }
            } else {
                result += s[pos];
            }
            pos++;
        }
        if (pos < s.size()) pos++; // skip "
        return Json(result);
    }
    
    static Json parse_number(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') pos++;
        while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '.')) pos++;
        return Json(std::stod(s.substr(start, pos - start)));
    }
    
    static Json parse_bool(const std::string& s, size_t& pos) {
        if (s.substr(pos, 4) == "true") { pos += 4; return Json(true); }
        if (s.substr(pos, 5) == "false") { pos += 5; return Json(false); }
        return Json();
    }
    
    static Json parse_object(const std::string& s, size_t& pos) {
        Json j = Json::object();
        pos++; // skip {
        skip_ws(s, pos);
        while (pos < s.size() && s[pos] != '}') {
            skip_ws(s, pos);
            if (s[pos] != '"') break;
            auto key = parse_string(s, pos);
            skip_ws(s, pos);
            if (s[pos] == ':') pos++;
            j.obj_[key.str_] = parse_value(s, pos);
            skip_ws(s, pos);
            if (s[pos] == ',') pos++;
        }
        if (pos < s.size()) pos++; // skip }
        return j;
    }
    
    static Json parse_array(const std::string& s, size_t& pos) {
        Json j = Json::array();
        pos++; // skip [
        skip_ws(s, pos);
        while (pos < s.size() && s[pos] != ']') {
            j.arr_.push_back(parse_value(s, pos));
            skip_ws(s, pos);
            if (s[pos] == ',') pos++;
        }
        if (pos < s.size()) pos++; // skip ]
        return j;
    }
    
    Type type_ = Null;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Json> arr_;
    std::unordered_map<std::string, Json> obj_;
};

// ============= Main Server =============
chess::ChessBot bot(4);
std::mutex bot_mutex;
int search_depth = 4;
auto start_time = std::chrono::steady_clock::now();
std::atomic<int> total_requests{0};
std::atomic<int> bot_moves_made{0};

// Battle state
std::atomic<bool> battle_running{false};
std::atomic<bool> battle_paused{false};
std::atomic<int> battle_wins{0};
std::atomic<int> battle_losses{0};
std::atomic<int> battle_draws{0};
std::thread battle_thread;
std::mutex battle_mutex;
std::condition_variable battle_cv;
std::queue<std::string> battle_events;

void add_battle_event(const std::string& event) {
    std::lock_guard<std::mutex> lock(battle_mutex);
    battle_events.push(event);
    battle_cv.notify_all();
}

void run_battle(int iterations, const std::string& engine_path, int elo, 
                const std::string& start_fen, int depth, const std::string& bot_side,
                int threads, int hash_size) {
    
    StockfishEngine stockfish;
    if (!stockfish.start(engine_path, elo, threads, hash_size)) {
        Json event;
        event["type"] = "error";
        event["message"] = "Failed to start Stockfish";
        add_battle_event(event.dump());
        battle_running = false;
        return;
    }
    
    bool bot_plays_white = (bot_side == "white");
    
    for (int game = 0; game < iterations && battle_running; game++) {
        // Alternate sides if requested
        if (bot_side == "alternate") {
            bot_plays_white = (game % 2 == 0);
        }
        
        // Reset board
        chess::ChessBot game_board(depth);
        if (!start_fen.empty()) {
            game_board.set_fen(start_fen);
        }
        
        int move_count = 0;
        std::string last_move;
        
        // Send game start event
        {
            Json event;
            event["type"] = "game_start";
            event["game"] = game + 1;
            event["total"] = iterations;
            event["bot_color"] = bot_plays_white ? "white" : "black";
            event["fen"] = game_board.get_fen();
            add_battle_event(event.dump());
        }
        
        while (!game_board.is_game_over() && battle_running && move_count < 200) {
            // Check for pause
            while (battle_paused && battle_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            bool bot_turn = (game_board.get_fen().find(" w ") != std::string::npos) == bot_plays_white;
            std::string move;
            
            if (bot_turn) {
                // Bot's turn
                move = game_board.get_best_move(depth);
            } else {
                // Stockfish's turn
                move = stockfish.get_best_move(game_board.get_fen(), 10);
            }
            
            if (move.empty() || !game_board.make_move(move)) {
                break;
            }
            
            last_move = move;
            move_count++;
            
            // Send move event on every move (frontend expects frequent updates).
            {
                Json event;
                event["type"] = "move";
                event["game"] = game + 1;
                event["move_count"] = move_count;
                event["move"] = move;                    // UCI
                event["move_san"] = move;                // SAN not implemented: use UCI as fallback
                event["fen"] = game_board.get_fen();
                event["bot_turn"] = !bot_turn;
                add_battle_event(event.dump());
            }
        }
        
        // Determine result
        std::string result = game_board.get_result();
        bool bot_won = false, bot_lost = false, draw = false;
        
        if (result == "1-0") {
            if (bot_plays_white) { bot_won = true; battle_wins++; }
            else { bot_lost = true; battle_losses++; }
        } else if (result == "0-1") {
            if (!bot_plays_white) { bot_won = true; battle_wins++; }
            else { bot_lost = true; battle_losses++; }
        } else {
            draw = true;
            battle_draws++;
        }
        
        // Send game end event (include current_stats for frontend compatibility)
        {
            Json event;
            event["type"] = "game_end";
            event["game"] = game + 1;
            event["result"] = result;
            event["bot_won"] = bot_won;
            event["bot_lost"] = bot_lost;
            event["draw"] = draw;
            event["moves"] = move_count;
            event["wins"] = (int)battle_wins;
            event["losses"] = (int)battle_losses;
            event["draws"] = (int)battle_draws;
            Json stats = Json::object();
            stats["w"] = (int)battle_wins;
            stats["l"] = (int)battle_losses;
            stats["d"] = (int)battle_draws;
            event["current_stats"] = stats;
            add_battle_event(event.dump());
        }
    }
    
    stockfish.stop();
    
    // Send completion event
    {
        Json event;
        event["type"] = "complete";
        event["wins"] = (int)battle_wins;
        event["losses"] = (int)battle_losses;
        event["draws"] = (int)battle_draws;
        add_battle_event(event.dump());
    }
    
    battle_running = false;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Chess C++ Backend v2.0" << std::endl;
    std::cout << "  With Stockfish Support" << std::endl;
    std::cout << "  Replicating Python API" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout.flush();
    
    SimpleHttpServer server;
    
    // Root endpoint
    server.Get("/", [](const SimpleHttpServer::Request&) {
        total_requests++;
        std::lock_guard<std::mutex> lock(bot_mutex);
        Json res;
        res["message"] = "Chess Bot API is running";
        res["fen"] = bot.get_fen();
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Get current FEN
    server.Get("/fen", [](const SimpleHttpServer::Request&) {
        total_requests++;
        std::lock_guard<std::mutex> lock(bot_mutex);
        Json res;
        res["fen"] = bot.get_fen();
        res["game_over"] = bot.is_game_over();
        res["result"] = bot.get_result();
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Make a move (accepts both "uci" and "move" for compatibility)
    server.Post("/move", [](const SimpleHttpServer::Request& req) {
        total_requests++;
        Json body = Json::parse(req.body);
        std::string move = body.get_string("uci");
        if (move.empty()) {
            move = body.get_string("move");  // Fallback for compatibility
        }
        
        if (move.empty()) {
            Json res;
            res["error"] = "No move provided";
            return SimpleHttpServer::Response{400, res.dump()};
        }
        
        std::lock_guard<std::mutex> lock(bot_mutex);
        bool success = bot.make_move(move);
        
        if (!success) {
            Json res;
            res["error"] = "Invalid move";
            return SimpleHttpServer::Response{400, res.dump()};
        }
        
        Json res;
        res["fen"] = bot.get_fen();
        res["game_over"] = bot.is_game_over();
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Get bot's best move
    server.Get("/bot-move", [](const SimpleHttpServer::Request&) {
        total_requests++;
        bot_moves_made++;
        
        std::lock_guard<std::mutex> lock(bot_mutex);
        
        if (bot.is_game_over()) {
            Json res;
            res["message"] = "Game Over";
            res["fen"] = bot.get_fen();
            return SimpleHttpServer::Response{200, res.dump()};
        }
        
        auto t1 = std::chrono::steady_clock::now();
        std::string best_move = bot.get_best_move(search_depth);
        auto t2 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t2 - t1).count();
        
        // Make the move
        bot.make_move(best_move);
        
        Json res;
        res["move"] = best_move;
        res["fen"] = bot.get_fen();
        res["thinking_time"] = elapsed;
        res["depth"] = search_depth;
        res["game_over"] = bot.is_game_over();
        res["result"] = bot.get_result();
        
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Reset game
    server.Post("/reset", [](const SimpleHttpServer::Request&) {
        total_requests++;
        std::lock_guard<std::mutex> lock(bot_mutex);
        bot.reset();
        Json res;
        res["message"] = "Game reset";
        res["fen"] = bot.get_fen();
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Get/Set parameters
    server.Get("/parameters", [](const SimpleHttpServer::Request&) {
        total_requests++;
        Json res;
        
        // Material section
        Json material;
        material["pawn"] = 100;
        material["knight"] = 320;
        material["bishop"] = 330;
        material["rook"] = 500;
        material["queen"] = 900;
        res["material"] = material;
        
        // Positional section
        Json positional;
        positional["mobility_weight"] = 5;
        positional["castling_bonus"] = 50;
        positional["king_exposure_penalty"] = 25;
        positional["king_safety_penalty"] = 30;
        positional["rook_open_file"] = 15;
        positional["rook_semi_open"] = 10;
        positional["passed_pawn_scale"] = 10;
        positional["threat_divisor"] = 5;
        positional["lpdo_divisor"] = 2;
        positional["queen_early_penalty"] = 20;
        positional["queen_exposure_penalty"] = 40;
        positional["pin_penalty"] = 50;
        res["positional"] = positional;
        
        // Neural section
        Json neural;
        neural["neural_blend"] = 0;
        res["neural"] = neural;
        
        // Search section
        Json search;
        search["quiescence_depth"] = 2;
        res["search"] = search;
        
        // Training section
        Json training;
        training["learning_rate"] = 0.001;
        res["training"] = training;
        
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    server.Post("/parameters", [](const SimpleHttpServer::Request& req) {
        total_requests++;
        Json body = Json::parse(req.body);
        Json updated;
        int count = 0;
        
        int new_depth = body.get_int("search_depth", -1);
        if (new_depth >= 1 && new_depth <= 10) {
            search_depth = new_depth;
            std::lock_guard<std::mutex> lock(bot_mutex);
            bot.search_depth = new_depth;
            updated.push_back(Json("search_depth"));
            count++;
        }
        
        Json res;
        res["updated"] = updated;
        res["count"] = count;
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Metrics
    server.Get("/metrics", [](const SimpleHttpServer::Request&) {
        Json res;
        res["model_type"] = "Classical Alpha-Beta (C++)";
        res["file_path"] = "N/A (no neural network)";
        res["device"] = "CPU";
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Analytics
    server.Get("/analytics", [](const SimpleHttpServer::Request&) {
        auto now = std::chrono::steady_clock::now();
        double uptime = std::chrono::duration<double>(now - start_time).count();
        Json res;
        res["games_played"] = 0;
        res["training_samples"] = 0;
        res["engine"] = "C++ Alpha-Beta";
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Set FEN
    server.Post("/fen", [](const SimpleHttpServer::Request& req) {
        total_requests++;
        Json body = Json::parse(req.body);
        std::string fen = body.get_string("fen");
        
        if (fen.empty()) {
            Json res;
            res["error"] = "No FEN provided";
            return SimpleHttpServer::Response{400, res.dump()};
        }
        
        std::lock_guard<std::mutex> lock(bot_mutex);
        bot.set_fen(fen);
        Json res;
        res["success"] = true;
        res["fen"] = bot.get_fen();
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Training status
    server.Get("/training-status", [](const SimpleHttpServer::Request&) {
        Json res;
        res["active"] = false;
        res["game"] = "0/0";
        res["fen"] = "";
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Train stream - not supported
    server.Post("/train-stream", [](const SimpleHttpServer::Request&) {
        Json res;
        res["error"] = "Neural network training not supported in C++ backend. Use Python backend.";
        return SimpleHttpServer::Response{501, res.dump()};
    });
    
    // Train - not supported
    server.Post("/train", [](const SimpleHttpServer::Request&) {
        Json res;
        res["error"] = "Neural network training not supported in C++ backend. Use Python backend.";
        return SimpleHttpServer::Response{501, res.dump()};
    });
    
    // Battle stream - run bot vs Stockfish battles
    server.Post("/battle-stream", [](const SimpleHttpServer::Request& req) {
        total_requests++;
        
        // Stop any existing battle
        if (battle_running) {
            battle_running = false;
            if (battle_thread.joinable()) {
                battle_thread.join();
            }
        }
        
        // Clear event queue
        {
            std::lock_guard<std::mutex> lock(battle_mutex);
            while (!battle_events.empty()) battle_events.pop();
        }
        
        // Parse request
        Json body = Json::parse(req.body);
        int iterations = body.get_int("iterations", 10);
        std::string engine_path = body.get_string("engine_path", DEFAULT_STOCKFISH_PATH);
        int elo = body.get_int("elo", 1350);
        std::string fen = body.get_string("fen", "");
        int depth = body.get_int("depth", 3);
        std::string bot_side = body.get_string("bot_side", "alternate");
        int threads = body.get_int("threads", 1);
        int hash_size = body.get_int("hash_size", 16);
        
        // Reset counters
        battle_wins = 0;
        battle_losses = 0;
        battle_draws = 0;
        battle_paused = false;
        battle_running = true;
        
        // Start battle in background thread
        battle_thread = std::thread(run_battle, iterations, engine_path, elo, 
                                    fen, depth, bot_side, threads, hash_size);
        battle_thread.detach();
        
        // Return streaming response with all events
        // Note: This is a simplified implementation - returns first batch of events
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string response = "";
        {
            std::lock_guard<std::mutex> lock(battle_mutex);
            while (!battle_events.empty()) {
                response += battle_events.front() + "\n";
                battle_events.pop();
            }
        }
        
        if (response.empty()) {
            Json event;
            event["type"] = "started";
            event["iterations"] = iterations;
            event["elo"] = elo;
            response = event.dump() + "\n";
        }
        
        SimpleHttpServer::Response res;
        res.status = 200;
        res.body = response;
        res.content_type = "application/x-ndjson";
        return res;
    });
    
    // Get battle events (for polling)
    server.Get("/battle-events", [](const SimpleHttpServer::Request&) {
        std::string response = "";
        {
            std::lock_guard<std::mutex> lock(battle_mutex);
            while (!battle_events.empty()) {
                response += battle_events.front() + "\n";
                battle_events.pop();
            }
        }
        
        SimpleHttpServer::Response res;
        res.status = 200;
        res.body = response;
        res.content_type = "application/x-ndjson";
        return res;
    });
    
    // Battle status
    server.Get("/battle-status", [](const SimpleHttpServer::Request&) {
        Json res;
        res["running"] = (bool)battle_running;
        res["paused"] = (bool)battle_paused;
        res["wins"] = (int)battle_wins;
        res["losses"] = (int)battle_losses;
        res["draws"] = (int)battle_draws;
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Battle pause
    server.Post("/battle-pause", [](const SimpleHttpServer::Request&) {
        battle_paused = !battle_paused;
        Json res;
        res["paused"] = (bool)battle_paused;
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Stop battle
    server.Post("/battle-stop", [](const SimpleHttpServer::Request&) {
        battle_running = false;
        Json res;
        res["stopped"] = true;
        res["wins"] = (int)battle_wins;
        res["losses"] = (int)battle_losses;
        res["draws"] = (int)battle_draws;
        return SimpleHttpServer::Response{200, res.dump()};
    });
    
    // Train history stream - not supported
    server.Post("/train-history-stream", [](const SimpleHttpServer::Request&) {
        Json res;
        res["error"] = "Training not supported in C++ backend. Use Python backend.";
        return SimpleHttpServer::Response{501, res.dump()};
    });
    
    // Train from history - not supported
    server.Post("/train_from_history", [](const SimpleHttpServer::Request&) {
        Json res;
        res["error"] = "Training not supported in C++ backend. Use Python backend.";
        return SimpleHttpServer::Response{501, res.dump()};
    });
    
    std::cout << "Starting Chess C++ Backend on port 8000..." << std::endl;
    server.listen("0.0.0.0", 8000);
    
    return 0;
}
