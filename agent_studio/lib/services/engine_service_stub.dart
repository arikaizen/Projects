// Stub used on web — always returns the HTTP/mock backend.
import 'agent_api_service.dart';

AgentBackend createBackend() => HttpMockBackend();
