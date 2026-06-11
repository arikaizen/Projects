import 'package:flutter/material.dart';
import 'package:uuid/uuid.dart';

const _pid = Uuid();

enum ProviderType {
  anthropic,   // Claude — api.anthropic.com
  openai,      // GPT-4o, o3, o1 — api.openai.com
  gemini,      // Gemini — generativelanguage.googleapis.com (Google OAuth or API key)
  ollama,      // Local Ollama — localhost:11434
  mistral,     // Mistral AI — api.mistral.ai
  groq,        // Groq (fast inference) — api.groq.com
  together,    // Together AI — api.together.xyz
  cohere,      // Cohere — api.cohere.com
  xai,         // xAI Grok — api.x.ai
  perplexity,  // Perplexity — api.perplexity.ai
  custom,      // Any OpenAI-compatible endpoint
}

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
  bool googleAuth; // true = authenticated via Google OAuth (Gemini)

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
    this.googleAuth = false,
  })  : id = id ?? _pid.v4(),
        models = models ?? [];

  static String defaultUrl(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'https://api.anthropic.com';
      case ProviderType.openai:     return 'https://api.openai.com';
      case ProviderType.gemini:     return 'https://generativelanguage.googleapis.com';
      case ProviderType.ollama:     return 'http://localhost:11434';
      case ProviderType.mistral:    return 'https://api.mistral.ai';
      case ProviderType.groq:       return 'https://api.groq.com/openai';
      case ProviderType.together:   return 'https://api.together.xyz';
      case ProviderType.cohere:     return 'https://api.cohere.com';
      case ProviderType.xai:        return 'https://api.x.ai';
      case ProviderType.perplexity: return 'https://api.perplexity.ai';
      case ProviderType.custom:     return 'http://localhost:8080';
    }
  }

  static String defaultName(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'Anthropic (Claude)';
      case ProviderType.openai:     return 'OpenAI (ChatGPT)';
      case ProviderType.gemini:     return 'Google Gemini';
      case ProviderType.ollama:     return 'Local Ollama';
      case ProviderType.mistral:    return 'Mistral AI';
      case ProviderType.groq:       return 'Groq';
      case ProviderType.together:   return 'Together AI';
      case ProviderType.cohere:     return 'Cohere';
      case ProviderType.xai:        return 'xAI (Grok)';
      case ProviderType.perplexity: return 'Perplexity';
      case ProviderType.custom:     return 'Custom Endpoint';
    }
  }

  static String typeLabel(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'Anthropic';
      case ProviderType.openai:     return 'OpenAI';
      case ProviderType.gemini:     return 'Gemini';
      case ProviderType.ollama:     return 'Ollama';
      case ProviderType.mistral:    return 'Mistral';
      case ProviderType.groq:       return 'Groq';
      case ProviderType.together:   return 'Together';
      case ProviderType.cohere:     return 'Cohere';
      case ProviderType.xai:        return 'xAI';
      case ProviderType.perplexity: return 'Perplexity';
      case ProviderType.custom:     return 'Custom';
    }
  }

  static IconData typeIcon(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return Icons.auto_awesome;
      case ProviderType.openai:     return Icons.chat_bubble_outline;
      case ProviderType.gemini:     return Icons.star_border_purple500_outlined;
      case ProviderType.ollama:     return Icons.computer;
      case ProviderType.mistral:    return Icons.wind_power;
      case ProviderType.groq:       return Icons.bolt;
      case ProviderType.together:   return Icons.group_work_outlined;
      case ProviderType.cohere:     return Icons.waves;
      case ProviderType.xai:        return Icons.close; // X logo approximation
      case ProviderType.perplexity: return Icons.manage_search;
      case ProviderType.custom:     return Icons.settings_ethernet;
    }
  }

  static Color typeColor(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return const Color(0xFFD97757);
      case ProviderType.openai:     return const Color(0xFF10A37F);
      case ProviderType.gemini:     return const Color(0xFF4285F4);
      case ProviderType.ollama:     return const Color(0xFF7C3AED);
      case ProviderType.mistral:    return const Color(0xFFFF7000);
      case ProviderType.groq:       return const Color(0xFFF55036);
      case ProviderType.together:   return const Color(0xFF0066FF);
      case ProviderType.cohere:     return const Color(0xFF39594D);
      case ProviderType.xai:        return const Color(0xFF000000);
      case ProviderType.perplexity: return const Color(0xFF20B2AA);
      case ProviderType.custom:     return const Color(0xFF6B7280);
    }
  }

  static bool needsApiKey(ProviderType t) => t != ProviderType.ollama;
  static bool supportsGoogleAuth(ProviderType t) => t == ProviderType.gemini;

  /// API key setup URL for each provider
  static String apiKeyUrl(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return 'https://console.anthropic.com/settings/keys';
      case ProviderType.openai:     return 'https://platform.openai.com/api-keys';
      case ProviderType.gemini:     return 'https://aistudio.google.com/app/apikey';
      case ProviderType.mistral:    return 'https://console.mistral.ai/api-keys/';
      case ProviderType.groq:       return 'https://console.groq.com/keys';
      case ProviderType.together:   return 'https://api.together.xyz/settings/api-keys';
      case ProviderType.cohere:     return 'https://dashboard.cohere.com/api-keys';
      case ProviderType.xai:        return 'https://console.x.ai/';
      case ProviderType.perplexity: return 'https://www.perplexity.ai/settings/api';
      default:                      return '';
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
