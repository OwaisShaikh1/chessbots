import 'package:flutter/material.dart';
import 'package:flutter_chess_board/flutter_chess_board.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';
import 'dart:async';
import 'package:flutter/services.dart';

class GameSession {
  final int id;
  final String white;
  final String black;
  List<String> fens;
  List<String> moves;
  String result;
  int historyIndex;

  GameSession({
    required this.id,
    required this.white,
    required this.black,
    required this.fens,
    required this.moves,
    this.result = "*",
    this.historyIndex = 0,
  });
}

class ArenaScreen extends StatefulWidget {
  final String baseUrl;
  const ArenaScreen({super.key, required this.baseUrl});

  @override
  State<ArenaScreen> createState() => _ArenaScreenState();
}

class _ArenaScreenState extends State<ArenaScreen> with AutomaticKeepAliveClientMixin {
  @override
  bool get wantKeepAlive => true;  // Keep state alive when navigating away
  
  ChessBoardController controller = ChessBoardController();
  
  // Settings
  int _elo = 1350;
  int _games = 10;
  int _depth = 3;
  String _botSide = "alternate";
  String _enginePath = r"C:\Users\OWAIS\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2.exe"; // Default absolute path
  
  // Status
  bool _isBattling = false;
  String _status = "Ready to Fight";
  String _logs = "";
  String _whiteName = "White";
  String _blackName = "Black";
  String _fen = "";
  bool _setupMode = false;
  int _whiteMat = 0;
  int _blackMat = 0;
  
  Map<String, int> _stats = {"w": 0, "l": 0, "d": 0};

  // History & Management
  List<GameSession> _sessions = [];
  int _activeSessionIndex = -1;
  bool _isViewingHistory = false;
  bool _isPaused = false;
  bool _isTraining = false;

  Future<void> _togglePause() async {
    try {
      setState(() {
        _isPaused = !_isPaused;
      });
      final response = await http.post(Uri.parse('${widget.baseUrl}/battle-pause'));
      if (response.statusCode != 200) {
        // Revert if server failed
        setState(() {
          _isPaused = !_isPaused;
        });
        throw Exception("Server failed to toggle pause");
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Failed to toggle pause: $e"))
      );
    }
  }

  Future<void> _learnFromHistory() async {
    if (!mounted) return;
    setState(() {
      _isTraining = true;
      _logs += "Starting training from history...\n";
    });

    final request = http.Request('POST', Uri.parse('${widget.baseUrl}/train-history-stream'));
    request.body = jsonEncode({"batch_size": 64, "epochs": 20});
    request.headers['Content-Type'] = 'application/json';

    try {
      final response = await request.send();
      
      response.stream.transform(utf8.decoder).transform(const LineSplitter()).listen(
        (line) {
          if (line.isEmpty) return;
          if (!mounted) return; // Check if widget is still mounted
          try {
            final data = jsonDecode(line);
            setState(() {
              if (data['type'] == 'start') {
                _logs += "Training on ${data['total_moves']} positions from Stockfish battles (${data['method']})\n";
              } else if (data['type'] == 'info') {
                _logs += "Train set: ${data['train_size']} | Validation set: ${data['val_size']}\n";
              } else if (data['type'] == 'epoch_end') {
                final trainLoss = (data['train_loss'] as num).toStringAsFixed(6);
                final valLoss = (data['val_loss'] as num).toStringAsFixed(6);
                _logs += "Epoch ${data['epoch']}/${request.body.contains('20') ? 20 : 5} - Train: $trainLoss, Val: $valLoss ${data['improvement']}\n";
              } else if (data['type'] == 'complete') {
                _logs += "✓ ${data['message']} (${data['duration']})\n";
              } else if (data['type'] == 'error') {
                _logs += "ERROR: ${data['message']}\n";
              }
            });
          } catch (e) {
            if (mounted) setState(() => _logs += "Parse error: $e\n");
          }
        },
        onDone: () {
          if (!mounted) return;
          setState(() {
            _isTraining = false;
            _logs += "Training finished.\n";
          });
        },
        onError: (e) {
          if (!mounted) return;
          setState(() {
            _isTraining = false;
            _logs += "Training error: $e\n";
          });
        },
      );
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _isTraining = false;
        _logs += "Training connection error: $e\n";
      });
    }
  }

  Future<void> _startBattle() async {
    setState(() {
      _isBattling = true;
      _status = "Initializing Battle...";
      _logs = "";
      _stats = {"w": 0, "l": 0, "d": 0};
      _setupMode = false;
      _whiteMat = 0;
      _blackMat = 0;
      _sessions = [];
      _activeSessionIndex = -1;
      _isViewingHistory = false;
      _isPaused = false;
    });

    final request = http.Request('POST', Uri.parse('${widget.baseUrl}/battle-stream'));
    request.body = jsonEncode({
      "iterations": _games,
      "engine_path": _enginePath,
      "elo": _elo,
      "fen": _fen.isEmpty ? null : _fen,
      "depth": _depth,
      "bot_side": _botSide
    });
    request.headers['Content-Type'] = 'application/json';

    try {
      final response = await request.send();
      _status = "Battle in Progress...";
      
      response.stream.transform(utf8.decoder).transform(const LineSplitter()).listen((line) {
          if (line.isEmpty) return;
          try {
             final data = jsonDecode(line);
             if (data['type'] == 'move') {
                 setState(() {
                     final session = _sessions.firstWhere((s) => s.id == data['game']);
                     session.fens.add(data['fen']);
                     session.moves.add(data['move_san']);
                     
                     // If we are currently viewing this session and NOT looking at history, update the board
                     if (_activeSessionIndex != -1 && _sessions[_activeSessionIndex].id == data['game'] && !_isViewingHistory) {
                        _sessions[_activeSessionIndex].historyIndex = session.fens.length - 1;
                        controller.loadFen(data['fen']);
                        final mat = _calculateMaterialFromFen(data['fen']);
                        _whiteMat = mat['w']!;
                        _blackMat = mat['b']!;
                     }
                 });
             } else if (data['type'] == 'game_start') {
                 setState(() {
                     final newSession = GameSession(
                       id: data['game'],
                       white: data['white'],
                       black: data['black'],
                       fens: [_fen.isEmpty ? "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" : _fen],
                       moves: [],
                     );
                     _sessions.add(newSession);
                     
                     // Automatically switch to the new game if it's the first one OR if we were on the previous game
                     if (_activeSessionIndex == -1 || (_activeSessionIndex == _sessions.length - 2 && !_isViewingHistory)) {
                        _activeSessionIndex = _sessions.length - 1;
                        _isViewingHistory = false;
                        controller.loadFen(newSession.fens[0]);
                     }
                     
                     _logs = "Starting Game ${data['game']}: ${data['white']} vs ${data['black']}\n$_logs";
                 });
             } else if (data['type'] == 'info') {
                 setState(() {
                     _logs = "[INFO] ${data['message']}\n$_logs";
                 });
             } else if (data['type'] == 'warning') {
                 setState(() {
                     _logs = "[⚠️ WARNING] ${data['message']}\n$_logs";
                 });
             } else if (data['type'] == 'game_end') {
                 setState(() {
                     final session = _sessions.firstWhere((s) => s.id == data['game']);
                     session.result = data['result'];
                     _logs = "Game ${data['game']} Finished: ${data['result']}\n$_logs";
                     _stats['w'] = data['current_stats']['w'];
                     _stats['l'] = data['current_stats']['l'];
                     _stats['d'] = data['current_stats']['d'];
                 });
             } else if (data['type'] == 'complete') {
                 setState(() {
                     _status = "Battle Complete!";
                     _isBattling = false;
                 });
             } else if (data['type'] == 'error') {
                 setState(() {
                     _status = "Error: ${data['message']}";
                     _isBattling = false;
                 });
             }
          } catch (e) {
             print("Parse Error: $e");
          }
      }, onDone: () {
          if (_isBattling) {
              setState(() {
                  _isBattling = false;
                  _status = "Connection Closed";
              });
          }
      });
      
    } catch (e) {
      setState(() {
        _status = "Connection Failed: $e";
        _isBattling = false;
      });
    }
  }

  void _goToMove(int index) {
      if (_activeSessionIndex == -1) return;
      final session = _sessions[_activeSessionIndex];
      if (index < 0 || index >= session.fens.length) return;
      setState(() {
          session.historyIndex = index;
          _isViewingHistory = index < session.fens.length - 1;
          controller.loadFen(session.fens[index]);
          final mat = _calculateMaterialFromFen(session.fens[index]);
          _whiteMat = mat['w']!;
          _blackMat = mat['b']!;
      });
  }

  void _switchSession(int index) {
    if (index < 0 || index >= _sessions.length) return;
    setState(() {
      _activeSessionIndex = index;
      final session = _sessions[index];
      controller.loadFen(session.fens[session.historyIndex]);
      final mat = _calculateMaterialFromFen(session.fens[session.historyIndex]);
      _whiteMat = mat['w']!;
      _blackMat = mat['b']!;
      // If we switch to a game that's ongoing and we are at the last move, don't show "history" mode
      _isViewingHistory = session.historyIndex < session.fens.length - 1;
    });
  }

  String _generatePGN() {
      if (_activeSessionIndex == -1) return "";
      final session = _sessions[_activeSessionIndex];
      StringBuffer pgn = StringBuffer();
      pgn.writeln('[Event "Arena Battle"]');
      pgn.writeln('[Site "Experimental Chess Engine"]');
      pgn.writeln('[Date "${DateTime.now().year}.${DateTime.now().month}.${DateTime.now().day}"]');
      pgn.writeln('[White "${session.white}"]');
      pgn.writeln('[Black "${session.black}"]');
      pgn.writeln('[Result "${session.result}"]');
      if (_fen.isNotEmpty) pgn.writeln('[FEN "$_fen"]');
      pgn.writeln();

      for (int i = 0; i < session.moves.length; i++) {
          if (i % 2 == 0) {
              pgn.write("${(i / 2 + 1).toInt()}. ");
          }
          pgn.write("${session.moves[i]} ");
      }
      return pgn.toString();
  }

  void _copyPGN() {
      final pgn = _generatePGN();
      Clipboard.setData(ClipboardData(text: pgn));
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text("PGN copied to clipboard!"))
      );
  }

  Map<String, int> _calculateMaterialFromFen(String fen) {
    if (fen.isEmpty) return {"w": 0, "b": 0};
    
    // We only care about the piece placement part of FEN
    String pieces = fen.split(' ')[0];
    int white = 0;
    int black = 0;
    
    Map<String, int> values = {
      'p': 100, 'n': 320, 'b': 330, 'r': 500, 'q': 900,
      'P': 100, 'N': 320, 'B': 330, 'R': 500, 'Q': 900,
    };
    
    for (int i = 0; i < pieces.length; i++) {
      String char = pieces[i];
      if (values.containsKey(char)) {
        int val = values[char]!;
        if (char == char.toUpperCase()) {
          white += val;
        } else {
          black += val;
        }
      }
    }
    return {"w": white, "b": black};
  }

  void _updateMaterial() {
    final mat = _calculateMaterialFromFen(controller.game.fen);
    setState(() {
      _whiteMat = mat['w']!;
      _blackMat = mat['b']!;
    });
  }

  void _captureCurrentPosition() {
    setState(() {
      _fen = controller.game.fen;
      _updateMaterial();
    });
  }

  void _resetToStandard() {
    setState(() {
      _fen = "";
      controller.resetBoard();
      _updateMaterial();
    });
  }

  @override
  @override
  Widget build(BuildContext context) {
    super.build(context);  // Required for AutomaticKeepAliveClientMixin
    return Scaffold(
      appBar: AppBar(title: const Text("Arena: Bot vs Stockfish")),
      body: Row(
        children: [
          // Left: Control & Stats
          Expanded(
            flex: 1,
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                children: [
                  Expanded(
                    flex: 3,
                    child: SingleChildScrollView(
                      child: Column(
                        children: [
                          Card(
                            elevation: 4,
                            child: Padding(
                              padding: const EdgeInsets.all(16.0),
                              child: Column(
                                children: [
                                  const Text("Battle Configuration", style: TextStyle(fontWeight: FontWeight.bold)),
                                  const SizedBox(height: 10),
                                  Row(
                                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                    children: [
                                      const Text("Setup Mode:"),
                                      Switch(
                                        value: _setupMode,
                                        onChanged: _isBattling ? null : (v) => setState(() => _setupMode = v),
                                      ),
                                    ],
                                  ),
                                  if (_setupMode) ...[
                                    const SizedBox(height: 5),
                                    const Text("Drag pieces on the board →", style: TextStyle(fontSize: 12, fontStyle: FontStyle.italic)),
                                    const SizedBox(height: 5),
                                    Row(
                                      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                                      children: [
                                        ElevatedButton.icon(
                                          icon: const Icon(Icons.check, size: 16),
                                          label: const Text("Use Position", style: TextStyle(fontSize: 12)),
                                          onPressed: _captureCurrentPosition,
                                        ),
                                        ElevatedButton.icon(
                                          icon: const Icon(Icons.refresh, size: 16),
                                          label: const Text("Reset", style: TextStyle(fontSize: 12)),
                                          onPressed: _resetToStandard,
                                        ),
                                      ],
                                    ),
                                  ],
                                  const SizedBox(height: 10),
                                  Text("Stockfish Elo: $_elo"),
                                  Slider(
                                    value: _elo.toDouble(), 
                                    min: 100, max: 3000, divisions: 29, 
                                    label: _elo.toString(),
                                    onChanged: (v) => setState(() => _elo = v.toInt())
                                  ),
                                  Text("Games: $_games"),
                                  Slider(
                                    value: _games.toDouble(), 
                                    min: 1, max: 100, divisions: 99, 
                                    label: _games.toString(),
                                    onChanged: (v) => setState(() => _games = v.toInt())
                                  ),
                                  Text("Search Depth: $_depth ply"),
                                  Slider(
                                    value: _depth.toDouble(), 
                                    min: 1, max: 6, divisions: 5, 
                                    label: "$_depth ply",
                                    onChanged: (v) => setState(() => _depth = v.toInt())
                                  ),
                                  const SizedBox(height: 10),
                                  Row(
                                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                    children: [
                                      const Text("Bot Plays:"),
                                      DropdownButton<String>(
                                        value: _botSide,
                                        items: const [
                                          DropdownMenuItem(value: "alternate", child: Text("Alternate")),
                                          DropdownMenuItem(value: "white", child: Text("White")),
                                          DropdownMenuItem(value: "black", child: Text("Black")),
                                        ],
                                        onChanged: (v) => setState(() => _botSide = v!),
                                      ),
                                    ],
                                  ),
                                  const SizedBox(height: 10),
                                  Row(
                                    children: [
                                      Expanded(
                                        child: ElevatedButton.icon(
                                          icon: const Icon(Icons.flash_on),
                                          label: Text(_isBattling ? "Fighting..." : "Start Battle"),
                                          style: ElevatedButton.styleFrom(backgroundColor: Colors.red, foregroundColor: Colors.white),
                                          onPressed: _isBattling ? null : _startBattle
                                        ),
                                      ),
                                      if (_isBattling) ...[
                                        const SizedBox(width: 8),
                                        IconButton.filled(
                                          icon: Icon(_isPaused ? Icons.play_arrow : Icons.pause),
                                          onPressed: _togglePause,
                                          tooltip: _isPaused ? "Resume Battle" : "Pause Battle",
                                        ),
                                      ],
                                    ],
                                  ),
                                  const SizedBox(height: 8),
                                  ElevatedButton.icon(
                                    icon: const Icon(Icons.school),
                                    label: Text(_isTraining ? "Training..." : "Learn from History"),
                                    style: ElevatedButton.styleFrom(backgroundColor: Colors.green, foregroundColor: Colors.white),
                                    onPressed: _isTraining ? null : _learnFromHistory,
                                  ),
                                ],
                              ),
                            ),
                          ),
                          const SizedBox(height: 20),
                          Card(
                            color: Colors.blue[50],
                            child: Padding(
                              padding: const EdgeInsets.all(16.0),
                              child: Column(
                                 children: [
                                   const Text("Live Results (Bot Perspective)", style: TextStyle(fontWeight: FontWeight.bold)),
                                   const SizedBox(height: 10),
                                   Row(
                                     mainAxisAlignment: MainAxisAlignment.spaceAround,
                                     children: [
                                       Column(children: [const Text("Wins", style: TextStyle(color: Colors.green)), Text("${_stats['w']}", style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold))]),
                                       Column(children: [const Text("Draws", style: TextStyle(color: Colors.grey)), Text("${_stats['d']}", style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold))]),
                                       Column(children: [const Text("Losses", style: TextStyle(color: Colors.red)), Text("${_stats['l']}", style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold))]),
                                     ],
                                   )
                                 ],
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                  if (_sessions.isNotEmpty) ...[
                    const SizedBox(height: 20),
                    const Text("Current Set Games:", style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 5),
                    Expanded(
                      flex: 2,
                      child: ListView.builder(
                        itemCount: _sessions.length,
                        itemBuilder: (context, index) {
                          final session = _sessions[index];
                          final isActive = _activeSessionIndex == index;
                          return ListTile(
                            dense: true,
                            selected: isActive,
                            selectedTileColor: Colors.blue[50],
                            title: Text("Game ${session.id}: ${session.white} vs ${session.black}"),
                            subtitle: Text("Result: ${session.result} | Moves: ${session.moves.length}"),
                            leading: CircleAvatar(
                              radius: 12,
                              child: Text("${session.id}", style: const TextStyle(fontSize: 10)),
                            ),
                            onTap: () => _switchSession(index),
                          );
                        },
                      ),
                    ),
                  ],
                  const SizedBox(height: 20),
                  const Text("Battle Logs:"),
                  if (_sessions.isEmpty) Expanded(
                    flex: 1,
                    child: Container(
                      padding: const EdgeInsets.all(8),
                      color: Colors.black12,
                      child: SingleChildScrollView(child: Text(_logs)),
                    ),
                  ) else Container(
                      height: 100,
                      padding: const EdgeInsets.all(8),
                      color: Colors.black12,
                      child: SingleChildScrollView(child: Text(_logs)),
                    )
                 ],
               ),
             ),
           ),
           
           // Right: Board
           Expanded(
             flex: 1,
             child: Padding(
               padding: const EdgeInsets.all(16.0),
               child: _activeSessionIndex == -1 
               ? const Center(child: Text("Start a battle to see the board"))
               : Column(
                 children: [
                   Text(_status, style: const TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
                   const SizedBox(height: 10),
                   // Material Bar
                   Container(
                     height: 25,
                     width: double.infinity,
                     decoration: BoxDecoration(
                       borderRadius: BorderRadius.circular(12),
                       border: Border.all(color: Colors.black26),
                     ),
                     clipBehavior: Clip.antiAlias,
                     child: Row(
                       children: [
                         Expanded(
                           flex: _whiteMat == 0 && _blackMat == 0 ? 50 : _whiteMat,
                           child: Container(
                             color: Colors.white,
                             alignment: Alignment.centerLeft,
                             padding: const EdgeInsets.only(left: 8),
                             child: Text("$_whiteMat", style: const TextStyle(color: Colors.black, fontWeight: FontWeight.bold, fontSize: 12)),
                           ),
                         ),
                         Expanded(
                           flex: _whiteMat == 0 && _blackMat == 0 ? 50 : _blackMat,
                           child: Container(
                             color: Colors.black,
                             alignment: Alignment.centerRight,
                             padding: const EdgeInsets.only(right: 8),
                             child: Text("$_blackMat", style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold, fontSize: 12)),
                           ),
                         ),
                       ],
                     ),
                   ),
                   const SizedBox(height: 10),
                   Card(
                       color: Colors.black12,
                       child: Padding(
                           padding: const EdgeInsets.all(8.0),
                           child: Text(_sessions[_activeSessionIndex].black, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold))
                       )
                   ),
                   ChessBoard(
                     controller: controller,
                     boardColor: BoardColor.green,
                     boardOrientation: PlayerColor.white,
                     enableUserMoves: _setupMode && !_isBattling,
                   ),
                   // Playback Controls
                   Container(
                     margin: const EdgeInsets.symmetric(vertical: 8),
                     padding: const EdgeInsets.all(4),
                     decoration: BoxDecoration(
                       color: Colors.white,
                       borderRadius: BorderRadius.circular(8),
                       boxShadow: [BoxShadow(color: Colors.black.withOpacity(0.05), blurRadius: 4)]
                     ),
                     child: Row(
                       mainAxisAlignment: MainAxisAlignment.center,
                       children: [
                         IconButton(icon: const Icon(Icons.first_page), onPressed: () => _goToMove(0)),
                         IconButton(icon: const Icon(Icons.chevron_left), onPressed: () => _goToMove(_sessions[_activeSessionIndex].historyIndex - 1)),
                         Container(
                           padding: const EdgeInsets.symmetric(horizontal: 12),
                           child: Text("${_sessions[_activeSessionIndex].historyIndex} / ${_sessions[_activeSessionIndex].fens.length - 1}", style: const TextStyle(fontWeight: FontWeight.bold)),
                         ),
                         IconButton(icon: const Icon(Icons.chevron_right), onPressed: () => _goToMove(_sessions[_activeSessionIndex].historyIndex + 1)),
                         IconButton(icon: const Icon(Icons.last_page), onPressed: () => _goToMove(_sessions[_activeSessionIndex].fens.length - 1)),
                         const VerticalDivider(width: 20),
                         IconButton(
                           icon: const Icon(Icons.copy), 
                           tooltip: "Copy PGN",
                           onPressed: _sessions[_activeSessionIndex].moves.isEmpty ? null : _copyPGN
                         ),
                       ],
                     ),
                   ),
                   // Move List
                   Expanded(
                     child: Container(
                       width: double.infinity,
                       padding: const EdgeInsets.all(8),
                       decoration: BoxDecoration(
                         color: Colors.white70,
                         borderRadius: BorderRadius.circular(8),
                         border: Border.all(color: Colors.black12)
                       ),
                       child: SingleChildScrollView(
                         child: Wrap(
                           spacing: 4,
                           runSpacing: 4,
                           children: List.generate(_sessions[_activeSessionIndex].moves.length, (index) {
                              bool isWhite = index % 2 == 0;
                              int moveNum = (index / 2 + 1).toInt();
                              bool isCurrent = _sessions[_activeSessionIndex].historyIndex == index + 1;
                              
                              return InkWell(
                                onTap: () => _goToMove(index + 1),
                                child: Container(
                                  padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 2),
                                  decoration: BoxDecoration(
                                    color: isCurrent ? Colors.blue[100] : Colors.transparent,
                                    borderRadius: BorderRadius.circular(4),
                                  ),
                                  child: Text(
                                    "${isWhite ? '$moveNum.' : ''}${_sessions[_activeSessionIndex].moves[index]}",
                                    style: TextStyle(
                                      fontWeight: isCurrent ? FontWeight.bold : FontWeight.normal,
                                      color: isCurrent ? Colors.blue[900] : Colors.black87,
                                    ),
                                  ),
                                ),
                              );
                           }),
                         ),
                       ),
                     ),
                   ),
                   Card(
                       color: Colors.white,
                       child: Padding(
                           padding: const EdgeInsets.all(8.0),
                           child: Text(_sessions[_activeSessionIndex].white, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold))
                       )
                   ),
                  if (_fen.isNotEmpty) ...[
                    const SizedBox(height: 10),
                    Container(
                      padding: const EdgeInsets.all(8),
                      decoration: BoxDecoration(
                        color: Colors.green[50],
                        border: Border.all(color: Colors.green),
                        borderRadius: BorderRadius.circular(4),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text("Battle Start Position:", style: TextStyle(fontWeight: FontWeight.bold, fontSize: 12)),
                          const SizedBox(height: 4),
                          Text(_fen, style: const TextStyle(fontSize: 10, fontFamily: 'monospace')),
                        ],
                      ),
                    ),
                  ],
                ],
              ),
            ),
          )
        ],
      ),
    );
  }
}
