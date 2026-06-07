enum TaskStatus { pending, running, done, error, cancelled }
enum TaskTarget { agent, group, hierarchy }

class TaskModel {
  final String id;
  String prompt;
  TaskStatus status;
  TaskTarget target;
  String targetId;       // agentId or groupId
  String? result;
  String? error;
  DateTime createdAt;
  DateTime? startedAt;
  DateTime? completedAt;
  int iterations;
  List<String> logs;

  TaskModel({
    required this.id,
    required this.prompt,
    required this.targetId,
    this.target = TaskTarget.agent,
    this.status = TaskStatus.pending,
    this.result,
    this.error,
    DateTime? createdAt,
    this.startedAt,
    this.completedAt,
    this.iterations = 0,
    List<String>? logs,
  })  : createdAt = createdAt ?? DateTime.now(),
        logs = logs ?? [];

  Duration? get duration {
    if (startedAt == null) return null;
    final end = completedAt ?? DateTime.now();
    return end.difference(startedAt!);
  }

  bool get isActive => status == TaskStatus.running;

  TaskModel copyWith({
    String? prompt,
    TaskStatus? status,
    TaskTarget? target,
    String? targetId,
    String? result,
    String? error,
    DateTime? startedAt,
    DateTime? completedAt,
    int? iterations,
    List<String>? logs,
  }) {
    return TaskModel(
      id: id,
      prompt: prompt ?? this.prompt,
      status: status ?? this.status,
      target: target ?? this.target,
      targetId: targetId ?? this.targetId,
      result: result ?? this.result,
      error: error ?? this.error,
      createdAt: createdAt,
      startedAt: startedAt ?? this.startedAt,
      completedAt: completedAt ?? this.completedAt,
      iterations: iterations ?? this.iterations,
      logs: logs ?? List.from(this.logs),
    );
  }
}
