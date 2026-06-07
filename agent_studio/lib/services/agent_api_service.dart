import 'dart:async';
import 'dart:convert';
import 'dart:io' show Platform;
import 'dart:math';
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:http/http.dart' as http;
import '../models/task_model.dart';

// FFI import (conditionally — not available on web)
import 'engine_service_stub.dart'
    if (dart.library.ffi) 'engine_service_ffi.dart';

/// Public interface that both FFI and HTTP backends implement.
abstract class AgentBackend {
  Future<bool> connect(String target);  // path to .so OR http URL
  void disconnect();
  bool get isConnected;
  String get connectionLabel;

  Future<String> runTask(String prompt, String targetId, TaskTarget target);
  Future<String> spawnEngineAgent(Map<String, dynamic> config);
  Future<Map<String, dynamic>> getEngineStatus(String agentId);
  Future<void> cancelEngineAgent(String agentId);
  Future<List<Map<String, dynamic>>> listEngineAgents();

  Stream<Map<String, dynamic>>? get engineEvents;
}

/// Picks the right backend:
///  - On desktop (linux/macos/windows): try FFI first, fall back to HTTP mock
///  - On web: HTTP only (FFI not available)
class AgentApiService {
  late AgentBackend _backend;

  AgentApiService() {
    _backend = createBackend(); // resolved by conditional import
  }

  Future<bool> connect(String target) => _backend.connect(target);
  void         disconnect()           => _backend.disconnect();
  bool         get isConnected        => _backend.isConnected;
  String       get connectionLabel    => _backend.connectionLabel;
  Stream<Map<String, dynamic>>? get engineEvents => _backend.engineEvents;

  Future<String> runTask(String prompt, String targetId, TaskTarget target) =>
      _backend.runTask(prompt, targetId, target);

  Future<String> spawnEngineAgent(Map<String, dynamic> config) =>
      _backend.spawnEngineAgent(config);

  Future<Map<String, dynamic>> getEngineStatus(String id) =>
      _backend.getEngineStatus(id);

  Future<void> cancelEngineAgent(String id) =>
      _backend.cancelEngineAgent(id);

  Future<List<Map<String, dynamic>>> listEngineAgents() =>
      _backend.listEngineAgents();
}

// ── HTTP/Mock backend (works everywhere) ────────────────────────────────────

class HttpMockBackend implements AgentBackend {
  String? _baseUrl;
  bool _connected = false;
  bool _isMock    = true;
  final _rng      = Random();

  final _eventCtrl = StreamController<Map<String, dynamic>>.broadcast();

  @override
  Stream<Map<String, dynamic>> get engineEvents => _eventCtrl.stream;

  @override
  bool   get isConnected     => _connected;

  @override
  String get connectionLabel => _isMock
      ? 'Mock'
      : (_connected ? 'HTTP: ${Uri.parse(_baseUrl!).host}' : 'Disconnected');

  @override
  Future<bool> connect(String target) async {
    _baseUrl = target.endsWith('/') ? target.substring(0, target.length - 1) : target;
    try {
      final res = await http.get(Uri.parse('$_baseUrl/health'))
          .timeout(const Duration(seconds: 4));
      _connected = res.statusCode == 200;
      _isMock    = !_connected;
    } catch (_) {
      _connected = false;
      _isMock    = true;
    }
    return _connected;
  }

  @override
  void disconnect() {
    _connected = false;
    _isMock    = true;
    _baseUrl   = null;
  }

  @override
  Future<String> runTask(String prompt, String targetId, TaskTarget target) async {
    if (_isMock) return _mockResponse(prompt);

    try {
      final endpoint = target == TaskTarget.group
          ? '/api/groups/$targetId/run'
          : '/api/agents/$targetId/run';
      final res = await http.post(
        Uri.parse('$_baseUrl$endpoint'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'task': prompt}),
      ).timeout(const Duration(minutes: 5));

      if (res.statusCode == 200) {
        final data = jsonDecode(res.body);
        return (data is Map ? (data['output'] ?? data.toString()) : data).toString();
      }
      throw Exception('HTTP ${res.statusCode}');
    } catch (e) {
      return _mockResponse(prompt);
    }
  }

  @override
  Future<String> spawnEngineAgent(Map<String, dynamic> config) async {
    if (_isMock) return 'mock-${DateTime.now().millisecondsSinceEpoch}';
    final res = await http.post(
      Uri.parse('$_baseUrl/api/agents'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode(config),
    ).timeout(const Duration(seconds: 10));
    return (jsonDecode(res.body) as Map)['agent_id'] as String;
  }

  @override
  Future<Map<String, dynamic>> getEngineStatus(String id) async {
    if (_isMock) return {'status': 'idle', 'iterations': 0};
    final res = await http.get(Uri.parse('$_baseUrl/api/agents/$id/status'));
    return jsonDecode(res.body) as Map<String, dynamic>;
  }

  @override
  Future<void> cancelEngineAgent(String id) async {
    if (_isMock) return;
    await http.post(Uri.parse('$_baseUrl/api/agents/$id/cancel'));
  }

  @override
  Future<List<Map<String, dynamic>>> listEngineAgents() async {
    if (_isMock) return [];
    final res = await http.get(Uri.parse('$_baseUrl/api/agents'));
    return (jsonDecode(res.body) as List).cast<Map<String, dynamic>>();
  }

  // ── Mock responses ─────────────────────────────────────────────────────────

  Future<String> _mockResponse(String prompt) async {
    await Future.delayed(Duration(milliseconds: 600 + _rng.nextInt(1000)));
    return _responses[_rng.nextInt(_responses.length)];
  }

  static const _responses = [
    "I've processed your request and here's the result:\n\n"
    "**Analysis complete.** The task was executed through the reasoning pipeline.\n\n"
    "- Input validated ✓\n- Reasoning steps applied ✓\n- Output synthesised ✓\n\n"
    "_Note: running in mock mode — connect to your agent engine for real results._",

    "Task received and handled:\n\n```\nStatus: SUCCESS (mock)\nIterations: 3\nConfidence: 0.91\n```\n\n"
    "Connect to `libagent_engine.so` or the HTTP endpoint to run real agent inference.",

    "Here's my response to your query:\n\n"
    "The pipeline completed all stages successfully. "
    "For live results, point the app at your compiled agent engine.",
  ];
}
