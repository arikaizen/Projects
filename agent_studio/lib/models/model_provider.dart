import 'package:uuid/uuid.dart';

const _pid = Uuid();

/// Every LLM service the studio can talk to, hosted and local.
/// All "OpenAI-compatible" types share one protocol implementation and differ
/// only in base URL and branding.
enum ProviderType {
  // Hosted
  anthropic,   // Claude
  openai,      // ChatGPT
  google,      // Gemini
  groq,
  mistral,
  deepseek,
  xai,         // Grok
  openrouter,  // meta-provider: one key, many models
  together,
  // Local
  ollama,
  lmstudio,
  llamacpp,    // llama.cpp ./llama-server
  vllm,
  // Anything else speaking the OpenAI protocol
  custom,
}

/// How the provider is authenticated.
enum AuthMethod {
  apiKey,       // classic API key
  googleOAuth,  // Sign in with Google → OAuth access token (Gemini)
  bearerToken,  // paste an OAuth/bearer token obtained elsewhere
  none,         // local servers
}

class ModelProvider {
  final String id;
  String name;
  ProviderType type;
  String baseUrl;
  String apiKey;            // API key or OAuth access token, per authMethod
  AuthMethod authMethod;
  String oauthClientId;     // Google sign-in: OAuth client id
  String oauthClientSecret; // Google sign-in: desktop-app client secret
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
    AuthMethod? authMethod,
    this.oauthClientId = '',
    this.oauthClientSecret = '',
    this.isConnected = false,
    this.isLoading = false,
    List<ModelInfo>? models,
    this.error,
  })  : id = id ?? _pid.v4(),
        authMethod = authMethod ?? defaultAuthMethod(type),
        models = models ?? [];

  bool get isLocal => isLocalType(type);

  static bool isLocalType(ProviderType t) =>
      t == ProviderType.ollama ||
      t == ProviderType.lmstudio ||
      t == ProviderType.llamacpp ||
      t == ProviderType.vllm;

  /// True when the provider speaks the OpenAI Chat Completions protocol.
  static bool isOpenAiCompatible(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:
      case ProviderType.google:
      case ProviderType.ollama:
        return false;
      default:
        return true;
    }
  }

  static AuthMethod defaultAuthMethod(ProviderType t) =>
      isLocalType(t) ? AuthMethod.none : AuthMethod.apiKey;

  static String defaultUrl(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'https://api.anthropic.com';
      case ProviderType.openai:     return 'https://api.openai.com';
      case ProviderType.google:     return 'https://generativelanguage.googleapis.com';
      case ProviderType.groq:       return 'https://api.groq.com/openai';
      case ProviderType.mistral:    return 'https://api.mistral.ai';
      case ProviderType.deepseek:   return 'https://api.deepseek.com';
      case ProviderType.xai:        return 'https://api.x.ai';
      case ProviderType.openrouter: return 'https://openrouter.ai/api';
      case ProviderType.together:   return 'https://api.together.xyz';
      case ProviderType.ollama:     return 'http://localhost:11434';
      case ProviderType.lmstudio:   return 'http://localhost:1234';
      case ProviderType.llamacpp:   return 'http://localhost:8080';
      case ProviderType.vllm:       return 'http://localhost:8000';
      case ProviderType.custom:     return 'http://localhost:8000';
    }
  }

  static String defaultName(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'Anthropic';
      case ProviderType.openai:     return 'OpenAI';
      case ProviderType.google:     return 'Google Gemini';
      case ProviderType.groq:       return 'Groq';
      case ProviderType.mistral:    return 'Mistral';
      case ProviderType.deepseek:   return 'DeepSeek';
      case ProviderType.xai:        return 'xAI Grok';
      case ProviderType.openrouter: return 'OpenRouter';
      case ProviderType.together:   return 'Together AI';
      case ProviderType.ollama:     return 'Local Ollama';
      case ProviderType.lmstudio:   return 'LM Studio';
      case ProviderType.llamacpp:   return 'llama.cpp';
      case ProviderType.vllm:       return 'vLLM';
      case ProviderType.custom:     return 'Custom Server';
    }
  }

  static String typeLabel(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'Claude';
      case ProviderType.openai:     return 'ChatGPT';
      case ProviderType.google:     return 'Gemini';
      case ProviderType.groq:       return 'Groq';
      case ProviderType.mistral:    return 'Mistral';
      case ProviderType.deepseek:   return 'DeepSeek';
      case ProviderType.xai:        return 'Grok';
      case ProviderType.openrouter: return 'OpenRouter';
      case ProviderType.together:   return 'Together';
      case ProviderType.ollama:     return 'Ollama';
      case ProviderType.lmstudio:   return 'LM Studio';
      case ProviderType.llamacpp:   return 'llama.cpp';
      case ProviderType.vllm:       return 'vLLM';
      case ProviderType.custom:     return 'Custom';
    }
  }

  /// Provider id string understood by the C++ engine's LLM factory
  /// (am_configure_llm / per-agent "llm" config).
  static String engineProviderName(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'anthropic';
      case ProviderType.openai:     return 'openai';
      case ProviderType.google:     return 'google';
      case ProviderType.groq:       return 'groq';
      case ProviderType.mistral:    return 'mistral';
      case ProviderType.deepseek:   return 'deepseek';
      case ProviderType.xai:        return 'xai';
      case ProviderType.openrouter: return 'openrouter';
      case ProviderType.together:   return 'together';
      case ProviderType.ollama:     return 'ollama';
      case ProviderType.lmstudio:   return 'lmstudio';
      case ProviderType.llamacpp:   return 'llamacpp';
      case ProviderType.vllm:       return 'vllm';
      case ProviderType.custom:     return 'custom';
    }
  }

  /// Engine-side config for this provider + model, matching the C++
  /// llm_factory JSON shape. Credentials stay between the GUI and the engine —
  /// they are never placed in prompts.
  Map<String, dynamic> toEngineLlmConfig(String modelId) => {
        'provider': engineProviderName(type),
        'model': modelId,
        'base_url': baseUrl,
        if (apiKey.isNotEmpty) 'api_key': apiKey,
        if (type == ProviderType.google)
          'auth_method':
              authMethod == AuthMethod.googleOAuth || authMethod == AuthMethod.bearerToken
                  ? 'bearer'
                  : 'api_key',
      };
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
