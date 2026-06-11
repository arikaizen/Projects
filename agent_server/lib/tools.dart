import 'dart:convert';
import 'package:http/http.dart' as http;
import 'llm_router.dart';

// All tools available to agents
final kToolDefinitions = <String, ToolDefinition>{
  'calculator': ToolDefinition(
    name: 'calculator',
    description: 'Evaluate a mathematical expression. Input: {"expression": "2+2*3"}',
    inputSchema: {
      'type': 'object',
      'properties': {'expression': {'type': 'string', 'description': 'Math expression to evaluate'}},
      'required': ['expression'],
    },
  ),
  'get_time': ToolDefinition(
    name: 'get_time',
    description: 'Get the current date and time.',
    inputSchema: {'type': 'object', 'properties': {}},
  ),
  'web_fetch': ToolDefinition(
    name: 'web_fetch',
    description: 'Fetch the text content of a URL.',
    inputSchema: {
      'type': 'object',
      'properties': {'url': {'type': 'string', 'description': 'URL to fetch'}},
      'required': ['url'],
    },
  ),
};

Future<String> executeTool(String name, Map<String, dynamic> input) async {
  switch (name) {
    case 'calculator':
      final expr = input['expression'] as String? ?? '';
      try {
        return _evalMath(expr);
      } catch (e) {
        return 'Error evaluating expression: $e';
      }
    case 'get_time':
      return DateTime.now().toIso8601String();
    case 'web_fetch':
      final url = input['url'] as String? ?? '';
      try {
        final res = await http.get(Uri.parse(url))
            .timeout(const Duration(seconds: 10));
        // Strip HTML tags, return first 3000 chars
        final text = res.body
            .replaceAll(RegExp(r'<[^>]+>'), ' ')
            .replaceAll(RegExp(r'\s+'), ' ')
            .trim();
        return text.length > 3000 ? '${text.substring(0, 3000)}…' : text;
      } catch (e) {
        return 'Error fetching URL: $e';
      }
    default:
      return 'Unknown tool: $name';
  }
}

// Trivial math evaluator for simple expressions (no eval() in Dart)
String _evalMath(String expr) {
  // Only handle simple expressions like "2 + 3 * 4"
  // For safety, we use a whitelist approach
  final clean = expr.replaceAll(' ', '');
  if (!RegExp(r'^[\d\+\-\*\/\.\(\)]+$').hasMatch(clean)) {
    return 'Error: only basic math operators allowed';
  }
  // Simple recursive parser
  try {
    final result = _parseExpr(clean, 0).$1;
    return result.toString();
  } catch (e) {
    return 'Error: $e';
  }
}

(double, int) _parseExpr(String s, int pos) {
  var (left, p) = _parseTerm(s, pos);
  while (p < s.length && (s[p] == '+' || s[p] == '-')) {
    final op = s[p]; p++;
    final (right, np) = _parseTerm(s, p);
    left = op == '+' ? left + right : left - right;
    p = np;
  }
  return (left, p);
}

(double, int) _parseTerm(String s, int pos) {
  var (left, p) = _parseFactor(s, pos);
  while (p < s.length && (s[p] == '*' || s[p] == '/')) {
    final op = s[p]; p++;
    final (right, np) = _parseFactor(s, p);
    left = op == '*' ? left * right : left / right;
    p = np;
  }
  return (left, p);
}

(double, int) _parseFactor(String s, int pos) {
  if (pos < s.length && s[pos] == '(') {
    final (val, p) = _parseExpr(s, pos + 1);
    if (p < s.length && s[p] == ')') return (val, p + 1);
    return (val, p);
  }
  int end = pos;
  if (end < s.length && s[end] == '-') end++;
  while (end < s.length && (s[end].contains(RegExp(r'[\d\.]')))) end++;
  if (end == pos) throw Exception('Expected number at $pos');
  return (double.parse(s.substring(pos, end)), end);
}
