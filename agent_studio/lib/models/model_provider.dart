import 'package:uuid/uuid.dart';

const _pid = Uuid();

enum ProviderType { anthropic, ollama, openai, custom }

class ModelProvider {
  final String id;
  String name;
  ProviderType type;
  String baseUrl;
  String apiKey;
  bool isConnected;
  bool isLoading;
  List<ModelInfo> models;
  String? error;

  ModelProvider({
    String? id,
    required this.name,
    required this.type,
    required this.baseUrl,
    this.apiKey = '',
    this.isConnected = false,
    this.isLoading = false,
    List<ModelInfo>? models,
    this.error,
  })  : id = id ?? _pid.v4(),
        models = models ?? [];

  static String defaultUrl(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic: return 'https://api.anthropic.com';
      case ProviderType.ollama:    return 'http://localhost:11434';
      case ProviderType.openai:    return 'https://api.openai.com';
      case ProviderType.custom:    return 'http://localhost:8080';
    }
  }

  static String defaultName(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic: return 'Anthropic';
      case ProviderType.ollama:    return 'Local Ollama';
      case ProviderType.openai:    return 'OpenAI';
      case ProviderType.custom:    return 'Custom Server';
    }
  }

  static String typeLabel(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic: return 'Anthropic';
      case ProviderType.ollama:    return 'Ollama';
      case ProviderType.openai:    return 'OpenAI';
      case ProviderType.custom:    return 'Custom';
    }
  }
}

class ModelInfo {
  final String id;
  final String name;
  final String providerId;
  final String providerName;
  final ProviderType providerType;

  const ModelInfo({
    required this.id,
    required this.name,
    required this.providerId,
    required this.providerName,
    required this.providerType,
  });

  String get displayName => (name.isNotEmpty && name != id) ? name : id;
}
