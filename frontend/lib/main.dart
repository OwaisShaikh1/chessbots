import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_chess_board/flutter_chess_board.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';
import 'dart:async';
import 'analytics_screen.dart'; 
import 'arena_screen.dart'; // Import Arena Screen
import 'parameters_screen.dart'; // Import Parameters Screen

void main() {
  runApp(const ProviderScope(child: ChessApp()));
}


class ChessApp extends StatelessWidget {
  const ChessApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Experimental Chess Bot',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        useMaterial3: true,
      ),
      home: const MyHomePage(title: 'Experimental Chess Bot'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});
  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  ChessBoardController controller = ChessBoardController();
  String _status = "Ready";
  // Use localhost for Web/Windows. For Android Emulator use 10.0.2.2
  // Since user is trying Chrome, we use 127.0.0.1
  final String baseUrl = "http://127.0.0.1:8000"; 

  double _drawPunishment = 0.5;
  double _materialWeight = 0.5;
  double _depth = 1.0;

  @override
  void initState() {
    super.initState();
    _fetchGameState();
  }



  Future<void> _fetchGameState() async {
    try {
      final response = await http.get(Uri.parse('$baseUrl/fen'));
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        String fen = data['fen'];
        controller.loadFen(fen);
        setState(() {
          _status = data['game_over'] ? "Game Over" : "Playing";
        });
      }
    } catch (e) {
      setState(() {
        _status = "Connection Error: $e";
      });
    }
  }

  Future<void> _resetGame() async {
    try {
      await http.post(Uri.parse('$baseUrl/reset'));
      _fetchGameState();
    } catch (e) {
      setState(() {
          _status = "Error resetting: $e";
      });
    }
  }

  Future<void> _triggerBotMove() async {
    try {
      final response = await http.get(Uri.parse('$baseUrl/bot-move'));
      if (response.statusCode == 200) {
         _fetchGameState();
      }
    } catch (e) {
      setState(() {
        _status = "Bot Error: $e";
      });
    }
  }

  Future<void> _trainFromHere() async {
      setState(() {
          _status = "Training Started (Stream)...";
      });
      String currentFen = controller.game.fen;
      
      final client = http.Client();
      final request = http.Request('POST', Uri.parse('$baseUrl/train-stream'));
      request.headers['Content-Type'] = 'application/json';
      request.body = jsonEncode({
          "iterations": 50,
          "fen": currentFen,
          "draw_punishment": _drawPunishment,
          "material_weight": _materialWeight,
          "depth": _depth.toInt()
      });

      try {
          final response = await client.send(request);
          
          response.stream
              .transform(utf8.decoder)
              .transform(const LineSplitter())
              .listen((line) {
                  if (line.trim().isEmpty) return;
                  try {
                      final event = jsonDecode(line);
                      if (!mounted) return;

                      setState(() {
                          if (event['type'] == 'start') {
                              _status = "Training: 0/${event['total_games']} games";
                          } else if (event['type'] == 'move') {
                              controller.loadFen(event['fen']);
                          } else if (event['type'] == 'game_end') {
                              var stats = event['current_stats'];
                              _status = "Game ${event['game']} Done. W:${stats['w']} L:${stats['l']} D:${stats['d']}";
                          } else if (event['type'] == 'complete') {
                              var stats = event['stats'];
                              _status = "Training Complete! W:${stats['white_wins']} L:${stats['black_wins']} D:${stats['draws']}";
                          }
                      });
                  } catch (e) {
                      print("Error parsing stream line: $e");
                  }
              }, onError: (e) {
                  setState(() { _status = "Stream Error: $e"; });
              }, onDone: () {
                  client.close();
              });
              
      } catch (e) {
          client.close();
          setState(() { _status = "Request Error: $e"; });
      }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
        actions: [
            IconButton(
                icon: const Icon(Icons.flash_on), 
                tooltip: "Arena (Vs Stockfish)",
                onPressed: () {
                    Navigator.push(context, MaterialPageRoute(builder: (_) => ArenaScreen(baseUrl: baseUrl)));
                }
            ),
            IconButton(
                icon: const Icon(Icons.tune), 
                tooltip: "Bot Parameters",
                onPressed: () {
                    Navigator.push(context, MaterialPageRoute(builder: (_) => ParametersScreen(baseUrl: baseUrl)));
                }
            ),
            IconButton(
                icon: const Icon(Icons.analytics), 
                tooltip: "Analytics",
                onPressed: () {
                    Navigator.push(context, MaterialPageRoute(builder: (_) => AnalyticsScreen(baseUrl: baseUrl)));
                }
            ),
            IconButton(icon: const Icon(Icons.refresh), onPressed: _fetchGameState)
        ],
      ),
      body: Row(
        children: [
          Expanded(
            child: Center(
              child: ChessBoard(
                controller: controller,
                // Removed invalid theme parameters
                enableUserMoves: true,
                onMove: () {
                    // Sync with backend
                    var lastMove = controller.game.history.last;
                    String uci = lastMove.move.fromAlgebraic + lastMove.move.toAlgebraic; 
                    if (lastMove.move.promotion != null) {
                        uci += lastMove.move.promotion!.name;
                    }
                    
                    http.post(
                        Uri.parse('$baseUrl/move'),
                        headers: {"Content-Type": "application/json"},
                        body: jsonEncode({"uci": uci})
                    ).then((response) {
                        if (response.statusCode != 200) {
                            controller.undoMove();
                            ScaffoldMessenger.of(context).showSnackBar(
                                const SnackBar(content: Text("Invalid Move according to Backend"))
                            );
                        } else {
                           _fetchGameState(); 
                        }
                    });
                },
              ),
            ),
          ),
          Container(
            width: 300,
            color: Colors.grey[200],
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text("Status: $_status", style: const TextStyle(fontSize: 20)),
                const SizedBox(height: 20),
                ElevatedButton(
                  onPressed: _resetGame,
                  child: const Text("Reset Game"),
                ),
                const SizedBox(height: 10),
                ElevatedButton(
                  onPressed: _triggerBotMove,
                  child: const Text("Bot Move (Random)"),
                ),
                const SizedBox(height: 20),
                Text("Draw Punishment: ${_drawPunishment.toStringAsFixed(1)}"),
                Slider(
                  value: _drawPunishment,
                  min: 0.0,
                  max: 1.0,
                  divisions: 10,
                  label: _drawPunishment.toStringAsFixed(1),
                  onChanged: (val) => setState(() => _drawPunishment = val),
                ),
                Text("Material Greed: ${_materialWeight.toStringAsFixed(1)}"),
                Slider(
                  value: _materialWeight,
                  min: 0.0,
                  max: 2.0, // Allow up to 2.0 for strong bias
                  divisions: 20,
                  label: _materialWeight.toStringAsFixed(1),
                  onChanged: (val) => setState(() => _materialWeight = val),
                ),
                Text("Think Depth: ${_depth.toInt()}"),
                Slider(
                  value: _depth,
                  min: 1.0,
                  max: 2.0, // Limited to 2 for now as requested
                  divisions: 1,
                  label: _depth.toInt().toString(),
                  onChanged: (val) => setState(() => _depth = val),
                ),
                const Divider(),
                ElevatedButton(
                  onPressed: _trainFromHere,
                  style: ElevatedButton.styleFrom(backgroundColor: Colors.orange, foregroundColor: Colors.white),
                  child: const Text("Train (50 games)"),
                ),
                const SizedBox(height: 20),
                const Divider(),
                const Text("Metrics (See Console/Brain.json)"),
              ],
            ),
          )
        ],
      ),
    );
  }
}
