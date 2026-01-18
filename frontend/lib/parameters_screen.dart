import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';

class ParametersScreen extends StatefulWidget {
  final String baseUrl;
  const ParametersScreen({super.key, required this.baseUrl});

  @override
  State<ParametersScreen> createState() => _ParametersScreenState();
}

class _ParametersScreenState extends State<ParametersScreen> {
  bool _loading = true;
  bool _saving = false;
  
  // Material values
  int _pawnValue = 100;
  int _knightValue = 320;
  int _bishopValue = 330;
  int _rookValue = 500;
  int _queenValue = 900;
  
  // Positional parameters
  int _mobilityWeight = 5;
  int _castlingBonus = 50;
  int _kingExposurePenalty = 25;
  int _kingSafetyPenalty = 30;
  int _rookOpenFile = 15;
  int _rookSemiOpen = 10;
  int _passedPawnScale = 10;
  int _threatDivisor = 5;
  int _lpdoDivisor = 2;
  
  // Queen safety and pins
  int _queenEarlyPenalty = 20;
  int _queenExposurePenalty = 40;
  int _pinPenalty = 50;
  
  // Neural blend
  int _neuralBlend = 50;
  
  // Search
  int _quiescenceDepth = 3;
  
  // Training
  double _learningRate = 0.0002;

  @override
  void initState() {
    super.initState();
    _fetchParameters();
  }

  Future<void> _fetchParameters() async {
    try {
      final response = await http.get(Uri.parse('${widget.baseUrl}/parameters'));
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        setState(() {
          // Material
          _pawnValue = data['material']['pawn'];
          _knightValue = data['material']['knight'];
          _bishopValue = data['material']['bishop'];
          _rookValue = data['material']['rook'];
          _queenValue = data['material']['queen'];
          
          // Positional
          _mobilityWeight = data['positional']['mobility_weight'];
          _castlingBonus = data['positional']['castling_bonus'];
          _kingExposurePenalty = data['positional']['king_exposure_penalty'];
          _kingSafetyPenalty = data['positional']['king_safety_penalty'];
          _rookOpenFile = data['positional']['rook_open_file'];
          _rookSemiOpen = data['positional']['rook_semi_open'];
          _passedPawnScale = data['positional']['passed_pawn_scale'];
          _threatDivisor = data['positional']['threat_divisor'];
          _lpdoDivisor = data['positional']['lpdo_divisor'];
          _queenEarlyPenalty = data['positional']['queen_early_penalty'];
          _queenExposurePenalty = data['positional']['queen_exposure_penalty'];
          _pinPenalty = data['positional']['pin_penalty'];
          
          // Neural
          _neuralBlend = data['neural']['neural_blend'];
          
          // Search
          _quiescenceDepth = data['search']['quiescence_depth'];
          
          // Training
          _learningRate = data['training']['learning_rate'];
          
          _loading = false;
        });
      }
    } catch (e) {
      setState(() => _loading = false);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Failed to load parameters: $e"))
      );
    }
  }

  Future<void> _saveParameters() async {
    setState(() => _saving = true);
    
    try {
      final response = await http.post(
        Uri.parse('${widget.baseUrl}/parameters'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({
          'pawn_value': _pawnValue,
          'knight_value': _knightValue,
          'bishop_value': _bishopValue,
          'rook_value': _rookValue,
          'queen_value': _queenValue,
          'mobility_weight': _mobilityWeight,
          'castling_bonus': _castlingBonus,
          'king_exposure_penalty': _kingExposurePenalty,
          'king_safety_penalty': _kingSafetyPenalty,
          'rook_open_file': _rookOpenFile,
          'rook_semi_open': _rookSemiOpen,
          'passed_pawn_scale': _passedPawnScale,
          'threat_divisor': _threatDivisor,
          'lpdo_divisor': _lpdoDivisor,
          'queen_early_penalty': _queenEarlyPenalty,
          'queen_exposure_penalty': _queenExposurePenalty,
          'pin_penalty': _pinPenalty,
          'neural_blend': _neuralBlend,
          'quiescence_depth': _quiescenceDepth,
          'learning_rate': _learningRate,
        }),
      );
      
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text("Updated ${data['count']} parameters!"),
            backgroundColor: Colors.green,
          )
        );
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Failed to save: $e"), backgroundColor: Colors.red)
      );
    }
    
    setState(() => _saving = false);
  }

  void _resetToDefaults() {
    setState(() {
      _pawnValue = 100;
      _knightValue = 320;
      _bishopValue = 330;
      _rookValue = 500;
      _queenValue = 900;
      _mobilityWeight = 5;
      _castlingBonus = 50;
      _kingExposurePenalty = 25;
      _kingSafetyPenalty = 30;
      _rookOpenFile = 15;
      _rookSemiOpen = 10;
      _passedPawnScale = 10;
      _threatDivisor = 5;
      _lpdoDivisor = 2;
      _queenEarlyPenalty = 20;
      _queenExposurePenalty = 40;
      _pinPenalty = 50;
      _neuralBlend = 50;
      _quiescenceDepth = 3;
      _learningRate = 0.0002;
    });
  }

  Widget _buildSlider({
    required String label,
    required int value,
    required int min,
    required int max,
    required Function(int) onChanged,
    String? suffix,
    String? tooltip,
  }) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 180,
            child: Row(
              children: [
                Text(label, style: const TextStyle(fontSize: 13)),
                if (tooltip != null) ...[
                  const SizedBox(width: 4),
                  Tooltip(
                    message: tooltip,
                    child: Icon(Icons.info_outline, size: 14, color: Colors.grey[600]),
                  ),
                ],
              ],
            ),
          ),
          Expanded(
            child: Slider(
              value: value.toDouble(),
              min: min.toDouble(),
              max: max.toDouble(),
              divisions: max - min,
              label: "$value${suffix ?? ''}",
              onChanged: (v) => onChanged(v.toInt()),
            ),
          ),
          SizedBox(
            width: 60,
            child: Text("$value${suffix ?? ''}", style: const TextStyle(fontWeight: FontWeight.bold)),
          ),
        ],
      ),
    );
  }

  Widget _buildDoubleSlider({
    required String label,
    required double value,
    required double min,
    required double max,
    required int divisions,
    required Function(double) onChanged,
    String? tooltip,
  }) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 180,
            child: Row(
              children: [
                Text(label, style: const TextStyle(fontSize: 13)),
                if (tooltip != null) ...[
                  const SizedBox(width: 4),
                  Tooltip(
                    message: tooltip,
                    child: Icon(Icons.info_outline, size: 14, color: Colors.grey[600]),
                  ),
                ],
              ],
            ),
          ),
          Expanded(
            child: Slider(
              value: value,
              min: min,
              max: max,
              divisions: divisions,
              label: value.toStringAsFixed(5),
              onChanged: onChanged,
            ),
          ),
          SizedBox(
            width: 80,
            child: Text(value.toStringAsFixed(5), style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 11)),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Bot Parameters"),
        actions: [
          IconButton(
            onPressed: _resetToDefaults,
            icon: const Icon(Icons.restore),
            tooltip: "Reset to Defaults",
          ),
          const SizedBox(width: 8),
          ElevatedButton.icon(
            onPressed: _saving ? null : _saveParameters,
            icon: _saving 
              ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2))
              : const Icon(Icons.save),
            label: const Text("Apply Changes"),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.green,
              foregroundColor: Colors.white,
            ),
          ),
          const SizedBox(width: 16),
        ],
      ),
      body: _loading 
        ? const Center(child: CircularProgressIndicator())
        : SingleChildScrollView(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Material Values Section
                _buildSectionCard(
                  title: "Material Values",
                  icon: Icons.diamond,
                  color: Colors.amber,
                  description: "Centipawn values for each piece type",
                  children: [
                    _buildSlider(
                      label: "Pawn",
                      value: _pawnValue,
                      min: 50, max: 200,
                      onChanged: (v) => setState(() => _pawnValue = v),
                      suffix: " cp",
                      tooltip: "Base value of a pawn in centipawns",
                    ),
                    _buildSlider(
                      label: "Knight",
                      value: _knightValue,
                      min: 200, max: 400,
                      onChanged: (v) => setState(() => _knightValue = v),
                      suffix: " cp",
                    ),
                    _buildSlider(
                      label: "Bishop",
                      value: _bishopValue,
                      min: 200, max: 400,
                      onChanged: (v) => setState(() => _bishopValue = v),
                      suffix: " cp",
                    ),
                    _buildSlider(
                      label: "Rook",
                      value: _rookValue,
                      min: 400, max: 600,
                      onChanged: (v) => setState(() => _rookValue = v),
                      suffix: " cp",
                    ),
                    _buildSlider(
                      label: "Queen",
                      value: _queenValue,
                      min: 800, max: 1200,
                      onChanged: (v) => setState(() => _queenValue = v),
                      suffix: " cp",
                    ),
                  ],
                ),
                
                const SizedBox(height: 16),
                
                // Positional Parameters Section
                _buildSectionCard(
                  title: "Positional Evaluation",
                  icon: Icons.grid_on,
                  color: Colors.blue,
                  description: "Bonuses and penalties for positional features",
                  children: [
                    _buildSlider(
                      label: "Mobility Weight",
                      value: _mobilityWeight,
                      min: 0, max: 20,
                      onChanged: (v) => setState(() => _mobilityWeight = v),
                      suffix: " cp/sq",
                      tooltip: "Bonus per attacked square (non-pawn pieces)",
                    ),
                    _buildSlider(
                      label: "Castling Bonus",
                      value: _castlingBonus,
                      min: 0, max: 100,
                      onChanged: (v) => setState(() => _castlingBonus = v),
                      suffix: " cp",
                      tooltip: "Reward for having castled",
                    ),
                    _buildSlider(
                      label: "King Exposure",
                      value: _kingExposurePenalty,
                      min: 0, max: 50,
                      onChanged: (v) => setState(() => _kingExposurePenalty = v),
                      suffix: " cp",
                      tooltip: "Penalty per attacked square near king",
                    ),
                    _buildSlider(
                      label: "King Safety",
                      value: _kingSafetyPenalty,
                      min: 0, max: 60,
                      onChanged: (v) => setState(() => _kingSafetyPenalty = v),
                      suffix: " cp",
                      tooltip: "Penalty for enemy piece adjacent to king",
                    ),
                    _buildSlider(
                      label: "Rook Open File",
                      value: _rookOpenFile,
                      min: 0, max: 40,
                      onChanged: (v) => setState(() => _rookOpenFile = v),
                      suffix: " cp",
                    ),
                    _buildSlider(
                      label: "Rook Semi-Open",
                      value: _rookSemiOpen,
                      min: 0, max: 30,
                      onChanged: (v) => setState(() => _rookSemiOpen = v),
                      suffix: " cp",
                    ),
                    _buildSlider(
                      label: "Passed Pawn Scale",
                      value: _passedPawnScale,
                      min: 0, max: 30,
                      onChanged: (v) => setState(() => _passedPawnScale = v),
                      suffix: " cp/rank",
                      tooltip: "Bonus per rank for passed pawns",
                    ),
                    _buildSlider(
                      label: "Threat Divisor",
                      value: _threatDivisor,
                      min: 1, max: 10,
                      onChanged: (v) => setState(() => _threatDivisor = v),
                      tooltip: "Divide piece value by this for threat penalty (higher = less penalty)",
                    ),
                    _buildSlider(
                      label: "LPDO Divisor",
                      value: _lpdoDivisor,
                      min: 1, max: 5,
                      onChanged: (v) => setState(() => _lpdoDivisor = v),
                      tooltip: "Loose Piece Detection: divide piece value (higher = less penalty)",
                    ),
                  ],
                ),
                
                const SizedBox(height: 16),
                
                // Queen Safety & Pins Section
                _buildSectionCard(
                  title: "Queen Safety & Pins",
                  icon: Icons.security,
                  color: Colors.red,
                  description: "Evaluation of queen safety and pinned pieces",
                  children: [
                    _buildSlider(
                      label: "Queen Early Penalty",
                      value: _queenEarlyPenalty,
                      min: 0, max: 50,
                      onChanged: (v) => setState(() => _queenEarlyPenalty = v),
                      suffix: " cp",
                      tooltip: "Penalty for developing queen before 2 minor pieces",
                    ),
                    _buildSlider(
                      label: "Queen Exposure",
                      value: _queenExposurePenalty,
                      min: 0, max: 80,
                      onChanged: (v) => setState(() => _queenExposurePenalty = v),
                      suffix: " cp",
                      tooltip: "Penalty when queen is under attack",
                    ),
                    _buildSlider(
                      label: "Pin Penalty",
                      value: _pinPenalty,
                      min: 0, max: 100,
                      onChanged: (v) => setState(() => _pinPenalty = v),
                      suffix: "%",
                      tooltip: "Penalty as % of piece value when pinned to king",
                    ),
                  ],
                ),
                
                const SizedBox(height: 16),
                
                // Neural Network Section
                _buildSectionCard(
                  title: "Neural Network",
                  icon: Icons.psychology,
                  color: Colors.purple,
                  description: "Control how much the neural network influences evaluation",
                  children: [
                    _buildSlider(
                      label: "Neural Blend",
                      value: _neuralBlend,
                      min: 0, max: 100,
                      onChanged: (v) => setState(() => _neuralBlend = v),
                      suffix: "%",
                      tooltip: "0% = pure heuristic, 100% = pure neural network",
                    ),
                    Container(
                      margin: const EdgeInsets.only(top: 8),
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(
                        color: Colors.purple[50],
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Row(
                        children: [
                          Container(
                            width: 100,
                            height: 8,
                            decoration: BoxDecoration(
                              borderRadius: BorderRadius.circular(4),
                              gradient: LinearGradient(
                                colors: [Colors.blue[300]!, Colors.purple[300]!],
                              ),
                            ),
                          ),
                          const SizedBox(width: 12),
                          Text(
                            _neuralBlend == 0 ? "Pure Heuristic (Classic)" :
                            _neuralBlend == 100 ? "Pure Neural (AlphaZero-style)" :
                            "${100 - _neuralBlend}% Heuristic + $_neuralBlend% Neural",
                            style: TextStyle(fontSize: 12, color: Colors.purple[800]),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
                
                const SizedBox(height: 16),
                
                // Search Parameters Section
                _buildSectionCard(
                  title: "Search Engine",
                  icon: Icons.search,
                  color: Colors.green,
                  description: "Configure the search algorithm behavior",
                  children: [
                    _buildSlider(
                      label: "Quiescence Depth",
                      value: _quiescenceDepth,
                      min: 1, max: 8,
                      onChanged: (v) => setState(() => _quiescenceDepth = v),
                      suffix: " ply",
                      tooltip: "How deep to search captures after main search",
                    ),
                  ],
                ),
                
                const SizedBox(height: 16),
                
                // Training Parameters Section
                _buildSectionCard(
                  title: "Training",
                  icon: Icons.school,
                  color: Colors.orange,
                  description: "Neural network training configuration",
                  children: [
                    _buildDoubleSlider(
                      label: "Learning Rate",
                      value: _learningRate,
                      min: 0.00001, max: 0.01,
                      divisions: 100,
                      onChanged: (v) => setState(() => _learningRate = v),
                      tooltip: "Adam optimizer learning rate (lower = more stable, slower)",
                    ),
                  ],
                ),
                
                const SizedBox(height: 32),
              ],
            ),
          ),
    );
  }

  Widget _buildSectionCard({
    required String title,
    required IconData icon,
    required Color color,
    required String description,
    required List<Widget> children,
  }) {
    return Card(
      elevation: 2,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: color.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Icon(icon, color: color),
                ),
                const SizedBox(width: 12),
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                    Text(description, style: TextStyle(fontSize: 12, color: Colors.grey[600])),
                  ],
                ),
              ],
            ),
            const Divider(height: 24),
            ...children,
          ],
        ),
      ),
    );
  }
}
