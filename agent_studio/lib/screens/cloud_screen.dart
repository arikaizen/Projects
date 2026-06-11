import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/cloud_provider.dart';
import '../theme/app_theme.dart';

class CloudScreen extends StatefulWidget {
  const CloudScreen({super.key});
  @override
  State<CloudScreen> createState() => _CloudScreenState();
}

class _CloudScreenState extends State<CloudScreen> {
  String? _selectedAgentId;

  @override
  Widget build(BuildContext context) {
    return Consumer<CloudProvider>(builder: (ctx, cp, _) {
      if (!cp.connected) return _connectPanel(cp);
      return Row(children: [
        _sidebar(cp),
        const VerticalDivider(width: 1, color: AppColors.border),
        Expanded(child: _selectedAgentId != null
            ? _chatPanel(cp, _selectedAgentId!)
            : _emptyState(cp)),
      ]);
    });
  }

  Widget _connectPanel(CloudProvider cp) {
    final urlCtrl = TextEditingController(text: cp.serverUrl);
    return Center(
      child: Container(
        width: 420,
        padding: const EdgeInsets.all(32),
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: AppColors.border),
        ),
        child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.start, children: [
          const Row(children: [
            Icon(Icons.cloud_outlined, color: AppColors.primary, size: 22),
            SizedBox(width: 12),
            Text('Connect to Agent Server', style: TextStyle(color: AppColors.textPrimary,
                fontSize: 16, fontWeight: FontWeight.w700)),
          ]),
          const SizedBox(height: 20),
          const Text('Server URL', style: TextStyle(color: AppColors.textSecondary,
              fontSize: 12, fontWeight: FontWeight.w600)),
          const SizedBox(height: 6),
          TextField(controller: urlCtrl,
            style: const TextStyle(color: AppColors.textPrimary),
            decoration: const InputDecoration(hintText: 'http://localhost:3001')),
          const SizedBox(height: 12),
          _codeHint('Start the server:\n  cd agent_server\n  dart run bin/server.dart'),
          const SizedBox(height: 16),
          if (cp.error != null) ...[
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
              decoration: BoxDecoration(
                color: AppColors.error.withOpacity(0.08),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: AppColors.error.withOpacity(0.3)),
              ),
              child: Row(children: [
                const Icon(Icons.error_outline, size: 14, color: AppColors.error),
                const SizedBox(width: 8),
                Expanded(child: Text(cp.error!, style: const TextStyle(color: AppColors.error, fontSize: 12))),
              ]),
            ),
            const SizedBox(height: 16),
          ],
          SizedBox(
            width: double.infinity,
            child: ElevatedButton(
              onPressed: cp.connecting ? null : () => cp.connect(urlCtrl.text.trim()),
              child: cp.connecting
                  ? const SizedBox(width: 16, height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white))
                  : const Text('Connect'),
            ),
          ),
        ]),
      ),
    );
  }

  Widget _sidebar(CloudProvider cp) {
    return Container(
      width: 240,
      color: AppColors.surface,
      child: Column(children: [
        // Header
        Container(
          padding: const EdgeInsets.fromLTRB(16, 14, 8, 14),
          decoration: const BoxDecoration(border: Border(bottom: BorderSide(color: AppColors.border))),
          child: Row(children: [
            const Icon(Icons.cloud_done_outlined, size: 14, color: AppColors.statusDone),
            const SizedBox(width: 8),
            Expanded(child: Text(cp.serverUrl,
                style: const TextStyle(color: AppColors.textSecondary, fontSize: 11),
                overflow: TextOverflow.ellipsis)),
            IconButton(
              icon: const Icon(Icons.link_off, size: 14, color: AppColors.textMuted),
              tooltip: 'Disconnect',
              padding: EdgeInsets.zero,
              constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
              onPressed: cp.disconnect,
            ),
          ]),
        ),
        // Agent list
        Expanded(
          child: cp.agents.isEmpty
              ? const Center(child: Padding(
                  padding: EdgeInsets.all(16),
                  child: Text('No agents yet.\nClick + to create one.',
                      textAlign: TextAlign.center,
                      style: TextStyle(color: AppColors.textMuted, fontSize: 12))))
              : ListView(children: cp.agents.map((a) => _agentTile(a, cp)).toList()),
        ),
        // Add button
        Container(
          padding: const EdgeInsets.all(12),
          decoration: const BoxDecoration(border: Border(top: BorderSide(color: AppColors.border))),
          child: SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              icon: const Icon(Icons.add, size: 16),
              label: const Text('New Cloud Agent'),
              onPressed: () => _showCreateAgent(cp),
            ),
          ),
        ),
      ]),
    );
  }

  Widget _agentTile(CloudAgent a, CloudProvider cp) {
    final sel = _selectedAgentId == a.id;
    return ListTile(
      selected: sel,
      selectedTileColor: AppColors.primary.withOpacity(0.08),
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 2),
      leading: Container(
        width: 32, height: 32,
        decoration: BoxDecoration(
          color: AppColors.secondary.withOpacity(0.15),
          borderRadius: BorderRadius.circular(8),
        ),
        child: const Icon(Icons.cloud, size: 16, color: AppColors.secondary),
      ),
      title: Text(a.name, style: TextStyle(
          color: sel ? AppColors.primary : AppColors.textPrimary,
          fontSize: 13, fontWeight: FontWeight.w600)),
      subtitle: Text('${a.provider} / ${a.model}',
          style: const TextStyle(color: AppColors.textMuted, fontSize: 10),
          overflow: TextOverflow.ellipsis),
      onTap: () => setState(() => _selectedAgentId = a.id),
      trailing: IconButton(
        icon: const Icon(Icons.delete_outline, size: 14, color: AppColors.textMuted),
        padding: EdgeInsets.zero,
        constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
        onPressed: () async {
          await cp.deleteAgent(a.id);
          if (_selectedAgentId == a.id) setState(() => _selectedAgentId = null);
        },
      ),
    );
  }

  Widget _emptyState(CloudProvider cp) {
    return Center(
      child: Column(mainAxisSize: MainAxisSize.min, children: [
        const Icon(Icons.smart_toy_outlined, size: 48, color: AppColors.textMuted),
        const SizedBox(height: 16),
        const Text('Select an agent or create a new one',
            style: TextStyle(color: AppColors.textSecondary, fontSize: 14)),
        const SizedBox(height: 20),
        ElevatedButton.icon(
          icon: const Icon(Icons.add, size: 16),
          label: const Text('Create Cloud Agent'),
          onPressed: () => _showCreateAgent(cp),
        ),
      ]),
    );
  }

  Widget _chatPanel(CloudProvider cp, String agentId) {
    final agent = cp.agents.firstWhere((a) => a.id == agentId,
        orElse: () => CloudAgent(id: '', name: '', systemPrompt: '', provider: '',
            model: '', baseUrl: '', apiKey: ''));
    final history = cp.historyFor(agentId);
    final running = cp.isRunning(agentId);
    final inputCtrl = TextEditingController();

    return Column(children: [
      // Chat header
      Container(
        height: 48,
        padding: const EdgeInsets.symmetric(horizontal: 20),
        decoration: const BoxDecoration(
          color: AppColors.surface,
          border: Border(bottom: BorderSide(color: AppColors.border)),
        ),
        child: Row(children: [
          const Icon(Icons.cloud, size: 16, color: AppColors.secondary),
          const SizedBox(width: 10),
          Text(agent.name, style: const TextStyle(color: AppColors.textPrimary,
              fontSize: 14, fontWeight: FontWeight.w600)),
          const SizedBox(width: 8),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
            decoration: BoxDecoration(
              color: AppColors.secondary.withOpacity(0.12),
              borderRadius: BorderRadius.circular(4),
            ),
            child: Text('${agent.provider} / ${agent.model}',
                style: const TextStyle(color: AppColors.secondary, fontSize: 10,
                    fontWeight: FontWeight.w600)),
          ),
        ]),
      ),
      // Messages
      Expanded(
        child: history.isEmpty
            ? const Center(child: Text('Send a message to start',
                style: TextStyle(color: AppColors.textMuted)))
            : ListView.builder(
                padding: const EdgeInsets.all(16),
                itemCount: history.length,
                itemBuilder: (_, i) => _messageBubble(history[i]),
              ),
      ),
      // Input
      Container(
        padding: const EdgeInsets.fromLTRB(16, 8, 16, 16),
        decoration: const BoxDecoration(
          color: AppColors.surface,
          border: Border(top: BorderSide(color: AppColors.border)),
        ),
        child: Row(children: [
          Expanded(
            child: TextField(
              controller: inputCtrl,
              style: const TextStyle(color: AppColors.textPrimary),
              decoration: const InputDecoration(hintText: 'Send a message…'),
              onSubmitted: running ? null : (v) {
                if (v.trim().isNotEmpty) {
                  cp.sendMessage(agentId, v.trim());
                  inputCtrl.clear();
                }
              },
            ),
          ),
          const SizedBox(width: 10),
          ElevatedButton(
            onPressed: running ? null : () {
              final t = inputCtrl.text.trim();
              if (t.isNotEmpty) { cp.sendMessage(agentId, t); inputCtrl.clear(); }
            },
            child: running
                ? const SizedBox(width: 16, height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white))
                : const Text('Send'),
          ),
        ]),
      ),
    ]);
  }

  Widget _messageBubble(CloudChatMessage m) {
    final isUser = m.role == 'user';
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Row(
        mainAxisAlignment: isUser ? MainAxisAlignment.end : MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (!isUser) ...[
            Container(
              width: 28, height: 28,
              margin: const EdgeInsets.only(right: 8, top: 2),
              decoration: BoxDecoration(
                color: AppColors.secondary.withOpacity(0.15),
                borderRadius: BorderRadius.circular(8),
              ),
              child: const Icon(Icons.cloud, size: 14, color: AppColors.secondary),
            ),
          ],
          Flexible(
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
              decoration: BoxDecoration(
                color: isUser ? AppColors.primary.withOpacity(0.15) : AppColors.surface,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: isUser ? AppColors.primary.withOpacity(0.3) : AppColors.border),
              ),
              child: m.isStreaming
                  ? const SizedBox(width: 16, height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2, color: AppColors.secondary))
                  : SelectableText(m.content,
                      style: const TextStyle(color: AppColors.textPrimary, fontSize: 13, height: 1.5)),
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _showCreateAgent(CloudProvider cp) async {
    await showDialog(context: context, builder: (_) => _CreateAgentDialog(cp: cp));
  }

  Widget _codeHint(String text) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.border),
      ),
      child: Text(text, style: const TextStyle(
          color: AppColors.accent, fontSize: 11, fontFamily: 'monospace', height: 1.7)),
    );
  }
}

class _CreateAgentDialog extends StatefulWidget {
  final CloudProvider cp;
  const _CreateAgentDialog({required this.cp});
  @override
  State<_CreateAgentDialog> createState() => _CreateAgentDialogState();
}

class _CreateAgentDialogState extends State<_CreateAgentDialog> {
  final _nameCtrl   = TextEditingController(text: 'My Agent');
  final _sysCtrl    = TextEditingController(text: 'You are a helpful assistant.');
  final _modelCtrl  = TextEditingController(text: 'claude-sonnet-4-6');
  final _urlCtrl    = TextEditingController();
  final _keyCtrl    = TextEditingController();
  String _provider  = 'anthropic';
  bool _obscureKey  = true;
  bool _saving      = false;

  static const _providers = [
    'anthropic', 'openai', 'gemini', 'ollama', 'vllm',
    'mistral', 'groq', 'together', 'xai', 'perplexity', 'custom',
  ];

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: AppColors.surface,
      shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: const BorderSide(color: AppColors.border)),
      child: SizedBox(
        width: 480,
        child: SingleChildScrollView(
          child: Column(mainAxisSize: MainAxisSize.min, children: [
            // Header
            Container(
              padding: const EdgeInsets.fromLTRB(24, 20, 16, 16),
              decoration: const BoxDecoration(border: Border(bottom: BorderSide(color: AppColors.border))),
              child: Row(children: [
                const Icon(Icons.cloud_upload_outlined, color: AppColors.primary, size: 20),
                const SizedBox(width: 12),
                const Expanded(child: Text('Create Cloud Agent', style: TextStyle(
                    color: AppColors.textPrimary, fontSize: 16, fontWeight: FontWeight.w600))),
                IconButton(icon: const Icon(Icons.close, size: 18, color: AppColors.textMuted),
                    onPressed: () => Navigator.pop(context)),
              ]),
            ),
            Padding(
              padding: const EdgeInsets.all(24),
              child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                _label('Name'),
                const SizedBox(height: 6),
                TextField(controller: _nameCtrl,
                    style: const TextStyle(color: AppColors.textPrimary),
                    decoration: const InputDecoration(hintText: 'Agent name')),
                const SizedBox(height: 14),
                _label('System Prompt'),
                const SizedBox(height: 6),
                TextField(controller: _sysCtrl,
                    maxLines: 3,
                    style: const TextStyle(color: AppColors.textPrimary),
                    decoration: const InputDecoration(hintText: 'You are a helpful assistant.')),
                const SizedBox(height: 14),
                _label('Provider'),
                const SizedBox(height: 8),
                Wrap(spacing: 6, runSpacing: 6, children: _providers.map((p) {
                  final sel = _provider == p;
                  return GestureDetector(
                    onTap: () {
                      setState(() {
                        _provider = p;
                        if (p == 'vllm') _urlCtrl.text = 'http://localhost:8000';
                        if (p == 'ollama') { _urlCtrl.text = 'http://localhost:11434'; _modelCtrl.text = 'llama3'; }
                        if (p == 'anthropic') { _urlCtrl.clear(); _modelCtrl.text = 'claude-sonnet-4-6'; }
                        if (p == 'openai') { _urlCtrl.clear(); _modelCtrl.text = 'gpt-4o'; }
                      });
                    },
                    child: AnimatedContainer(
                      duration: const Duration(milliseconds: 120),
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
                      decoration: BoxDecoration(
                        color: sel ? AppColors.primary.withOpacity(0.12) : AppColors.surfaceAlt,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(
                            color: sel ? AppColors.primary : AppColors.border,
                            width: sel ? 1.5 : 1),
                      ),
                      child: Text(p, style: TextStyle(
                          color: sel ? AppColors.primary : AppColors.textSecondary,
                          fontSize: 12, fontWeight: sel ? FontWeight.w600 : FontWeight.normal)),
                    ),
                  );
                }).toList()),
                const SizedBox(height: 14),
                _label('Model'),
                const SizedBox(height: 6),
                TextField(controller: _modelCtrl,
                    style: const TextStyle(color: AppColors.textPrimary),
                    decoration: const InputDecoration(hintText: 'claude-sonnet-4-6')),
                if (_provider != 'anthropic' && _provider != 'openai' &&
                    _provider != 'mistral' && _provider != 'groq') ...[
                  const SizedBox(height: 14),
                  _label('Base URL'),
                  const SizedBox(height: 6),
                  TextField(controller: _urlCtrl,
                      style: const TextStyle(color: AppColors.textPrimary),
                      decoration: InputDecoration(
                          hintText: _provider == 'vllm' ? 'http://your-server:8000'
                              : _provider == 'ollama' ? 'http://localhost:11434'
                              : 'https://api.example.com')),
                ],
                if (_provider != 'ollama') ...[
                  const SizedBox(height: 14),
                  _label('API Key'),
                  const SizedBox(height: 6),
                  TextField(
                    controller: _keyCtrl,
                    obscureText: _obscureKey,
                    style: const TextStyle(color: AppColors.textPrimary),
                    decoration: InputDecoration(
                      hintText: 'sk-…',
                      suffixIcon: IconButton(
                        icon: Icon(_obscureKey ? Icons.visibility_outlined : Icons.visibility_off_outlined,
                            size: 16, color: AppColors.textMuted),
                        onPressed: () => setState(() => _obscureKey = !_obscureKey),
                      ),
                    ),
                  ),
                ],
              ]),
            ),
            // Footer
            Container(
              padding: const EdgeInsets.fromLTRB(24, 12, 24, 20),
              decoration: const BoxDecoration(border: Border(top: BorderSide(color: AppColors.border))),
              child: Row(mainAxisAlignment: MainAxisAlignment.end, children: [
                TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancel')),
                const SizedBox(width: 8),
                ElevatedButton(
                  onPressed: _saving ? null : _submit,
                  child: _saving
                      ? const SizedBox(width: 16, height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white))
                      : const Text('Create'),
                ),
              ]),
            ),
          ]),
        ),
      ),
    );
  }

  Future<void> _submit() async {
    setState(() => _saving = true);
    final agent = CloudAgent(
      id: '', name: _nameCtrl.text.trim(),
      systemPrompt: _sysCtrl.text.trim(),
      provider: _provider, model: _modelCtrl.text.trim(),
      baseUrl: _urlCtrl.text.trim(), apiKey: _keyCtrl.text.trim(),
    );
    final created = await widget.cp.createAgent(agent);
    if (mounted) Navigator.pop(context, created);
  }

  Widget _label(String t) => Text(t, style: const TextStyle(color: AppColors.textSecondary,
      fontSize: 12, fontWeight: FontWeight.w600));
}
