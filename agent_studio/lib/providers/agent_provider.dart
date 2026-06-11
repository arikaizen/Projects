import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:uuid/uuid.dart';
import '../models/agent_model.dart';
import '../models/agent_group.dart';
import '../models/task_model.dart';
import '../services/agent_api_service.dart';
import '../theme/app_theme.dart';
import '../models/model_provider.dart';
import '../services/model_service.dart';
import '../services/llm_service.dart';

const _uuid = Uuid();

class AgentProvider extends ChangeNotifier {
  final AgentApiService _api;

  AgentProvider(this._api) {
    _loadDefaults();
    _loadPersistedProviders();
  }

  static const _prefsKey = 'agent_studio.model_providers';

  // ── State ──────────────────────────────────────────────────────────────────
  final Map<String, AgentModel> _agents = {};
  final Map<String, AgentGroup> _groups = {};
  final List<TaskModel>         _tasks  = [];
  String?  _selectedAgentId;
  String?  _selectedGroupId;
  String?  _activeConversationId;
  bool     _activeIsGroup = false;
  bool     _isConnected   = false;
  String?  _backendUrl;
  String   _connectionLabel = 'Mock';
  StreamSubscription<Map<String, dynamic>>? _eventSub;
  final List<String> _eventLog = [];
  final List<ModelProvider> _modelProviders = [];
  final _modelService = ModelService();
  final _llmService   = LlmService();

  // MCP server configs managed by the UI (name → config map)
  final Map<String, Map<String, String>> _mcpServers = {};

  // ── Getters ────────────────────────────────────────────────────────────────
  List<AgentModel> get agents        => _agents.values.toList();
  List<AgentModel> get rootAgents    => _agents.values.where((a) => a.parentId == null).toList();
  List<AgentGroup> get groups        => _groups.values.toList();
  List<TaskModel>  get tasks         => List.unmodifiable(_tasks);
  List<TaskModel>  get activeTasks   => _tasks.where((t) => t.isActive).toList();
  String?          get selectedAgentId  => _selectedAgentId;
  String?          get selectedGroupId  => _selectedGroupId;
  String?          get activeConvId     => _activeConversationId;
  bool             get activeConvIsGroup => _activeIsGroup;
  bool             get isConnected       => _isConnected;
  String?          get backendUrl        => _backendUrl;
  String           get connectionLabel   => _api.connectionLabel;
  List<String>     get eventLog          => List.unmodifiable(_eventLog);
  List<ModelProvider> get modelProviders => List.unmodifiable(_modelProviders);
  bool get hasAnyProvider => _modelProviders.any((p) => p.isConnected);

  List<Map<String, String>> get mcpServers =>
      _mcpServers.values.toList();
  bool providerReadyFor(AgentModel agent) => _resolveProvider(agent) != null;
  List<ModelInfo>     get availableModels => _modelProviders
      .where((p) => p.isConnected)
      .expand((p) => p.models)
      .toList();

  AgentModel?  get selectedAgent  => _selectedAgentId != null ? _agents[_selectedAgentId] : null;
  AgentGroup?  get selectedGroup  => _selectedGroupId != null ? _groups[_selectedGroupId] : null;
  AgentModel?  get activeAgent    => !_activeIsGroup && _activeConversationId != null
                                      ? _agents[_activeConversationId] : null;
  AgentGroup?  get activeGroup    => _activeIsGroup && _activeConversationId != null
                                      ? _groups[_activeConversationId] : null;

  List<AgentModel> childrenOf(String parentId) =>
      _agents.values.where((a) => a.parentId == parentId).toList();

  AgentModel? agentById(String id) => _agents[id];
  AgentGroup? groupById(String id) => _groups[id];

  int get runningCount  => _agents.values.where((a) => a.isRunning).length;
  int get totalCount    => _agents.length;
  int get groupCount    => _groups.length;

  // ── Selection ──────────────────────────────────────────────────────────────
  void selectAgent(String? id)  { _selectedAgentId = id; notifyListeners(); }
  void selectGroup(String? id)  { _selectedGroupId = id; notifyListeners(); }

  void openConversation(String id, {bool isGroup = false}) {
    _activeConversationId = id;
    _activeIsGroup = isGroup;
    notifyListeners();
  }

  void closeConversation() {
    _activeConversationId = null;
    notifyListeners();
  }

  void clearHistory(String id, {bool isGroup = false}) {
    if (isGroup) {
      _groups[id]?.sharedHistory.clear();
    } else {
      _agents[id]?.history.clear();
    }
    notifyListeners();
  }

  // ── Agent CRUD ─────────────────────────────────────────────────────────────
  AgentModel createAgent({
    required String name,
    String role           = 'worker',
    AgentRole agentRole   = AgentRole.worker,
    String systemPrompt   = '',
    String llmModel       = 'claude-sonnet-4-6',
    String? providerId,
    String? parentId,
    List<AgentTool>? tools,
    int maxIterations     = 20,
    double temperature    = 0.7,
    Color? color,
  }) {
    final id    = _uuid.v4();
    final agent = AgentModel(
      id: id,
      name: name,
      role: role,
      agentRole: agentRole,
      systemPrompt: systemPrompt,
      llmModel: llmModel,
      providerId: providerId,
      parentId: parentId,
      tools: tools ?? _defaultTools(),
      maxIterations: maxIterations,
      temperature: temperature,
      color: color ?? AppColors.agentColors[_agents.length % AppColors.agentColors.length],
    );

    _agents[id] = agent;

    if (parentId != null && _agents.containsKey(parentId)) {
      _agents[parentId]!.childIds.add(id);
    }

    _log('Agent created: ${agent.name} [$id]');
    notifyListeners();
    return agent;
  }

  void updateAgent(String id, AgentModel updated) {
    if (!_agents.containsKey(id)) return;
    _agents[id] = updated;
    notifyListeners();
  }

  void deleteAgent(String id) {
    final agent = _agents[id];
    if (agent == null) return;

    // Detach from parent
    if (agent.parentId != null) {
      _agents[agent.parentId!]?.childIds.remove(id);
    }
    // Orphan children
    for (final cid in agent.childIds) {
      _agents[cid] = _agents[cid]!.copyWith(parentId: null);
    }
    // Remove from groups
    for (final g in _groups.values) {
      g.agentIds.remove(id);
    }

    _agents.remove(id);
    if (_selectedAgentId == id)     _selectedAgentId = null;
    if (_activeConversationId == id) _activeConversationId = null;

    _log('Agent deleted: ${agent.name}');
    notifyListeners();
  }

  void reparentAgent(String agentId, String? newParentId) {
    final agent = _agents[agentId];
    if (agent == null) return;
    if (agent.parentId != null) {
      _agents[agent.parentId!]?.childIds.remove(agentId);
    }
    if (newParentId != null) {
      _agents[newParentId]?.childIds.add(agentId);
    }
    _agents[agentId] = agent.copyWith(parentId: newParentId);
    notifyListeners();
  }

  // ── Group CRUD ─────────────────────────────────────────────────────────────
  AgentGroup createGroup({
    required String name,
    String description             = '',
    FormationType formation         = FormationType.parallel,
    List<String>? agentIds,
    String? coordinatorId,
    Map<String, List<String>>? edges,
  }) {
    final id    = _uuid.v4();
    final group = AgentGroup(
      id: id,
      name: name,
      description: description,
      formation: formation,
      agentIds: agentIds ?? [],
      coordinatorId: coordinatorId,
      edges: edges,
    );
    _groups[id] = group;
    _log('Group created: $name [$id] [${AgentGroup.formationLabel(formation)}]');
    notifyListeners();
    return group;
  }

  void updateGroup(String id, AgentGroup updated) {
    if (!_groups.containsKey(id)) return;
    _groups[id] = updated;
    notifyListeners();
  }

  void deleteGroup(String id) {
    final g = _groups.remove(id);
    if (_selectedGroupId == id)      _selectedGroupId = null;
    if (_activeConversationId == id) _activeConversationId = null;
    _log('Group deleted: ${g?.name}');
    notifyListeners();
  }

  void addAgentToGroup(String groupId, String agentId) {
    final g = _groups[groupId];
    if (g == null || g.agentIds.contains(agentId)) return;
    g.agentIds.add(agentId);
    notifyListeners();
  }

  void removeAgentFromGroup(String groupId, String agentId) {
    _groups[groupId]?.agentIds.remove(agentId);
    notifyListeners();
  }

  void reorderGroupAgent(String groupId, int oldIndex, int newIndex) {
    final g = _groups[groupId];
    if (g == null) return;
    final item = g.agentIds.removeAt(oldIndex);
    g.agentIds.insert(newIndex, item);
    notifyListeners();
  }

  // ── Task dispatch ──────────────────────────────────────────────────────────
  Future<TaskModel> dispatchTask({
    required String prompt,
    required String targetId,
    TaskTarget target = TaskTarget.agent,
  }) async {
    final task = TaskModel(
      id: _uuid.v4(),
      prompt: prompt,
      targetId: targetId,
      target: target,
      status: TaskStatus.pending,
    );
    _tasks.insert(0, task);
    notifyListeners();

    _updateTask(task.id, task.copyWith(
      status: TaskStatus.running,
      startedAt: DateTime.now(),
    ));

    // Mark agents as running
    if (target == TaskTarget.agent) {
      _setAgentStatus(targetId, AgentStatus.running, task: prompt);
    } else if (target == TaskTarget.group) {
      final g = _groups[targetId];
      if (g != null) {
        for (final aid in g.agentIds) {
          _setAgentStatus(aid, AgentStatus.running, task: prompt);
        }
      }
    }

    // Point the engine at the target agent's provider/model before running so
    // engine-backed agents use the model the user picked for them.
    if (_isConnected && target == TaskTarget.agent) {
      final agent = _agents[targetId];
      if (agent != null) {
        final llm = engineLlmConfigFor(agent);
        if (llm != null && agent.providerId != _engineDefaultProviderId) {
          await setEngineDefaultProvider(
              agent.providerId ?? _resolveProvider(agent)!.id, agent.llmModel);
        }
      }
    }

    try {
      final result = await _api.runTask(prompt, targetId, target);
      final completed = task.copyWith(
        status: TaskStatus.done,
        result: result,
        completedAt: DateTime.now(),
      );
      _updateTask(task.id, completed);
      _addResultToHistory(targetId, target, prompt, result);
    } catch (e) {
      _updateTask(task.id, task.copyWith(
        status: TaskStatus.error,
        error: e.toString(),
        completedAt: DateTime.now(),
      ));
      _log('Task error: $e');
    } finally {
      if (target == TaskTarget.agent) {
        _setAgentStatus(targetId, AgentStatus.idle, task: null);
      } else if (target == TaskTarget.group) {
        final g = _groups[targetId];
        if (g != null) {
          for (final aid in g.agentIds) {
            _setAgentStatus(aid, AgentStatus.idle, task: null);
          }
        }
      }
    }

    return _tasks.firstWhere((t) => t.id == task.id);
  }

  Future<void> sendMessage(String targetId, String content, {bool isGroup = false}) async {
    final userMsg = ChatMessage(
      id: _uuid.v4(),
      content: content,
      isUser: true,
      timestamp: DateTime.now(),
      agentId: targetId,
    );

    if (isGroup) {
      _groups[targetId]?.sharedHistory.add(userMsg);
    } else {
      _agents[targetId]?.history.add(userMsg);
    }
    notifyListeners();

    if (!isGroup) {
      final agent = _agents[targetId];
      if (agent != null) {
        final provider = _resolveProvider(agent);
        if (provider != null) {
          _setAgentStatus(targetId, AgentStatus.running, task: content);
          try {
            final reply = await _llmService.chat(
              provider: provider,
              modelId: agent.llmModel,
              systemPrompt: agent.systemPrompt,
              history: List.from(agent.history),
              temperature: agent.temperature,
            );
            agent.history.add(ChatMessage(
              id: _uuid.v4(),
              content: reply,
              isUser: false,
              timestamp: DateTime.now(),
              agentId: targetId,
            ));
            _setAgentStatus(targetId, AgentStatus.idle, task: null);
            notifyListeners();
            return;
          } catch (e) {
            _log('LLM error for ${agent.name}: $e');
            agent.history.add(ChatMessage(
              id: _uuid.v4(),
              content: 'Error: $e',
              isUser: false,
              timestamp: DateTime.now(),
              agentId: targetId,
            ));
            _setAgentStatus(targetId, AgentStatus.error, task: null);
            notifyListeners();
            return;
          }
        }
      }
    }

    // Fall back to engine if no direct LLM provider
    if (isGroup || (_groups.containsKey(targetId))) {
      await dispatchTask(
        prompt: content,
        targetId: targetId,
        target: isGroup ? TaskTarget.group : TaskTarget.agent,
      );
      return;
    }

    // No provider and no engine — surface a real error in the chat
    final errMsg = ChatMessage(
      id: _uuid.v4(),
      content: 'No model provider connected.\n\n'
          'Open **Settings → Model Providers** and add one:\n'
          '• **Ollama** for local models (llama3, mistral, gemma…)\n'
          '• **Anthropic** with your API key for Claude\n'
          '• **OpenAI / vLLM** for any OpenAI-compatible endpoint\n\n'
          'Or connect to the Agent Engine (FFI / HTTP) in Settings → Engine Connection.',
      isUser: false,
      timestamp: DateTime.now(),
      agentId: targetId,
      type: MessageType.error,
    );
    _agents[targetId]?.history.add(errMsg);
    notifyListeners();
  }

  /// Find the connected ModelProvider for the given agent.
  /// Prefers explicit providerId, then searches by matching model id.
  ModelProvider? _resolveProvider(AgentModel agent) {
    if (agent.providerId != null) {
      final matches = _modelProviders.where((p) => p.id == agent.providerId);
      if (matches.isNotEmpty) return matches.first;
    }
    // Match by model id across all connected providers
    for (final p in _modelProviders) {
      if (!p.isConnected) continue;
      if (p.models.any((m) => m.id == agent.llmModel)) return p;
    }
    // If there's exactly one connected provider, use it regardless
    final connected = _modelProviders.where((p) => p.isConnected).toList();
    if (connected.length == 1) return connected.first;
    return null;
  }

  void cancelTask(String taskId) {
    final idx = _tasks.indexWhere((t) => t.id == taskId);
    if (idx < 0) return;
    _tasks[idx] = _tasks[idx].copyWith(
      status: TaskStatus.cancelled,
      completedAt: DateTime.now(),
    );
    notifyListeners();
  }

  // ── Backend connection ─────────────────────────────────────────────────────

  /// [target] is either:
  ///   - A path to libagent_engine.so  (FFI, desktop only)
  ///   - An HTTP URL like http://localhost:8080  (REST)
  Future<void> connect(String target) async {
    _backendUrl = target;
    _eventSub?.cancel();

    try {
      _isConnected = await _api.connect(target);
    } catch (_) {
      _isConnected = false;
    }

    if (_isConnected) {
      // Subscribe to live engine events
      _eventSub = _api.engineEvents?.listen(_handleEngineEvent);
    }

    _log(_isConnected
        ? 'Connected — ${_api.connectionLabel}'
        : 'Failed to connect to $target (using mock mode)');
    notifyListeners();
  }

  void disconnect() {
    _eventSub?.cancel();
    _api.disconnect();
    _isConnected = false;
    _backendUrl  = null;
    notifyListeners();
  }

  void _handleEngineEvent(Map<String, dynamic> evt) {
    final type    = evt['type'] as String? ?? '';
    final agentId = evt['agent_id'] as String?;

    switch (type) {
      case 'agent_started':
        if (agentId != null) _setAgentStatus(agentId, AgentStatus.running);
        break;
      case 'agent_finished':
        if (agentId != null) _setAgentStatus(agentId, AgentStatus.done, task: null);
        break;
      case 'agent_failed':
        if (agentId != null) _setAgentStatus(agentId, AgentStatus.error, task: null);
        break;
      case 'agent_cancelled':
        if (agentId != null) _setAgentStatus(agentId, AgentStatus.cancelled, task: null);
        break;
      case 'work_item_started':
        final name = evt['work_item_name'] as String?;
        if (agentId != null && name != null) {
          _setAgentStatus(agentId, AgentStatus.running, task: name);
        }
        break;
      case 'blackboard_updated':
        _log('[blackboard] ${evt['key']} updated');
        break;
      case 'mcp_connected':
        _log('[MCP] connected: ${evt['server_name']}');
        break;
      case 'mcp_disconnected':
        _log('[MCP] disconnected: ${evt['server_name']}');
        break;
      case 'quota_exceeded':
        _log('[quota] exceeded for user ${evt['user_id']}');
        break;
    }

    _log('[engine] $type${agentId != null ? ' agent=$agentId' : ''}');
    notifyListeners();
  }

  // ── Model providers ────────────────────────────────────────────────────────
  Future<void> addModelProvider(ModelProvider provider) async {
    // Replace existing provider with same id (edit) instead of duplicating
    _modelProviders.removeWhere((p) => p.id == provider.id);
    _modelProviders.add(provider);
    notifyListeners();
    await _persistProviders();
    await refreshModelProvider(provider.id);
  }

  void removeModelProvider(String id) {
    _modelProviders.removeWhere((p) => p.id == id);
    notifyListeners();
    _persistProviders();
  }

  /// Persist all model providers (including API keys) to local storage.
  Future<void> _persistProviders() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final json = jsonEncode(_modelProviders.map((p) => p.toJson()).toList());
      await prefs.setString(_prefsKey, json);
    } catch (e) {
      _log('Failed to save providers: $e');
    }
  }

  /// Load providers saved in a previous session and reconnect them so the user
  /// never has to re-enter API keys.
  Future<void> _loadPersistedProviders() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final raw = prefs.getString(_prefsKey);
      if (raw == null || raw.isEmpty) return;
      final list = (jsonDecode(raw) as List).cast<Map<String, dynamic>>();
      for (final m in list) {
        final p = ModelProvider.fromJson(m);
        if (_modelProviders.any((e) => e.id == p.id)) continue;
        _modelProviders.add(p);
      }
      notifyListeners();
      // Reconnect each in the background to repopulate model lists.
      for (final p in _modelProviders) {
        unawaited(refreshModelProvider(p.id));
      }
      _log('Restored ${list.length} saved model provider(s)');
    } catch (e) {
      _log('Failed to load saved providers: $e');
    }
  }

  /// Change which model (and provider) a given agent uses. Accepts any model
  /// from any connected provider — lets the user re-point an agent at Claude,
  /// GPT-4o, Gemini, a local Ollama model, etc. without rebuilding the agent.
  void changeAgentModel(String agentId, ModelInfo model) {
    final a = _agents[agentId];
    if (a == null) return;
    _agents[agentId] = a.copyWith(
      llmModel: model.id,
      providerId: model.providerId,
    );
    _log('Agent "${a.name}" model → ${model.displayName} (${model.providerName})');
    notifyListeners();
  }

  // ── MCP servers ───────────────────────────────────────────────────────────
  Future<void> addMcpServer({
    required String name,
    required String url,
    String bearerToken = '',
    String transport   = 'http',
  }) async {
    _mcpServers[name] = {
      'name':         name,
      'url':          url,
      'bearer_token': bearerToken,
      'transport':    transport,
    };
    try {
      await _api.connectMcp(
        name:        name,
        url:         url,
        bearerToken: bearerToken,
        transport:   transport,
      );
      _log('MCP server "$name" connected at $url');
    } catch (e) {
      _log('MCP server "$name" connection error: $e');
    }
    notifyListeners();
  }

  Future<void> removeMcpServer(String name) async {
    _mcpServers.remove(name);
    try {
      await _api.disconnectMcp(name);
    } catch (_) {}
    _log('MCP server "$name" removed');
    notifyListeners();
  }

  Future<void> refreshModelProvider(String id) async {
    final p = _modelProviders.firstWhere((p) => p.id == id,
        orElse: () => throw StateError('not found'));
    p.isLoading = true;
    notifyListeners();

    final (connected, models, error) = await _modelService.connect(p);
    p.isConnected = connected;
    p.models      = models;
    p.error       = error;
    p.isLoading   = false;

    _log(connected
        ? 'Model provider "${p.name}": ${models.length} models'
        : 'Model provider "${p.name}" failed: $error');

    // When the engine is connected and this is the first provider to come
    // online, make it the engine's default LLM so engine-backed agents have a
    // real model to run on out of the box.
    if (connected && _isConnected && _engineDefaultProviderId == null &&
        models.isNotEmpty) {
      await setEngineDefaultProvider(p.id, models.first.id);
    }
    notifyListeners();
  }

  // ── Engine LLM wiring ──────────────────────────────────────────────────────
  String? _engineDefaultProviderId;
  String? _engineDefaultModelId;

  String?  get engineDefaultProviderId => _engineDefaultProviderId;
  String?  get engineDefaultModelId    => _engineDefaultModelId;

  /// Build the engine llm_factory config for [agent], resolving its provider.
  /// Returns null when no connected provider backs the agent.
  Map<String, dynamic>? engineLlmConfigFor(AgentModel agent) {
    final p = _resolveProvider(agent);
    if (p == null) return null;
    final modelId = agent.llmModel.isNotEmpty ? agent.llmModel
        : (p.models.isNotEmpty ? p.models.first.id : '');
    if (modelId.isEmpty) return null;
    return {
      'provider': p.type.name,
      'model': modelId,
      'api_key': p.apiKey,
      'base_url': p.baseUrl,
    };
  }

  /// Push a provider+model to the engine as its default backend.
  Future<void> setEngineDefaultProvider(String providerId, String modelId) async {
    final matches = _modelProviders.where((p) => p.id == providerId);
    if (matches.isEmpty) return;
    final p = matches.first;
    try {
      await _api.configureLlm({
        'provider': p.type.name,
        'model': modelId,
        'api_key': p.apiKey,
        'base_url': p.baseUrl,
      });
      _engineDefaultProviderId = providerId;
      _engineDefaultModelId    = modelId;
      _log('Engine LLM set to ${p.name} / $modelId');
    } catch (e) {
      _log('Engine LLM config failed: $e');
    }
    notifyListeners();
  }

  // ── Private helpers ────────────────────────────────────────────────────────
  void _updateTask(String id, TaskModel updated) {
    final idx = _tasks.indexWhere((t) => t.id == id);
    if (idx >= 0) _tasks[idx] = updated;
    notifyListeners();
  }

  void _setAgentStatus(String id, AgentStatus status, {String? task}) {
    final a = _agents[id];
    if (a == null) return;
    _agents[id] = a.copyWith(status: status, currentTask: task);
    notifyListeners();
  }

  void _addResultToHistory(
    String targetId,
    TaskTarget target,
    String prompt,
    String result,
  ) {
    final agentMsg = ChatMessage(
      id: _uuid.v4(),
      content: result,
      isUser: false,
      timestamp: DateTime.now(),
      agentId: targetId,
    );

    if (target == TaskTarget.group) {
      _groups[targetId]?.sharedHistory.add(agentMsg);
    } else {
      _agents[targetId]?.history.add(agentMsg);
    }
    notifyListeners();
  }

  void _log(String msg) {
    final ts = DateTime.now().toLocal().toIso8601String().substring(11, 19);
    _eventLog.insert(0, '[$ts] $msg');
    if (_eventLog.length > 200) _eventLog.removeLast();
  }

  void _loadDefaults() {
    // Seed a demo agent so the app isn't empty on first launch
    createAgent(
      name: 'Assistant',
      role: 'assistant',
      agentRole: AgentRole.worker,
      systemPrompt: 'You are a helpful AI assistant. Be concise and accurate.',
      llmModel: 'claude-sonnet-4-6',
      color: AppColors.agentColors[0],
    );
  }

  List<AgentTool> _defaultTools() => [
    AgentTool(name: 'web_search', description: 'Search the web for information'),
    AgentTool(name: 'code_exec',  description: 'Execute code snippets', enabled: false),
    AgentTool(name: 'file_read',  description: 'Read files from the filesystem'),
    AgentTool(name: 'file_write', description: 'Write files to the filesystem', enabled: false),
  ];
}
