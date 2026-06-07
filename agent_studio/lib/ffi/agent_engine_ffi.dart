/// High-level Dart wrapper around the raw FFI bindings.
library agent_engine_ffi;

import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:isolate';
import 'package:ffi/ffi.dart';

import 'agent_engine_bindings.dart';

class AgentEvent {
  final String type;
  final String? agentId;
  final String? userId;
  final DateTime timestamp;
  final Map<String, dynamic> raw;

  AgentEvent({
    required this.type,
    this.agentId,
    this.userId,
    required this.timestamp,
    required this.raw,
  });

  factory AgentEvent.fromJson(Map<String, dynamic> j) => AgentEvent(
    type:      j['type'] as String? ?? 'unknown',
    agentId:   j['agent_id'] as String?,
    userId:    j['user_id'] as String?,
    timestamp: DateTime.tryParse(j['timestamp'] as String? ?? '') ?? DateTime.now(),
    raw:       j,
  );
}

class AgentEngineFfi {
  final AgentEngineBindings _b;
  late final Pointer<AgentManager_> _mgr;
  bool _initialised = false;

  final _eventController = StreamController<AgentEvent>.broadcast();
  Stream<AgentEvent> get events => _eventController.stream;

  NativeCallable<AmEventCbNative>? _nativeCb;

  AgentEngineFfi(AgentEngineBindings bindings) : _b = bindings;

  void init(Map<String, dynamic> config) {
    if (_initialised) return;
    final configStr = jsonEncode(config);
    final mgr = using((arena) {
      return _b.amCreate(configStr.toNativeUtf8(allocator: arena));
    });
    if (mgr == nullptr) {
      final err = _lastError(nullptr);
      throw StateError('am_create failed: $err');
    }
    _mgr = mgr;
    _initialised = true;
    _subscribeEvents();
  }

  void dispose() {
    if (!_initialised) return;
    if (_nativeCb != null) {
      using((arena) {
        _b.amUnsubscribeEvents(_mgr, _nativeCb!.nativeFunction);
      });
      _nativeCb!.close();
      _nativeCb = null;
    }
    _b.amDestroy(_mgr);
    _initialised = false;
    _eventController.close();
  }

  int get apiVersion => _b.amApiVersion();

  String spawnAgent({
    String name          = 'agent',
    String userId        = 'default',
    int    maxIterations = 20,
    int    maxDepth      = 3,
    Map<String, dynamic> extra = const {},
  }) {
    _assertInit();
    final config = jsonEncode({
      'name': name,
      'user_id': userId,
      'max_iterations': maxIterations,
      'max_depth': maxDepth,
      'extra': extra,
    });

    return using((arena) {
      final outBuf = arena<Char>(256);
      final status = _b.amSpawnAgent(
        _mgr,
        config.toNativeUtf8(allocator: arena),
        outBuf,
        256,
      );
      _checkStatus(status, 'am_spawn_agent');
      return outBuf.cast<Utf8>().toDartString();
    });
  }

  void destroyAgent(String agentId) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amDestroyAgent(_mgr, agentId.toNativeUtf8(allocator: arena)),
        'am_destroy_agent',
      );
    });
  }

  void cancelAgent(String agentId) {
    _assertInit();
    using((arena) {
      _b.amCancelAgent(_mgr, agentId.toNativeUtf8(allocator: arena));
    });
  }

  Map<String, dynamic> getStatus(String agentId) {
    _assertInit();
    return jsonDecode(_bufOut((buf, sz) => using((arena) =>
      _b.amGetStatus(_mgr, agentId.toNativeUtf8(allocator: arena), buf, sz)
    )));
  }

  List<Map<String, dynamic>> listAgents({String userId = ''}) {
    _assertInit();
    final json = _bufOut((buf, sz) => using((arena) =>
      _b.amListAgents(_mgr, userId.toNativeUtf8(allocator: arena), buf, sz)
    ));
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  Future<Map<String, dynamic>> runAgent(
    String agentId,
    String task, {
    int timeoutMs = -1,
  }) async {
    _assertInit();
    final futurePtr = using((arena) =>
      _b.amRunAgent(
        _mgr,
        agentId.toNativeUtf8(allocator: arena),
        jsonEncode({'task': task}).toNativeUtf8(allocator: arena),
      )
    );
    if (futurePtr == nullptr) {
      throw StateError('am_run_agent failed: ${_lastError(_mgr)}');
    }

    final futureAddr = futurePtr.address;
    final mgrAddr    = _mgr.address;

    return Isolate.run(() {
      final b        = AgentEngineBindings(AgentEngineBindings.defaultLibPath());
      final futurePtrLocal = Pointer<AgentFuture_>.fromAddress(futureAddr);
      const bufSize  = 65536;

      return using((arena) {
        final buf    = arena<Uint8>(bufSize);
        final outBuf = buf.cast<Utf8>();
        final status = b.amFutureWait(futurePtrLocal, timeoutMs, outBuf, bufSize);
        b.amFutureFree(futurePtrLocal);

        if (status == amErrorTimeout) throw TimeoutException('Agent timed out');
        if (status != amOk) {
          final mgrLocal = Pointer<AgentManager_>.fromAddress(mgrAddr);
          throw StateError('am_future_wait error $status: '
              '${b.amLastError(mgrLocal).toDartString()}');
        }

        final raw = outBuf.toDartString();
        try {
          return jsonDecode(raw) as Map<String, dynamic>;
        } catch (_) {
          return <String, dynamic>{'output': raw};
        }
      });
    });
  }

  void pipe(String fromId, String toId, {String template = '{prev_output}'}) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amPipe(
          _mgr,
          fromId.toNativeUtf8(allocator: arena),
          toId.toNativeUtf8(allocator: arena),
          template.toNativeUtf8(allocator: arena),
        ),
        'am_pipe',
      );
    });
  }

  void sendMessage(String from, String to, Map<String, dynamic> message) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amSendMessage(
          _mgr,
          from.toNativeUtf8(allocator: arena),
          to.toNativeUtf8(allocator: arena),
          jsonEncode(message).toNativeUtf8(allocator: arena),
        ),
        'am_send_message',
      );
    });
  }

  void broadcast(String from, Map<String, dynamic> message) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amBroadcast(
          _mgr,
          from.toNativeUtf8(allocator: arena),
          jsonEncode(message).toNativeUtf8(allocator: arena),
        ),
        'am_broadcast',
      );
    });
  }

  List<Map<String, dynamic>> drainInbox(String agentId) {
    _assertInit();
    final json = _bufOut((buf, sz) => using((arena) =>
      _b.amDrainInbox(_mgr, agentId.toNativeUtf8(allocator: arena), buf, sz)
    ));
    return (jsonDecode(json) as List).cast<Map<String, dynamic>>();
  }

  void blackboardWrite(String key, dynamic value) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amBlackboardWrite(
          _mgr,
          key.toNativeUtf8(allocator: arena),
          jsonEncode(value).toNativeUtf8(allocator: arena),
        ),
        'am_blackboard_write',
      );
    });
  }

  dynamic blackboardRead(String key) {
    _assertInit();
    final json = _bufOut((buf, sz) => using((arena) =>
      _b.amBlackboardRead(_mgr, key.toNativeUtf8(allocator: arena), buf, sz)
    ));
    return jsonDecode(json);
  }

  List<String> blackboardKeys({String prefix = ''}) {
    _assertInit();
    final json = _bufOut((buf, sz) => using((arena) =>
      _b.amBlackboardKeys(_mgr, prefix.toNativeUtf8(allocator: arena), buf, sz)
    ));
    return (jsonDecode(json) as List).cast<String>();
  }

  Future<Map<String, dynamic>> researchFromAngles(
    List<String> angles,
    String topic,
  ) async {
    _assertInit();
    final anglesJson = jsonEncode(angles);
    return Isolate.run(() {
      final b        = AgentEngineBindings(AgentEngineBindings.defaultLibPath());
      final mgrLocal = Pointer<AgentManager_>.fromAddress(_mgr.address);
      const bufSize  = 65536;
      return using((arena) {
        final buf = arena<Uint8>(bufSize).cast<Utf8>();
        final st  = b.amResearchFromAngles(
          mgrLocal,
          anglesJson.toNativeUtf8(allocator: arena),
          topic.toNativeUtf8(allocator: arena),
          buf,
          bufSize,
        );
        if (st != amOk) throw StateError('am_research_from_angles error $st');
        return jsonDecode(buf.toDartString()) as Map<String, dynamic>;
      });
    });
  }

  void injectWork(String agentId, Map<String, dynamic> workItem) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amInjectWork(
          _mgr,
          agentId.toNativeUtf8(allocator: arena),
          jsonEncode(workItem).toNativeUtf8(allocator: arena),
        ),
        'am_inject_work',
      );
    });
  }

  void connectMcp({required String name, required String url, Map<String, dynamic>? extra}) {
    _assertInit();
    final config = jsonEncode({'name': name, 'url': url, 'extra': extra ?? {}});
    using((arena) {
      _checkStatus(
        _b.amConnectMcp(_mgr, config.toNativeUtf8(allocator: arena)),
        'am_connect_mcp',
      );
    });
  }

  void disconnectMcp(String serverName) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amDisconnectMcp(_mgr, serverName.toNativeUtf8(allocator: arena)),
        'am_disconnect_mcp',
      );
    });
  }

  List<String> listMcpServers() {
    _assertInit();
    final json = _bufOut((buf, sz) =>
      _b.amListMcpServers(_mgr, buf, sz)
    );
    return (jsonDecode(json) as List).cast<String>();
  }

  void reloadPrompts() {
    _assertInit();
    _checkStatus(_b.amReloadPrompts(_mgr), 'am_reload_prompts');
  }

  void setPromptsDir(String path) {
    _assertInit();
    using((arena) {
      _checkStatus(
        _b.amSetPromptsDir(_mgr, path.toNativeUtf8(allocator: arena)),
        'am_set_prompts_dir',
      );
    });
  }

  void setUserQuota(String userId, {
    int? maxConcurrentAgents,
    int? maxLlmInflight,
    int? maxToolInflight,
  }) {
    _assertInit();
    final quota = <String, int>{
      if (maxConcurrentAgents != null) 'max_concurrent_agents': maxConcurrentAgents,
      if (maxLlmInflight      != null) 'max_llm_inflight': maxLlmInflight,
      if (maxToolInflight     != null) 'max_tool_inflight': maxToolInflight,
    };
    using((arena) {
      _checkStatus(
        _b.amSetUserQuota(
          _mgr,
          userId.toNativeUtf8(allocator: arena),
          jsonEncode(quota).toNativeUtf8(allocator: arena),
        ),
        'am_set_user_quota',
      );
    });
  }

  void _subscribeEvents() {
    _nativeCb = NativeCallable<AmEventCbNative>.listener(_onNativeEvent);
    using((arena) {
      _b.amSubscribeEvents(_mgr, _nativeCb!.nativeFunction, nullptr);
    });
  }

  void _onNativeEvent(Pointer<Utf8> eventJson, Pointer<Void> _) {
    try {
      final raw = jsonDecode(eventJson.toDartString()) as Map<String, dynamic>;
      _eventController.add(AgentEvent.fromJson(raw));
    } catch (_) {}
  }

  String _bufOut(
    int Function(Pointer<Utf8> buf, int sz) fn, {
    int initSize = 4096,
  }) {
    var sz = initSize;
    while (true) {
      final result = using((arena) {
        final buf    = arena<Uint8>(sz).cast<Utf8>();
        final status = fn(buf, sz);
        if (status == amErrorBufferTooSmall) return null;
        _checkStatus(status, 'bufOut');
        return buf.toDartString();
      });
      if (result != null) return result;
      sz *= 2;
      if (sz > 16 * 1024 * 1024) throw StateError('Buffer exceeded 16 MB');
    }
  }

  void _checkStatus(int status, String ctx) {
    if (status != amOk) {
      final msg = _initialised ? _lastError(_mgr) : 'not initialised';
      throw StateError('$ctx failed (status $status): $msg');
    }
  }

  String _lastError(Pointer<AgentManager_> mgr) {
    final ptr = _b.amLastError(mgr);
    return ptr == nullptr ? 'unknown error' : ptr.toDartString();
  }

  void _assertInit() {
    if (!_initialised) throw StateError('AgentEngineFfi not initialised — call init() first');
  }
}
