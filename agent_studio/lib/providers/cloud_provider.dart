import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

class CloudAgent {
  final String id;
  String name;
  String systemPrompt;
  String provider;
  String model;
  String baseUrl;
  String apiKey;
  double temperature;
  int maxSteps;
  List<String> tools;
  CloudAgent({required this.id, required this.name, required this.systemPrompt,
      required this.provider, required this.model, required this.baseUrl,
      required this.apiKey, this.temperature = 0.7, this.maxSteps = 10,
      List<String>? tools}) : tools = tools ?? [];
  factory CloudAgent.fromJson(Map<String, dynamic> j) => CloudAgent(
    id: j['id'] as String,
    name: j['name'] as String? ?? 'Agent',
    systemPrompt: j['system_prompt'] as String? ?? '',
    provider: (j['llm'] as Map?)?['provider'] as String? ?? 'anthropic',
    model: (j['llm'] as Map?)?['model'] as String? ?? '',
    baseUrl: (j['llm'] as Map?)?['base_url'] as String? ?? '',
    apiKey: '',
    temperature: ((j['llm'] as Map?)?['temperature'] as num?)?.toDouble() ?? 0.7,
    maxSteps: j['max_steps'] as int? ?? 10,
    tools: (j['tools'] as List?)?.cast<String>() ?? [],
  );
  Map<String, dynamic> toCreateJson() => {
    'name': name,
    'system_prompt': systemPrompt,
    'llm': {
      'provider': provider, 'model': model,
      'base_url': baseUrl, 'api_key': apiKey,
      'temperature': temperature,
    },
    'tools': tools,
    'max_steps': maxSteps,
  };
}

class CloudChatMessage {
  final String role;
  final String content;
  final bool isStreaming;
  CloudChatMessage({required this.role, required this.content, this.isStreaming = false});
}

class CloudProvider extends ChangeNotifier {
  String _serverUrl = 'http://localhost:3001';
  bool _connected = false;
  bool _connecting = false;
  String? _error;
  List<CloudAgent> _agents = [];
  final Map<String, List<CloudChatMessage>> _history = {};
  final Map<String, bool> _running = {};

  String get serverUrl => _serverUrl;
  bool get connected => _connected;
  bool get connecting => _connecting;
  String? get error => _error;
  List<CloudAgent> get agents => List.unmodifiable(_agents);
  List<CloudChatMessage> historyFor(String agentId) =>
      List.unmodifiable(_history[agentId] ?? []);
  bool isRunning(String agentId) => _running[agentId] ?? false;

  Future<bool> connect(String url) async {
    _serverUrl = url.replaceAll(RegExp(r'/$'), '');
    _connecting = true;
    _error = null;
    notifyListeners();
    try {
      final res = await http.get(Uri.parse('$_serverUrl/health'))
          .timeout(const Duration(seconds: 5));
      _connected = res.statusCode == 200;
      if (_connected) await _loadAgents();
    } catch (e) {
      _connected = false;
      _error = 'Cannot reach server: $e';
    }
    _connecting = false;
    notifyListeners();
    return _connected;
  }

  void disconnect() {
    _connected = false;
    _agents = [];
    _error = null;
    notifyListeners();
  }

  Future<void> _loadAgents() async {
    try {
      final res = await http.get(Uri.parse('$_serverUrl/api/agents'));
      if (res.statusCode == 200) {
        final data = jsonDecode(res.body) as Map<String, dynamic>;
        _agents = (data['agents'] as List)
            .map((j) => CloudAgent.fromJson(j as Map<String, dynamic>))
            .toList();
      }
    } catch (_) {}
  }

  Future<CloudAgent?> createAgent(CloudAgent agent) async {
    try {
      final res = await http.post(
        Uri.parse('$_serverUrl/api/agents'),
        headers: {'content-type': 'application/json'},
        body: jsonEncode(agent.toCreateJson()),
      );
      if (res.statusCode == 201) {
        final created = CloudAgent.fromJson(
            jsonDecode(res.body) as Map<String, dynamic>);
        _agents.add(created);
        notifyListeners();
        return created;
      }
    } catch (e) {
      _error = 'Failed to create agent: $e';
    }
    notifyListeners();
    return null;
  }

  Future<void> deleteAgent(String id) async {
    try {
      await http.delete(Uri.parse('$_serverUrl/api/agents/$id'));
      _agents.removeWhere((a) => a.id == id);
      _history.remove(id);
      notifyListeners();
    } catch (_) {}
  }

  Future<void> sendMessage(String agentId, String content) async {
    _history.putIfAbsent(agentId, () => []);
    _history[agentId]!.add(CloudChatMessage(role: 'user', content: content));
    _running[agentId] = true;
    notifyListeners();

    // Add streaming placeholder
    _history[agentId]!.add(CloudChatMessage(role: 'assistant', content: '', isStreaming: true));
    notifyListeners();

    try {
      final historyPayload = _history[agentId]!
          .where((m) => !m.isStreaming)
          .map((m) => {'role': m.role, 'content': m.content})
          .toList();

      final res = await http.post(
        Uri.parse('$_serverUrl/api/agents/$agentId/run'),
        headers: {'content-type': 'application/json'},
        body: jsonEncode({'prompt': content, 'history': historyPayload}),
      );

      String reply = '';
      if (res.statusCode == 200) {
        // Parse SSE response
        for (final line in res.body.split('\n')) {
          if (line.startsWith('data: ')) {
            try {
              final event = jsonDecode(line.substring(6)) as Map<String, dynamic>;
              if (event['event'] == 'done') reply = event['reply'] as String? ?? '';
              if (event['event'] == 'error') reply = 'Error: ${event['error']}';
            } catch (_) {}
          }
        }
      } else {
        reply = 'Server error ${res.statusCode}: ${res.body}';
      }

      // Replace streaming placeholder with final reply
      final hist = _history[agentId]!;
      final idx = hist.lastIndexWhere((m) => m.isStreaming);
      if (idx >= 0) hist[idx] = CloudChatMessage(role: 'assistant', content: reply);
    } catch (e) {
      final hist = _history[agentId]!;
      final idx = hist.lastIndexWhere((m) => m.isStreaming);
      if (idx >= 0) hist[idx] = CloudChatMessage(role: 'assistant', content: 'Error: $e');
    }

    _running[agentId] = false;
    notifyListeners();
  }
}
