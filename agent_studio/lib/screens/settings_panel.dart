import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';
import '../models/model_provider.dart';
import '../services/google_auth/google_auth_service.dart';

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
              _modelProvidersSection(prov),
              const SizedBox(height: 20),
              _mcpServersSection(prov),
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

  Widget _modelProvidersSection(AgentProvider prov) {
    final providers = prov.modelProviders;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            _section('Model Providers'),
            const Spacer(),
            GestureDetector(
              onTap: () => _addProvider(prov),
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                decoration: BoxDecoration(
                  color: AppColors.primary.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(6),
                  border: Border.all(color: AppColors.primary.withOpacity(0.3)),
                ),
                child: const Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.add, size: 12, color: AppColors.primary),
                    SizedBox(width: 4),
                    Text('Add', style: TextStyle(color: AppColors.primary, fontSize: 11,
                        fontWeight: FontWeight.w600)),
                  ],
                ),
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        if (providers.isEmpty)
          Container(
            width: double.infinity,
            padding: const EdgeInsets.symmetric(vertical: 16),
            decoration: BoxDecoration(
              color: AppColors.surfaceAlt,
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: AppColors.border),
            ),
            child: const Column(
              children: [
                Icon(Icons.hub_outlined, size: 28, color: AppColors.textMuted),
                SizedBox(height: 8),
                Text('No model providers yet',
                    style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
                SizedBox(height: 2),
                Text('Add Ollama for local models or an API for remote models',
                    style: TextStyle(color: AppColors.textMuted, fontSize: 11)),
              ],
            ),
          )
        else
          ...providers.map((p) => _providerRow(p, prov)),
      ],
    );
  }

  Widget _providerRow(ModelProvider p, AgentProvider prov) {
    final statusColor = p.isLoading
        ? AppColors.warning
        : p.isConnected
            ? AppColors.statusDone
            : AppColors.statusError;

    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppColors.border),
      ),
      child: Row(
        children: [
          // Status dot
          Container(
            width: 8, height: 8,
            decoration: BoxDecoration(color: statusColor, shape: BoxShape.circle,
              boxShadow: p.isConnected
                  ? [BoxShadow(color: statusColor.withOpacity(0.5), blurRadius: 4)]
                  : null,
            ),
          ),
          const SizedBox(width: 10),
          // Type chip
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
            decoration: BoxDecoration(
              color: AppColors.primary.withOpacity(0.12),
              borderRadius: BorderRadius.circular(4),
            ),
            child: Text(ModelProvider.typeLabel(p.type),
                style: const TextStyle(color: AppColors.primary, fontSize: 9,
                    fontWeight: FontWeight.w700, letterSpacing: 0.5)),
          ),
          const SizedBox(width: 8),
          // Name + URL
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Flexible(
                      child: Text(p.name,
                          style: const TextStyle(color: AppColors.textPrimary,
                              fontSize: 12, fontWeight: FontWeight.w600),
                          overflow: TextOverflow.ellipsis),
                    ),
                    if (prov.engineDefaultProviderId == p.id) ...[
                      const SizedBox(width: 6),
                      Container(
                        padding: const EdgeInsets.symmetric(horizontal: 5, vertical: 1),
                        decoration: BoxDecoration(
                          color: AppColors.statusDone.withOpacity(0.15),
                          borderRadius: BorderRadius.circular(3),
                        ),
                        child: const Text('ENGINE',
                            style: TextStyle(color: AppColors.statusDone, fontSize: 8,
                                fontWeight: FontWeight.w700, letterSpacing: 0.5)),
                      ),
                    ],
                  ],
                ),
                Text(
                  p.isLoading ? 'Connecting…'
                      : p.error != null ? p.error!
                      : p.isConnected ? '${p.models.length} model${p.models.length == 1 ? '' : 's'}'
                      : Uri.tryParse(p.baseUrl)?.host ?? p.baseUrl,
                  style: TextStyle(
                    color: p.error != null ? AppColors.error : AppColors.textMuted,
                    fontSize: 10,
                  ),
                  overflow: TextOverflow.ellipsis,
                ),
              ],
            ),
          ),
          // Set as engine default
          if (p.isConnected && p.models.isNotEmpty &&
              prov.engineDefaultProviderId != p.id)
            IconButton(
              icon: const Icon(Icons.bolt_outlined, size: 14),
              color: AppColors.textMuted,
              tooltip: 'Use for engine agents',
              padding: EdgeInsets.zero,
              constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
              onPressed: () =>
                  prov.setEngineDefaultProvider(p.id, p.models.first.id),
            ),
          // Loading or refresh button
          if (p.isLoading)
            const SizedBox(
              width: 18, height: 18,
              child: CircularProgressIndicator(
                strokeWidth: 1.5,
                valueColor: AlwaysStoppedAnimation(AppColors.primary),
              ),
            )
          else
            IconButton(
              icon: const Icon(Icons.refresh, size: 14),
              color: AppColors.textMuted,
              tooltip: 'Refresh',
              padding: EdgeInsets.zero,
              constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
              onPressed: () => prov.refreshModelProvider(p.id),
            ),
          IconButton(
            icon: const Icon(Icons.close, size: 14),
            color: AppColors.textMuted,
            tooltip: 'Remove',
            padding: EdgeInsets.zero,
            constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
            onPressed: () => prov.removeModelProvider(p.id),
          ),
        ],
      ),
    );
  }

  Future<void> _addProvider(AgentProvider prov) async {
    final provider = await showDialog<ModelProvider>(
      context: context,
      builder: (_) => const _AddProviderDialog(),
    );
    if (provider != null) {
      await prov.addModelProvider(provider);
    }
  }

  Widget _mcpServersSection(AgentProvider prov) {
    final servers = prov.mcpServers;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            _section('MCP Servers'),
            const Spacer(),
            GestureDetector(
              onTap: () => _addMcpServer(prov),
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                decoration: BoxDecoration(
                  color: AppColors.primary.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(6),
                  border: Border.all(color: AppColors.primary.withOpacity(0.3)),
                ),
                child: const Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.add, size: 12, color: AppColors.primary),
                    SizedBox(width: 4),
                    Text('Add', style: TextStyle(color: AppColors.primary,
                        fontSize: 11, fontWeight: FontWeight.w600)),
                  ],
                ),
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        if (servers.isEmpty)
          Container(
            width: double.infinity,
            padding: const EdgeInsets.symmetric(vertical: 16),
            decoration: BoxDecoration(
              color: AppColors.surfaceAlt,
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: AppColors.border),
            ),
            child: const Column(
              children: [
                Icon(Icons.hub_outlined, size: 28, color: AppColors.textMuted),
                SizedBox(height: 8),
                Text('No MCP servers connected',
                    style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
                SizedBox(height: 2),
                Text('Add a server to expose tools to the agent',
                    style: TextStyle(color: AppColors.textMuted, fontSize: 11)),
              ],
            ),
          )
        else
          ...servers.map((s) => _mcpServerRow(s, prov)),
      ],
    );
  }

  Widget _mcpServerRow(Map<String, String> server, AgentProvider prov) {
    final name      = server['name'] ?? '';
    final url       = server['url'] ?? '';
    final transport = server['transport'] ?? 'http';
    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppColors.border),
      ),
      child: Row(
        children: [
          Container(
            width: 8, height: 8,
            decoration: const BoxDecoration(
              color: AppColors.statusDone,
              shape: BoxShape.circle,
            ),
          ),
          const SizedBox(width: 10),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
            decoration: BoxDecoration(
              color: AppColors.primary.withOpacity(0.12),
              borderRadius: BorderRadius.circular(4),
            ),
            child: Text(transport.toUpperCase(),
                style: const TextStyle(color: AppColors.primary, fontSize: 9,
                    fontWeight: FontWeight.w700, letterSpacing: 0.5)),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(name, style: const TextStyle(color: AppColors.textPrimary,
                    fontSize: 12, fontWeight: FontWeight.w600)),
                Text(url, style: const TextStyle(color: AppColors.textMuted,
                    fontSize: 10), overflow: TextOverflow.ellipsis),
              ],
            ),
          ),
          IconButton(
            icon: const Icon(Icons.close, size: 14),
            color: AppColors.textMuted,
            tooltip: 'Remove',
            padding: EdgeInsets.zero,
            constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
            onPressed: () => prov.removeMcpServer(name),
          ),
        ],
      ),
    );
  }

  Future<void> _addMcpServer(AgentProvider prov) async {
    final result = await showDialog<Map<String, String>>(
      context: context,
      builder: (_) => const _AddMcpServerDialog(),
    );
    if (result != null) {
      await prov.addMcpServer(
        name:        result['name']!,
        url:         result['url']!,
        bearerToken: result['bearer_token'] ?? '',
        transport:   result['transport'] ?? 'http',
      );
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// Add MCP Server dialog
// ─────────────────────────────────────────────────────────────────────────────

class _AddMcpServerDialog extends StatefulWidget {
  const _AddMcpServerDialog();

  @override
  State<_AddMcpServerDialog> createState() => _AddMcpServerDialogState();
}

class _AddMcpServerDialogState extends State<_AddMcpServerDialog> {
  final _nameCtrl    = TextEditingController();
  final _urlCtrl     = TextEditingController(text: 'http://localhost:8081');
  final _tokenCtrl   = TextEditingController();
  String _transport  = 'http';
  bool _obscureToken = true;

  @override
  void dispose() {
    _nameCtrl.dispose();
    _urlCtrl.dispose();
    _tokenCtrl.dispose();
    super.dispose();
  }

  void _submit() {
    final name = _nameCtrl.text.trim();
    final url  = _urlCtrl.text.trim();
    if (name.isEmpty || url.isEmpty) return;
    Navigator.pop(context, {
      'name':         name,
      'url':          url,
      'bearer_token': _tokenCtrl.text.trim(),
      'transport':    _transport,
    });
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
            // Header
            Container(
              padding: const EdgeInsets.fromLTRB(24, 20, 16, 16),
              decoration: const BoxDecoration(
                border: Border(bottom: BorderSide(color: AppColors.border)),
              ),
              child: Row(
                children: [
                  const Icon(Icons.hub, color: AppColors.primary, size: 20),
                  const SizedBox(width: 12),
                  const Expanded(
                    child: Text('Connect MCP Server',
                        style: TextStyle(color: AppColors.textPrimary,
                            fontSize: 16, fontWeight: FontWeight.w600)),
                  ),
                  IconButton(
                    icon: const Icon(Icons.close, size: 18,
                        color: AppColors.textMuted),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
            ),
            // Body
            Padding(
              padding: const EdgeInsets.all(24),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _label('Transport'),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      _transportBtn('http',  Icons.http,       'Streamable HTTP'),
                      const SizedBox(width: 8),
                      _transportBtn('stdio', Icons.terminal,   'stdio (local)'),
                    ],
                  ),
                  const SizedBox(height: 16),
                  _label('Server Name'),
                  const SizedBox(height: 6),
                  TextField(
                    controller: _nameCtrl,
                    style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                    decoration: const InputDecoration(hintText: 'my-mcp-server'),
                  ),
                  const SizedBox(height: 14),
                  _label(_transport == 'stdio' ? 'Command / path' : 'Base URL'),
                  const SizedBox(height: 6),
                  TextField(
                    controller: _urlCtrl,
                    style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                    decoration: InputDecoration(
                      hintText: _transport == 'stdio'
                          ? '/usr/local/bin/mcp-server'
                          : 'http://localhost:8081',
                    ),
                  ),
                  if (_transport == 'http') ...[
                    const SizedBox(height: 14),
                    _label('Bearer Token (optional)'),
                    const SizedBox(height: 6),
                    TextField(
                      controller: _tokenCtrl,
                      obscureText: _obscureToken,
                      style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                      decoration: InputDecoration(
                        hintText: 'eyJ…',
                        suffixIcon: IconButton(
                          icon: Icon(
                            _obscureToken
                                ? Icons.visibility_outlined
                                : Icons.visibility_off_outlined,
                            size: 16, color: AppColors.textMuted,
                          ),
                          onPressed: () => setState(() => _obscureToken = !_obscureToken),
                        ),
                      ),
                    ),
                  ],
                ],
              ),
            ),
            // Footer
            Container(
              padding: const EdgeInsets.fromLTRB(24, 12, 24, 20),
              decoration: const BoxDecoration(
                border: Border(top: BorderSide(color: AppColors.border)),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  TextButton(
                    onPressed: () => Navigator.pop(context),
                    child: const Text('Cancel'),
                  ),
                  const SizedBox(width: 8),
                  ElevatedButton.icon(
                    icon: const Icon(Icons.link, size: 16),
                    label: const Text('Connect'),
                    onPressed: _submit,
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _transportBtn(String value, IconData icon, String label) {
    final sel = _transport == value;
    return Expanded(
      child: GestureDetector(
        onTap: () => setState(() => _transport = value),
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 120),
          padding: const EdgeInsets.symmetric(vertical: 10),
          decoration: BoxDecoration(
            color: sel ? AppColors.primary.withOpacity(0.12) : AppColors.surfaceAlt,
            borderRadius: BorderRadius.circular(8),
            border: Border.all(
              color: sel ? AppColors.primary : AppColors.border,
              width: sel ? 1.5 : 1,
            ),
          ),
          child: Column(
            children: [
              Icon(icon, size: 18,
                  color: sel ? AppColors.primary : AppColors.textMuted),
              const SizedBox(height: 4),
              Text(label, style: TextStyle(
                color: sel ? AppColors.primary : AppColors.textSecondary,
                fontSize: 10,
                fontWeight: sel ? FontWeight.w600 : FontWeight.normal,
              )),
            ],
          ),
        ),
      ),
    );
  }

  Widget _label(String text) => Text(text,
      style: const TextStyle(
          color: AppColors.textSecondary,
          fontSize: 12,
          fontWeight: FontWeight.w600));
}

// ─────────────────────────────────────────────────────────────────────────────

class _AddProviderDialog extends StatefulWidget {
  const _AddProviderDialog();

  @override
  State<_AddProviderDialog> createState() => _AddProviderDialogState();
}

class _AddProviderDialogState extends State<_AddProviderDialog> {
  // Hosted providers first, then local ones.
  static const _hosted = [
    ProviderType.anthropic, ProviderType.openai, ProviderType.google,
    ProviderType.groq, ProviderType.mistral, ProviderType.deepseek,
    ProviderType.xai, ProviderType.openrouter, ProviderType.together,
  ];
  static const _local = [
    ProviderType.ollama, ProviderType.lmstudio, ProviderType.llamacpp,
    ProviderType.vllm, ProviderType.custom,
  ];

  ProviderType _type = ProviderType.anthropic;
  late final _nameCtrl = TextEditingController(
      text: ModelProvider.defaultName(ProviderType.anthropic));
  late final _urlCtrl = TextEditingController(
      text: ModelProvider.defaultUrl(ProviderType.anthropic));
  final _keyCtrl      = TextEditingController();
  final _clientIdCtrl = TextEditingController();
  bool _obscureKey = true;
  bool _googleSignedIn = false;
  bool _signingIn = false;
  String? _googleAccessToken;
  String? _googleError;

  @override
  void dispose() {
    _nameCtrl.dispose();
    _urlCtrl.dispose();
    _keyCtrl.dispose();
    _clientIdCtrl.dispose();
    super.dispose();
  }

  void _selectType(ProviderType t) {
    setState(() {
      _type = t;
      _nameCtrl.text = ModelProvider.defaultName(t);
      _urlCtrl.text  = ModelProvider.defaultUrl(t);
      _googleSignedIn = false;
      _googleAccessToken = null;
      _googleError = null;
    });
  }

  bool get _isLocal  => ModelProvider.isLocalType(_type);
  bool get _isGoogle => _type == ProviderType.google;

  Future<void> _doGoogleSignIn() async {
    final clientId = _clientIdCtrl.text.trim();
    if (clientId.isEmpty) {
      setState(() => _googleError = 'Enter your Google OAuth client ID first');
      return;
    }
    setState(() { _signingIn = true; _googleError = null; });
    try {
      final tokens = await googleAuthService.signIn(clientId: clientId);
      setState(() {
        _googleAccessToken = tokens.accessToken;
        _googleSignedIn = true;
        _signingIn = false;
      });
    } catch (e) {
      setState(() {
        _googleError = e.toString();
        _signingIn = false;
        _googleSignedIn = false;
      });
    }
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
            // Header
            Container(
              padding: const EdgeInsets.fromLTRB(24, 20, 16, 16),
              decoration: const BoxDecoration(
                border: Border(bottom: BorderSide(color: AppColors.border)),
              ),
              child: Row(
                children: [
                  const Icon(Icons.hub_outlined, color: AppColors.primary, size: 20),
                  const SizedBox(width: 12),
                  const Expanded(
                    child: Text('Add Model Provider',
                        style: TextStyle(color: AppColors.textPrimary,
                            fontSize: 16, fontWeight: FontWeight.w600)),
                  ),
                  IconButton(
                    icon: const Icon(Icons.close, size: 18,
                        color: AppColors.textMuted),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
            ),
            // Body
            Flexible(
              child: SingleChildScrollView(
                padding: const EdgeInsets.all(24),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    _label('Hosted'),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 8, runSpacing: 8,
                      children: _hosted.map(_typeChip).toList(),
                    ),
                    const SizedBox(height: 14),
                    _label('Local'),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 8, runSpacing: 8,
                      children: _local.map(_typeChip).toList(),
                    ),
                    const SizedBox(height: 16),
                    _label('Name'),
                    const SizedBox(height: 6),
                    TextField(
                      controller: _nameCtrl,
                      style: const TextStyle(
                          color: AppColors.textPrimary, fontSize: 13),
                      decoration:
                          const InputDecoration(hintText: 'Provider name'),
                    ),
                    const SizedBox(height: 14),
                    _label('Base URL'),
                    const SizedBox(height: 6),
                    TextField(
                      controller: _urlCtrl,
                      style: const TextStyle(
                          color: AppColors.textPrimary, fontSize: 13),
                      decoration: const InputDecoration(
                          hintText: 'http://localhost:11434'),
                    ),
                    if (_isLocal)
                      Padding(
                        padding: const EdgeInsets.only(top: 10),
                        child: _infoBox(Icons.computer,
                          'Local provider — no API key needed. Make sure the '
                          'server is running and reachable at the URL above.'),
                      ),
                    if (_isGoogle) ..._googleAuthFields(),
                    if (!_isLocal && !_isGoogle) ..._apiKeyField(),
                  ],
                ),
              ),
            ),
            // Footer
            Container(
              padding: const EdgeInsets.fromLTRB(24, 12, 24, 20),
              decoration: const BoxDecoration(
                border: Border(top: BorderSide(color: AppColors.border)),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  TextButton(
                    onPressed: () => Navigator.pop(context),
                    child: const Text('Cancel'),
                  ),
                  const SizedBox(width: 8),
                  ElevatedButton.icon(
                    icon: const Icon(Icons.link, size: 16),
                    label: const Text('Connect'),
                    onPressed: _submit,
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _typeChip(ProviderType t) {
    final sel = _type == t;
    return GestureDetector(
      onTap: () => _selectType(t),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 120),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        decoration: BoxDecoration(
          color: sel ? AppColors.primary.withOpacity(0.12) : AppColors.surfaceAlt,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: sel ? AppColors.primary : AppColors.border,
            width: sel ? 1.5 : 1,
          ),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(_typeIcon(t), size: 15,
                color: sel ? AppColors.primary : AppColors.textMuted),
            const SizedBox(width: 6),
            Text(ModelProvider.typeLabel(t),
                style: TextStyle(
                  color: sel ? AppColors.primary : AppColors.textSecondary,
                  fontSize: 12,
                  fontWeight: sel ? FontWeight.w600 : FontWeight.normal,
                )),
          ],
        ),
      ),
    );
  }

  List<Widget> _apiKeyField() => [
        const SizedBox(height: 14),
        _label('API Key'),
        const SizedBox(height: 6),
        TextField(
          controller: _keyCtrl,
          obscureText: _obscureKey,
          style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
          decoration: InputDecoration(
            hintText: 'sk-…',
            suffixIcon: IconButton(
              icon: Icon(
                _obscureKey ? Icons.visibility_outlined : Icons.visibility_off_outlined,
                size: 16, color: AppColors.textMuted,
              ),
              onPressed: () => setState(() => _obscureKey = !_obscureKey),
            ),
          ),
        ),
      ];

  List<Widget> _googleAuthFields() => [
        const SizedBox(height: 14),
        _label('Authentication'),
        const SizedBox(height: 6),
        const Text(
          'Use an API key from Google AI Studio, or sign in with your Google '
          'account (OAuth). Sign-in needs a Google Cloud OAuth client ID.',
          style: TextStyle(color: AppColors.textMuted, fontSize: 11, height: 1.4),
        ),
        const SizedBox(height: 10),
        // API key option
        _label('API Key'),
        const SizedBox(height: 6),
        TextField(
          controller: _keyCtrl,
          obscureText: _obscureKey,
          style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
          decoration: InputDecoration(
            hintText: 'AIza… (leave blank to sign in instead)',
            suffixIcon: IconButton(
              icon: Icon(
                _obscureKey ? Icons.visibility_outlined : Icons.visibility_off_outlined,
                size: 16, color: AppColors.textMuted,
              ),
              onPressed: () => setState(() => _obscureKey = !_obscureKey),
            ),
          ),
        ),
        const SizedBox(height: 14),
        Row(children: const [
          Expanded(child: Divider(color: AppColors.border)),
          Padding(
            padding: EdgeInsets.symmetric(horizontal: 8),
            child: Text('or', style: TextStyle(color: AppColors.textMuted, fontSize: 11)),
          ),
          Expanded(child: Divider(color: AppColors.border)),
        ]),
        const SizedBox(height: 14),
        _label('Google OAuth Client ID'),
        const SizedBox(height: 6),
        TextField(
          controller: _clientIdCtrl,
          style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
          decoration: const InputDecoration(
              hintText: '…apps.googleusercontent.com'),
        ),
        const SizedBox(height: 10),
        Row(
          children: [
            ElevatedButton.icon(
              onPressed: _signingIn ? null : _doGoogleSignIn,
              icon: _signingIn
                  ? const SizedBox(width: 14, height: 14,
                      child: CircularProgressIndicator(strokeWidth: 2))
                  : Icon(_googleSignedIn ? Icons.check_circle : Icons.login, size: 16),
              label: Text(_googleSignedIn ? 'Signed in' : 'Sign in with Google'),
              style: ElevatedButton.styleFrom(
                backgroundColor: _googleSignedIn
                    ? AppColors.statusDone.withOpacity(0.2)
                    : null,
              ),
            ),
          ],
        ),
        if (_googleError != null)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(_googleError!,
                style: const TextStyle(color: AppColors.error, fontSize: 11)),
          ),
      ];

  void _submit() {
    final name = _nameCtrl.text.trim();
    final url  = _urlCtrl.text.trim();
    if (name.isEmpty || url.isEmpty) return;

    AuthMethod auth;
    String key;
    if (_isLocal) {
      auth = AuthMethod.none;
      key  = '';
    } else if (_isGoogle && _googleSignedIn && _googleAccessToken != null) {
      auth = AuthMethod.googleOAuth;
      key  = _googleAccessToken!;
    } else {
      auth = AuthMethod.apiKey;
      key  = _keyCtrl.text.trim();
    }

    Navigator.pop(
      context,
      ModelProvider(
        name:    name,
        type:    _type,
        baseUrl: url,
        apiKey:  key,
        authMethod: auth,
        oauthClientId: _clientIdCtrl.text.trim(),
      ),
    );
  }

  Widget _label(String text) => Text(text,
      style: const TextStyle(
          color: AppColors.textSecondary,
          fontSize: 12,
          fontWeight: FontWeight.w600));

  Widget _infoBox(IconData icon, String text) => Container(
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
                      color: AppColors.textSecondary, fontSize: 11, height: 1.5)),
            ),
          ],
        ),
      );

  IconData _typeIcon(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic:  return Icons.auto_awesome;
      case ProviderType.openai:     return Icons.cloud_outlined;
      case ProviderType.google:     return Icons.travel_explore;
      case ProviderType.groq:       return Icons.bolt;
      case ProviderType.mistral:    return Icons.air;
      case ProviderType.deepseek:   return Icons.travel_explore;
      case ProviderType.xai:        return Icons.close;
      case ProviderType.openrouter: return Icons.alt_route;
      case ProviderType.together:   return Icons.group_work;
      case ProviderType.ollama:     return Icons.computer;
      case ProviderType.lmstudio:   return Icons.science_outlined;
      case ProviderType.llamacpp:   return Icons.terminal;
      case ProviderType.vllm:       return Icons.memory;
      case ProviderType.custom:     return Icons.settings_ethernet;
    }
  }
}
