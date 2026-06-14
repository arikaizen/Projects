import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/model_provider.dart';
import '../models/agent_model.dart';

/// The result of a single LLM completion: the text plus token-usage metrics
/// (when the provider reports them) used for benchmarking.
class LlmResult {
  final String content;
  final int? promptTokens;
  final int? completionTokens;
  final int? totalTokens;

  const LlmResult(
    this.content, {
    this.promptTokens,
    this.completionTokens,
    this.totalTokens,
  });

  /// Best-effort total: explicit total, else the sum of the parts.
  int? get total {
    if (totalTokens != null) return totalTokens;
    if (promptTokens != null && completionTokens != null) {
      return promptTokens! + completionTokens!;
    }
    return null;
  }
}

class LlmService {
  /// Convenience wrapper that returns just the reply text. Used by the chat UI.
  Future<String> chat({
    required ModelProvider provider,
    required String modelId,
    required String systemPrompt,
    required List<ChatMessage> history,
    double temperature = 0.7,
  }) async {
    final r = await complete(
      provider: provider,
      modelId: modelId,
      systemPrompt: systemPrompt,
      history: history,
      temperature: temperature,
    );
    return r.content;
  }

  /// Full completion including token usage. Used by the benchmark runner.
  Future<LlmResult> complete({
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
      case ProviderType.gemini:
        return _gemini(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.ollama:
        return _ollama(base, modelId, systemPrompt, history, temperature);
      case ProviderType.cohere:
        return _cohere(provider, base, modelId, systemPrompt, history, temperature);
      case ProviderType.openai:
      case ProviderType.mistral:
      case ProviderType.groq:
      case ProviderType.together:
      case ProviderType.xai:
      case ProviderType.perplexity:
      case ProviderType.custom:
        return _openAiCompat(provider, base, modelId, systemPrompt, history, temperature);
    }
  }

  // ── Anthropic ─────────────────────────────────────────────────────────────

  Future<LlmResult> _anthropic(ModelProvider p, String base, String model,
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
    final text = (data['content'] as List?)?.first?['text'] as String? ?? '';
    final usage = data['usage'] as Map<String, dynamic>?;
    return LlmResult(
      text,
      promptTokens: _asInt(usage?['input_tokens']),
      completionTokens: _asInt(usage?['output_tokens']),
    );
  }

  // ── OpenAI-compatible (OpenAI, Mistral, Groq, Together, xAI, Perplexity, custom) ──

  Future<LlmResult> _openAiCompat(ModelProvider p, String base, String model,
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
    final text = ((data['choices'] as List?)?.first?['message'] as Map?)?['content']
            as String? ?? '';
    final usage = data['usage'] as Map<String, dynamic>?;
    return LlmResult(
      text,
      promptTokens: _asInt(usage?['prompt_tokens']),
      completionTokens: _asInt(usage?['completion_tokens']),
      totalTokens: _asInt(usage?['total_tokens']),
    );
  }

  // ── Google Gemini ─────────────────────────────────────────────────────────

  Future<LlmResult> _gemini(ModelProvider p, String base, String model,
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
    final text = parts?.first?['text'] as String? ?? '';
    final usage = data['usageMetadata'] as Map<String, dynamic>?;
    return LlmResult(
      text,
      promptTokens: _asInt(usage?['promptTokenCount']),
      completionTokens: _asInt(usage?['candidatesTokenCount']),
      totalTokens: _asInt(usage?['totalTokenCount']),
    );
  }

  // ── Ollama ────────────────────────────────────────────────────────────────

  Future<LlmResult> _ollama(String base, String model,
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
    final text = (data['message'] as Map?)?['content'] as String? ?? '';
    return LlmResult(
      text,
      promptTokens: _asInt(data['prompt_eval_count']),
      completionTokens: _asInt(data['eval_count']),
    );
  }

  // ── Cohere ────────────────────────────────────────────────────────────────

  Future<LlmResult> _cohere(ModelProvider p, String base, String model,
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
    final text = data['text'] as String? ?? '';
    final billed = (data['meta'] as Map?)?['billed_units'] as Map?;
    return LlmResult(
      text,
      promptTokens: _asInt(billed?['input_tokens']),
      completionTokens: _asInt(billed?['output_tokens']),
    );
  }

  // ── Shared ────────────────────────────────────────────────────────────────

  static int? _asInt(Object? v) {
    if (v is int) return v;
    if (v is num) return v.toInt();
    if (v is String) return int.tryParse(v);
    return null;
  }

  void _checkStatus(http.Response res, String provider) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      throw Exception('$provider error ${res.statusCode}: ${res.body}');
    }
  }
}
