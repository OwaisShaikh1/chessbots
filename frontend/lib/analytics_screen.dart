import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';
import 'dart:async';

class AnalyticsScreen extends StatefulWidget {
  final String baseUrl;
  const AnalyticsScreen({super.key, required this.baseUrl});

  @override
  State<AnalyticsScreen> createState() => _AnalyticsScreenState();
}

class _AnalyticsScreenState extends State<AnalyticsScreen> {
  List<dynamic> _history = [];
  List<dynamic> _recentGames = [];
  bool _loading = true;
  Timer? _timer;
  bool _isTraining = false;
  String _trainingStatus = "";

  @override
  void initState() {
    super.initState();
    _fetchData();
    _timer = Timer.periodic(const Duration(seconds: 5), (timer) => _fetchData());
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  Future<void> _fetchData() async {
    try {
      final response = await http.get(Uri.parse('${widget.baseUrl}/analytics'));
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (mounted) {
          setState(() {
            _history = data['history'];
            _recentGames = data['recent'];
            _loading = false;
          });
        }
      }
    } catch (e) {
      print("Analytics Error: $e");
    }
  }

  Future<void> _trainFromHistory() async {
    setState(() {
      _isTraining = true;
      _trainingStatus = "Fetching battle data...";
    });

    try {
      final request = http.Request('POST', Uri.parse('${widget.baseUrl}/train-history-stream'));
      request.headers['Content-Type'] = 'application/json';
      request.body = jsonEncode({"batch_size": 32, "epochs": 5});
      
      final response = await http.Client().send(request);
      
      response.stream.transform(utf8.decoder).transform(const LineSplitter()).listen((line) {
        if (line.isEmpty) return;
        try {
          final data = jsonDecode(line);
          if (data['type'] == 'start') {
            setState(() => _trainingStatus = "Starting: ${data['total_moves']} moves across ${data['epochs']} epochs");
          } else if (data['type'] == 'progress') {
            setState(() => _trainingStatus = "Epoch ${data['epoch']} | Batch ${data['batch']}/${data['total_batches']} | Loss: ${data['loss'].toStringAsFixed(4)}");
          } else if (data['type'] == 'complete') {
            setState(() {
              _trainingStatus = "Success: ${data['message']}";
              _isTraining = false;
            });
            _fetchData();
          } else if (data['type'] == 'error') {
            setState(() {
              _trainingStatus = "Error: ${data['message']}";
              _isTraining = false;
            });
          }
        } catch (e) {
          print("Stream Decode Error: $e");
        }
      });
    } catch (e) {
      setState(() {
        _isTraining = false;
        _trainingStatus = "Connection Failed";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Training Analytics"),
        actions: [
          IconButton(onPressed: _fetchData, icon: const Icon(Icons.refresh))
        ],
      ),
      body: _loading 
        ? const Center(child: CircularProgressIndicator()) 
        : SingleChildScrollView(
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                   if (_isTraining) ...[
                     Container(
                       padding: const EdgeInsets.all(12),
                       decoration: BoxDecoration(color: Colors.blue[50], borderRadius: BorderRadius.circular(8)),
                       child: Column(
                         children: [
                            const LinearProgressIndicator(),
                            const SizedBox(height: 8),
                            Text(_trainingStatus, style: const TextStyle(fontWeight: FontWeight.bold, color: Colors.blue)),
                         ],
                       ),
                     ),
                     const SizedBox(height: 20),
                   ],
                   Row(
                     mainAxisAlignment: MainAxisAlignment.spaceBetween,
                     children: [
                        const Text("Intelligence Evolution", style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
                        ElevatedButton.icon(
                          onPressed: _isTraining ? null : _trainFromHistory,
                          icon: const Icon(Icons.psychology),
                          label: const Text("Learn from Battles"),
                          style: ElevatedButton.styleFrom(backgroundColor: Colors.orange[100], foregroundColor: Colors.orange[900]),
                        )
                     ],
                   ),
                   const Text("Tracking win rate percentage over time", style: TextStyle(fontSize: 12, color: Colors.grey)),
                   const SizedBox(height: 10),
                   SizedBox(
                     height: 250,
                     child: _buildChart(),
                   ),
                   const SizedBox(height: 30),
                   const Text("Recent Games Tracker", style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
                   const SizedBox(height: 10),
                   _buildRecentList(),
                ],
              ),
            ),
        ),
    );
  }

  Widget _buildChart() {
    if (_history.isEmpty) return const Center(child: Text("No training history yet."));

    List<FlSpot> winSpots = [];
    List<FlSpot> drawSpots = [];

    for (var batch in _history) {
        double x = batch['game'].toDouble();
        double winRate = batch['win_rate'].toDouble() * 100;
        double drawRate = batch['draw_rate'].toDouble() * 100;
        
        winSpots.add(FlSpot(x, winRate));
        drawSpots.add(FlSpot(x, drawRate));
    }

    return LineChart(
      LineChartData(
        minY: 0,
        maxY: 100,
        lineBarsData: [
          LineChartBarData(
            spots: winSpots,
            isCurved: true,
            color: Colors.green,
            barWidth: 3,
            dotData: const FlDotData(show: false),
            belowBarData: BarAreaData(show: true, color: Colors.green.withOpacity(0.1)),
          ),
          LineChartBarData(
            spots: drawSpots,
            isCurved: true,
            color: Colors.grey,
            barWidth: 2,
            dashArray: [5, 5],
            dotData: const FlDotData(show: false),
          ),
        ],
        titlesData: const FlTitlesData(
            leftTitles: AxisTitles(sideTitles: SideTitles(showTitles: true, reservedSize: 40)),
            bottomTitles: AxisTitles(sideTitles: SideTitles(showTitles: true, reservedSize: 30)),
            topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
            rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
        ),
        gridData: const FlGridData(show: true, drawVerticalLine: false),
        borderData: FlBorderData(show: true, border: Border.all(color: Colors.black12)),
      ),
    );
  }

  Widget _buildRecentList() {
      if (_recentGames.isEmpty) return const Center(child: Text("No games recorded yet."));
      return ListView.builder(
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          itemCount: _recentGames.length,
          itemBuilder: (context, index) {
              final game = _recentGames[index];
              Color color = Colors.grey[100]!;
              if (game['result'] == 'Win') color = Colors.green[50]!;
              if (game['result'] == 'Loss') color = Colors.red[50]!;
              
              return Card(
                  color: color,
                  elevation: 0,
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8), side: const BorderSide(color: Colors.black12)),
                  child: ListTile(
                      leading: Icon(
                        game['result'] == 'Win' ? Icons.emoji_events : Icons.history,
                        color: game['result'] == 'Win' ? Colors.orange : Colors.grey,
                      ),
                      title: Text("Result: ${game['result']}", style: const TextStyle(fontWeight: FontWeight.bold)),
                      subtitle: Text("Opponent: ${game['opponent']}\nStart: ${game['start_fen']}"),
                      trailing: Text("#${game['game_id'].toString().substring(0, 4)}", style: const TextStyle(fontSize: 10, color: Colors.grey)),
                      isThreeLine: true,
                  ),
              );
          },
      );
  }
}
