import 'dart:convert';
import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:http/http.dart' as http;
import 'package:fl_chart/fl_chart.dart';

class TrainingState {
  final bool isHistoryTraining;
  final String historyTrainingStatus;
  final double historyTrainingProgress;
  final List<FlSpot> lossSpots;
  final int currentEpoch;
  final int totalEpochs;
  
  // Enhanced training metrics
  final double bestLoss;
  final double worstLoss;
  final int totalPositionsTrained;
  final double avgLossPerEpoch;
  final List<double> epochLosses;  // Track loss per epoch for trend analysis
  final String trainingQuality;    // "improving", "stagnant", "overfitting"

  TrainingState({
    this.isHistoryTraining = false,
    this.historyTrainingStatus = "IDLE",
    this.historyTrainingProgress = 0.0,
    this.lossSpots = const [],
    this.currentEpoch = 0,
    this.totalEpochs = 0,
    this.bestLoss = double.infinity,
    this.worstLoss = 0.0,
    this.totalPositionsTrained = 0,
    this.avgLossPerEpoch = 0.0,
    this.epochLosses = const [],
    this.trainingQuality = "unknown",
  });

  TrainingState copyWith({
    bool? isHistoryTraining,
    String? historyTrainingStatus,
    double? historyTrainingProgress,
    List<FlSpot>? lossSpots,
    int? currentEpoch,
    int? totalEpochs,
    double? bestLoss,
    double? worstLoss,
    int? totalPositionsTrained,
    double? avgLossPerEpoch,
    List<double>? epochLosses,
    String? trainingQuality,
  }) {
    return TrainingState(
      isHistoryTraining: isHistoryTraining ?? this.isHistoryTraining,
      historyTrainingStatus: historyTrainingStatus ?? this.historyTrainingStatus,
      historyTrainingProgress: historyTrainingProgress ?? this.historyTrainingProgress,
      lossSpots: lossSpots ?? this.lossSpots,
      currentEpoch: currentEpoch ?? this.currentEpoch,
      totalEpochs: totalEpochs ?? this.totalEpochs,
      bestLoss: bestLoss ?? this.bestLoss,
      worstLoss: worstLoss ?? this.worstLoss,
      totalPositionsTrained: totalPositionsTrained ?? this.totalPositionsTrained,
      avgLossPerEpoch: avgLossPerEpoch ?? this.avgLossPerEpoch,
      epochLosses: epochLosses ?? this.epochLosses,
      trainingQuality: trainingQuality ?? this.trainingQuality,
    );
  }
  
  // Helper to analyze training trend
  String analyzeTrainingTrend() {
    if (epochLosses.length < 2) return "unknown";
    
    // Check last 3 epochs if available
    int checkCount = epochLosses.length >= 3 ? 3 : epochLosses.length;
    List<double> recent = epochLosses.sublist(epochLosses.length - checkCount);
    
    bool decreasing = true;
    bool increasing = true;
    for (int i = 1; i < recent.length; i++) {
      if (recent[i] >= recent[i-1]) decreasing = false;
      if (recent[i] <= recent[i-1]) increasing = false;
    }
    
    if (decreasing) return "improving";
    if (increasing) return "overfitting";
    return "stagnant";
  }
}

class TrainingNotifier extends StateNotifier<TrainingState> {
  TrainingNotifier() : super(TrainingState());

  StreamSubscription? _subscription;

  Future<void> startHistoryTraining(String baseUrl, int batchSize, int epochs) async {
    if (state.isHistoryTraining) return;

    state = state.copyWith(
      isHistoryTraining: true,
      historyTrainingStatus: "Initializing...",
      historyTrainingProgress: 0.0,
      lossSpots: [],
      bestLoss: double.infinity,
      worstLoss: 0.0,
      totalPositionsTrained: 0,
      epochLosses: [],
      trainingQuality: "unknown",
    );

    try {
      final request = http.Request('POST', Uri.parse('$baseUrl/train-history-stream'));
      request.headers['Content-Type'] = 'application/json';
      request.body = jsonEncode({"batch_size": batchSize, "epochs": epochs});
      
      final client = http.Client();
      final response = await client.send(request);
      
      double runningLossSum = 0;
      int lossCount = 0;
      
      _subscription = response.stream
          .transform(utf8.decoder)
          .transform(const LineSplitter())
          .listen((line) {
        if (line.isEmpty) return;
        try {
          final data = jsonDecode(line);
          if (data['type'] == 'start') {
            state = state.copyWith(
              totalEpochs: data['epochs'],
              totalPositionsTrained: data['total_moves'] ?? 0,
              historyTrainingStatus: "Learning from ${data['total_moves']} positions...",
            );
          } else if (data['type'] == 'progress') {
            double epochProgress = data['batch'] / data['total_batches'];
            double totalProgress = ((data['epoch'] - 1) + epochProgress) / state.totalEpochs;
            
            double currentLoss = (data['loss'] as num).toDouble();
            runningLossSum += currentLoss;
            lossCount++;
            
            List<FlSpot> updatedSpots = List.from(state.lossSpots);
            if (data['batch'] % 20 == 0) {
              double x = (data['epoch'] - 1) * data['total_batches'] + data['batch'].toDouble();
              updatedSpots.add(FlSpot(x, currentLoss));
            }

            state = state.copyWith(
              currentEpoch: data['epoch'],
              historyTrainingProgress: totalProgress,
              historyTrainingStatus: "Epoch ${data['epoch']}/${state.totalEpochs} | Loss: ${currentLoss.toStringAsFixed(4)}",
              lossSpots: updatedSpots,
              bestLoss: currentLoss < state.bestLoss ? currentLoss : state.bestLoss,
              worstLoss: currentLoss > state.worstLoss ? currentLoss : state.worstLoss,
              avgLossPerEpoch: runningLossSum / lossCount,
            );
          } else if (data['type'] == 'epoch_end') {
            double epochAvgLoss = (data['avg_loss'] as num).toDouble();
            List<double> updatedEpochLosses = List.from(state.epochLosses)..add(epochAvgLoss);
            
            // Reset running totals for next epoch
            runningLossSum = 0;
            lossCount = 0;
            
            state = state.copyWith(
              historyTrainingStatus: "Epoch ${data['epoch']} Complete | Avg Loss: ${epochAvgLoss.toStringAsFixed(4)}",
              epochLosses: updatedEpochLosses,
              trainingQuality: state.copyWith(epochLosses: updatedEpochLosses).analyzeTrainingTrend(),
            );
          } else if (data['type'] == 'complete') {
            state = state.copyWith(
              historyTrainingStatus: "Success: ${data['message']} (${data['duration']})",
              isHistoryTraining: false,
              historyTrainingProgress: 1.0,
            );
            _subscription?.cancel();
          } else if (data['type'] == 'error') {
            state = state.copyWith(
              historyTrainingStatus: "Error: ${data['message']}",
              isHistoryTraining: false,
            );
            _subscription?.cancel();
          }
        } catch (e) {
          print("Training Provider Decode Error: $e");
        }
      }, onDone: () {
        state = state.copyWith(isHistoryTraining: false);
      }, onError: (e) {
        state = state.copyWith(
          isHistoryTraining: false,
          historyTrainingStatus: "Stream Error: $e",
        );
      });
    } catch (e) {
      state = state.copyWith(
        isHistoryTraining: false,
        historyTrainingStatus: "Connection Failed",
      );
    }
  }

  void stopTraining() {
    _subscription?.cancel();
    state = state.copyWith(isHistoryTraining: false, historyTrainingStatus: "Stopped by user");
  }

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }
}

final trainingProvider = StateNotifierProvider<TrainingNotifier, TrainingState>((ref) {
  return TrainingNotifier();
});
