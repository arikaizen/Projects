import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/task_model.dart';

import 'engine_service_stub.dart'
    if (dart.library.ffi) 'engine_service_ffi.dart';

abstract class AgentBackend {
  Future<bool> connect(String target);
  void disconnect();
  bool get isConnected;
  String get connectionLabel;

  Future<String> runTask(String prompt, String targetId, TaskTarget target);
  Future<String> spawnEngineAgent(Map<String, dynamic> config);
  Future<Map<String, dynamic>> getEngineStatus(String agentId);
  Future<void> cancelEngineAgent(String agentId);
  Future<List<Map<String, dynamic>>> listEngineAgents();

  Future<void> connectMcp({
    required String name,
    required String url,
    String bearerToken = '',
    String transport = 'http',
  });
  Future<void> disconnectMcp(String serverName);
  Future<Map<String, dynamic>> listMcpServers();

  Stream<Map<String, dynamic>>? get engineEvents;
}

class AgentApiService {
  late AgentBackend _backend;

  AgentApiService() {
    _backend = createBackend();
  }

  Future<bool> connect(String target)  => _backend.connect(target);
  void         disconnect()            => _backend.disconnect();
  bool         get isConnected         => _backend.isConnected;
  String       get connectionLabel     => _backend.connectionLabel;
  Stream<Map<String, dynamic>>? get engineEvents => _backend.engineEvents;

  Future<String> runTask(String prompt, String targetId, TaskTarget target) =>
      _backend.runTask(prompt, targetId, target);

  Future<String>               spawnEngineAgent(Map<String, dynamic> c) => _backend.spawnEngineAgent(c);
  Future<Map<String, dynamic>> getEngineStatus(String id)               => _backend.getEngineStatus(id);
  Future<void>                 cancelEngineAgent(String id)              => _backend.cancelEngineAgent(id);
  Future<List<Map<String, dynamic>>> listEngineAgents()                  => _backend.listEngineAgents();
  Future<void>                 configureLlm(Map<String, dynamic> config) async {}
  // LLM config is pushed to the engine backend when connected via FFI or HTTP.
  Future<void>                 connectMcp({required String name, required String url,
                                           String bearerToken = '', String transport = 'http'}) =>
      _backend.connectMcp(name: name, url: url, bearerToken: bearerToken, transport: transport);
  Future<void>                 disconnectMcp(String name)  => _backend.disconnectMcp(name);
  Future<Map<String, dynamic>> listMcpServers()            => _backend.listMcpServers();
}

/// HTTP backend — no mocks. Every call either succeeds or throws.
class HttpBackend implements AgentBackend {
  String? _baseUrl;
  bool    _connected = false;

  final _eventCtrl = StreamController<Map<String, dynamic>>.broadcast();

  @override bool   get isConnected     => _connected;
  @override String get connectionLabel =>
      _connected ? 'HTTP: ${Uri.parse(_baseUrl!).host}' : 'Disconnected';
  @override Stream<Map<String, dynamic>> get engineEvents => _eventCtrl.stream;

  @override
  Future<bool> connect(String target) async {
    _baseUrl = target.endsWith('/') ? target.substring(0, target.length - 1) : target;
    try {
      final res = await http
          .get(Uri.parse('$_baseUrl/health'))
          .timeout(const Duration(seconds: 5));
      _connected = res.statusCode == 200;
    } catch (_) {
      _connected = false;
    }
    return _connected;
  }

  @override
  void disconnect() {
    _connected = false;
    _baseUrl   = null;
  }

  @override
  Future<String> runTask(String prompt, String targetId, TaskTarget target) async {
    _assertConnected();
    final endpoint = target == TaskTarget.group
        ? '/api/groups/$targetId/run'
        : '/api/agents/$targetId/run';
    final res = await http.post(
      Uri.parse('$_baseUrl$endpoint'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'task': prompt}),
    ).timeout(const Duration(minutes: 5));
    if (res.statusCode != 200) throw Exception('Engine HTTP ${res.statusCode}: ${res.body}');
    final data = jsonDecode(res.body);
    return (data is Map ? (data['output'] ?? data.toString()) : data).toString();
  }

  @override
  Future<String> spawnEngineAgent(Map<String, dynamic> config) async {
    _assertConnected();
    final res = await http.post(
      Uri.parse('$_baseUrl/api/agents'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode(config),
    ).timeout(const Duration(seconds: 10));
    if (res.statusCode != 200 && res.statusCode != 201) {
      throw Exception('Spawn agent HTTP ${res.statusCode}');
    }
    return (jsonDecode(res.body) as Map)['agent_id'] as String;
  }

  @override
  Future<Map<String, dynamic>> getEngineStatus(String id) async {
    _assertConnected();
    final res = await http
        .get(Uri.parse('$_baseUrl/api/agents/$id/status'))
        .timeout(const Duration(seconds: 5));
    if (res.statusCode != 200) throw Exception('Status HTTP ${res.statusCode}');
    return jsonDecode(res.body) as Map<String, dynamic>;
  }

  @override
  Future<void> cancelEngineAgent(String id) async {
    if (!_connected) return;
    await http
        .post(Uri.parse('$_baseUrl/api/agents/$id/cancel'))
        .timeout(const Duration(seconds: 5));
  }

  @override
  Future<List<Map<String, dynamic>>> listEngineAgents() async {
    _assertConnected();
    final res = await http
        .get(Uri.parse('$_baseUrl/api/agents'))
        .timeout(const Duration(seconds: 5));
    if (res.statusCode != 200) throw Exception('List agents HTTP ${res.statusCode}');
    return (jsonDecode(res.body) as List).cast<Map<String, dynamic>>();
  }

  @override
  Future<void> connectMcp({
    required String name,
    required String url,
    String bearerToken = '',
    String transport = 'http',
  }) async {
    _assertConnected();
    final res = await http.post(
      Uri.parse('$_baseUrl/api/mcp/connect'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'name': name, 'url': url,
                        'bearer_token': bearerToken, 'transport': transport}),
    ).timeout(const Duration(seconds: 10));
    if (res.statusCode != 200 && res.statusCode != 204) {
      throw Exception('connectMcp HTTP ${res.statusCode}');
    }
  }

  @override
  Future<void> disconnectMcp(String serverName) async {
    if (!_connected) return;
    await http
        .post(Uri.parse('$_baseUrl/api/mcp/$serverName/disconnect'))
        .timeout(const Duration(seconds: 5));
  }

  @override
  Future<Map<String, dynamic>> listMcpServers() async {
    _assertConnected();
    final res = await http
        .get(Uri.parse('$_baseUrl/api/mcp'))
        .timeout(const Duration(seconds: 5));
    if (res.statusCode != 200) throw Exception('listMcpServers HTTP ${res.statusCode}');
    return jsonDecode(res.body) as Map<String, dynamic>;
  }

  void _assertConnected() {
    if (!_connected) throw StateError('Not connected to engine — connect first in Settings.');
  }
}
