import 'package:file_picker/file_picker.dart';
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:provider/provider.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';
import '../models/model_provider.dart';

enum _ConnMode { ffi, http }

class SettingsPanel extends StatefulWidget {
  const SettingsPanel({super.key});

  @override
  State<SettingsPanel> createState() => _SettingsPanelState();
}

class _SettingsPanelState extends State<SettingsPanel> {
  final _pathCtrl      = TextEditingController();
  final _urlCtrl       = TextEditingController();
  final _mcpUrlCtrl    = TextEditingController(text: 'http://localhost:3000');
  bool _connecting         = false;
  bool _ollamaDetecting    = false;
  bool _mcpConnecting      = false;
  String? _ollamaDetectMsg;
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
    _mcpUrlCtrl.dispose();
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

              const SizedBox(height: 24),
              _localLlmSection(prov),
              const SizedBox(height: 24),
              _mcpServerSection(prov),
              const SizedBox(height: 24),
              _modelProvidersSection(prov),
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
            const SizedBox(width: 8),
            IconButton(
              icon: const Icon(Icons.folder_open, size: 20),
              color: AppColors.textSecondary,
              tooltip: 'Browse…',
              onPressed: () async {
                final result = await FilePicker.platform.pickFiles(
                  type: FileType.custom,
                  allowedExtensions: ['so', 'dylib', 'dll'],
                  dialogTitle: 'Select libagent_engine',
                );
                if (result != null && result.files.single.path != null) {
                  setState(() => _pathCtrl.text = result.files.single.path!);
                }
              },
            ),
            const SizedBox(width: 4),
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

  // ── Local LLM (Ollama) quick-connect ────────────────────────────────────────

  Widget _localLlmSection(AgentProvider prov) {
    final ollamaProviders = prov.modelProviders
        .where((p) => p.type == ProviderType.ollama)
        .toList();
    final connected = ollamaProviders.any((p) => p.isConnected);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(children: [
          const Icon(Icons.computer, size: 14, color: AppColors.accent),
          const SizedBox(width: 6),
          _section('Local LLM (Ollama)'),
          const Spacer(),
          if (connected)
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
              decoration: BoxDecoration(
                color: AppColors.statusDone.withOpacity(0.12),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(color: AppColors.statusDone.withOpacity(0.4)),
              ),
              child: const Row(mainAxisSize: MainAxisSize.min, children: [
                Icon(Icons.circle, size: 6, color: AppColors.statusDone),
                SizedBox(width: 5),
                Text('Connected', style: TextStyle(color: AppColors.statusDone,
                    fontSize: 10, fontWeight: FontWeight.w600)),
              ]),
            ),
        ]),
        const SizedBox(height: 8),
        const Text(
          'Run AI models locally using Ollama. No API key needed — '
          'just install Ollama and pull a model.',
          style: TextStyle(color: AppColors.textMuted, fontSize: 11, height: 1.5),
        ),
        const SizedBox(height: 10),
        if (_ollamaDetectMsg != null) ...[
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
            decoration: BoxDecoration(
              color: AppColors.surfaceAlt,
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: AppColors.border),
            ),
            child: Text(_ollamaDetectMsg!,
                style: const TextStyle(color: AppColors.textSecondary,
                    fontSize: 11, fontFamily: 'monospace')),
          ),
          const SizedBox(height: 8),
        ],
        Row(children: [
          Expanded(
            child: ElevatedButton.icon(
              icon: _ollamaDetecting
                  ? const SizedBox(width: 14, height: 14,
                      child: CircularProgressIndicator(strokeWidth: 1.5,
                          valueColor: AlwaysStoppedAnimation(Colors.white)))
                  : const Icon(Icons.search, size: 16),
              label: Text(_ollamaDetecting ? 'Detecting…' : 'Auto-detect Ollama'),
              onPressed: _ollamaDetecting ? null : () => _detectOllama(prov),
              style: ElevatedButton.styleFrom(
                backgroundColor: AppColors.accent.withOpacity(0.8),
              ),
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: OutlinedButton.icon(
              icon: const Icon(Icons.add, size: 16),
              label: const Text('Add manually'),
              onPressed: () => _addOllamaManually(prov),
            ),
          ),
        ]),
        const SizedBox(height: 10),
        _codeBox(
          '# Install Ollama (Linux/Mac)\n'
          'curl -fsSL https://ollama.com/install.sh | sh\n\n'
          '# Pull a model\n'
          'ollama pull llama3\n'
          'ollama pull mistral\n'
          'ollama pull gemma3',
        ),
      ],
    );
  }

  Future<void> _detectOllama(AgentProvider prov) async {
    setState(() { _ollamaDetecting = true; _ollamaDetectMsg = null; });

    const candidates = [
      'http://localhost:11434',
      'http://127.0.0.1:11434',
      'http://0.0.0.0:11434',
    ];

    String? found;
    for (final url in candidates) {
      try {
        final res = await http.get(Uri.parse('$url/api/tags'))
            .timeout(const Duration(seconds: 3));
        if (res.statusCode == 200) { found = url; break; }
      } catch (_) {}
    }

    if (!mounted) return;

    if (found == null) {
      setState(() {
        _ollamaDetecting = false;
        _ollamaDetectMsg = 'Ollama not found on localhost:11434.\n'
            'Make sure Ollama is running: ollama serve';
      });
      return;
    }

    // Already added?
    final already = prov.modelProviders.any(
        (p) => p.type == ProviderType.ollama && p.baseUrl == found);
    if (!already) {
      await prov.addModelProvider(ModelProvider(
        name: 'Local Ollama',
        type: ProviderType.ollama,
        baseUrl: found!,
      ));
    }

    setState(() {
      _ollamaDetecting = false;
      _ollamaDetectMsg = 'Found Ollama at $found ✓';
    });
  }

  Future<void> _addOllamaManually(AgentProvider prov) async {
    final provider = await showDialog<ModelProvider>(
      context: context,
      builder: (_) => const _AddProviderDialog(),
    );
    if (provider != null) await prov.addModelProvider(provider);
  }

  // ── MCP Server connection ────────────────────────────────────────────────────

  Widget _mcpServerSection(AgentProvider prov) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(children: [
          const Icon(Icons.hub_outlined, size: 14, color: AppColors.secondary),
          const SizedBox(width: 6),
          _section('MCP Server'),
        ]),
        const SizedBox(height: 8),
        const Text(
          'Connect to the Agent Studio MCP server (mcp_server/) which '
          'bridges Claude API and Ollama under the MCP protocol.',
          style: TextStyle(color: AppColors.textMuted, fontSize: 11, height: 1.5),
        ),
        const SizedBox(height: 10),
        Row(children: [
          Expanded(
            child: TextField(
              controller: _mcpUrlCtrl,
              style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
              decoration: const InputDecoration(
                hintText: 'http://localhost:3000',
                prefixIcon: Icon(Icons.hub_outlined, size: 16,
                    color: AppColors.textMuted),
              ),
            ),
          ),
          const SizedBox(width: 10),
          _mcpConnecting
              ? const SizedBox(width: 44, height: 44,
                  child: Center(child: SizedBox(width: 20, height: 20,
                      child: CircularProgressIndicator(strokeWidth: 2,
                          valueColor: AlwaysStoppedAnimation(AppColors.secondary)))))
              : ElevatedButton(
                  onPressed: () => _connectMcp(prov),
                  style: ElevatedButton.styleFrom(
                      backgroundColor: AppColors.secondary,
                      padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14)),
                  child: const Text('Connect'),
                ),
        ]),
        const SizedBox(height: 10),
        _infoBox(Icons.terminal,
          'Start the server:\n'
          '  cd mcp_server\n'
          '  ANTHROPIC_API_KEY=sk-ant-… dart run bin/server.dart\n\n'
          'Then click Connect above to link the app to it.'),
      ],
    );
  }

  Future<void> _connectMcp(AgentProvider prov) async {
    final url = _mcpUrlCtrl.text.trim();
    if (url.isEmpty) return;
    setState(() => _mcpConnecting = true);

    try {
      final res = await http.get(Uri.parse('$url/health'))
          .timeout(const Duration(seconds: 5));
      if (res.statusCode == 200) {
        // Add as a custom model provider so its models are available
        final already = prov.modelProviders.any(
            (p) => p.type == ProviderType.custom && p.baseUrl == url);
        if (!already) {
          await prov.addModelProvider(ModelProvider(
            name: 'MCP Server',
            type: ProviderType.custom,
            baseUrl: url,
          ));
        }
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
            content: Text('MCP server connected'),
            backgroundColor: AppColors.statusDone,
            duration: Duration(seconds: 2),
          ));
        }
      } else {
        throw Exception('HTTP ${res.statusCode}');
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(
          content: Text('MCP server unreachable: $e'),
          backgroundColor: AppColors.error,
          duration: const Duration(seconds: 3),
        ));
      }
    }

    if (mounted) setState(() => _mcpConnecting = false);
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
                Text(p.name, style: const TextStyle(color: AppColors.textPrimary,
                    fontSize: 12, fontWeight: FontWeight.w600)),
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

class _AddProviderDialog extends StatefulWidget {
  const _AddProviderDialog();

  @override
  State<_AddProviderDialog> createState() => _AddProviderDialogState();
}

class _AddProviderDialogState extends State<_AddProviderDialog> {
  ProviderType _type = ProviderType.ollama;
  late final _nameCtrl = TextEditingController(
      text: ModelProvider.defaultName(ProviderType.ollama));
  late final _urlCtrl = TextEditingController(
      text: ModelProvider.defaultUrl(ProviderType.ollama));
  final _keyCtrl  = TextEditingController();
  bool _obscureKey = true;

  @override
  void dispose() {
    _nameCtrl.dispose();
    _urlCtrl.dispose();
    _keyCtrl.dispose();
    super.dispose();
  }

  void _selectType(ProviderType t) {
    setState(() {
      _type = t;
      _nameCtrl.text = ModelProvider.defaultName(t);
      _urlCtrl.text  = ModelProvider.defaultUrl(t);
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
            Padding(
              padding: const EdgeInsets.all(24),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _label('Provider Type'),
                  const SizedBox(height: 8),
                  Row(
                    children: ProviderType.values.map((t) {
                      final sel = _type == t;
                      return Expanded(
                        child: GestureDetector(
                          onTap: () => _selectType(t),
                          child: AnimatedContainer(
                            duration: const Duration(milliseconds: 120),
                            margin: const EdgeInsets.only(right: 6),
                            padding: const EdgeInsets.symmetric(vertical: 10),
                            decoration: BoxDecoration(
                              color: sel
                                  ? AppColors.primary.withOpacity(0.12)
                                  : AppColors.surfaceAlt,
                              borderRadius: BorderRadius.circular(8),
                              border: Border.all(
                                color: sel ? AppColors.primary : AppColors.border,
                                width: sel ? 1.5 : 1,
                              ),
                            ),
                            child: Column(
                              children: [
                                Icon(_typeIcon(t), size: 18,
                                    color: sel ? AppColors.primary
                                        : AppColors.textMuted),
                                const SizedBox(height: 4),
                                Text(ModelProvider.typeLabel(t),
                                    style: TextStyle(
                                      color: sel ? AppColors.primary
                                          : AppColors.textSecondary,
                                      fontSize: 10,
                                      fontWeight: sel
                                          ? FontWeight.w600
                                          : FontWeight.normal,
                                    )),
                              ],
                            ),
                          ),
                        ),
                      );
                    }).toList(),
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
                  if (_type != ProviderType.ollama) ...[
                    const SizedBox(height: 14),
                    _label('API Key'),
                    const SizedBox(height: 6),
                    TextField(
                      controller: _keyCtrl,
                      obscureText: _obscureKey,
                      style: const TextStyle(
                          color: AppColors.textPrimary, fontSize: 13),
                      decoration: InputDecoration(
                        hintText: 'sk-…',
                        suffixIcon: IconButton(
                          icon: Icon(
                            _obscureKey
                                ? Icons.visibility_outlined
                                : Icons.visibility_off_outlined,
                            size: 16,
                            color: AppColors.textMuted,
                          ),
                          onPressed: () =>
                              setState(() => _obscureKey = !_obscureKey),
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

  void _submit() {
    final name = _nameCtrl.text.trim();
    final url  = _urlCtrl.text.trim();
    if (name.isEmpty || url.isEmpty) return;
    Navigator.pop(
      context,
      ModelProvider(
        name:   name,
        type:   _type,
        baseUrl: url,
        apiKey: _keyCtrl.text.trim(),
      ),
    );
  }

  Widget _label(String text) => Text(text,
      style: const TextStyle(
          color: AppColors.textSecondary,
          fontSize: 12,
          fontWeight: FontWeight.w600));

  IconData _typeIcon(ProviderType t) {
    switch (t) {
      case ProviderType.anthropic: return Icons.auto_awesome;
      case ProviderType.ollama:    return Icons.computer;
      case ProviderType.openai:    return Icons.cloud_outlined;
      case ProviderType.custom:    return Icons.settings_ethernet;
    }
  }
}
