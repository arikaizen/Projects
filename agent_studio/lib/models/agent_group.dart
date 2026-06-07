import 'agent_model.dart';

enum GroupMode {
  parallel,    // all agents run simultaneously
  sequential,  // agents run one after another, output feeds next
  broadcast,   // same task sent to all, best result wins
  consensus,   // all agents vote/agree before proceeding
  pipeline,    // waterfall — stage A → B → C
}

enum GroupStatus { idle, running, done, error }

class AgentGroup {
  final String id;
  String name;
  String description;
  GroupMode mode;
  List<String> agentIds;  // ordered for sequential/pipeline
  GroupStatus status;
  String? currentTask;
  List<ChatMessage> sharedHistory;
  DateTime createdAt;
  Map<String, dynamic> metadata;

  AgentGroup({
    required this.id,
    required this.name,
    this.description = '',
    this.mode = GroupMode.parallel,
    List<String>? agentIds,
    this.status = GroupStatus.idle,
    this.currentTask,
    List<ChatMessage>? sharedHistory,
    DateTime? createdAt,
    Map<String, dynamic>? metadata,
  })  : agentIds = agentIds ?? [],
        sharedHistory = sharedHistory ?? [],
        createdAt = createdAt ?? DateTime.now(),
        metadata = metadata ?? {};

  String get modeLabel {
    switch (mode) {
      case GroupMode.parallel:   return 'Parallel';
      case GroupMode.sequential: return 'Sequential';
      case GroupMode.broadcast:  return 'Broadcast';
      case GroupMode.consensus:  return 'Consensus';
      case GroupMode.pipeline:   return 'Pipeline';
    }
  }

  String get modeDescription {
    switch (mode) {
      case GroupMode.parallel:   return 'All agents work simultaneously on the same task';
      case GroupMode.sequential: return 'Each agent hands off results to the next';
      case GroupMode.broadcast:  return 'Task sent to all; best result selected';
      case GroupMode.consensus:  return 'All agents must agree before output is accepted';
      case GroupMode.pipeline:   return 'Waterfall — each stage transforms the output';
    }
  }

  AgentGroup copyWith({
    String? name,
    String? description,
    GroupMode? mode,
    List<String>? agentIds,
    GroupStatus? status,
    String? currentTask,
    List<ChatMessage>? sharedHistory,
    Map<String, dynamic>? metadata,
  }) {
    return AgentGroup(
      id: id,
      name: name ?? this.name,
      description: description ?? this.description,
      mode: mode ?? this.mode,
      agentIds: agentIds ?? List.from(this.agentIds),
      status: status ?? this.status,
      currentTask: currentTask ?? this.currentTask,
      sharedHistory: sharedHistory ?? List.from(this.sharedHistory),
      createdAt: createdAt,
      metadata: metadata ?? Map.from(this.metadata),
    );
  }
}
