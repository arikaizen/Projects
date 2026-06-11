import 'dart:convert';
import 'package:http/http.dart' as http;

class OllamaClient {
  final String baseUrl;

  OllamaClient({this.baseUrl = 'http://localhost:11434'});

  Future<bool> isAvailable() async {
    try {
      final res = await http
          .get(Uri.parse('$baseUrl/api/tags'))
          .timeout(const Duration(seconds: 4));
      return res.statusCode == 200;
    } catch (_) {
      return false;
    }
  }

  Future<String> chat({
    required String model,
    required List<Map<String, dynamic>> messages,
    double temperature = 0.7,
  }) async {
    final body = {
      'model': model,
      'messages': messages,
      'stream': false,
      'options': {'temperature': temperature},
    };

    final res = await http.post(
      Uri.parse('$baseUrl/api/chat'),
      headers: {'content-type': 'application/json'},
      body: jsonEncode(body),
    ).timeout(const Duration(minutes: 5));

    if (res.statusCode != 200) {
      throw Exception('Ollama ${res.statusCode}: ${res.body}');
    }

    final data = jsonDecode(res.body) as Map<String, dynamic>;
    return ((data['message'] as Map?)?['content'] as String?) ?? '';
  }

  Future<List<Map<String, dynamic>>> listModels() async {
    final res = await http
        .get(Uri.parse('$baseUrl/api/tags'))
        .timeout(const Duration(seconds: 6));

    if (res.statusCode != 200) return [];

    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final raw = (data['models'] as List?) ?? [];
    return raw.map((m) => {
          'id': m['name'] as String,
          'name': m['name'] as String,
          'size': m['size'],
          'provider': 'ollama',
        } as Map<String, dynamic>).toList();
  }
}
