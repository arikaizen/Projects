/// Raw Dart FFI bindings for agent_engine/c_api.h
library agent_engine_bindings;

import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';

final class AgentManager_ extends Opaque {}
final class AgentFuture_  extends Opaque {}

const amOk                    = 0;
const amErrorInvalidArg       = 1;
const amErrorNotFound         = 2;
const amErrorInternal         = 3;
const amErrorTimeout          = 4;
const amErrorBufferTooSmall   = 5;
const amErrorNotInitialised   = 6;
const amErrorDeprecated       = 7;
const amErrorCancelled        = 8;
const amErrorPromptNotFound   = 9;
const amErrorPromptSubst      = 10;
const amErrorQuotaExceeded    = 11;
const amErrorDependencyCycle  = 12;

typedef _AmCreateNative = Pointer<AgentManager_> Function(Pointer<Utf8> configJson);
typedef _AmCreate       = Pointer<AgentManager_> Function(Pointer<Utf8> configJson);

typedef _AmDestroyNative = Void Function(Pointer<AgentManager_> mgr);
typedef _AmDestroy       = void Function(Pointer<AgentManager_> mgr);

typedef _AmLastErrorNative = Pointer<Utf8> Function(Pointer<AgentManager_> mgr);
typedef _AmLastError       = Pointer<Utf8> Function(Pointer<AgentManager_> mgr);

typedef _AmApiVersionNative = Int32 Function();
typedef _AmApiVersion       = int   Function();

typedef _AmSpawnAgentNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> configJson,
    Pointer<Utf8> outIdBuf,
    Size outSize,
);
typedef _AmSpawnAgent = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> configJson,
    Pointer<Utf8> outIdBuf,
    int outSize,
);

typedef _AmDestroyAgentNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> agentId);
typedef _AmDestroyAgent = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> agentId);

typedef _AmListAgentsNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> userId,
    Pointer<Utf8> outJson,
    Size outSize,
);
typedef _AmListAgents = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> userId,
    Pointer<Utf8> outJson,
    int outSize,
);

typedef _AmGetStatusNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> outJson,
    Size outSize,
);
typedef _AmGetStatus = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> outJson,
    int outSize,
);

typedef _AmCancelAgentNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> agentId);
typedef _AmCancelAgent = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> agentId);

typedef _AmRunAgentNative = Pointer<AgentFuture_> Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> taskJson,
);
typedef _AmRunAgent = Pointer<AgentFuture_> Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> taskJson,
);

typedef _AmFutureWaitNative = Int32 Function(
    Pointer<AgentFuture_> future,
    Int32 timeoutMs,
    Pointer<Utf8> outResultJson,
    Size outSize,
);
typedef _AmFutureWait = int Function(
    Pointer<AgentFuture_> future,
    int timeoutMs,
    Pointer<Utf8> outResultJson,
    int outSize,
);

typedef _AmFutureFreeNative = Void Function(Pointer<AgentFuture_> future);
typedef _AmFutureFree       = void Function(Pointer<AgentFuture_> future);

typedef _AmPipeNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> fromId,
    Pointer<Utf8> toId,
    Pointer<Utf8> templateString,
);
typedef _AmPipe = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> fromId,
    Pointer<Utf8> toId,
    Pointer<Utf8> templateString,
);

typedef _AmSendMessageNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> from, Pointer<Utf8> to, Pointer<Utf8> msgJson);
typedef _AmSendMessage = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> from, Pointer<Utf8> to, Pointer<Utf8> msgJson);

typedef _AmBroadcastNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> from, Pointer<Utf8> msgJson);
typedef _AmBroadcast = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> from, Pointer<Utf8> msgJson);

typedef _AmDrainInboxNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> outJson,
    Size outSize,
);
typedef _AmDrainInbox = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> outJson,
    int outSize,
);

typedef _AmBlackboardWriteNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> key, Pointer<Utf8> valueJson);
typedef _AmBlackboardWrite = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> key, Pointer<Utf8> valueJson);

typedef _AmBlackboardReadNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> key,
    Pointer<Utf8> outValueJson,
    Size outSize,
);
typedef _AmBlackboardRead = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> key,
    Pointer<Utf8> outValueJson,
    int outSize,
);

typedef _AmBlackboardKeysNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> prefix,
    Pointer<Utf8> outJson,
    Size outSize,
);
typedef _AmBlackboardKeys = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> prefix,
    Pointer<Utf8> outJson,
    int outSize,
);

typedef _AmResearchFromAnglesNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> anglesJsonArray,
    Pointer<Utf8> topic,
    Pointer<Utf8> outResultJson,
    Size outSize,
);
typedef _AmResearchFromAngles = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> anglesJsonArray,
    Pointer<Utf8> topic,
    Pointer<Utf8> outResultJson,
    int outSize,
);

typedef _AmInjectWorkNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> workItemJson,
);
typedef _AmInjectWork = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> agentId,
    Pointer<Utf8> workItemJson,
);

typedef AmEventCbNative = Void Function(Pointer<Utf8> eventJson, Pointer<Void> userData);
typedef AmEventCb       = void Function(Pointer<Utf8> eventJson, Pointer<Void> userData);

typedef _AmSubscribeEventsNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<NativeFunction<AmEventCbNative>> cb,
    Pointer<Void> userData,
);
typedef _AmSubscribeEvents = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<NativeFunction<AmEventCbNative>> cb,
    Pointer<Void> userData,
);

typedef _AmUnsubscribeEventsNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<NativeFunction<AmEventCbNative>> cb,
);
typedef _AmUnsubscribeEvents = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<NativeFunction<AmEventCbNative>> cb,
);

typedef _AmReloadPromptsNative = Int32 Function(Pointer<AgentManager_> mgr);
typedef _AmReloadPrompts       = int   Function(Pointer<AgentManager_> mgr);

typedef _AmSetPromptsDirNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> dirPath);
typedef _AmSetPromptsDir = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> dirPath);

typedef _AmSetUserQuotaNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> userId,
    Pointer<Utf8> quotaJson,
);
typedef _AmSetUserQuota = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> userId,
    Pointer<Utf8> quotaJson,
);

typedef _AmConnectMcpNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> serverConfigJson);
typedef _AmConnectMcp = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> serverConfigJson);

typedef _AmDisconnectMcpNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> serverName);
typedef _AmDisconnectMcp = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> serverName);

typedef _AmListMcpServersNative = Int32 Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> outJson, Size outSize);
typedef _AmListMcpServers = int Function(
    Pointer<AgentManager_> mgr, Pointer<Utf8> outJson, int outSize);

typedef _AmFanOutNative = Int32 Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> configsJsonArray,
    Pointer<Utf8> sharedTask,
    Pointer<Pointer<Pointer<AgentFuture_>>> outFutures,
    Pointer<Size> outCount,
);
typedef _AmFanOut = int Function(
    Pointer<AgentManager_> mgr,
    Pointer<Utf8> configsJsonArray,
    Pointer<Utf8> sharedTask,
    Pointer<Pointer<Pointer<AgentFuture_>>> outFutures,
    Pointer<Size> outCount,
);

typedef _AmFanOutFreeArrayNative = Void Function(
    Pointer<Pointer<AgentFuture_>> arr);
typedef _AmFanOutFreeArray = void Function(
    Pointer<Pointer<AgentFuture_>> arr);

class AgentEngineBindings {
  late final DynamicLibrary _lib;

  late final _AmCreate            amCreate;
  late final _AmDestroy           amDestroy;
  late final _AmLastError         amLastError;
  late final _AmApiVersion        amApiVersion;
  late final _AmSpawnAgent        amSpawnAgent;
  late final _AmDestroyAgent      amDestroyAgent;
  late final _AmListAgents        amListAgents;
  late final _AmGetStatus         amGetStatus;
  late final _AmCancelAgent       amCancelAgent;
  late final _AmRunAgent          amRunAgent;
  late final _AmFutureWait        amFutureWait;
  late final _AmFutureFree        amFutureFree;
  late final _AmPipe              amPipe;
  late final _AmSendMessage       amSendMessage;
  late final _AmBroadcast         amBroadcast;
  late final _AmDrainInbox        amDrainInbox;
  late final _AmBlackboardWrite   amBlackboardWrite;
  late final _AmBlackboardRead    amBlackboardRead;
  late final _AmBlackboardKeys    amBlackboardKeys;
  late final _AmResearchFromAngles amResearchFromAngles;
  late final _AmInjectWork        amInjectWork;
  late final _AmSubscribeEvents   amSubscribeEvents;
  late final _AmUnsubscribeEvents amUnsubscribeEvents;
  late final _AmReloadPrompts     amReloadPrompts;
  late final _AmSetPromptsDir     amSetPromptsDir;
  late final _AmSetUserQuota      amSetUserQuota;
  late final _AmConnectMcp        amConnectMcp;
  late final _AmDisconnectMcp     amDisconnectMcp;
  late final _AmListMcpServers    amListMcpServers;
  late final _AmFanOut            amFanOut;
  late final _AmFanOutFreeArray   amFanOutFreeArray;

  AgentEngineBindings(String libraryPath) {
    _lib = DynamicLibrary.open(libraryPath);
    _bind();
  }

  void _bind() {
    amCreate         = _lib.lookupFunction<_AmCreateNative,         _AmCreate        >('am_create');
    amDestroy        = _lib.lookupFunction<_AmDestroyNative,        _AmDestroy       >('am_destroy');
    amLastError      = _lib.lookupFunction<_AmLastErrorNative,      _AmLastError     >('am_last_error');
    amApiVersion     = _lib.lookupFunction<_AmApiVersionNative,     _AmApiVersion    >('am_api_version');
    amSpawnAgent     = _lib.lookupFunction<_AmSpawnAgentNative,     _AmSpawnAgent    >('am_spawn_agent');
    amDestroyAgent   = _lib.lookupFunction<_AmDestroyAgentNative,   _AmDestroyAgent  >('am_destroy_agent');
    amListAgents     = _lib.lookupFunction<_AmListAgentsNative,     _AmListAgents    >('am_list_agents');
    amGetStatus      = _lib.lookupFunction<_AmGetStatusNative,      _AmGetStatus     >('am_get_status');
    amCancelAgent    = _lib.lookupFunction<_AmCancelAgentNative,    _AmCancelAgent   >('am_cancel_agent');
    amRunAgent       = _lib.lookupFunction<_AmRunAgentNative,       _AmRunAgent      >('am_run_agent');
    amFutureWait     = _lib.lookupFunction<_AmFutureWaitNative,     _AmFutureWait    >('am_future_wait');
    amFutureFree     = _lib.lookupFunction<_AmFutureFreeNative,     _AmFutureFree    >('am_future_free');
    amPipe           = _lib.lookupFunction<_AmPipeNative,           _AmPipe          >('am_pipe');
    amSendMessage    = _lib.lookupFunction<_AmSendMessageNative,    _AmSendMessage   >('am_send_message');
    amBroadcast      = _lib.lookupFunction<_AmBroadcastNative,      _AmBroadcast     >('am_broadcast');
    amDrainInbox     = _lib.lookupFunction<_AmDrainInboxNative,     _AmDrainInbox    >('am_drain_inbox');
    amBlackboardWrite = _lib.lookupFunction<_AmBlackboardWriteNative, _AmBlackboardWrite>('am_blackboard_write');
    amBlackboardRead  = _lib.lookupFunction<_AmBlackboardReadNative,  _AmBlackboardRead >('am_blackboard_read');
    amBlackboardKeys  = _lib.lookupFunction<_AmBlackboardKeysNative,  _AmBlackboardKeys >('am_blackboard_keys');
    amResearchFromAngles = _lib.lookupFunction<_AmResearchFromAnglesNative, _AmResearchFromAngles>('am_research_from_angles');
    amInjectWork     = _lib.lookupFunction<_AmInjectWorkNative,     _AmInjectWork    >('am_inject_work');
    amSubscribeEvents   = _lib.lookupFunction<_AmSubscribeEventsNative,   _AmSubscribeEvents  >('am_subscribe_events');
    amUnsubscribeEvents = _lib.lookupFunction<_AmUnsubscribeEventsNative, _AmUnsubscribeEvents>('am_unsubscribe_events');
    amReloadPrompts  = _lib.lookupFunction<_AmReloadPromptsNative,  _AmReloadPrompts >('am_reload_prompts');
    amSetPromptsDir  = _lib.lookupFunction<_AmSetPromptsDirNative,  _AmSetPromptsDir >('am_set_prompts_dir');
    amSetUserQuota   = _lib.lookupFunction<_AmSetUserQuotaNative,   _AmSetUserQuota  >('am_set_user_quota');
    amConnectMcp     = _lib.lookupFunction<_AmConnectMcpNative,     _AmConnectMcp    >('am_connect_mcp');
    amDisconnectMcp  = _lib.lookupFunction<_AmDisconnectMcpNative,  _AmDisconnectMcp >('am_disconnect_mcp');
    amListMcpServers = _lib.lookupFunction<_AmListMcpServersNative, _AmListMcpServers>('am_list_mcp_servers');
    amFanOut         = _lib.lookupFunction<_AmFanOutNative,         _AmFanOut        >('am_fan_out');
    amFanOutFreeArray = _lib.lookupFunction<_AmFanOutFreeArrayNative, _AmFanOutFreeArray>('am_fan_out_free_array');
  }

  static String defaultLibPath() {
    if (Platform.isLinux)   return 'libagent_engine.so';
    if (Platform.isMacOS)   return 'libagent_engine.dylib';
    if (Platform.isWindows) return 'agent_engine.dll';
    throw UnsupportedError('Unsupported platform for FFI: ${Platform.operatingSystem}');
  }
}
