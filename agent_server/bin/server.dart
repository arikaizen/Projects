import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:args/args.dart';
import 'package:shelf/shelf.dart';
import 'package:shelf/shelf_io.dart' as io;
import 'package:shelf_router/shelf_router.dart';
import '../lib/models.dart';
import '../lib/agent_loop.dart';
import '../lib/formation_runner.dart';
import '../lib/agent_store.dart';

void main(List<String> argv) async {
  final parser = ArgParser()
    ..addOption('port',   abbr: 'p', defaultsTo: '3001', help: 'HTTP port')
    ..addOption('host',   abbr: 'H', defaultsTo: '0.0.0.0', help: 'Bind address')
    ..addFlag('verbose',  abbr: 'v', defaultsTo: false)
    ..addFlag('help',     abbr: 'h', negatable: false);
  final args = parser.parse(argv);
  if (args['help'] as bool) { stdout.writeln(parser.usage); exit(0); }

  final port    = int.parse(args['port'] as String);
  final host    = args['host'] as String;
  final verbose = args['verbose'] as bool;

  final store    = AgentStore();
  final loop     = AgentLoop();
  final fRunner  = FormationRunner();

  final router = Router()
    // Health
    ..get('/health', (_) => _json({'status': 'ok', 'server': 'agent_server', 'version': '1.0.0'}))

    // ── Agents ────────────────────────────────────────────────────────────────
    ..get('/api/agents', (_) => _json({'agents': store.listAgents().map((a) => a.toJson()).toList()}))
    ..post('/api/agents', (Request req) async {
      final body = jsonDecode(await req.readAsString()) as Map<String, dynamic>;
      final agent = store.put(AgentConfig.fromJson(body));
      return _json(agent.toJson(), 201);
    })
    ..get('/api/agents/<id>', (Request _, String id) {
      final a = store.get(id);
      return a != null ? _json(a.toJson()) : _notFound('Agent $id not found');
    })
    ..put('/api/agents/<id>', (Request req, String id) async {
      final a = store.get(id);
      if (a == null) return _notFound('Agent $id not found');
      final body = jsonDecode(await req.readAsString()) as Map<String, dynamic>;
      final updated = AgentConfig.fromJson({...a.toJson(), ...body, 'id': id});
      store.put(updated);
      return _json(updated.toJson());
    })
    ..delete('/api/agents/<id>', (Request _, String id) {
      store.remove(id);
      return Response(204);
    })

    // ── Agent run (SSE) ───────────────────────────────────────────────────────
    ..post('/api/agents/<id>/run', (Request req, String id) async {
      final agent = store.get(id);
      if (agent == null) return _notFound('Agent $id not found');
      final body = jsonDecode(await req.readAsString()) as Map<String, dynamic>;
      final prompt = body['prompt'] as String? ?? '';
      final history = (body['history'] as List?)?.cast<Map<String, dynamic>>() ?? [];
      return _sseRun(() => loop.run(
          agent: agent, userPrompt: prompt, history: history,
          onStep: (_) {})); // onStep handled inside _sseRun
    })

    // ── Groups ────────────────────────────────────────────────────────────────
    ..get('/api/groups', (_) => _json({'groups': store.listGroups().map((g) => g.toJson()).toList()}))
    ..post('/api/groups', (Request req) async {
      final body = jsonDecode(await req.readAsString()) as Map<String, dynamic>;
      final group = store.putGroup(GroupConfig.fromJson(body));
      return _json(group.toJson(), 201);
    })
    ..get('/api/groups/<id>', (Request _, String id) {
      final g = store.getGroup(id);
      return g != null ? _json(g.toJson()) : _notFound('Group $id not found');
    })
    ..put('/api/groups/<id>', (Request req, String id) async {
      final g = store.getGroup(id);
      if (g == null) return _notFound('Group $id not found');
      final body = jsonDecode(await req.readAsString()) as Map<String, dynamic>;
      final updated = GroupConfig.fromJson({...g.toJson(), ...body, 'id': id});
      store.putGroup(updated);
      return _json(updated.toJson());
    })
    ..delete('/api/groups/<id>', (Request _, String id) {
      store.removeGroup(id);
      return Response(204);
    })

    // ── Group run (SSE) ───────────────────────────────────────────────────────
    ..post('/api/groups/<id>/run', (Request req, String id) async {
      final group = store.getGroup(id);
      if (group == null) return _notFound('Group $id not found');
      final body = jsonDecode(await req.readAsString()) as Map<String, dynamic>;
      final prompt = body['prompt'] as String? ?? '';
      return _sseRun(() => fRunner.run(
          group: group, agents: store.agentsMap, userPrompt: prompt,
          onStep: (_) {}));
    });

  final pipeline = const Pipeline()
      .addMiddleware(_cors())
      .addMiddleware(logRequests(logger: verbose ? null : (_, __) {}))
      .addHandler(router.call);

  final server = await io.serve(pipeline, host, port);
  stdout.writeln('Agent Server on http://${server.address.host}:$port');
  stdout.writeln('Endpoints:');
  stdout.writeln('  GET  /health');
  stdout.writeln('  GET  /api/agents  POST /api/agents');
  stdout.writeln('  POST /api/agents/:id/run   (SSE)');
  stdout.writeln('  GET  /api/groups  POST /api/groups');
  stdout.writeln('  POST /api/groups/:id/run   (SSE)');
}

// SSE streaming — runs [fn], emitting steps and a final done/error event
Response _sseRun(Future<String> Function() fn) {
  final controller = StreamController<List<int>>();

  void emit(String event, Map<String, dynamic> data) {
    final line = 'data: ${jsonEncode({'event': event, ...data})}\n\n';
    controller.add(utf8.encode(line));
  }

  // We need to intercept steps — rewire the formation/loop calls
  // For this lightweight version we run and emit final result
  fn().then((reply) {
    emit('done', {'reply': reply});
    controller.close();
  }).catchError((e) {
    emit('error', {'error': e.toString()});
    controller.close();
  });

  return Response.ok(
    controller.stream,
    headers: {
      'content-type': 'text/event-stream',
      'cache-control': 'no-cache',
      'x-accel-buffering': 'no',
      ..._corsMap,
    },
  );
}

Response _json(Object body, [int status = 200]) =>
    Response(status, body: jsonEncode(body),
        headers: {'content-type': 'application/json', ..._corsMap});

Response _notFound(String msg) =>
    Response.notFound(jsonEncode({'error': msg}),
        headers: {'content-type': 'application/json'});

const _corsMap = {
  'access-control-allow-origin': '*',
  'access-control-allow-methods': 'GET, POST, PUT, DELETE, OPTIONS',
  'access-control-allow-headers': 'content-type, authorization',
};

Middleware _cors() => (inner) => (req) async {
  if (req.method == 'OPTIONS') return Response.ok('', headers: _corsMap);
  final res = await inner(req);
  return res.change(headers: _corsMap);
};
