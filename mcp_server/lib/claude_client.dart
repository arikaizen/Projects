import 'dart:convert';
import 'package:http/http.dart' as http;

class ClaudeClient {
  final String apiKey;
  final String baseUrl;

  ClaudeClient({
    required this.apiKey,
    this.baseUrl = 'https://api.anthropic.com',
  });

  bool get isConfigured => apiKey.isNotEmpty;

  Future<String> chat({
    required String model,
    required List<Map<String, dynamic>> messages,
    String? system,
    double temperature = 0.7,
    int maxTokens = 4096,
  }) async {
    if (!isConfigured) throw Exception('ANTHROPIC_API_KEY not set');

    final body = <String, dynamic>{
      'model': model,
      'max_tokens': maxTokens,
      'temperature': temperature,
      'messages': messages,
      if (system != null && system.isNotEmpty) 'system': system,
    };

    final res = await http.post(
      Uri.parse('$baseUrl/v1/messages'),
      headers: {
        'content-type': 'application/json',
        'x-api-key': apiKey,
        'anthropic-version': '2023-06-01',
      },
      body: jsonEncode(body),
    ).timeout(const Duration(minutes: 3));

    if (res.statusCode != 200) {
      throw Exception('Anthropic ${res.statusCode}: ${res.body}');
    }

    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final content = (data['content'] as List?)?.first as Map?;
    return content?['text'] as String? ?? '';
  }

  Future<List<Map<String, dynamic>>> listModels() async {
    if (!isConfigured) return _fallbackModels();

    try {
      final res = await http.get(
        Uri.parse('$baseUrl/v1/models'),
        headers: {
          'x-api-key': apiKey,
          'anthropic-version': '2023-06-01',
        },
      ).timeout(const Duration(seconds: 8));

      if (res.statusCode == 200) {
        final data = jsonDecode(res.body) as Map<String, dynamic>;
        final raw = (data['data'] as List?) ?? [];
        if (raw.isNotEmpty) {
          return raw.map((m) => {
            'id': m['id'],
            'name': m['display_name'] ?? m['id'],
            'provider': 'anthropic',
          } as Map<String, dynamic>).toList();
        }
      }
    } catch (_) {}

    return _fallbackModels();
  }

  List<Map<String, dynamic>> _fallbackModels() => [
        {'id': 'claude-opus-4-8',          'name': 'Claude Opus 4',    'provider': 'anthropic'},
        {'id': 'claude-sonnet-4-6',         'name': 'Claude Sonnet 4',  'provider': 'anthropic'},
        {'id': 'claude-haiku-4-5-20251001', 'name': 'Claude Haiku 4',   'provider': 'anthropic'},
      ];
}
