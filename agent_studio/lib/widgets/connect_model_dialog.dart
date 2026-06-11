import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:url_launcher/url_launcher.dart';
import '../models/model_provider.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';

/// A friendly "connect to your AI" gallery. Each provider is a card:
///  • Gemini  → one-click Google sign-in (no key)
///  • Ollama  → connect to a local server (no key)
///  • others  → paste an API key (with a "Get a key" link)
/// On success the provider is connected and persisted, so it's remembered.
class ConnectModelDialog extends StatelessWidget {
  const ConnectModelDialog({super.key});

  static Future<void> show(BuildContext context) =>
      showDialog(context: context, builder: (_) => const ConnectModelDialog());

  @override
  Widget build(BuildContext context) {
    final prov = context.watch<AgentProvider>();
    return Dialog(
      backgroundColor: AppColors.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: AppColors.border),
      ),
      child: SizedBox(
        width: 600,
        height: 620,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(24, 22, 16, 8),
              child: Row(
                children: [
                  const Icon(Icons.hub_outlined, color: AppColors.primary),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Connect a Model',
                            style: Theme.of(context).textTheme.titleLarge),
                        const Text(
                          'Sign in or paste a key once — your agents can then use it.',
                          style: TextStyle(color: AppColors.textMuted, fontSize: 12),
                        ),
                      ],
                    ),
                  ),
                  IconButton(
                    icon: const Icon(Icons.close, size: 18, color: AppColors.textMuted),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
            ),
            const Divider(height: 1, color: AppColors.border),
            Expanded(
              child: ListView(
                padding: const EdgeInsets.all(16),
                children: ProviderType.values
                    .map((t) => _ProviderRow(type: t, prov: prov))
                    .toList(),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _ProviderRow extends StatelessWidget {
  final ProviderType type;
  final AgentProvider prov;
  const _ProviderRow({required this.type, required this.prov});

  ModelProvider? get _existing {
    final matches = prov.modelProviders.where((p) => p.type == type);
    return matches.isEmpty ? null : matches.first;
  }

  bool get _connected => _existing?.isConnected == true && (_existing?.models.isNotEmpty ?? false);

  @override
  Widget build(BuildContext context) {
    final color = ModelProvider.typeColor(type);
    return Container(
      margin: const EdgeInsets.only(bottom: 10),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: _connected ? color.withOpacity(0.5) : AppColors.border,
        ),
      ),
      child: Row(
        children: [
          Container(
            width: 38,
            height: 38,
            decoration: BoxDecoration(
              color: color.withOpacity(0.15),
              borderRadius: BorderRadius.circular(10),
            ),
            child: Icon(ModelProvider.typeIcon(type), size: 20, color: color),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(ModelProvider.defaultName(type),
                    style: const TextStyle(
                        color: AppColors.textPrimary,
                        fontWeight: FontWeight.w600,
                        fontSize: 13)),
                Text(
                  _connected
                      ? '${_existing!.models.length} models available'
                      : _subtitle(),
                  style: TextStyle(
                      color: _connected ? color : AppColors.textMuted, fontSize: 11),
                ),
              ],
            ),
          ),
          _actionButton(context),
        ],
      ),
    );
  }

  String _subtitle() {
    if (type == ProviderType.gemini) return 'Sign in with Google — no API key';
    if (type == ProviderType.ollama) return 'Local models on your machine';
    if (type == ProviderType.custom) return 'Any OpenAI-compatible endpoint';
    return 'Connect with your API key';
  }

  Widget _actionButton(BuildContext context) {
    if (_connected) {
      return Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.check_circle, size: 16, color: AppColors.accent),
          const SizedBox(width: 6),
          TextButton(
            onPressed: () => prov.removeModelProvider(_existing!.id),
            child: const Text('Disconnect',
                style: TextStyle(color: AppColors.textMuted, fontSize: 12)),
          ),
        ],
      );
    }

    if (type == ProviderType.gemini) {
      return ElevatedButton.icon(
        icon: const Icon(Icons.login, size: 15),
        label: const Text('Sign in'),
        style: ElevatedButton.styleFrom(backgroundColor: ModelProvider.typeColor(type)),
        onPressed: () async {
          final ok = await prov.connectGoogleGemini();
          if (context.mounted) _toast(context, ok
              ? 'Gemini connected via Google'
              : 'Google sign-in failed or cancelled');
        },
      );
    }

    return ElevatedButton.icon(
      icon: Icon(type == ProviderType.ollama ? Icons.link : Icons.vpn_key, size: 15),
      label: Text(type == ProviderType.ollama ? 'Connect' : 'Add key'),
      style: ElevatedButton.styleFrom(backgroundColor: ModelProvider.typeColor(type)),
      onPressed: () => _openConnectSheet(context),
    );
  }

  Future<void> _openConnectSheet(BuildContext context) async {
    final keyCtrl = TextEditingController(text: _existing?.apiKey ?? '');
    final urlCtrl = TextEditingController(
        text: _existing?.baseUrl ?? ModelProvider.defaultUrl(type));
    final needsKey = ModelProvider.needsApiKey(type);

    final connect = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AppColors.surface,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: const BorderSide(color: AppColors.border),
        ),
        title: Text('Connect ${ModelProvider.defaultName(type)}',
            style: const TextStyle(color: AppColors.textPrimary, fontSize: 16)),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            if (type == ProviderType.ollama || type == ProviderType.custom) ...[
              const Text('Server URL',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
              const SizedBox(height: 6),
              TextField(
                controller: urlCtrl,
                style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                decoration: const InputDecoration(hintText: 'http://localhost:11434'),
              ),
              const SizedBox(height: 12),
            ],
            if (needsKey) ...[
              const Text('API Key',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
              const SizedBox(height: 6),
              TextField(
                controller: keyCtrl,
                obscureText: true,
                autofocus: true,
                style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                decoration: const InputDecoration(hintText: 'sk-…'),
              ),
              const SizedBox(height: 8),
              if (ModelProvider.apiKeyUrl(type).isNotEmpty)
                TextButton.icon(
                  icon: const Icon(Icons.open_in_new, size: 14),
                  label: const Text('Get an API key', style: TextStyle(fontSize: 12)),
                  onPressed: () => launchUrl(Uri.parse(ModelProvider.apiKeyUrl(type)),
                      mode: LaunchMode.externalApplication),
                ),
            ],
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Connect'),
          ),
        ],
      ),
    );

    if (connect != true) return;
    if (needsKey && keyCtrl.text.trim().isEmpty) {
      if (context.mounted) _toast(context, 'API key required');
      return;
    }

    // Replace any existing provider of this type, then connect + persist.
    if (_existing != null) prov.removeModelProvider(_existing!.id);
    await prov.addModelProvider(ModelProvider(
      name: ModelProvider.defaultName(type),
      type: type,
      baseUrl: urlCtrl.text.trim().isEmpty
          ? ModelProvider.defaultUrl(type)
          : urlCtrl.text.trim(),
      apiKey: keyCtrl.text.trim(),
    ));
    if (context.mounted) {
      _toast(context, '${ModelProvider.defaultName(type)} connected');
    }
  }

  void _toast(BuildContext context, String msg) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(msg), behavior: SnackBarBehavior.floating),
    );
  }
}
