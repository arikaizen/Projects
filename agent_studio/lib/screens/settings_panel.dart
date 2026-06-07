import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';

enum _ConnMode { ffi, http }

class SettingsPanel extends StatefulWidget {
  const SettingsPanel({super.key});

  @override
  State<SettingsPanel> createState() => _SettingsPanelState();
}

class _SettingsPanelState extends State<SettingsPanel> {
  final _pathCtrl = TextEditingController();
  final _urlCtrl  = TextEditingController();
  bool _connecting = false;
  _ConnMode _mode  = kIsWeb ? _ConnMode.http : _ConnMode.ffi;

  @override
  void initState() {
    super.initState();
    final prov  = context.read<AgentProvider>();
    final saved = prov.backendUrl ?? '';
    if (saved.startsWith('http')) {
      _mode = _ConnMode.http;
      _urlCtrl.text = saved;
    } else {
      _pathCtrl.text = saved.isNotEmpty ? saved : '/path/to/libagent_engine.so';
      _urlCtrl.text  = 'http://localhost:8080';
    }
  }

  @override
  void dispose() {
    _pathCtrl.dispose();
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
        width: 520,
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
              _section('Engine Connection'),
              const SizedBox(height: 8),

              // Status banner
              _statusBanner(prov),
              const SizedBox(height: 16),

              // Mode toggle (not on web — FFI unavailable)
              if (!kIsWeb) ...[
                _modeToggle(),
                const SizedBox(height: 16),
              ],

              // Connection input
              if (_mode == _ConnMode.ffi && !kIsWeb)
                _ffiInput()
              else
                _httpInput(),

              const SizedBox(height: 20),
              _section('About'),
              const SizedBox(height: 8),
              _infoBox(
                Icons.hub,
                'Agent Studio connects directly to your compiled '
                'libagent_engine.so via Dart FFI on desktop, or to a '
                'REST API server via HTTP. Both expose the full C ABI.',
              ),

              const SizedBox(height: 16),
              _section('Expected REST endpoints (HTTP mode)'),
              const SizedBox(height: 8),
              _codeBox(
                'GET  /health\n'
                'POST /api/agents          → spawn agent\n'
                'GET  /api/agents/:id/status\n'
                'POST /api/agents/:id/run  → { "task": "..." }\n'
                'POST /api/agents/:id/cancel\n'
                'POST /api/groups/:id/run  → { "task": "..." }',
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _statusBanner(AgentProvider prov) {
    final ok    = prov.isConnected;
    final color = ok ? AppColors.statusDone : AppColors.textMuted;
    final label = ok
        ? 'Connected — ${prov.connectionLabel}'
        : 'Not connected — running in mock mode';
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: color.withOpacity(0.08),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: color.withOpacity(0.3)),
      ),
      child: Row(
        children: [
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(color: color, shape: BoxShape.circle),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(label, style: TextStyle(color: color, fontSize: 12)),
          ),
          if (ok)
            TextButton(
              onPressed: prov.disconnect,
              style: TextButton.styleFrom(
                foregroundColor: AppColors.error,
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
              ),
              child: const Text('Disconnect', style: TextStyle(fontSize: 11)),
            ),
        ],
      ),
    );
  }

  Widget _modeToggle() {
    return Row(
      children: [
        _modeBtn(_ConnMode.ffi,  Icons.memory,      'FFI (direct .so)'),
        const SizedBox(width: 10),
        _modeBtn(_ConnMode.http, Icons.cloud_outlined, 'HTTP REST API'),
      ],
    );
  }

  Widget _modeBtn(_ConnMode mode, IconData icon, String label) {
    final sel = _mode == mode;
    return Expanded(
      child: GestureDetector(
        onTap: () => setState(() => _mode = mode),
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 150),
          padding: const EdgeInsets.symmetric(vertical: 12),
          decoration: BoxDecoration(
            color: sel ? AppColors.primary.withOpacity(0.12) : AppColors.surfaceAlt,
            borderRadius: BorderRadius.circular(10),
            border: Border.all(
              color: sel ? AppColors.primary : AppColors.border,
              width: sel ? 1.5 : 1,
            ),
          ),
          child: Column(
            children: [
              Icon(icon, size: 20, color: sel ? AppColors.primary : AppColors.textMuted),
              const SizedBox(height: 6),
              Text(label, style: TextStyle(
                color: sel ? AppColors.primary : AppColors.textSecondary,
                fontSize: 12,
                fontWeight: sel ? FontWeight.w600 : FontWeight.normal,
              )),
            ],
          ),
        ),
      ),
    );
  }

  Widget _ffiInput() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _label('Path to libagent_engine.so'),
        const SizedBox(height: 6),
        const Text(
          'Point to the compiled shared library on your machine. '
          'The app loads it directly via Dart FFI — no server needed.',
          style: TextStyle(color: AppColors.textMuted, fontSize: 12, height: 1.4),
        ),
        const SizedBox(height: 10),
        Row(
          children: [
            Expanded(
              child: TextField(
                controller: _pathCtrl,
                style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                decoration: const InputDecoration(
                  hintText: '/path/to/libagent_engine.so',
                  prefixIcon: Icon(Icons.folder_open_outlined, size: 16,
                    color: AppColors.textMuted),
                ),
              ),
            ),
            const SizedBox(width: 10),
            _connectBtn(_pathCtrl),
          ],
        ),
        const SizedBox(height: 10),
        _infoBox(Icons.info_outline,
          'Build your agent engine with:\n'
          '  cmake -DBUILD_SHARED_LIBS=ON ..\n'
          '  make agent_engine\n'
          'Then point this path to the resulting .so file.'),
      ],
    );
  }

  Widget _httpInput() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _label('Backend URL'),
        const SizedBox(height: 6),
        const Text(
          'Connect to a running AgentManager HTTP server. '
          'Falls back to mock mode if unreachable.',
          style: TextStyle(color: AppColors.textMuted, fontSize: 12, height: 1.4),
        ),
        const SizedBox(height: 10),
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
            const SizedBox(width: 10),
            _connectBtn(_urlCtrl),
          ],
        ),
      ],
    );
  }

  Widget _connectBtn(TextEditingController ctrl) {
    return _connecting
        ? const SizedBox(
            width: 44,
            height: 44,
            child: Center(
              child: SizedBox(
                width: 20, height: 20,
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                  valueColor: AlwaysStoppedAnimation(AppColors.primary),
                ),
              ),
            ),
          )
        : ElevatedButton(
            onPressed: () => _connect(ctrl.text.trim()),
            style: ElevatedButton.styleFrom(
              padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
            ),
            child: const Text('Connect'),
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

  Future<void> _connect(String target) async {
    if (target.isEmpty) return;
    setState(() => _connecting = true);
    await context.read<AgentProvider>().connect(target);
    if (mounted) setState(() => _connecting = false);
  }

  Widget _section(String title) => Text(title,
    style: const TextStyle(
      color: AppColors.textSecondary,
      fontSize: 11,
      fontWeight: FontWeight.w600,
      letterSpacing: 1,
    ));

  Widget _label(String text) => Text(text,
    style: const TextStyle(
      color: AppColors.textSecondary,
      fontSize: 12,
      fontWeight: FontWeight.w600,
    ));

  Widget _infoBox(IconData icon, String text) {
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
              style: const TextStyle(
                color: AppColors.textSecondary,
                fontSize: 11,
                height: 1.6,
                fontFamily: 'monospace',
              )),
          ),
        ],
      ),
    );
  }

  Widget _codeBox(String code) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.border),
      ),
      child: Text(code,
        style: const TextStyle(
          color: AppColors.accent,
          fontSize: 11,
          fontFamily: 'monospace',
          height: 1.8,
        )),
    );
  }
}
