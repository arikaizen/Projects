import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:flutter_animate/flutter_animate.dart';
import '../models/agent_model.dart';
import '../providers/agent_provider.dart';
import '../screens/settings_panel.dart';
import '../theme/app_theme.dart';
import 'status_badge.dart';

class ChatPanel extends StatefulWidget {
  const ChatPanel({super.key});

  @override
  State<ChatPanel> createState() => _ChatPanelState();
}

class _ChatPanelState extends State<ChatPanel> {
  final _ctrl   = TextEditingController();
  final _scroll = ScrollController();
  bool  _sending = false;

  @override
  void dispose() {
    _ctrl.dispose();
    _scroll.dispose();
    super.dispose();
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scroll.hasClients) {
        _scroll.animateTo(
          _scroll.position.maxScrollExtent,
          duration: const Duration(milliseconds: 300),
          curve: Curves.easeOut,
        );
      }
    });
  }

  Future<void> _send() async {
    final text = _ctrl.text.trim();
    if (text.isEmpty || _sending) return;

    final prov = context.read<AgentProvider>();
    final id   = prov.activeConvId;
    if (id == null) return;

    setState(() => _sending = true);
    _ctrl.clear();

    await prov.sendMessage(id, text, isGroup: prov.activeConvIsGroup);

    if (mounted) setState(() => _sending = false);
    _scrollToBottom();
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<AgentProvider>(
      builder: (context, prov, _) {
        final isGroup = prov.activeConvIsGroup;
        final agent   = prov.activeAgent;
        final group   = prov.activeGroup;

        if (agent == null && group == null) {
          return _placeholder();
        }

        final name     = agent?.name ?? group?.name ?? '';
        final messages = agent?.history ?? group?.sharedHistory ?? [];
        final status   = agent?.status ?? AgentStatus.idle;

        _scrollToBottom();

        return Column(
          children: [
            _header(name, status, agent, isGroup, prov),
            Expanded(
              child: messages.isEmpty
                  ? _emptyState(name)
                  : ListView.builder(
                      controller: _scroll,
                      padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
                      itemCount: messages.length,
                      itemBuilder: (_, i) => _bubble(messages[i], prov),
                    ),
            ),
            if (!isGroup && agent != null && !prov.providerReadyFor(agent))
              _noProviderBanner(context),
            _inputBar(),
          ],
        );
      },
    );
  }

  Widget _header(
    String name,
    AgentStatus status,
    AgentModel? agent,
    bool isGroup,
    AgentProvider prov,
  ) {
    return Container(
      padding: const EdgeInsets.fromLTRB(16, 12, 8, 12),
      decoration: const BoxDecoration(
        border: Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          Container(
            width: 32,
            height: 32,
            decoration: BoxDecoration(
              color: (agent?.color ?? AppColors.secondary).withOpacity(0.15),
              borderRadius: BorderRadius.circular(8),
              border: Border.all(
                color: (agent?.color ?? AppColors.secondary).withOpacity(0.4),
              ),
            ),
            child: Center(
              child: Icon(
                isGroup ? Icons.group : Icons.smart_toy,
                size: 16,
                color: agent?.color ?? AppColors.secondary,
              ),
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(name, style: Theme.of(context).textTheme.titleMedium),
                const SizedBox(height: 1),
                StatusBadge(status: status, showLabel: true),
              ],
            ),
          ),
          IconButton(
            icon: const Icon(Icons.delete_sweep_outlined, size: 18),
            tooltip: 'Clear history',
            color: AppColors.textMuted,
            onPressed: () => prov.clearHistory(
              prov.activeConvId!,
              isGroup: isGroup,
            ),
          ),
          IconButton(
            icon: const Icon(Icons.close, size: 18),
            color: AppColors.textMuted,
            onPressed: prov.closeConversation,
          ),
        ],
      ),
    );
  }

  Widget _bubble(ChatMessage msg, AgentProvider prov) {
    final isUser = msg.isUser;

    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.end,
        mainAxisAlignment: isUser ? MainAxisAlignment.end : MainAxisAlignment.start,
        children: [
          if (!isUser) ...[
            _agentAvatar(msg, prov),
            const SizedBox(width: 8),
          ],
          Flexible(
            child: GestureDetector(
              onLongPress: () => _copyMessage(msg.content),
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
                constraints: const BoxConstraints(maxWidth: 520),
                decoration: BoxDecoration(
                  color: isUser ? AppColors.primary.withOpacity(0.2) : AppColors.card,
                  borderRadius: BorderRadius.only(
                    topLeft:     const Radius.circular(14),
                    topRight:    const Radius.circular(14),
                    bottomLeft:  Radius.circular(isUser ? 14 : 4),
                    bottomRight: Radius.circular(isUser ? 4 : 14),
                  ),
                  border: Border.all(
                    color: isUser
                        ? AppColors.primary.withOpacity(0.3)
                        : AppColors.border,
                  ),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    _renderContent(msg.content),
                    const SizedBox(height: 4),
                    Text(
                      _formatTime(msg.timestamp),
                      style: const TextStyle(
                        color: AppColors.textMuted,
                        fontSize: 10,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
          if (isUser) const SizedBox(width: 8),
        ],
      ),
    ).animate().fadeIn(duration: 200.ms).slideY(begin: 0.1, end: 0);
  }

  Widget _agentAvatar(ChatMessage msg, AgentProvider prov) {
    final agent = msg.agentId != null ? prov.agentById(msg.agentId!) : null;
    final color = agent?.color ?? AppColors.secondary;
    final letter = agent?.name.isNotEmpty == true ? agent!.name[0].toUpperCase() : 'A';
    return Container(
      width: 28,
      height: 28,
      decoration: BoxDecoration(
        color: color.withOpacity(0.15),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color.withOpacity(0.4)),
      ),
      child: Center(
        child: Text(letter, style: TextStyle(color: color, fontSize: 12, fontWeight: FontWeight.w700)),
      ),
    );
  }

  Widget _renderContent(String content) {
    if (content.contains('```')) {
      final parts = content.split('```');
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: List.generate(parts.length, (i) {
          if (i.isOdd) {
            final lines = parts[i].split('\n');
            final lang   = lines.first.trim();
            final code   = lines.skip(1).join('\n');
            return Container(
              width: double.infinity,
              margin: const EdgeInsets.symmetric(vertical: 6),
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: AppColors.background,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: AppColors.border),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  if (lang.isNotEmpty) Text(lang,
                    style: const TextStyle(color: AppColors.textMuted, fontSize: 10)),
                  Text(code,
                    style: const TextStyle(
                      color: AppColors.accent,
                      fontSize: 12,
                      fontFamily: 'monospace',
                    )),
                ],
              ),
            );
          }
          return _markdownText(parts[i]);
        }),
      );
    }
    return _markdownText(content);
  }

  Widget _markdownText(String text) {
    return Text(
      text,
      style: const TextStyle(color: AppColors.textPrimary, fontSize: 14, height: 1.5),
    );
  }

  Widget _inputBar() {
    return Container(
      padding: const EdgeInsets.fromLTRB(12, 8, 12, 12),
      decoration: const BoxDecoration(
        border: Border(top: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          Expanded(
            child: TextField(
              controller: _ctrl,
              maxLines: 4,
              minLines: 1,
              style: const TextStyle(color: AppColors.textPrimary, fontSize: 14),
              decoration: InputDecoration(
                hintText: 'Send a message or assign a task…',
                hintStyle: const TextStyle(color: AppColors.textMuted),
                fillColor: AppColors.surfaceAlt,
                filled: true,
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                  borderSide: const BorderSide(color: AppColors.border),
                ),
                enabledBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                  borderSide: const BorderSide(color: AppColors.border),
                ),
                focusedBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                  borderSide: const BorderSide(color: AppColors.primary, width: 1.5),
                ),
                contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
              ),
              onSubmitted: (_) => _send(),
              textInputAction: TextInputAction.send,
            ),
          ),
          const SizedBox(width: 8),
          AnimatedContainer(
            duration: const Duration(milliseconds: 200),
            child: _sending
                ? Container(
                    width: 44,
                    height: 44,
                    decoration: BoxDecoration(
                      color: AppColors.primary.withOpacity(0.3),
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: const Center(
                      child: SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          valueColor: AlwaysStoppedAnimation(AppColors.primary),
                        ),
                      ),
                    ),
                  )
                : ElevatedButton(
                    onPressed: _send,
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AppColors.primary,
                      minimumSize: const Size(44, 44),
                      padding: EdgeInsets.zero,
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(12),
                      ),
                    ),
                    child: const Icon(Icons.send_rounded, size: 18),
                  ),
          ),
        ],
      ),
    );
  }

  Widget _noProviderBanner(BuildContext context) {
    return Container(
      margin: const EdgeInsets.fromLTRB(12, 0, 12, 6),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: AppColors.warning.withOpacity(0.08),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppColors.warning.withOpacity(0.35)),
      ),
      child: Row(
        children: [
          const Icon(Icons.warning_amber_rounded,
              size: 16, color: AppColors.warning),
          const SizedBox(width: 10),
          const Expanded(
            child: Text(
              'No LLM provider connected — responses will be mocked.\n'
              'Add a provider in Settings to use a real model.',
              style: TextStyle(
                  color: AppColors.warning, fontSize: 11, height: 1.4),
            ),
          ),
          const SizedBox(width: 8),
          TextButton(
            style: TextButton.styleFrom(
              foregroundColor: AppColors.warning,
              padding:
                  const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
              side: BorderSide(color: AppColors.warning.withOpacity(0.5)),
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(8)),
            ),
            onPressed: () => showDialog(
              context: context,
              builder: (_) => const SettingsPanel(),
            ),
            child: const Text('Open Settings',
                style: TextStyle(fontSize: 11, fontWeight: FontWeight.w600)),
          ),
        ],
      ),
    );
  }

  Widget _placeholder() {
    return const Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.chat_bubble_outline, size: 48, color: AppColors.textMuted),
          SizedBox(height: 12),
          Text('Select an agent or group to chat',
            style: TextStyle(color: AppColors.textMuted, fontSize: 14)),
        ],
      ),
    );
  }

  Widget _emptyState(String name) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.smart_toy_outlined, size: 48, color: AppColors.textMuted),
          const SizedBox(height: 12),
          Text('Start chatting with $name',
            style: const TextStyle(color: AppColors.textSecondary, fontSize: 15)),
          const SizedBox(height: 6),
          const Text('Type a message or assign a task below',
            style: TextStyle(color: AppColors.textMuted, fontSize: 12)),
        ],
      ),
    );
  }

  void _copyMessage(String text) {
    Clipboard.setData(ClipboardData(text: text));
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('Copied to clipboard'),
        duration: Duration(seconds: 2),
        backgroundColor: AppColors.card,
      ),
    );
  }

  String _formatTime(DateTime dt) {
    final h = dt.hour.toString().padLeft(2, '0');
    final m = dt.minute.toString().padLeft(2, '0');
    return '$h:$m';
  }
}
