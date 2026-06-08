import 'dart:convert';
import 'dart:io';

import 'package:args/args.dart';
import 'package:shelf/shelf.dart';
import 'package:shelf/shelf_io.dart' as io;
import 'package:shelf_router/shelf_router.dart';

import '../lib/claude_client.dart';
import '../lib/mcp_handler.dart';
import '../lib/mcp_protocol.dart';
import '../lib/ollama_client.dart';

void main(List<String> argv) async {
  final parser = ArgParser()
    ..addOption('port',        abbr: 'p', defaultsTo: '3000',                    help: 'HTTP port to listen on')
    ..addOption('host',        abbr: 'H', defaultsTo: '127.0.0.1',               help: 'Interface to bind')
    ..addOption('anthropic-key',          defaultsTo: '',                         help: 'Anthropic API key (overrides ANTHROPIC_API_KEY env var)')
    ..addOption('anthropic-url',          defaultsTo: 'https://api.anthropic.com',help: 'Anthropic base URL')
    ..addOption('ollama-url',             defaultsTo: 'http://localhost:11434',   help: 'Ollama base URL')
    ..addFlag('verbose',       abbr: 'v', defaultsTo: false,                      help: 'Log every request/response')
    ..addFlag('help',          abbr: 'h', negatable: false,                       help: 'Show this help');

  final args = parser.parse(argv);
  if (args['help'] as bool) {
    stdout.writeln('Agent Studio MCP Server\n');
    stdout.writeln(parser.usage);
    exit(0);
  }

  final port       = int.parse(args['port'] as String);
  final host       = args['host'] as String;
  final apiKey     = (args['anthropic-key'] as String).isNotEmpty
      ? args['anthropic-key'] as String
      : Platform.environment['ANTHROPIC_API_KEY'] ?? '';
  final anthropicUrl = args['anthropic-url'] as String;
  final ollamaUrl    = args['ollama-url'] as String;
  final verbose      = args['verbose'] as bool;

  final handler = McpHandler(
    claude: ClaudeClient(apiKey: apiKey, baseUrl: anthropicUrl),
    ollama: OllamaClient(baseUrl: ollamaUrl),
  );

  final router = Router()
    ..post('/rpc',    (Request req) => _rpc(req, handler, verbose))
    ..get('/health',  (_) => Response.ok(
        jsonEncode({'status': 'ok', 'server': kServerInfo}),
        headers: _json))
    ..get('/tools',   (_) => Response.ok(
        jsonEncode({'tools': kTools}),
        headers: _json));

  final pipeline = const Pipeline()
      .addMiddleware(_cors())
      .addMiddleware(logRequests(logger: verbose ? null : (msg, _) {}))
      .addHandler(router.call);

  final server = await io.serve(pipeline, host, port);
  stdout.writeln('MCP server listening on http://${server.address.host}:${server.port}');
  stdout.writeln('  Anthropic API key : ${apiKey.isNotEmpty ? '${apiKey.substring(0, 8)}…' : '(not set — set ANTHROPIC_API_KEY)'}');
  stdout.writeln('  Anthropic URL     : $anthropicUrl');
  stdout.writeln('  Ollama URL        : $ollamaUrl');
  stdout.writeln();
  stdout.writeln('Endpoints:');
  stdout.writeln('  POST http://$host:$port/rpc    — MCP JSON-RPC 2.0');
  stdout.writeln('  GET  http://$host:$port/health — health check');
  stdout.writeln('  GET  http://$host:$port/tools  — list available tools');
}

// ── RPC handler ───────────────────────────────────────────────────────────────

Future<Response> _rpc(
    Request req, McpHandler handler, bool verbose) async {
  final body = await req.readAsString();
  Map<String, dynamic> raw;
  try {
    raw = jsonDecode(body) as Map<String, dynamic>;
  } catch (_) {
    return Response.badRequest(
      body: jsonEncode(errorResponse(null, -32700, 'Invalid JSON')),
      headers: _json,
    );
  }

  if (verbose) {
    stderr.writeln('→ ${prettyJson(raw)}');
  }

  final result = await handler.handle(raw);

  if (verbose && result.isNotEmpty) {
    stderr.writeln('← ${prettyJson(result)}');
  }

  // MCP notifications produce an empty map — send 204
  if (result.isEmpty) return Response(204);

  return Response.ok(jsonEncode(result), headers: _json);
}

// ── Middleware ────────────────────────────────────────────────────────────────

const _json = {'content-type': 'application/json'};

Middleware _cors() => (inner) => (req) async {
      if (req.method == 'OPTIONS') {
        return Response.ok('', headers: _corsHeaders);
      }
      final res = await inner(req);
      return res.change(headers: _corsHeaders);
    };

const _corsHeaders = {
  'access-control-allow-origin': '*',
  'access-control-allow-methods': 'GET, POST, OPTIONS',
  'access-control-allow-headers': 'content-type, authorization',
};
