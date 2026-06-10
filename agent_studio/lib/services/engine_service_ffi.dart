// FFI-backed engine service for desktop (Linux/macOS/Windows).
import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:math';

import 'package:ffi/ffi.dart';

import '../ffi/agent_engine_bindings.dart';
import '../ffi/agent_engine_ffi.dart';
import '../models/task_model.dart';
import 'agent_api_service.dart';

AgentBackend createBackend() => _FfiOrHttpBackend();

class _FfiOrHttpBackend implements AgentBackend {
  AgentBackend? _delegate;
  bool _ffiActive = false;

  @override
  bool   get isConnected     => _delegate?.isConnected ?? false;
  @override
  String get connectionLabel => _delegate?.connectionLabel ?? 'Not connected';
  @override
  Stream<Map<String, dynamic>>? get engineEvents => _delegate?.engineEvents;

  @override
  Future<bool> connect(String target) async {
    if (target.startsWith('http://') || target.startsWith('https://')) {
      _delegate  = HttpMockBackend();
      _ffiActive = false;
      return _delegate!.connect(target);
    }

    try {
      final ffiBackend = FfiBackend(target);
      await ffiBackend.init();
      _delegate  = ffiBackend;
      _ffiActive = true;
      return true;
    } catch (e) {
      _delegate  = HttpMockBackend();
      _ffiActive = false;
      return false;
    }
  }

  @override
  void disconnect() => _delegate?.disconnect();

  @override
  Future<String> runTask(String p, String t, TaskTarget tt) =>
      _delegate?.runTask(p, t, tt) ?? Future.value('Not connected');

  @override
  Future<String> spawnEngineAgent(Map<String, dynamic> c) =>
      _delegate?.spawnEngineAgent(c) ?? Future.value('');

  @override
  Future<Map<String, dynamic>> getEngineStatus(String id) =>
      _delegate?.getEngineStatus(id) ?? Future.value({'status': 'unknown'});

  @override
  Future<void> cancelEngineAgent(String id) =>
      _delegate?.cancelEngineAgent(id) ?? Future.value();

  @override
  Future<List<Map<String, dynamic>>> listEngineAgents() =>
      _delegate?.listEngineAgents() ?? Future.value([]);

  @override
  Future<void> configureLlm(Map<String, dynamic> config) =>
      _delegate?.configureLlm(config) ?? Future.value();

  @override
  Future<void> connectMcp({
    required String name,
    required String url,
    String bearerToken = '',
    String transport   = 'http',
  }) => _delegate?.connectMcp(
        name: name, url: url, bearerToken: bearerToken, transport: transport,
      ) ?? Future.value();

  @override
  Future<void> disconnectMcp(String serverName) =>
      _delegate?.disconnectMcp(serverName) ?? Future.value();

  @override
  Future<Map<String, dynamic>> listMcpServers() =>
      _delegate?.listMcpServers() ?? Future.value({});
}

class FfiBackend implements AgentBackend {
  final String _libPath;
  late final AgentEngineFfi _engine;

  final _idMap    = <String, String>{};
  final _idMapRev = <String, String>{};

  final _eventCtrl = StreamController<Map<String, dynamic>>.broadcast();

  bool _connected = false;

  FfiBackend(this._libPath);

  Future<void> init({Map<String, dynamic>? config}) async {
    final bindings = AgentEngineBindings(_libPath);
    _engine = AgentEngineFfi(bindings);
    _engine.init(config ?? {
      'thread_pool_size': 8,
      'max_agent_depth': 5,
    });

    _engine.events.listen((evt) {
      _eventCtrl.add(evt.raw);
    });

    _connected = true;
  }

  @override
  bool   get isConnected     => _connected;
  @override
  String get connectionLabel => 'FFI: ${_libPath.split('/').last}';
  @override
  Stream<Map<String, dynamic>> get engineEvents => _eventCtrl.stream;

  @override
  Future<bool> connect(String target) async => _connected;

  @override
  void disconnect() {
    _engine.dispose();
    _connected = false;
    _eventCtrl.close();
  }

  @override
  Future<String> runTask(
    String prompt,
    String dartTargetId,
    TaskTarget target,
  ) async {
    final engineId = _engineIdFor(dartTargetId) ?? _spawnForId(dartTargetId);
    final result = await _engine.runAgent(engineId, prompt);
    final output = result['output'] as String?
        ?? result['result'] as String?
        ?? jsonEncode(result);
    return output;
  }

  @override
  Future<String> spawnEngineAgent(Map<String, dynamic> config) async {
    return _engine.spawnAgent(
      name:          config['name'] as String? ?? 'agent',
      userId:        config['user_id'] as String? ?? 'default',
      maxIterations: config['max_iterations'] as int? ?? 20,
      llm:           config['llm'] as Map<String, dynamic>?,
    );
  }

  @override
  Future<void> configureLlm(Map<String, dynamic> config) async {
    _engine.configureLlm(config);
  }

  @override
  Future<Map<String, dynamic>> getEngineStatus(String dartId) async {
    final eid = _engineIdFor(dartId);
    if (eid == null) return {'status': 'not_found'};
    return _engine.getStatus(eid);
  }

  @override
  Future<void> cancelEngineAgent(String dartId) async {
    final eid = _engineIdFor(dartId);
    if (eid != null) _engine.cancelAgent(eid);
  }

  @override
  Future<List<Map<String, dynamic>>> listEngineAgents() async {
    return _engine.listAgents();
  }

  @override
  Future<void> connectMcp({
    required String name,
    required String url,
    String bearerToken = '',
    String transport   = 'http',
  }) async {
    _engine.connectMcp(
      name:        name,
      url:         url,
      bearerToken: bearerToken,
      transport:   transport,
    );
  }

  @override
  Future<void> disconnectMcp(String serverName) async {
    _engine.disconnectMcp(serverName);
  }

  @override
  Future<Map<String, dynamic>> listMcpServers() async {
    return _engine.listMcpServers();
  }

  String? _engineIdFor(String dartId) => _idMap[dartId];

  String _spawnForId(String dartId) {
    final eid = _engine.spawnAgent(name: 'agent-$dartId');
    _idMap[dartId]    = eid;
    _idMapRev[eid]    = dartId;
    return eid;
  }

  void registerMapping(String dartId, String engineId) {
    _idMap[dartId]       = engineId;
    _idMapRev[engineId]  = dartId;
  }
}
