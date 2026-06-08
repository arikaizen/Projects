import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/model_provider.dart';
import '../models/agent_model.dart';

class LlmService {
  /// Call the appropriate LLM API and return the assistant text.
  Future<String> chat({
    required ModelProvider provider,
    required String modelId,
    required String systemPrompt,
    required List<ChatMessage> history,
    double temperature = 0.7,
  }) async {
    final base = provider.baseUrl.endsWith('/')
        ? provider.baseUrl.substring(0, provider.baseUrl.length - 1)
        : provider.baseUrl;
    switch (provider.type) {
      case ProviderType.anthropic:
        return _anthropic(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.ollama:
        return _ollama(base, modelId, systemPrompt, history, temperature);
      case ProviderType.openai:
      case ProviderType.custom:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
    }
  }

  Future<String> _anthropic(
    ModelProvider p,
    String base,
    String model,
    String system,
    List<ChatMessage> history,
    double temperature,
  ) async {
    final messages = history.map((m) => {
      'role': m.isUser ? 'user' : 'assistant',
      'content': m.content,
    }).toList();

    final body = {
      'model': model,
      'max_tokens': 4096,
      'temperature': temperature,
      if (system.isNotEmpty) 'system': system,
      'messages': messages,
    };

    final res = await http.post(
      Uri.parse('$base/v1/messages'),
      headers: {
        'content-type': 'application/json',
        'x-api-key': p.apiKey,
        'anthropic-version': '2023-06-01',
      },
      body: jsonEncode(body),
    ).timeout(const Duration(minutes: 3));

    if (res.statusCode != 200) {
      throw Exception('Anthropic error ${res.statusCode}: ${res.body}');
    }
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final content = (data['content'] as List?)?.first;
    return (content?['text'] as String?) ?? '';
  }

  Future<String> _ollama(
    String base,
    String model,
    String system,
    List<ChatMessage> history,
    double temperature,
  ) async {
    final messages = <Map<String, String>>[
      if (system.isNotEmpty) {'role': 'system', 'content': system},
      ...history.map((m) => {
        'role': m.isUser ? 'user' : 'assistant',
        'content': m.content,
      }),
    ];

    final body = {
      'model': model,
      'messages': messages,
      'stream': false,
      'options': {'temperature': temperature},
    };

    final res = await http.post(
      Uri.parse('$base/api/chat'),
      headers: {'content-type': 'application/json'},
      body: jsonEncode(body),
    ).timeout(const Duration(minutes: 5));

    if (res.statusCode != 200) {
      throw Exception('Ollama error ${res.statusCode}: ${res.body}');
    }
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    return ((data['message'] as Map?)?['content'] as String?) ?? '';
  }

  Future<String> _openAiCompat(
    ModelProvider p,
    String base,
    String model,
    String system,
    List<ChatMessage> history,
    double temperature,
  ) async {
    final messages = <Map<String, String>>[
      if (system.isNotEmpty) {'role': 'system', 'content': system},
      ...history.map((m) => {
        'role': m.isUser ? 'user' : 'assistant',
        'content': m.content,
      }),
    ];

    final body = {
      'model': model,
      'messages': messages,
      'temperature': temperature,
    };

    final headers = {
      'content-type': 'application/json',
      if (p.apiKey.isNotEmpty) 'authorization': 'Bearer ${p.apiKey}',
    };

    final res = await http.post(
      Uri.parse('$base/v1/chat/completions'),
      headers: headers,
      body: jsonEncode(body),
    ).timeout(const Duration(minutes: 5));

    if (res.statusCode != 200) {
      throw Exception('OpenAI-compat error ${res.statusCode}: ${res.body}');
    }
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final choices = data['choices'] as List?;
    return ((choices?.first as Map?)?['message'] as Map?)?['content'] as String? ?? '';
  }
}
