import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/model_provider.dart';
import '../models/agent_model.dart';

class LlmService {
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
      case ProviderType.openai:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.gemini:
        return _gemini(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.ollama:
        return _ollama(base, modelId, systemPrompt, history, temperature);
      case ProviderType.mistral:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.groq:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.together:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.cohere:
        return _cohere(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.xai:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.perplexity:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.custom:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
    }
  }

  // ── Anthropic ─────────────────────────────────────────────────────────────

  Future<String> _anthropic(ModelProvider p, String base, String model,
      String system, List<ChatMessage> history, double temp) async {
    final messages = history.map((m) => {
      'role': m.isUser ? 'user' : 'assistant',
      'content': m.content,
    }).toList();

    final res = await http.post(
      Uri.parse('$base/v1/messages'),
      headers: {
        'content-type': 'application/json',
        'x-api-key': p.apiKey,
        'anthropic-version': '2023-06-01',
      },
      body: jsonEncode({
        'model': model,
        'max_tokens': 8192,
        'temperature': temp,
        if (system.isNotEmpty) 'system': system,
        'messages': messages,
      }),
    ).timeout(const Duration(minutes: 3));

    _checkStatus(res, 'Anthropic');
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    return (data['content'] as List?)?.first?['text'] as String? ?? '';
  }

  // ── OpenAI-compatible (OpenAI, Mistral, Groq, Together, xAI, Perplexity, custom) ──

  Future<String> _openAiCompat(ModelProvider p, String base, String model,
      String system, List<ChatMessage> history, double temp) async {
    final messages = <Map<String, String>>[
      if (system.isNotEmpty) {'role': 'system', 'content': system},
      ...history.map((m) => {
        'role': m.isUser ? 'user' : 'assistant',
        'content': m.content,
      }),
    ];

    final res = await http.post(
      Uri.parse('$base/v1/chat/completions'),
      headers: {
        'content-type': 'application/json',
        if (p.apiKey.isNotEmpty) 'authorization': 'Bearer ${p.apiKey}',
      },
      body: jsonEncode({
        'model': model,
        'messages': messages,
        'temperature': temp,
      }),
    ).timeout(const Duration(minutes: 5));

    _checkStatus(res, p.name);
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    return ((data['choices'] as List?)?.first?['message'] as Map?)?['content']
            as String? ?? '';
  }

  // ── Google Gemini ─────────────────────────────────────────────────────────

  Future<String> _gemini(ModelProvider p, String base, String model,
      String system, List<ChatMessage> history, double temp) async {
    final contents = history.map((m) => {
      'role': m.isUser ? 'user' : 'model',
      'parts': [{'text': m.content}],
    }).toList();

    final body = <String, dynamic>{
      'contents': contents,
      if (system.isNotEmpty)
        'systemInstruction': {'parts': [{'text': system}]},
      'generationConfig': {
        'temperature': temp,
        'maxOutputTokens': 8192,
      },
    };

    // Auth: Google OAuth access token takes priority over API key
    final String url;
    final Map<String, String> headers = {'content-type': 'application/json'};
    if (p.googleAuth) {
      url = '$base/v1beta/models/$model:generateContent';
      headers['authorization'] = 'Bearer ${p.apiKey}';
    } else {
      url = '$base/v1beta/models/$model:generateContent?key=${p.apiKey}';
    }

    final res = await http.post(
      Uri.parse(url),
      headers: headers,
      body: jsonEncode(body),
    ).timeout(const Duration(minutes: 3));

    _checkStatus(res, 'Gemini');
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final candidates = data['candidates'] as List?;
    final parts = candidates?.first?['content']?['parts'] as List?;
    return parts?.first?['text'] as String? ?? '';
  }

  // ── Ollama ────────────────────────────────────────────────────────────────

  Future<String> _ollama(String base, String model,
      String system, List<ChatMessage> history, double temp) async {
    final messages = <Map<String, String>>[
      if (system.isNotEmpty) {'role': 'system', 'content': system},
      ...history.map((m) => {
        'role': m.isUser ? 'user' : 'assistant',
        'content': m.content,
      }),
    ];

    final res = await http.post(
      Uri.parse('$base/api/chat'),
      headers: {'content-type': 'application/json'},
      body: jsonEncode({
        'model': model,
        'messages': messages,
        'stream': false,
        'options': {'temperature': temp},
      }),
    ).timeout(const Duration(minutes: 5));

    _checkStatus(res, 'Ollama');
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    return (data['message'] as Map?)?['content'] as String? ?? '';
  }

  // ── Cohere ────────────────────────────────────────────────────────────────

  Future<String> _cohere(ModelProvider p, String base, String model,
      String system, List<ChatMessage> history, double temp) async {
    // Split history: last user message is the current prompt, rest is history
    final chatHistory = history.take(history.length - 1).map((m) => {
      'role': m.isUser ? 'USER' : 'CHATBOT',
      'message': m.content,
    }).toList();
    final lastMsg = history.isNotEmpty ? history.last.content : '';

    final res = await http.post(
      Uri.parse('$base/v1/chat'),
      headers: {
        'content-type': 'application/json',
        'authorization': 'Bearer ${p.apiKey}',
      },
      body: jsonEncode({
        'model': model,
        'message': lastMsg,
        if (chatHistory.isNotEmpty) 'chat_history': chatHistory,
        if (system.isNotEmpty) 'preamble': system,
        'temperature': temp,
      }),
    ).timeout(const Duration(minutes: 3));

    _checkStatus(res, 'Cohere');
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    return data['text'] as String? ?? '';
  }

  // ── Shared ────────────────────────────────────────────────────────────────

  void _checkStatus(http.Response res, String provider) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      throw Exception('$provider error ${res.statusCode}: ${res.body}');
    }
  }
}
