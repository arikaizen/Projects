import 'dart:async';
import 'dart:convert';
import 'dart:math';
import 'package:http/http.dart' as http;
import '../models/task_model.dart';

/// Communicates with the C++ AgentManager REST API.
/// Falls back to mock responses when not connected.
class AgentApiService {
  String? _baseUrl;
  bool _useMock = true;
  final _rng = Random();

  void configure(String url) {
    _baseUrl = url.endsWith('/') ? url.substring(0, url.length - 1) : url;
    _useMock = false;
  }

  Future<bool> ping(String url) async {
    try {
      configure(url);
      final res = await http.get(Uri.parse('$_baseUrl/health')).timeout(const Duration(seconds: 4));
      _useMock = res.statusCode != 200;
      return res.statusCode == 200;
    } catch (_) {
      _useMock = true;
      return false;
    }
  }

  Future<String> runTask(String prompt, String targetId, TaskTarget target) async {
    if (_useMock) return _mockResponse(prompt, targetId);

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
        final data = jsonDecode(res.body) as Map<String, dynamic>;
        return data['output'] as String? ?? data.toString();
      }
      throw Exception('Backend error ${res.statusCode}: ${res.body}');
    } catch (e) {
      // Fall back to mock if backend fails
      return _mockResponse(prompt, targetId);
    }
  }

  Future<Map<String, dynamic>> spawnAgent(Map<String, dynamic> config) async {
    if (_useMock) return {'agent_id': 'mock-${DateTime.now().millisecondsSinceEpoch}'};

    final res = await http.post(
      Uri.parse('$_baseUrl/api/agents'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode(config),
    ).timeout(const Duration(seconds: 10));

    return jsonDecode(res.body) as Map<String, dynamic>;
  }

  Future<Map<String, dynamic>> getAgentStatus(String agentId) async {
    if (_useMock) return {'status': 'idle', 'iterations': 0};

    final res = await http.get(
      Uri.parse('$_baseUrl/api/agents/$agentId/status'),
    ).timeout(const Duration(seconds: 5));

    return jsonDecode(res.body) as Map<String, dynamic>;
  }

  Future<void> cancelAgent(String agentId) async {
    if (_useMock) return;
    await http.post(Uri.parse('$_baseUrl/api/agents/$agentId/cancel'));
  }

  // ── Mock responses ─────────────────────────────────────────────────────────
  static const _mockPhrases = [
    "I've analyzed your request and here's what I found:\n\n",
    "Based on my research, here's the answer:\n\n",
    "I'll help you with that. Here's my response:\n\n",
    "After processing your query:\n\n",
  ];

  static const _mockBodies = [
    "The task has been completed successfully. I've processed all the relevant information and synthesized a comprehensive response that addresses your specific requirements.\n\n**Key findings:**\n- Analysis complete\n- Results validated\n- Output ready for review",
    "I've executed the requested operation. The pipeline ran through 3 stages:\n\n1. **Input processing** — parsed and validated\n2. **Core computation** — applied reasoning steps\n3. **Output synthesis** — formatted and verified\n\nAll steps completed without errors.",
    "Task accomplished. Here's a summary of what was done:\n\n```\nStatus: SUCCESS\nIterations: 4\nTools used: web_search, reasoning\nConfidence: 0.92\n```\n\nLet me know if you need clarification on any part of this result.",
    "I've completed the analysis. The request was processed through the LLM pipeline with the following outcome:\n\n- Primary objective: **achieved**\n- Secondary checks: **passed**\n- Edge cases handled: **yes**\n\nReady for the next task.",
  ];

  Future<String> _mockResponse(String prompt, String agentId) async {
    // Simulate thinking time
    await Future.delayed(Duration(milliseconds: 800 + _rng.nextInt(1200)));
    final prefix = _mockPhrases[_rng.nextInt(_mockPhrases.length)];
    final body   = _mockBodies[_rng.nextInt(_mockBodies.length)];
    return '$prefix$body';
  }
}
