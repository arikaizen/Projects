/// MCP JSON-RPC 2.0 protocol types.
library mcp_protocol;

import 'dart:convert';

// ── Request / Response ────────────────────────────────────────────────────────

class JsonRpcRequest {
  final String jsonrpc;
  final dynamic id; // int | String | null
  final String method;
  final Map<String, dynamic>? params;

  JsonRpcRequest({
    this.jsonrpc = '2.0',
    required this.id,
    required this.method,
    this.params,
  });

  factory JsonRpcRequest.fromJson(Map<String, dynamic> j) => JsonRpcRequest(
        jsonrpc: j['jsonrpc'] as String? ?? '2.0',
        id: j['id'],
        method: j['method'] as String,
        params: j['params'] as Map<String, dynamic>?,
      );

  Map<String, dynamic> toJson() => {
        'jsonrpc': jsonrpc,
        if (id != null) 'id': id,
        'method': method,
        if (params != null) 'params': params,
      };
}

Map<String, dynamic> successResponse(dynamic id, dynamic result) => {
      'jsonrpc': '2.0',
      'id': id,
      'result': result,
    };

Map<String, dynamic> errorResponse(dynamic id, int code, String message,
        [dynamic data]) =>
    {
      'jsonrpc': '2.0',
      'id': id,
      'error': {
        'code': code,
        'message': message,
        if (data != null) 'data': data,
      },
    };

// ── MCP Content ───────────────────────────────────────────────────────────────

Map<String, dynamic> textContent(String text) =>
    {'type': 'text', 'text': text};

Map<String, dynamic> toolResult(String text, {bool isError = false}) => {
      'content': [textContent(text)],
      if (isError) 'isError': true,
    };

// ── Tool definitions ──────────────────────────────────────────────────────────

const kTools = [
  {
    'name': 'chat',
    'description':
        'Send a conversation to a Claude or Ollama model and get a reply. '
            'Supports multi-turn history.',
    'inputSchema': {
      'type': 'object',
      'required': ['messages'],
      'properties': {
        'model': {
          'type': 'string',
          'description':
              'Model ID, e.g. claude-sonnet-4-6, llama3:8b. '
              'Defaults to claude-sonnet-4-6.',
        },
        'system': {
          'type': 'string',
          'description': 'System prompt / persona for the assistant.',
        },
        'messages': {
          'type': 'array',
          'description': 'List of {role, content} messages.',
          'items': {
            'type': 'object',
            'required': ['role', 'content'],
            'properties': {
              'role': {'type': 'string', 'enum': ['user', 'assistant']},
              'content': {'type': 'string'},
            },
          },
        },
        'temperature': {
          'type': 'number',
          'description': '0–1. Defaults to 0.7.',
        },
        'max_tokens': {
          'type': 'integer',
          'description': 'Max tokens to generate. Defaults to 4096.',
        },
        'provider': {
          'type': 'string',
          'enum': ['auto', 'anthropic', 'ollama'],
          'description':
              'Force a specific backend. "auto" picks based on model prefix.',
        },
      },
    },
  },
  {
    'name': 'list_models',
    'description':
        'List all available models across connected providers (Claude + Ollama).',
    'inputSchema': {
      'type': 'object',
      'properties': {
        'provider': {
          'type': 'string',
          'enum': ['all', 'anthropic', 'ollama'],
          'description': 'Filter by provider. Defaults to "all".',
        },
      },
    },
  },
  {
    'name': 'complete',
    'description': 'Single-turn text completion (no history).',
    'inputSchema': {
      'type': 'object',
      'required': ['prompt'],
      'properties': {
        'prompt': {'type': 'string'},
        'model': {'type': 'string'},
        'system': {'type': 'string'},
        'temperature': {'type': 'number'},
        'max_tokens': {'type': 'integer'},
      },
    },
  },
  {
    'name': 'ping',
    'description': 'Health check — returns server status and provider availability.',
    'inputSchema': {'type': 'object', 'properties': {}},
  },
];

// ── Capabilities negotiation ──────────────────────────────────────────────────

const kServerInfo = {
  'name': 'agent-studio-mcp',
  'version': '1.0.0',
};

const kCapabilities = {
  'tools': {'listChanged': false},
};

String prettyJson(Object? obj) =>
    const JsonEncoder.withIndent('  ').convert(obj);
