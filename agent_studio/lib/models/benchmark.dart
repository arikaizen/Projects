import 'package:uuid/uuid.dart';
import 'model_provider.dart';

const _uuid = Uuid();

enum BenchmarkRunStatus { pending, running, done, error }

/// One model's attempt at a benchmark prompt. Holds the output plus the
/// performance metrics we measure: wall-clock latency and token usage.
class BenchmarkRun {
  final String id;
  final String modelId;
  final String modelLabel;
  final String providerId;
  final String providerName;
  final ProviderType providerType;

  BenchmarkRunStatus status;
  String? output;
  String? error;
  Duration? latency;
  int? promptTokens;
  int? completionTokens;
  int? totalTokens;

  BenchmarkRun({
    String? id,
    required this.modelId,
    required this.modelLabel,
    required this.providerId,
    required this.providerName,
    required this.providerType,
    this.status = BenchmarkRunStatus.pending,
    this.output,
    this.error,
    this.latency,
    this.promptTokens,
    this.completionTokens,
    this.totalTokens,
  }) : id = id ?? _uuid.v4();

  bool get isDone => status == BenchmarkRunStatus.done;
  bool get isActive =>
      status == BenchmarkRunStatus.running ||
      status == BenchmarkRunStatus.pending;

  /// Output tokens generated per second — a throughput score for the model.
  double? get tokensPerSecond {
    final t = completionTokens ?? totalTokens;
    final ms = latency?.inMilliseconds;
    if (t == null || ms == null || ms == 0) return null;
    return t / (ms / 1000);
  }

  int get charCount => output?.length ?? 0;
}

/// A benchmark: one prompt sent to many models at once for a head-to-head
/// comparison of speed, cost (tokens) and output.
class Benchmark {
  final String id;
  final String name;
  final String prompt;
  final double temperature;
  final DateTime createdAt;
  final List<BenchmarkRun> runs;

  Benchmark({
    String? id,
    required this.name,
    required this.prompt,
    this.temperature = 0.7,
    DateTime? createdAt,
    List<BenchmarkRun>? runs,
  })  : id = id ?? _uuid.v4(),
        createdAt = createdAt ?? DateTime.now(),
        runs = runs ?? [];

  bool get isRunning => runs.any((r) => r.isActive);

  List<BenchmarkRun> get completed =>
      runs.where((r) => r.isDone).toList();

  /// Runs that finished successfully, ranked fastest-first for the leaderboard.
  List<BenchmarkRun> get leaderboard {
    final done = completed.where((r) => r.latency != null).toList()
      ..sort((a, b) => a.latency!.compareTo(b.latency!));
    return done;
  }

  /// The fastest successful run, highlighted as the winner.
  BenchmarkRun? get fastest =>
      leaderboard.isEmpty ? null : leaderboard.first;
}
