import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';
import 'package:provider/provider.dart';
import '../models/agent_model.dart';
import '../models/task_model.dart';
import '../providers/agent_provider.dart';
import '../providers/auth_provider.dart';
import '../theme/app_theme.dart';
import '../widgets/chat_panel.dart';
import '../widgets/status_badge.dart';
import 'agent_builder_dialog.dart';

class UserShell extends StatefulWidget {
  const UserShell({super.key});

  @override
  State<UserShell> createState() => _UserShellState();
}

class _UserShellState extends State<UserShell> {
  String? _selectedAgentId;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      body: Column(
        children: [
          _topBar(),
          Expanded(
            child: Row(
              children: [
                _sidebar(),
                const VerticalDivider(width: 1, color: AppColors.border),
                Expanded(child: _mainArea()),
              ],
            ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => showDialog(
          context: context,
          builder: (_) => const AgentBuilderDialog(),
        ),
        backgroundColor: AppColors.primary,
        icon: const Icon(Icons.add, color: Colors.white),
        label: const Text('New Agent',
          style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600)),
      ),
    );
  }

  Widget _topBar() {
    final auth = context.watch<AuthProvider>();
    return Container(
      height: 48,
      padding: const EdgeInsets.symmetric(horizontal: 20),
      decoration: const BoxDecoration(
        color: AppColors.surface,
        border: Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          const Icon(Icons.smart_toy, color: AppColors.accent, size: 18),
          const SizedBox(width: 10),
          const Text('Agent Studio',
            style: TextStyle(color: AppColors.textPrimary,
                fontSize: 14, fontWeight: FontWeight.w700)),
          const SizedBox(width: 12),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
            decoration: BoxDecoration(
              color: AppColors.accent.withOpacity(0.12),
              borderRadius: BorderRadius.circular(4),
            ),
            child: const Text('USER',
              style: TextStyle(color: AppColors.accent, fontSize: 9,
                  fontWeight: FontWeight.w800, letterSpacing: 1)),
          ),
          const Spacer(),
          Text(auth.session?.username ?? '',
            style: const TextStyle(color: AppColors.textSecondary, fontSize: 12)),
          const SizedBox(width: 8),
          IconButton(
            icon: const Icon(Icons.logout, size: 18, color: AppColors.textMuted),
            tooltip: 'Sign out',
            onPressed: () => context.read<AuthProvider>().logout(),
          ),
        ],
      ),
    );
  }

  Widget _sidebar() {
    final agents = context.watch<AgentProvider>().agents;
    return Container(
      width: 220,
      color: AppColors.surface,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
            child: Text('MY AGENTS',
              style: const TextStyle(color: AppColors.textMuted, fontSize: 10,
                  fontWeight: FontWeight.w700, letterSpacing: 1.2)),
          ),
          if (agents.isEmpty)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Text('No agents yet.\nTap + New Agent to create one.',
                style: const TextStyle(color: AppColors.textMuted, fontSize: 12, height: 1.5)),
            )
          else
            Expanded(
              child: ListView.builder(
                padding: const EdgeInsets.symmetric(vertical: 4),
                itemCount: agents.length,
                itemBuilder: (_, i) => _agentTile(agents[i]),
              ),
            ),
        ],
      ),
    );
  }

  Widget _agentTile(AgentModel agent) {
    final sel = _selectedAgentId == agent.id;
    return GestureDetector(
      onTap: () {
        setState(() => _selectedAgentId = agent.id);
        context.read<AgentProvider>().openConversation(agent.id);
      },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 120),
        margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        decoration: BoxDecoration(
          color: sel ? AppColors.primary.withOpacity(0.12) : Colors.transparent,
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
            color: sel ? AppColors.primary.withOpacity(0.4) : Colors.transparent,
          ),
        ),
        child: Row(
          children: [
            Container(
              width: 8, height: 8,
              decoration: BoxDecoration(color: agent.color, shape: BoxShape.circle),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(agent.name,
                    style: TextStyle(
                      color: sel ? AppColors.primary : AppColors.textPrimary,
                      fontSize: 13, fontWeight: FontWeight.w500,
                    ),
                    overflow: TextOverflow.ellipsis),
                  Text(agent.role,
                    style: const TextStyle(color: AppColors.textMuted, fontSize: 10)),
                ],
              ),
            ),
            StatusBadge(status: agent.status),
          ],
        ),
      ),
    );
  }

  Widget _mainArea() {
    if (_selectedAgentId != null) {
      final agent = context.read<AgentProvider>().agentById(_selectedAgentId!);
      if (agent != null) {
        return const ChatPanel();
      }
    }
    return _welcome();
  }

  Widget _welcome() {
    final prov   = context.watch<AgentProvider>();
    final agents = prov.agents;
    final tasks  = prov.tasks;
    final done   = tasks.where((t) => t.status == TaskStatus.done).length;
    final running = tasks.where((t) => t.isActive).length;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(32),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Welcome back', style: const TextStyle(
            color: AppColors.textMuted, fontSize: 13)),
          const SizedBox(height: 4),
          Text(context.read<AuthProvider>().session?.username ?? 'User',
            style: const TextStyle(color: AppColors.textPrimary,
                fontSize: 28, fontWeight: FontWeight.w700)),
          const SizedBox(height: 32),

          // Stats row
          Row(
            children: [
              _statCard(Icons.smart_toy_outlined, '${agents.length}',
                  'Agents', AppColors.primary),
              const SizedBox(width: 16),
              _statCard(Icons.check_circle_outline, '$done',
                  'Tasks Done', AppColors.statusDone),
              const SizedBox(width: 16),
              _statCard(Icons.hourglass_top_outlined, '$running',
                  'Running', AppColors.warning),
            ],
          ),

          const SizedBox(height: 40),

          if (agents.isEmpty) ...[
            Center(
              child: Column(
                children: [
                  Container(
                    padding: const EdgeInsets.all(24),
                    decoration: BoxDecoration(
                      color: AppColors.primary.withOpacity(0.08),
                      shape: BoxShape.circle,
                    ),
                    child: const Icon(Icons.smart_toy_outlined,
                        size: 48, color: AppColors.primary),
                  ),
                  const SizedBox(height: 20),
                  const Text('No agents yet',
                    style: TextStyle(color: AppColors.textPrimary,
                        fontSize: 18, fontWeight: FontWeight.w600)),
                  const SizedBox(height: 8),
                  const Text('Create your first agent to get started.',
                    style: TextStyle(color: AppColors.textMuted, fontSize: 13)),
                  const SizedBox(height: 24),
                  ElevatedButton.icon(
                    icon: const Icon(Icons.add, size: 18),
                    label: const Text('Create Agent'),
                    onPressed: () => showDialog(
                      context: context,
                      builder: (_) => const AgentBuilderDialog(),
                    ),
                  ),
                ],
              ).animate().fadeIn(duration: 400.ms).slideY(begin: 0.05, end: 0),
            ),
          ] else ...[
            const Text('YOUR AGENTS',
              style: TextStyle(color: AppColors.textMuted, fontSize: 10,
                  fontWeight: FontWeight.w700, letterSpacing: 1.2)),
            const SizedBox(height: 12),
            GridView.builder(
              shrinkWrap: true,
              physics: const NeverScrollableScrollPhysics(),
              gridDelegate: const SliverGridDelegateWithMaxCrossAxisExtent(
                maxCrossAxisExtent: 280,
                crossAxisSpacing: 12,
                mainAxisSpacing: 12,
                childAspectRatio: 1.8,
              ),
              itemCount: agents.length,
              itemBuilder: (_, i) => _agentCard(agents[i]),
            ),
          ],

          if (tasks.isNotEmpty) ...[
            const SizedBox(height: 40),
            const Text('RECENT TASKS',
              style: TextStyle(color: AppColors.textMuted, fontSize: 10,
                  fontWeight: FontWeight.w700, letterSpacing: 1.2)),
            const SizedBox(height: 12),
            ...tasks.take(5).map(_taskRow),
          ],
        ],
      ),
    );
  }

  Widget _statCard(IconData icon, String value, String label, Color color) {
    return Expanded(
      child: Container(
        padding: const EdgeInsets.all(20),
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(14),
          border: Border.all(color: AppColors.border),
        ),
        child: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: color.withOpacity(0.12),
                borderRadius: BorderRadius.circular(10),
              ),
              child: Icon(icon, size: 20, color: color),
            ),
            const SizedBox(width: 14),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(value,
                  style: TextStyle(color: color, fontSize: 24,
                      fontWeight: FontWeight.w700)),
                Text(label,
                  style: const TextStyle(color: AppColors.textMuted, fontSize: 11)),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _agentCard(AgentModel agent) {
    return GestureDetector(
      onTap: () {
        setState(() => _selectedAgentId = agent.id);
        context.read<AgentProvider>().openConversation(agent.id);
      },
      child: Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: AppColors.border),
        ),
        child: Row(
          children: [
            Container(
              width: 10, height: 10,
              decoration: BoxDecoration(color: agent.color, shape: BoxShape.circle),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(agent.name,
                    style: const TextStyle(color: AppColors.textPrimary,
                        fontSize: 14, fontWeight: FontWeight.w600)),
                  Text(agent.role,
                    style: const TextStyle(color: AppColors.textMuted, fontSize: 11)),
                ],
              ),
            ),
            Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                StatusBadge(status: agent.status),
                const SizedBox(height: 6),
                const Icon(Icons.chevron_right, size: 14, color: AppColors.textMuted),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _taskRow(TaskModel task) {
    Color statusColor;
    IconData statusIcon;
    switch (task.status) {
      case TaskStatus.done:
        statusColor = AppColors.statusDone; statusIcon = Icons.check_circle_outline; break;
      case TaskStatus.error:
        statusColor = AppColors.error; statusIcon = Icons.error_outline; break;
      case TaskStatus.running:
        statusColor = AppColors.primary; statusIcon = Icons.hourglass_top_outlined; break;
      default:
        statusColor = AppColors.textMuted; statusIcon = Icons.radio_button_unchecked;
    }
    return Container(
      margin: const EdgeInsets.only(bottom: 6),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: AppColors.surface,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppColors.border),
      ),
      child: Row(
        children: [
          Icon(statusIcon, size: 14, color: statusColor),
          const SizedBox(width: 10),
          Expanded(
            child: Text(task.prompt,
              style: const TextStyle(color: AppColors.textPrimary, fontSize: 12),
              overflow: TextOverflow.ellipsis),
          ),
          Text(
            task.completedAt != null
                ? _timeAgo(task.completedAt!)
                : task.startedAt != null ? 'Running...' : 'Pending',
            style: TextStyle(color: statusColor, fontSize: 10),
          ),
        ],
      ),
    );
  }

  String _timeAgo(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inMinutes < 1) return 'just now';
    if (diff.inHours < 1) return '${diff.inMinutes}m ago';
    if (diff.inDays < 1) return '${diff.inHours}h ago';
    return '${diff.inDays}d ago';
  }
}
