import 'dart:convert';
import 'mcp_protocol.dart';
import 'claude_client.dart';
import 'ollama_client.dart';

class McpHandler {
  final ClaudeClient claude;
  final OllamaClient ollama;

  McpHandler({required this.claude, required this.ollama});

  // ── Dispatch ────────────────────────────────────────────────────────────────

  Future<Map<String, dynamic>> handle(Map<String, dynamic> raw) async {
    late JsonRpcRequest req;
    try {
      req = JsonRpcRequest.fromJson(raw);
    } catch (e) {
      return errorResponse(null, -32700, 'Parse error: $e');
    }

    try {
      switch (req.method) {
        case 'initialize':
          return _initialize(req);
        case 'notifications/initialized':
          // Fire-and-forget notification — no response needed but we must not crash
          return {};
        case 'tools/list':
          return successResponse(req.id, {'tools': kTools});
        case 'tools/call':
          return await _toolsCall(req);
        case 'ping':
          return successResponse(req.id, {'status': 'ok'});
        default:
          return errorResponse(req.id, -32601, 'Method not found: ${req.method}');
      }
    } catch (e, st) {
      return errorResponse(req.id, -32603, 'Internal error: $e',
          st.toString().split('\n').take(3).join('\n'));
    }
  }

  // ── initialize ──────────────────────────────────────────────────────────────

  Map<String, dynamic> _initialize(JsonRpcRequest req) {
    return successResponse(req.id, {
      'protocolVersion': '2024-11-05',
      'capabilities': kCapabilities,
      'serverInfo': kServerInfo,
    });
  }

  // ── tools/call ──────────────────────────────────────────────────────────────

  Future<Map<String, dynamic>> _toolsCall(JsonRpcRequest req) async {
    final params = req.params ?? {};
    final name   = params['name'] as String?;
    final args   = (params['arguments'] as Map<String, dynamic>?) ?? {};

    switch (name) {
      case 'chat':     return successResponse(req.id, await _chat(args));
      case 'complete': return successResponse(req.id, await _complete(args));
      case 'list_models': return successResponse(req.id, await _listModels(args));
      case 'ping':     return successResponse(req.id, await _ping());
      default:
        return errorResponse(req.id, -32601, 'Unknown tool: $name');
    }
  }

  // ── Tool: chat ──────────────────────────────────────────────────────────────

  Future<Map<String, dynamic>> _chat(Map<String, dynamic> args) async {
    final model       = args['model'] as String? ?? 'claude-sonnet-4-6';
    final system      = args['system'] as String?;
    final temperature = (args['temperature'] as num?)?.toDouble() ?? 0.7;
    final maxTokens   = (args['max_tokens'] as int?) ?? 4096;
    final rawMessages = (args['messages'] as List?) ?? [];
    final messages    = rawMessages.cast<Map<String, dynamic>>();
    final provider    = args['provider'] as String? ?? 'auto';

    final useOllama = provider == 'ollama' ||
        (provider == 'auto' && _isOllamaModel(model));

    String reply;
    if (useOllama) {
      final ollamaMessages = <Map<String, String>>[
        if (system != null && system.isNotEmpty)
          {'role': 'system', 'content': system},
        ...messages.map((m) => {
              'role': m['role'] as String,
              'content': m['content'] as String,
            }),
      ];
      reply = await ollama.chat(
        model: model,
        messages: ollamaMessages,
        temperature: temperature,
      );
    } else {
      reply = await claude.chat(
        model: model,
        messages: messages,
        system: system,
        temperature: temperature,
        maxTokens: maxTokens,
      );
    }

    return toolResult(reply);
  }

  // ── Tool: complete ──────────────────────────────────────────────────────────

  Future<Map<String, dynamic>> _complete(Map<String, dynamic> args) async {
    final prompt      = args['prompt'] as String? ?? '';
    final model       = args['model'] as String? ?? 'claude-sonnet-4-6';
    final system      = args['system'] as String?;
    final temperature = (args['temperature'] as num?)?.toDouble() ?? 0.7;
    final maxTokens   = (args['max_tokens'] as int?) ?? 4096;

    return _chat({
      'model': model,
      'system': system,
      'temperature': temperature,
      'max_tokens': maxTokens,
      'messages': [
        {'role': 'user', 'content': prompt}
      ],
    });
  }

  // ── Tool: list_models ───────────────────────────────────────────────────────

  Future<Map<String, dynamic>> _listModels(Map<String, dynamic> args) async {
    final filter = args['provider'] as String? ?? 'all';
    final models = <Map<String, dynamic>>[];

    if (filter == 'all' || filter == 'anthropic') {
      try {
        models.addAll(await claude.listModels());
      } catch (e) {
        models.add({'error': 'Anthropic: $e', 'provider': 'anthropic'});
      }
    }

    if (filter == 'all' || filter == 'ollama') {
      if (await ollama.isAvailable()) {
        try {
          models.addAll(await ollama.listModels());
        } catch (e) {
          models.add({'error': 'Ollama: $e', 'provider': 'ollama'});
        }
      } else {
        models.add({
          'error': 'Ollama not reachable at ${ollama.baseUrl}',
          'provider': 'ollama',
        });
      }
    }

    return toolResult(jsonEncode({'models': models}));
  }

  // ── Tool: ping ──────────────────────────────────────────────────────────────

  Future<Map<String, dynamic>> _ping() async {
    final ollamaOk = await ollama.isAvailable();
    return toolResult(jsonEncode({
      'status': 'ok',
      'providers': {
        'anthropic': {
          'configured': claude.isConfigured,
          'base_url': claude.baseUrl,
        },
        'ollama': {
          'available': ollamaOk,
          'base_url': ollama.baseUrl,
        },
      },
    }));
  }

  // ── Helpers ─────────────────────────────────────────────────────────────────

  bool _isOllamaModel(String model) {
    const claudePrefixes = ['claude-', 'claude_'];
    const openAiPrefixes = ['gpt-', 'o1-', 'o3-'];
    final lower = model.toLowerCase();
    if (claudePrefixes.any((p) => lower.startsWith(p))) return false;
    if (openAiPrefixes.any((p) => lower.startsWith(p))) return false;
    return true; // llama3:8b, mistral, gemma, etc. → Ollama
  }
}
