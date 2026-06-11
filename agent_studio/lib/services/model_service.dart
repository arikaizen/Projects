import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/model_provider.dart';

class ModelService {
  Future<(bool, List<ModelInfo>, String?)> connect(ModelProvider provider) async {
    final base = provider.baseUrl.endsWith('/')
        ? provider.baseUrl.substring(0, provider.baseUrl.length - 1)
        : provider.baseUrl;
    try {
      switch (provider.type) {
        case ProviderType.anthropic:
          return await _anthropic(provider, base);
        case ProviderType.openai:
          return await _openAiOfficial(provider, base);
        case ProviderType.gemini:
          return await _gemini(provider, base);
        case ProviderType.ollama:
          return await _ollama(provider, base);
        case ProviderType.mistral:
          return await _openAiCompat(provider, base);
        case ProviderType.groq:
          return await _openAiCompat(provider, base);
        case ProviderType.together:
          return await _openAiCompat(provider, base);
        case ProviderType.cohere:
          return await _cohere(provider, base);
        case ProviderType.xai:
          return await _openAiCompat(provider, base);
        case ProviderType.perplexity:
          return await _openAiCompat(provider, base);
        case ProviderType.custom:
          return await _openAiCompat(provider, base);
      }
    } catch (e) {
      // Fall back to built-in model list for known providers
      final builtin = _builtinModels(provider);
      if (builtin.isNotEmpty) return (true, builtin, null);
      return (false, <ModelInfo>[], e.toString());
    }
  }

  // ── Provider-specific fetchers ────────────────────────────────────────────

  Future<(bool, List<ModelInfo>, String?)> _anthropic(
      ModelProvider p, String base) async {
    if (p.apiKey.trim().isEmpty) return (true, _builtinModels(p), null);
    try {
      final res = await http.get(
        Uri.parse('$base/v1/models'),
        headers: {'x-api-key': p.apiKey, 'anthropic-version': '2023-06-01'},
      ).timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        final raw = (jsonDecode(res.body)['data'] as List?) ?? [];
        if (raw.isNotEmpty) {
          return (true, _mapModels(raw, p, idKey: 'id', nameKey: 'display_name'), null);
        }
      }
    } catch (_) {}
    return (true, _builtinModels(p), null);
  }

  Future<(bool, List<ModelInfo>, String?)> _openAiOfficial(
      ModelProvider p, String base) async {
    if (p.apiKey.trim().isEmpty) return (true, _builtinModels(p), null);
    try {
      final res = await http.get(
        Uri.parse('$base/v1/models'),
        headers: {'Authorization': 'Bearer ${p.apiKey}'},
      ).timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        final raw = (jsonDecode(res.body)['data'] as List?) ?? [];
        // Filter to chat models only
        final chat = raw.where((m) {
          final id = m['id'] as String? ?? '';
          return id.startsWith('gpt-') || id.startsWith('o1') ||
                 id.startsWith('o3') || id.startsWith('chatgpt');
        }).toList();
        if (chat.isNotEmpty) return (true, _mapModels(chat, p), null);
      }
    } catch (_) {}
    return (true, _builtinModels(p), null);
  }

  Future<(bool, List<ModelInfo>, String?)> _gemini(
      ModelProvider p, String base) async {
    if (p.apiKey.trim().isEmpty && !p.googleAuth) {
      return (true, _builtinModels(p), null);
    }
    try {
      final url = p.googleAuth
          ? '$base/v1beta/models'
          : '$base/v1beta/models?key=${p.apiKey}';
      final headers = p.googleAuth
          ? {'Authorization': 'Bearer ${p.apiKey}'} // access token stored in apiKey
          : <String, String>{};
      final res = await http
          .get(Uri.parse(url), headers: headers)
          .timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        final raw = (jsonDecode(res.body)['models'] as List?) ?? [];
        final chat = raw.where((m) {
          final name = m['name'] as String? ?? '';
          return name.contains('gemini') &&
                 (m['supportedGenerationMethods'] as List? ?? [])
                     .contains('generateContent');
        }).toList();
        if (chat.isNotEmpty) {
          return (true, chat.map((m) {
            final full = m['name'] as String; // e.g. models/gemini-pro
            final id   = full.split('/').last;
            return ModelInfo(
              id: id, name: m['displayName'] as String? ?? id,
              providerId: p.id, providerName: p.name, providerType: p.type,
            );
          }).toList(), null);
        }
      }
    } catch (_) {}
    return (true, _builtinModels(p), null);
  }

  Future<(bool, List<ModelInfo>, String?)> _ollama(
      ModelProvider p, String base) async {
    final res = await http
        .get(Uri.parse('$base/api/tags'))
        .timeout(const Duration(seconds: 6));
    if (res.statusCode != 200) return (false, <ModelInfo>[], 'HTTP ${res.statusCode}');
    final raw = (jsonDecode(res.body)['models'] as List?) ?? [];
    return (true, raw.map((m) {
      final id = m['name'] as String;
      return ModelInfo(id: id, name: id,
          providerId: p.id, providerName: p.name, providerType: p.type);
    }).toList(), null);
  }

  Future<(bool, List<ModelInfo>, String?)> _openAiCompat(
      ModelProvider p, String base) async {
    if (p.apiKey.trim().isEmpty) return (true, _builtinModels(p), null);
    try {
      final res = await http.get(
        Uri.parse('$base/v1/models'),
        headers: {'Authorization': 'Bearer ${p.apiKey}'},
      ).timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        final raw = (jsonDecode(res.body)['data'] as List?) ?? [];
        if (raw.isNotEmpty) return (true, _mapModels(raw, p), null);
      }
    } catch (_) {}
    return (true, _builtinModels(p), null);
  }

  Future<(bool, List<ModelInfo>, String?)> _cohere(
      ModelProvider p, String base) async {
    if (p.apiKey.trim().isEmpty) return (true, _builtinModels(p), null);
    try {
      final res = await http.get(
        Uri.parse('$base/v1/models'),
        headers: {'Authorization': 'Bearer ${p.apiKey}'},
      ).timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        final raw = (jsonDecode(res.body)['models'] as List?) ?? [];
        if (raw.isNotEmpty) {
          return (true, raw.map((m) => ModelInfo(
            id: m['name'] as String,
            name: m['name'] as String,
            providerId: p.id, providerName: p.name, providerType: p.type,
          )).toList(), null);
        }
      }
    } catch (_) {}
    return (true, _builtinModels(p), null);
  }

  // ── Helpers ────────────────────────────────────────────────────────────────

  List<ModelInfo> _mapModels(List raw, ModelProvider p,
      {String idKey = 'id', String nameKey = 'id'}) =>
      raw.map((m) => ModelInfo(
        id: m[idKey] as String,
        name: (m[nameKey] as String?) ?? (m[idKey] as String),
        providerId: p.id, providerName: p.name, providerType: p.type,
      )).toList();

  // ── Built-in model lists (used when API key absent or fetch fails) ─────────

  List<ModelInfo> _builtinModels(ModelProvider p) {
    final rows = _presets[p.type] ?? [];
    return rows.map((r) => ModelInfo(
      id: r[0], name: r[1],
      providerId: p.id, providerName: p.name, providerType: p.type,
    )).toList();
  }

  static const _presets = <ProviderType, List<List<String>>>{
    ProviderType.anthropic: [
      ['claude-opus-4-8',           'Claude Opus 4'],
      ['claude-sonnet-4-6',         'Claude Sonnet 4'],
      ['claude-haiku-4-5-20251001', 'Claude Haiku 4'],
      ['claude-opus-4-5',           'Claude Opus 4.5'],
      ['claude-sonnet-4-5',         'Claude Sonnet 4.5'],
    ],
    ProviderType.openai: [
      ['gpt-4o',           'GPT-4o'],
      ['gpt-4o-mini',      'GPT-4o Mini'],
      ['o3',               'o3'],
      ['o3-mini',          'o3 Mini'],
      ['o1',               'o1'],
      ['o1-mini',          'o1 Mini'],
      ['gpt-4-turbo',      'GPT-4 Turbo'],
    ],
    ProviderType.gemini: [
      ['gemini-2.5-pro',        'Gemini 2.5 Pro'],
      ['gemini-2.5-flash',      'Gemini 2.5 Flash'],
      ['gemini-2.0-flash',      'Gemini 2.0 Flash'],
      ['gemini-1.5-pro',        'Gemini 1.5 Pro'],
      ['gemini-1.5-flash',      'Gemini 1.5 Flash'],
    ],
    ProviderType.mistral: [
      ['mistral-large-latest',   'Mistral Large'],
      ['mistral-small-latest',   'Mistral Small'],
      ['codestral-latest',       'Codestral'],
      ['open-mistral-nemo',      'Mistral Nemo (free)'],
      ['open-codestral-mamba',   'Codestral Mamba'],
    ],
    ProviderType.groq: [
      ['llama-3.3-70b-versatile',      'LLaMA 3.3 70B'],
      ['llama-3.1-8b-instant',         'LLaMA 3.1 8B Instant'],
      ['mixtral-8x7b-32768',           'Mixtral 8x7B'],
      ['gemma2-9b-it',                 'Gemma 2 9B'],
      ['deepseek-r1-distill-llama-70b','DeepSeek R1 70B'],
    ],
    ProviderType.together: [
      ['meta-llama/Llama-3.3-70B-Instruct-Turbo', 'LLaMA 3.3 70B Turbo'],
      ['meta-llama/Llama-3.1-8B-Instruct-Turbo',  'LLaMA 3.1 8B Turbo'],
      ['mistralai/Mixtral-8x7B-Instruct-v0.1',     'Mixtral 8x7B'],
      ['Qwen/Qwen2.5-72B-Instruct-Turbo',          'Qwen 2.5 72B'],
      ['deepseek-ai/DeepSeek-R1',                  'DeepSeek R1'],
    ],
    ProviderType.cohere: [
      ['command-r-plus', 'Command R+'],
      ['command-r',      'Command R'],
      ['command',        'Command'],
    ],
    ProviderType.xai: [
      ['grok-3',         'Grok 3'],
      ['grok-3-mini',    'Grok 3 Mini'],
      ['grok-2',         'Grok 2'],
      ['grok-2-mini',    'Grok 2 Mini'],
    ],
    ProviderType.perplexity: [
      ['sonar-pro',             'Sonar Pro'],
      ['sonar',                 'Sonar'],
      ['sonar-reasoning-pro',   'Sonar Reasoning Pro'],
      ['sonar-reasoning',       'Sonar Reasoning'],
    ],
  };
}
