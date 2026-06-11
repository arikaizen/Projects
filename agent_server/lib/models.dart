import 'package:uuid/uuid.dart';
const _uuid = Uuid();

enum FormationType { hierarchy, parallel, sequential, pipeline, broadcast, consensus, mesh, star, graph }
enum RunStatus { pending, running, done, error }
enum StepType { llmCall, toolCall, toolResult, agentCall, agentResult, formation }

class LlmConfig {
  final String provider; // anthropic, openai, ollama, vllm, gemini, mistral, groq, together, cohere, xai, perplexity, custom
  final String model;
  final String baseUrl;
  final String apiKey;
  final double temperature;
  final int maxTokens;
  LlmConfig({required this.provider, required this.model, required this.baseUrl,
      this.apiKey = '', this.temperature = 0.7, this.maxTokens = 4096});
  factory LlmConfig.fromJson(Map<String, dynamic> j) => LlmConfig(
    provider: j['provider'] ?? 'anthropic',
    model: j['model'] ?? 'claude-sonnet-4-6',
    baseUrl: j['base_url'] ?? '',
    apiKey: j['api_key'] ?? '',
    temperature: (j['temperature'] as num?)?.toDouble() ?? 0.7,
    maxTokens: (j['max_tokens'] as int?) ?? 4096,
  );
  Map<String, dynamic> toJson() => {
    'provider': provider, 'model': model, 'base_url': baseUrl,
    'temperature': temperature, 'max_tokens': maxTokens,
  };
}

class AgentConfig {
  final String id;
  String name;
  String systemPrompt;
  LlmConfig llm;
  List<String> tools; // tool names available to this agent
  int maxSteps;
  AgentConfig({String? id, required this.name, required this.systemPrompt,
      required this.llm, List<String>? tools, this.maxSteps = 10})
      : id = id ?? _uuid.v4(), tools = tools ?? [];
  factory AgentConfig.fromJson(Map<String, dynamic> j) => AgentConfig(
    id: j['id'] as String?,
    name: j['name'] ?? 'Agent',
    systemPrompt: j['system_prompt'] ?? 'You are a helpful assistant.',
    llm: LlmConfig.fromJson(j['llm'] as Map<String, dynamic>? ?? {}),
    tools: (j['tools'] as List?)?.cast<String>() ?? [],
    maxSteps: (j['max_steps'] as int?) ?? 10,
  );
  Map<String, dynamic> toJson() => {
    'id': id, 'name': name, 'system_prompt': systemPrompt,
    'llm': llm.toJson(), 'tools': tools, 'max_steps': maxSteps,
  };
}

class GroupConfig {
  final String id;
  String name;
  List<String> agentIds;
  FormationType formation;
  String? coordinatorId;
  Map<String, List<String>> edges; // graph formation adjacency
  GroupConfig({String? id, required this.name, required this.agentIds,
      this.formation = FormationType.parallel, this.coordinatorId, Map<String, List<String>>? edges})
      : id = id ?? _uuid.v4(), edges = edges ?? {};
  factory GroupConfig.fromJson(Map<String, dynamic> j) => GroupConfig(
    id: j['id'] as String?,
    name: j['name'] ?? 'Group',
    agentIds: (j['agent_ids'] as List?)?.cast<String>() ?? [],
    formation: FormationType.values.firstWhere(
        (f) => f.name == j['formation'], orElse: () => FormationType.parallel),
    coordinatorId: j['coordinator_id'] as String?,
    edges: (j['edges'] as Map<String, dynamic>?)
        ?.map((k, v) => MapEntry(k, (v as List).cast<String>())) ?? {},
  );
  Map<String, dynamic> toJson() => {
    'id': id, 'name': name, 'agent_ids': agentIds,
    'formation': formation.name, 'coordinator_id': coordinatorId, 'edges': edges,
  };
}

class RunStep {
  final StepType type;
  final String? content;
  final String? toolName;
  final Map<String, dynamic>? toolInput;
  final String? agentId;
  final String? agentName;
  final DateTime timestamp;
  RunStep({required this.type, this.content, this.toolName,
      this.toolInput, this.agentId, this.agentName})
      : timestamp = DateTime.now();
  Map<String, dynamic> toJson() => {
    'type': type.name,
    if (content != null) 'content': content,
    if (toolName != null) 'tool': toolName,
    if (toolInput != null) 'tool_input': toolInput,
    if (agentId != null) 'agent_id': agentId,
    if (agentName != null) 'agent_name': agentName,
    'timestamp': timestamp.toIso8601String(),
  };
}

class AgentRun {
  final String id;
  final String agentId;
  RunStatus status;
  final List<RunStep> steps;
  String? reply;
  String? error;
  final DateTime startedAt;
  DateTime? completedAt;
  AgentRun({required this.agentId})
      : id = _uuid.v4(), status = RunStatus.pending,
        steps = [], startedAt = DateTime.now();
  Map<String, dynamic> toJson() => {
    'id': id, 'agent_id': agentId, 'status': status.name,
    'steps': steps.map((s) => s.toJson()).toList(),
    if (reply != null) 'reply': reply,
    if (error != null) 'error': error,
    'started_at': startedAt.toIso8601String(),
    if (completedAt != null) 'completed_at': completedAt!.toIso8601String(),
  };
}
