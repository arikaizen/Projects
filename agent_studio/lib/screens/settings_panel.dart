import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';

class SettingsPanel extends StatefulWidget {
  const SettingsPanel({super.key});

  @override
  State<SettingsPanel> createState() => _SettingsPanelState();
}

class _SettingsPanelState extends State<SettingsPanel> {
  final _urlCtrl = TextEditingController();
  bool _connecting = false;

  @override
  void initState() {
    super.initState();
    final prov = context.read<AgentProvider>();
    _urlCtrl.text = prov.backendUrl ?? 'http://localhost:8080';
  }

  @override
  void dispose() {
    _urlCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: AppColors.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: AppColors.border),
      ),
      child: SizedBox(
        width: 480,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            _header(),
            _body(),
            _footer(),
          ],
        ),
      ),
    );
  }

  Widget _header() {
    return Container(
      padding: const EdgeInsets.fromLTRB(24, 20, 16, 16),
      decoration: const BoxDecoration(
        border: Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          const Icon(Icons.settings, color: AppColors.textSecondary, size: 20),
          const SizedBox(width: 12),
          const Expanded(
            child: Text('Settings', style: TextStyle(
              color: AppColors.textPrimary,
              fontSize: 16,
              fontWeight: FontWeight.w600,
            )),
          ),
          IconButton(
            icon: const Icon(Icons.close, size: 18, color: AppColors.textMuted),
            onPressed: () => Navigator.pop(context),
          ),
        ],
      ),
    );
  }

  Widget _body() {
    return Consumer<AgentProvider>(
      builder: (ctx, prov, _) {
        return SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _section('Backend Connection'),
              const SizedBox(height: 8),
              const Text(
                'Connect to your C++ AgentManager REST API. '
                'Without a connection, the app uses mock responses.',
                style: TextStyle(color: AppColors.textMuted, fontSize: 12),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: TextField(
                      controller: _urlCtrl,
                      style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                      decoration: const InputDecoration(
                        hintText: 'http://localhost:8080',
                        prefixIcon: Icon(Icons.link, size: 16, color: AppColors.textMuted),
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  _connecting
                      ? const SizedBox(
                          width: 36,
                          height: 36,
                          child: CircularProgressIndicator(
                            strokeWidth: 2,
                            valueColor: AlwaysStoppedAnimation(AppColors.primary),
                          ),
                        )
                      : ElevatedButton(
                          onPressed: () => _connect(prov),
                          style: ElevatedButton.styleFrom(
                            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
                          ),
                          child: const Text('Connect'),
                        ),
                ],
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Container(
                    width: 8,
                    height: 8,
                    decoration: BoxDecoration(
                      color: prov.isConnected ? AppColors.statusDone : AppColors.statusIdle,
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 8),
                  Text(
                    prov.isConnected
                        ? 'Connected to ${prov.backendUrl}'
                        : 'Not connected — using mock mode',
                    style: TextStyle(
                      color: prov.isConnected ? AppColors.statusDone : AppColors.textMuted,
                      fontSize: 12,
                    ),
                  ),
                  if (prov.isConnected) ...[
                    const Spacer(),
                    TextButton(
                      onPressed: prov.disconnect,
                      child: const Text('Disconnect',
                        style: TextStyle(color: AppColors.error, fontSize: 12)),
                    ),
                  ],
                ],
              ),

              const SizedBox(height: 24),
              _section('API Configuration'),
              const SizedBox(height: 8),
              _infoRow(Icons.info_outline,
                'The app connects to /api/agents/* and /api/groups/* endpoints. '
                'Refer to the C++ agent_engine REST API docs for integration details.'),

              const SizedBox(height: 24),
              _section('About'),
              const SizedBox(height: 8),
              _infoRow(Icons.hub,
                'Agent Studio — Interactive GUI for your AI agent engine.\n'
                'Supports single agents, hierarchies, and collaborative groups.'),
            ],
          ),
        );
      },
    );
  }

  Widget _footer() {
    return Container(
      padding: const EdgeInsets.fromLTRB(24, 12, 24, 20),
      decoration: const BoxDecoration(
        border: Border(top: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.end,
        children: [
          ElevatedButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Done'),
          ),
        ],
      ),
    );
  }

  Widget _section(String title) {
    return Text(title,
      style: const TextStyle(
        color: AppColors.textSecondary,
        fontSize: 11,
        fontWeight: FontWeight.w600,
        letterSpacing: 1,
      ));
  }

  Widget _infoRow(IconData icon, String text) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.border),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, size: 14, color: AppColors.textMuted),
          const SizedBox(width: 10),
          Expanded(
            child: Text(text,
              style: const TextStyle(color: AppColors.textSecondary, fontSize: 12, height: 1.5)),
          ),
        ],
      ),
    );
  }

  Future<void> _connect(AgentProvider prov) async {
    setState(() => _connecting = true);
    await prov.connect(_urlCtrl.text.trim());
    if (mounted) setState(() => _connecting = false);
  }
}
