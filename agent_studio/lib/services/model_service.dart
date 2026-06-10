import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/model_provider.dart';

/// Connects to a provider and lists its models.
/// Google uses the Generative Language API, Anthropic its Messages API models
/// endpoint, Ollama its native /api/tags; every other provider speaks the
/// OpenAI /v1/models protocol.
class ModelService {
  Future<(bool, List<ModelInfo>, String?)> connect(ModelProvider provider) async {
    final base = provider.baseUrl.endsWith('/')
        ? provider.baseUrl.substring(0, provider.baseUrl.length - 1)
        : provider.baseUrl;
    try {
      switch (provider.type) {
        case ProviderType.ollama:
          return await _ollama(provider, base);
        case ProviderType.anthropic:
          return await _anthropic(provider, base);
        case ProviderType.google:
          return await _google(provider, base);
        default:
          return await _openAiCompat(provider, base);
      }
    } catch (e) {
      // Local servers that are down get a clean failure; hosted providers fall
      // back to a known-model list so the user can still pick one.
      final known = _knownModels(provider);
      if (known.isNotEmpty && !provider.isLocal) return (true, known, null);
      return (false, <ModelInfo>[], e.toString());
    }
  }

  Future<(bool, List<ModelInfo>, String?)> _ollama(
      ModelProvider p, String base) async {
    final res = await http
        .get(Uri.parse('$base/api/tags'))
        .timeout(const Duration(seconds: 6));
    if (res.statusCode != 200) {
      return (false, <ModelInfo>[], 'HTTP ${res.statusCode}');
    }
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final raw = (data['models'] as List?) ?? [];
    final models = raw.map((m) {
      final id = m['name'] as String;
      return ModelInfo(
        id: id, name: id,
        providerId: p.id, providerName: p.name, providerType: p.type,
      );
    }).toList();
    return (true, models, null);
  }

  Future<(bool, List<ModelInfo>, String?)> _anthropic(
      ModelProvider p, String base) async {
    if (p.apiKey.trim().isEmpty) {
      return (false, <ModelInfo>[], 'API key required');
    }
    try {
      final res = await http.get(
        Uri.parse('$base/v1/models'),
        headers: {
          'x-api-key': p.apiKey,
          'anthropic-version': '2023-06-01',
        },
      ).timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        final data = jsonDecode(res.body) as Map<String, dynamic>;
        final raw = (data['data'] as List?) ?? [];
        if (raw.isNotEmpty) {
          return (true, raw.map((m) => ModelInfo(
            id: m['id'] as String,
            name: (m['display_name'] as String?) ?? (m['id'] as String),
            providerId: p.id, providerName: p.name, providerType: p.type,
          )).toList(), null);
        }
      }
    } catch (_) {}
    return (true, _knownModels(p), null);
  }

  Future<(bool, List<ModelInfo>, String?)> _google(
      ModelProvider p, String base) async {
    if (p.apiKey.trim().isEmpty) {
      return (false, <ModelInfo>[], 'API key or Google sign-in required');
    }
    // API key goes in the query string; an OAuth access token in the header.
    final bearer = p.authMethod == AuthMethod.googleOAuth ||
        p.authMethod == AuthMethod.bearerToken;
    final uri = bearer
        ? Uri.parse('$base/v1beta/models?pageSize=100')
        : Uri.parse('$base/v1beta/models?pageSize=100&key=${p.apiKey}');
    try {
      final res = await http.get(
        uri,
        headers: {if (bearer) 'Authorization': 'Bearer ${p.apiKey}'},
      ).timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        final data = jsonDecode(res.body) as Map<String, dynamic>;
        final raw = (data['models'] as List?) ?? [];
        final models = raw
            .where((m) => ((m['supportedGenerationMethods'] as List?) ?? [])
                .contains('generateContent'))
            .map((m) {
          // "name" arrives as "models/gemini-2.0-flash" — strip the prefix.
          final full = (m['name'] as String?) ?? '';
          final id = full.startsWith('models/') ? full.substring(7) : full;
          return ModelInfo(
            id: id,
            name: (m['displayName'] as String?) ?? id,
            providerId: p.id, providerName: p.name, providerType: p.type,
          );
        }).toList();
        if (models.isNotEmpty) return (true, models, null);
      } else {
        return (false, <ModelInfo>[], 'HTTP ${res.statusCode}: ${res.body}');
      }
    } catch (_) {}
    return (true, _knownModels(p), null);
  }

  Future<(bool, List<ModelInfo>, String?)> _openAiCompat(
      ModelProvider p, String base) async {
    final headers = <String, String>{
      'Content-Type': 'application/json',
      if (p.apiKey.isNotEmpty) 'Authorization': 'Bearer ${p.apiKey}',
    };
    final res = await http
        .get(Uri.parse('$base/v1/models'), headers: headers)
        .timeout(const Duration(seconds: 6));
    if (res.statusCode != 200) {
      return (false, <ModelInfo>[], 'HTTP ${res.statusCode}');
    }
    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final raw = (data['data'] as List?) ?? [];
    final models = raw.map((m) {
      final id = m['id'] as String;
      return ModelInfo(
        id: id, name: id,
        providerId: p.id, providerName: p.name, providerType: p.type,
      );
    }).toList();
    return (true, models, null);
  }

  /// Fallback model lists used when live listing is unavailable (e.g. CORS in
  /// the web build, or a transient network error). Live listing is always
  /// preferred — these are just enough to get started.
  List<ModelInfo> _knownModels(ModelProvider p) {
    List<String> ids;
    switch (p.type) {
      case ProviderType.anthropic:
        ids = ['claude-opus-4-8', 'claude-sonnet-4-6', 'claude-haiku-4-5-20251001'];
        break;
      case ProviderType.openai:
        ids = ['gpt-4o', 'gpt-4o-mini', 'gpt-4.1', 'o3-mini'];
        break;
      case ProviderType.google:
        ids = ['gemini-2.5-pro', 'gemini-2.5-flash', 'gemini-2.0-flash'];
        break;
      case ProviderType.groq:
        ids = ['llama-3.3-70b-versatile', 'mixtral-8x7b-32768'];
        break;
      case ProviderType.mistral:
        ids = ['mistral-large-latest', 'mistral-small-latest', 'codestral-latest'];
        break;
      case ProviderType.deepseek:
        ids = ['deepseek-chat', 'deepseek-reasoner'];
        break;
      case ProviderType.xai:
        ids = ['grok-3', 'grok-3-mini'];
        break;
      case ProviderType.openrouter:
        ids = [
          'anthropic/claude-sonnet-4-6',
          'openai/gpt-4o',
          'google/gemini-2.5-pro',
          'meta-llama/llama-3.3-70b-instruct',
        ];
        break;
      case ProviderType.together:
        ids = ['meta-llama/Llama-3.3-70B-Instruct-Turbo'];
        break;
      default:
        ids = [];
    }
    return ids
        .map((id) => ModelInfo(
              id: id, name: id,
              providerId: p.id, providerName: p.name, providerType: p.type,
            ))
        .toList();
  }
}
