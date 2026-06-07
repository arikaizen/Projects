import 'package:flutter/material.dart';

enum AgentStatus { idle, running, waiting, done, error, cancelled }

enum AgentRole { orchestrator, worker, specialist, reviewer, planner }

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

class AgentModel {
  final String id;
  String name;
  String role;
  AgentRole agentRole;
  String systemPrompt;
  String llmModel;
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
