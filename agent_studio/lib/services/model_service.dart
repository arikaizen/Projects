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
        case ProviderType.ollama:
          return await _ollama(provider, base);
        case ProviderType.anthropic:
          return await _anthropic(provider, base);
        case ProviderType.openai:
        case ProviderType.custom:
          return await _openAiCompat(provider, base);
      }
    } catch (e) {
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
    return (true, _anthropicKnown(p), null);
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

  List<ModelInfo> _anthropicKnown(ModelProvider p) => [
    ModelInfo(id: 'claude-opus-4-8',          name: 'Claude Opus 4',
        providerId: p.id, providerName: p.name, providerType: p.type),
    ModelInfo(id: 'claude-sonnet-4-6',         name: 'Claude Sonnet 4',
        providerId: p.id, providerName: p.name, providerType: p.type),
    ModelInfo(id: 'claude-haiku-4-5-20251001', name: 'Claude Haiku 4',
        providerId: p.id, providerName: p.name, providerType: p.type),
  ];
}
