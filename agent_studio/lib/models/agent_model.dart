import 'package:flutter/material.dart';

enum AgentStatus { idle, running, waiting, done, error, cancelled }

enum AgentRole { orchestrator, worker, specialist, reviewer, planner }

extension AgentRoleInfo on AgentRole {
  String get label {
    switch (this) {
      case AgentRole.orchestrator: return 'Orchestrator';
      case AgentRole.worker:       return 'Worker';
      case AgentRole.specialist:   return 'Specialist';
      case AgentRole.reviewer:     return 'Reviewer';
      case AgentRole.planner:      return 'Planner';
    }
  }

  String get description {
    switch (this) {
      case AgentRole.orchestrator:
        return 'Coordinates other agents, breaks goals into subtasks and delegates.';
      case AgentRole.worker:
        return 'General-purpose agent that executes tasks directly.';
      case AgentRole.specialist:
        return 'Focused expert for a narrow domain (e.g. SQL, security, math).';
      case AgentRole.reviewer:
        return 'Critiques and verifies output from other agents for quality.';
      case AgentRole.planner:
        return 'Produces step-by-step plans before any execution happens.';
    }
  }

  /// A sensible default system prompt for this role, used to pre-fill the
  /// builder so the user has to configure less.
  String get defaultPrompt {
    switch (this) {
      case AgentRole.orchestrator:
        return 'You are an orchestrator. Break the user\'s goal into clear '
            'subtasks, decide which agent or tool handles each, and combine '
            'their results into a final answer.';
      case AgentRole.worker:
        return 'You are a helpful, capable assistant. Complete the task '
            'accurately and concisely.';
      case AgentRole.specialist:
        return 'You are a domain specialist. Give precise, expert-level answers '
            'within your area and say when something is outside it.';
      case AgentRole.reviewer:
        return 'You are a critical reviewer. Inspect the provided work, point '
            'out errors and risks, and suggest concrete improvements.';
      case AgentRole.planner:
        return 'You are a planner. Produce a numbered, step-by-step plan to '
            'achieve the goal before any execution. Do not execute, just plan.';
    }
  }
}

enum AgentPattern { solo, hierarchy, group }

class AgentTool {
  final String name;
  final String description;
  bool enabled;

  AgentTool({required this.name, required this.description, this.enabled = true});

  Map<String, dynamic> toJson() => {
    'name': name,
    'description': description,
    'enabled': enabled,
  };

  factory AgentTool.fromJson(Map<String, dynamic> j) => AgentTool(
    name: j['name'] as String,
    description: j['description'] as String,
    enabled: j['enabled'] as bool? ?? true,
  );
}

const Object _noChange = Object();

class AgentModel {
  final String id;
  String name;
  String role;
  AgentRole agentRole;
  String systemPrompt;
  String llmModel;
  String? providerId;
  String? chainToId; // pipe this agent's output into another agent's input
  AgentStatus status;
  String? parentId;
  List<String> childIds;
  List<String> groupIds;
  List<AgentTool> tools;
  String? currentTask;
  List<ChatMessage> history;
  DateTime createdAt;
  int maxIterations;
  double temperature;
  Map<String, dynamic> metadata;
  Color color;

  AgentModel({
    required this.id,
    required this.name,
    this.role = 'worker',
    this.agentRole = AgentRole.worker,
    this.systemPrompt = '',
    this.llmModel = 'claude-sonnet-4-6',
    this.providerId,
    this.chainToId,
    this.status = AgentStatus.idle,
    this.parentId,
    List<String>? childIds,
    List<String>? groupIds,
    List<AgentTool>? tools,
    this.currentTask,
    List<ChatMessage>? history,
    DateTime? createdAt,
    this.maxIterations = 20,
    this.temperature = 0.7,
    Map<String, dynamic>? metadata,
    Color? color,
  })  : childIds = childIds ?? [],
        groupIds = groupIds ?? [],
        tools = tools ?? [],
        history = history ?? [],
        createdAt = createdAt ?? DateTime.now(),
        metadata = metadata ?? {},
        color = color ?? Colors.blue;

  bool get isRunning => status == AgentStatus.running;
  bool get hasChildren => childIds.isNotEmpty;
  bool get isRoot => parentId == null;

  AgentModel copyWith({
    String? name,
    String? role,
    AgentRole? agentRole,
    String? systemPrompt,
    String? llmModel,
    String? providerId,
    Object? chainToId = _noChange,
    AgentStatus? status,
    String? parentId,
    List<String>? childIds,
    List<String>? groupIds,
    List<AgentTool>? tools,
    String? currentTask,
    List<ChatMessage>? history,
    int? maxIterations,
    double? temperature,
    Map<String, dynamic>? metadata,
    Color? color,
  }) {
    return AgentModel(
      id: id,
      name: name ?? this.name,
      role: role ?? this.role,
      agentRole: agentRole ?? this.agentRole,
      systemPrompt: systemPrompt ?? this.systemPrompt,
      llmModel: llmModel ?? this.llmModel,
      providerId: providerId ?? this.providerId,
      chainToId: chainToId == _noChange ? this.chainToId : chainToId as String?,
      status: status ?? this.status,
      parentId: parentId ?? this.parentId,
      childIds: childIds ?? List.from(this.childIds),
      groupIds: groupIds ?? List.from(this.groupIds),
      tools: tools ?? List.from(this.tools),
      currentTask: currentTask ?? this.currentTask,
      history: history ?? List.from(this.history),
      createdAt: createdAt,
      maxIterations: maxIterations ?? this.maxIterations,
      temperature: temperature ?? this.temperature,
      metadata: metadata ?? Map.from(this.metadata),
      color: color ?? this.color,
    );
  }

  Map<String, dynamic> toJson() => {
    'id': id,
    'name': name,
    'role': role,
    'agentRole': agentRole.name,
    'systemPrompt': systemPrompt,
    'llmModel': llmModel,
    'providerId': providerId,
    'chainToId': chainToId,
    'status': status.name,
    'parentId': parentId,
    'childIds': childIds,
    'groupIds': groupIds,
    'tools': tools.map((t) => t.toJson()).toList(),
    'currentTask': currentTask,
    'maxIterations': maxIterations,
    'temperature': temperature,
    'metadata': metadata,
    'color': color.value,
    'createdAt': createdAt.toIso8601String(),
  };
}

class ChatMessage {
  final String id;
  final String content;
  final bool isUser;
  final DateTime timestamp;
  final String? agentId;
  final MessageType type;
  final Map<String, dynamic>? toolCall;

  const ChatMessage({
    required this.id,
    required this.content,
    required this.isUser,
    required this.timestamp,
    this.agentId,
    this.type = MessageType.text,
    this.toolCall,
  });

  Map<String, dynamic> toJson() => {
    'id': id,
    'content': content,
    'isUser': isUser,
    'timestamp': timestamp.toIso8601String(),
    'agentId': agentId,
    'type': type.name,
    'toolCall': toolCall,
  };
}

enum MessageType { text, toolCall, toolResult, reasoning, error, system }
