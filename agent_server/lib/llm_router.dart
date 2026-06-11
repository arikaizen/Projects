import 'dart:convert';
import 'package:http/http.dart' as http;
import 'models.dart';

class ChatMessage {
  final String role; // user, assistant, system, tool
  final String content;
  final String? toolCallId;
  final String? toolName;
  ChatMessage({required this.role, required this.content,
      this.toolCallId, this.toolName});
  Map<String, dynamic> toJson() => {
    'role': role, 'content': content,
    if (toolCallId != null) 'tool_call_id': toolCallId,
    if (toolName != null) 'name': toolName,
  };
}

class ToolDefinition {
  final String name;
  final String description;
  final Map<String, dynamic> inputSchema;
  ToolDefinition({required this.name, required this.description, required this.inputSchema});
  Map<String, dynamic> toAnthropicJson() => {
    'name': name, 'description': description,
    'input_schema': inputSchema,
  };
  Map<String, dynamic> toOpenAiJson() => {
    'type': 'function',
    'function': {'name': name, 'description': description, 'parameters': inputSchema},
  };
}

class LlmResponse {
  final String? text;         // final text reply (null if tool_use)
  final String? toolName;     // non-null if the model wants to call a tool
  final Map<String, dynamic>? toolInput;
  final String? toolCallId;   // for OpenAI tool_call id
  final int inputTokens;
  final int outputTokens;
  LlmResponse({this.text, this.toolName, this.toolInput, this.toolCallId,
      this.inputTokens = 0, this.outputTokens = 0});
  bool get isToolCall => toolName != null;
}

class LlmRouter {
  final http.Client _client;
  LlmRouter() : _client = http.Client();

  Future<LlmResponse> chat({
    required LlmConfig config,
    required List<ChatMessage> messages,
    List<ToolDefinition> tools = const [],
  }) async {
    final provider = config.provider.toLowerCase();
    if (provider == 'anthropic') {
      return _anthropicChat(config, messages, tools);
    } else if (provider == 'ollama') {
      return _ollamaChat(config, messages, tools);
    } else {
      // OpenAI-compatible: openai, vllm, mistral, groq, together, cohere, xai, perplexity, gemini, custom
      return _openAiCompatChat(config, messages, tools);
    }
  }

  // ── Anthropic ──────────────────────────────────────────────────────────────

  Future<LlmResponse> _anthropicChat(LlmConfig config,
      List<ChatMessage> messages, List<ToolDefinition> tools) async {
    final baseUrl = config.baseUrl.isNotEmpty
        ? config.baseUrl : 'https://api.anthropic.com';
    final url = Uri.parse('$baseUrl/v1/messages');

    String? system;
    final filtered = <Map<String, dynamic>>[];
    for (final m in messages) {
      if (m.role == 'system') {
        system = m.content;
      } else if (m.role == 'tool') {
        filtered.add({
          'role': 'user',
          'content': [{'type': 'tool_result', 'tool_use_id': m.toolCallId, 'content': m.content}],
        });
      } else {
        filtered.add({'role': m.role, 'content': m.content});
      }
    }

    final body = <String, dynamic>{
      'model': config.model,
      'max_tokens': config.maxTokens,
      'temperature': config.temperature,
      'messages': filtered,
      if (system != null) 'system': system,
      if (tools.isNotEmpty) 'tools': tools.map((t) => t.toAnthropicJson()).toList(),
    };

    final res = await _client.post(url,
        headers: {
          'x-api-key': config.apiKey,
          'anthropic-version': '2023-06-01',
          'content-type': 'application/json',
        },
        body: jsonEncode(body));

    if (res.statusCode != 200) {
      throw Exception('Anthropic ${res.statusCode}: ${res.body}');
    }

    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final content = data['content'] as List;
    final usage = data['usage'] as Map<String, dynamic>? ?? {};

    for (final block in content) {
      final b = block as Map<String, dynamic>;
      if (b['type'] == 'tool_use') {
        return LlmResponse(
          toolName: b['name'] as String,
          toolInput: b['input'] as Map<String, dynamic>,
          toolCallId: b['id'] as String,
          inputTokens: (usage['input_tokens'] as int?) ?? 0,
          outputTokens: (usage['output_tokens'] as int?) ?? 0,
        );
      }
    }

    final text = content
        .where((b) => (b as Map)['type'] == 'text')
        .map((b) => (b as Map)['text'] as String)
        .join('');

    return LlmResponse(
      text: text,
      inputTokens: (usage['input_tokens'] as int?) ?? 0,
      outputTokens: (usage['output_tokens'] as int?) ?? 0,
    );
  }

  // ── OpenAI-compatible ──────────────────────────────────────────────────────

  Future<LlmResponse> _openAiCompatChat(LlmConfig config,
      List<ChatMessage> messages, List<ToolDefinition> tools) async {
    String baseUrl;
    if (config.baseUrl.isNotEmpty) {
      baseUrl = config.baseUrl;
    } else {
      baseUrl = _defaultOpenAiUrl(config.provider);
    }
    final url = Uri.parse('${baseUrl.replaceAll(RegExp(r'/$'), '')}/v1/chat/completions');

    final msgs = messages.map((m) {
      if (m.role == 'tool') {
        return {'role': 'tool', 'content': m.content, 'tool_call_id': m.toolCallId};
      }
      return {'role': m.role, 'content': m.content};
    }).toList();

    final body = <String, dynamic>{
      'model': config.model,
      'messages': msgs,
      'temperature': config.temperature,
      'max_tokens': config.maxTokens,
      if (tools.isNotEmpty) 'tools': tools.map((t) => t.toOpenAiJson()).toList(),
      if (tools.isNotEmpty) 'tool_choice': 'auto',
    };

    final res = await _client.post(url,
        headers: {
          'authorization': 'Bearer ${config.apiKey}',
          'content-type': 'application/json',
        },
        body: jsonEncode(body));

    if (res.statusCode != 200) {
      throw Exception('${config.provider} ${res.statusCode}: ${res.body}');
    }

    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final choice = (data['choices'] as List).first as Map<String, dynamic>;
    final message = choice['message'] as Map<String, dynamic>;
    final usage = data['usage'] as Map<String, dynamic>? ?? {};

    final toolCalls = message['tool_calls'] as List?;
    if (toolCalls != null && toolCalls.isNotEmpty) {
      final tc = toolCalls.first as Map<String, dynamic>;
      final fn = tc['function'] as Map<String, dynamic>;
      Map<String, dynamic> args;
      try { args = jsonDecode(fn['arguments'] as String) as Map<String, dynamic>; }
      catch (_) { args = {}; }
      return LlmResponse(
        toolName: fn['name'] as String,
        toolInput: args,
        toolCallId: tc['id'] as String,
        inputTokens: (usage['prompt_tokens'] as int?) ?? 0,
        outputTokens: (usage['completion_tokens'] as int?) ?? 0,
      );
    }

    return LlmResponse(
      text: message['content'] as String? ?? '',
      inputTokens: (usage['prompt_tokens'] as int?) ?? 0,
      outputTokens: (usage['completion_tokens'] as int?) ?? 0,
    );
  }

  String _defaultOpenAiUrl(String provider) {
    switch (provider) {
      case 'openai':     return 'https://api.openai.com';
      case 'mistral':    return 'https://api.mistral.ai';
      case 'groq':       return 'https://api.groq.com/openai';
      case 'together':   return 'https://api.together.xyz';
      case 'xai':        return 'https://api.x.ai';
      case 'perplexity': return 'https://api.perplexity.ai';
      case 'gemini':     return 'https://generativelanguage.googleapis.com/v1beta/openai';
      default:           return 'http://localhost:8000'; // vllm default
    }
  }

  // ── Ollama ─────────────────────────────────────────────────────────────────

  Future<LlmResponse> _ollamaChat(LlmConfig config,
      List<ChatMessage> messages, List<ToolDefinition> tools) async {
    final baseUrl = config.baseUrl.isNotEmpty
        ? config.baseUrl : 'http://localhost:11434';
    final url = Uri.parse('$baseUrl/api/chat');

    final msgs = messages.map((m) => {
      'role': m.role == 'tool' ? 'user' : m.role,
      'content': m.role == 'tool'
          ? '[Tool result for ${m.toolName}]: ${m.content}'
          : m.content,
    }).toList();

    final body = <String, dynamic>{
      'model': config.model,
      'messages': msgs,
      'stream': false,
      'options': {'temperature': config.temperature},
    };

    final res = await _client.post(url,
        headers: {'content-type': 'application/json'},
        body: jsonEncode(body));

    if (res.statusCode != 200) {
      throw Exception('Ollama ${res.statusCode}: ${res.body}');
    }

    final data = jsonDecode(res.body) as Map<String, dynamic>;
    final msg = data['message'] as Map<String, dynamic>;
    return LlmResponse(text: msg['content'] as String? ?? '');
  }
}
