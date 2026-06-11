import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_animate/flutter_animate.dart';
import '../models/agent_model.dart';
import '../models/agent_group.dart';
import '../models/task_model.dart';
import '../providers/agent_provider.dart';
import '../providers/auth_provider.dart';
import '../theme/app_theme.dart';
import '../widgets/agent_card.dart';
import '../widgets/chat_panel.dart';
import '../widgets/connect_model_dialog.dart';
import '../widgets/hierarchy_tree.dart';
import 'agent_builder_dialog.dart';
import 'group_builder_dialog.dart';
import 'settings_panel.dart';

enum _Tab { agents, groups, hierarchy, tasks, logs }

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  _Tab _tab = _Tab.agents;

  @override
  Widget build(BuildContext context) {
    return Consumer<AgentProvider>(
      builder: (ctx, prov, _) {
        final showChat = prov.activeConvId != null;
        return Scaffold(
          backgroundColor: AppColors.background,
          body: Row(
            children: [
              _Sidebar(
                current: _tab,
                onTab: (t) => setState(() => _tab = t),
                prov: prov,
              ),
              Expanded(
                child: Column(
                  children: [
                    _TopBar(tab: _tab, prov: prov),
                    Expanded(
                      child: Row(
                        children: [
                          Expanded(child: _tabContent(prov)),
                          if (showChat)
                            Container(
                              width: 380,
                              decoration: const BoxDecoration(
                                border: Border(
                                  left: BorderSide(color: AppColors.border),
                                ),
                              ),
                              child: const ChatPanel(),
                            ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _tabContent(AgentProvider prov) {
    switch (_tab) {
      case _Tab.agents:    return _AgentsTab(prov: prov);
      case _Tab.groups:    return _GroupsTab(prov: prov);
      case _Tab.hierarchy: return const HierarchyTree();
      case _Tab.tasks:     return _TasksTab(prov: prov);
      case _Tab.logs:      return _LogsTab(prov: prov);
    }
  }
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

class _Sidebar extends StatelessWidget {
  final _Tab current;
  final void Function(_Tab) onTab;
  final AgentProvider prov;

  const _Sidebar({required this.current, required this.onTab, required this.prov});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 64,
      color: AppColors.surface,
      child: Column(
        children: [
          const SizedBox(height: 16),
          // Logo
          Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(
              gradient: const LinearGradient(
                colors: [AppColors.primary, AppColors.secondary],
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
              ),
              borderRadius: BorderRadius.circular(12),
            ),
            child: const Icon(Icons.hub, color: Colors.white, size: 22),
          ),
          const SizedBox(height: 20),
          const Divider(color: AppColors.border, height: 1),
          const SizedBox(height: 12),
          _navItem(_Tab.agents,    Icons.smart_toy_outlined,  'Agents',
            badge: prov.runningCount > 0 ? '${prov.runningCount}' : null),
          _navItem(_Tab.groups,    Icons.group_outlined,      'Groups'),
          _navItem(_Tab.hierarchy, Icons.account_tree_outlined,'Tree'),
          _navItem(_Tab.tasks,     Icons.task_outlined,       'Tasks',
            badge: prov.activeTasks.isNotEmpty ? '${prov.activeTasks.length}' : null),
          _navItem(_Tab.logs,      Icons.receipt_long_outlined,'Logs'),
          const Spacer(),
          const Divider(color: AppColors.border, height: 1),
          Consumer<AuthProvider>(
            builder: (ctx, auth, _) => Tooltip(
              message: auth.session?.username ?? '',
              child: Container(
                margin: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                padding: const EdgeInsets.symmetric(vertical: 6),
                child: Column(
                  children: [
                    Text(
                      (auth.session?.username ?? '').isNotEmpty
                          ? (auth.session!.username[0].toUpperCase())
                          : '?',
                      style: const TextStyle(
                        color: AppColors.textSecondary,
                        fontSize: 12,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                    const SizedBox(height: 4),
                    GestureDetector(
                      onTap: () => ctx.read<AuthProvider>().logout(),
                      child: const Tooltip(
                        message: 'Sign out',
                        child: Icon(Icons.logout, size: 16, color: AppColors.textMuted),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
          const Divider(color: AppColors.border, height: 1),
          _settingsBtn(context),
          const SizedBox(height: 12),
        ],
      ),
    );
  }

  Widget _navItem(_Tab tab, IconData icon, String label, {String? badge}) {
    final active = current == tab;
    return Tooltip(
      message: label,
      preferBelow: false,
      child: GestureDetector(
        onTap: () => onTab(tab),
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 150),
          margin: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
          padding: const EdgeInsets.symmetric(vertical: 10),
          decoration: BoxDecoration(
            color: active ? AppColors.primary.withOpacity(0.15) : Colors.transparent,
            borderRadius: BorderRadius.circular(10),
          ),
          child: Stack(
            alignment: Alignment.center,
            children: [
              Icon(icon,
                size: 22,
                color: active ? AppColors.primary : AppColors.textMuted),
              if (badge != null)
                Positioned(
                  top: -2,
                  right: 4,
                  child: Container(
                    padding: const EdgeInsets.all(3),
                    decoration: BoxDecoration(
                      color: AppColors.primary,
                      shape: BoxShape.circle,
                    ),
                    child: Text(badge,
                      style: const TextStyle(
                        color: Colors.white,
                        fontSize: 9,
                        fontWeight: FontWeight.w700,
                      )),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _settingsBtn(BuildContext context) {
    return Tooltip(
      message: 'Settings',
      child: GestureDetector(
        onTap: () => showDialog(
          context: context,
          builder: (_) => const SettingsPanel(),
        ),
        child: Container(
          margin: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
          padding: const EdgeInsets.symmetric(vertical: 10),
          child: const Icon(Icons.settings_outlined,
            size: 22, color: AppColors.textMuted),
        ),
      ),
    );
  }
}

// ── Top bar ───────────────────────────────────────────────────────────────────

class _TopBar extends StatelessWidget {
  final _Tab tab;
  final AgentProvider prov;

  const _TopBar({required this.tab, required this.prov});

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 56,
      padding: const EdgeInsets.symmetric(horizontal: 20),
      decoration: const BoxDecoration(
        color: AppColors.surface,
        border: Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          Text(_tabTitle(tab),
            style: Theme.of(context).textTheme.headlineSmall),
          const SizedBox(width: 12),
          _statsRow(),
          const Spacer(),
          _connectionIndicator(),
          const SizedBox(width: 12),
          OutlinedButton.icon(
            icon: const Icon(Icons.hub_outlined, size: 15),
            label: const Text('Connect Model'),
            style: OutlinedButton.styleFrom(
              foregroundColor: AppColors.primary,
              side: const BorderSide(color: AppColors.primary),
            ),
            onPressed: () => ConnectModelDialog.show(context),
          ),
          const SizedBox(width: 12),
          if (tab == _Tab.agents)
            _addBtn(context, label: 'New Agent', onTap: () => _createAgent(context)),
          if (tab == _Tab.groups)
            _addBtn(context, label: 'New Group', onTap: () => _createGroup(context),
              color: AppColors.secondary),
        ],
      ),
    );
  }

  Widget _statsRow() {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        _stat('${prov.totalCount}', 'agents'),
        const SizedBox(width: 12),
        _stat('${prov.groupCount}', 'groups'),
        const SizedBox(width: 12),
        _stat('${prov.runningCount}', 'running',
          color: prov.runningCount > 0 ? AppColors.statusRunning : AppColors.textMuted),
      ],
    );
  }

  Widget _stat(String value, String label, {Color? color}) {
    return RichText(
      text: TextSpan(
        children: [
          TextSpan(
            text: value,
            style: TextStyle(
              color: color ?? AppColors.textPrimary,
              fontWeight: FontWeight.w700,
              fontSize: 14,
            ),
          ),
          TextSpan(
            text: ' $label',
            style: const TextStyle(color: AppColors.textMuted, fontSize: 12),
          ),
        ],
      ),
    );
  }

  Widget _connectionIndicator() {
    final connected = prov.isConnected;
    return GestureDetector(
      onTap: connected ? null : () {},
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
        decoration: BoxDecoration(
          color: (connected ? AppColors.accent : AppColors.textMuted).withOpacity(0.1),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: (connected ? AppColors.accent : AppColors.textMuted).withOpacity(0.3),
          ),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 6,
              height: 6,
              decoration: BoxDecoration(
                color: connected ? AppColors.accent : AppColors.textMuted,
                shape: BoxShape.circle,
              ),
            ),
            const SizedBox(width: 6),
            Text(
              connected ? prov.connectionLabel : 'Mock',
              style: TextStyle(
                color: connected ? AppColors.accent : AppColors.textMuted,
                fontSize: 11,
                fontWeight: FontWeight.w600,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _addBtn(BuildContext context, {
    required String label,
    required VoidCallback onTap,
    Color color = AppColors.primary,
  }) {
    return ElevatedButton.icon(
      icon: const Icon(Icons.add, size: 16),
      label: Text(label),
      onPressed: onTap,
      style: ElevatedButton.styleFrom(
        backgroundColor: color,
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      ),
    );
  }

  String _tabTitle(_Tab t) {
    switch (t) {
      case _Tab.agents:    return 'Agents';
      case _Tab.groups:    return 'Groups';
      case _Tab.hierarchy: return 'Hierarchy';
      case _Tab.tasks:     return 'Tasks';
      case _Tab.logs:      return 'Event Log';
    }
  }

  void _createAgent(BuildContext context) => showDialog(
    context: context,
    builder: (_) => const AgentBuilderDialog(),
  );

  void _createGroup(BuildContext context) => showDialog(
    context: context,
    builder: (_) => const GroupBuilderDialog(),
  );
}

// ── Agents tab ────────────────────────────────────────────────────────────────

class _AgentsTab extends StatelessWidget {
  final AgentProvider prov;
  const _AgentsTab({required this.prov});

  @override
  Widget build(BuildContext context) {
    if (prov.agents.isEmpty) {
      return _empty(context);
    }
    return GridView.builder(
      padding: const EdgeInsets.all(20),
      gridDelegate: const SliverGridDelegateWithMaxCrossAxisExtent(
        maxCrossAxisExtent: 300,
        mainAxisExtent: 160,
        crossAxisSpacing: 12,
        mainAxisSpacing: 12,
      ),
      itemCount: prov.agents.length,
      itemBuilder: (_, i) {
        final a = prov.agents[i];
        return AgentCard(
          agent: a,
          selected: prov.selectedAgentId == a.id,
          availableModels: prov.availableModels,
          onModelChanged: (m) => prov.changeAgentModel(a.id, m),
          onTap: () => prov.selectAgent(a.id),
          onChat: () => prov.openConversation(a.id),
          onEdit: () => showDialog(
            context: context,
            builder: (_) => AgentBuilderDialog(editing: a),
          ),
          onDelete: () => _confirmDelete(context, prov, a),
        ).animate().fadeIn(duration: 250.ms, delay: Duration(milliseconds: i * 30));
      },
    );
  }

  Widget _empty(BuildContext context) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.smart_toy_outlined, size: 64, color: AppColors.textMuted),
          const SizedBox(height: 16),
          const Text('No agents yet',
            style: TextStyle(color: AppColors.textPrimary, fontSize: 20, fontWeight: FontWeight.w600)),
          const SizedBox(height: 8),
          const Text('Create your first agent to get started',
            style: TextStyle(color: AppColors.textMuted)),
          const SizedBox(height: 24),
          ElevatedButton.icon(
            icon: const Icon(Icons.add),
            label: const Text('Create Agent'),
            onPressed: () => showDialog(
              context: context,
              builder: (_) => const AgentBuilderDialog(),
            ),
          ),
        ],
      ),
    );
  }

  void _confirmDelete(BuildContext context, AgentProvider prov, AgentModel a) {
    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        backgroundColor: AppColors.surface,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: const BorderSide(color: AppColors.border),
        ),
        title: const Text('Delete Agent', style: TextStyle(color: AppColors.textPrimary)),
        content: Text('Delete "${a.name}"? This cannot be undone.',
          style: const TextStyle(color: AppColors.textSecondary)),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () {
              prov.deleteAgent(a.id);
              Navigator.pop(context);
            },
            style: ElevatedButton.styleFrom(backgroundColor: AppColors.error),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
  }
}

// ── Groups tab ────────────────────────────────────────────────────────────────

class _GroupsTab extends StatelessWidget {
  final AgentProvider prov;
  const _GroupsTab({required this.prov});

  @override
  Widget build(BuildContext context) {
    if (prov.groups.isEmpty) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.group_outlined, size: 64, color: AppColors.textMuted),
            const SizedBox(height: 16),
            const Text('No groups yet',
              style: TextStyle(color: AppColors.textPrimary, fontSize: 20, fontWeight: FontWeight.w600)),
            const SizedBox(height: 8),
            const Text('Group agents together to tackle bigger tasks',
              style: TextStyle(color: AppColors.textMuted)),
            const SizedBox(height: 24),
            ElevatedButton.icon(
              icon: const Icon(Icons.add),
              label: const Text('Create Group'),
              style: ElevatedButton.styleFrom(backgroundColor: AppColors.secondary),
              onPressed: () => showDialog(
                context: context,
                builder: (_) => const GroupBuilderDialog(),
              ),
            ),
          ],
        ),
      );
    }

    return ListView.builder(
      padding: const EdgeInsets.all(20),
      itemCount: prov.groups.length,
      itemBuilder: (_, i) {
        final g = prov.groups[i];
        return _GroupCard(group: g, prov: prov)
            .animate()
            .fadeIn(duration: 250.ms, delay: Duration(milliseconds: i * 40));
      },
    );
  }
}

class _GroupCard extends StatelessWidget {
  final AgentGroup group;
  final AgentProvider prov;

  const _GroupCard({required this.group, required this.prov});

  @override
  Widget build(BuildContext context) {
    final agents = group.agentIds
        .map((id) => prov.agentById(id))
        .whereType<AgentModel>()
        .toList();

    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      decoration: BoxDecoration(
        color: AppColors.card,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: AppColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Header
          Padding(
            padding: const EdgeInsets.all(16),
            child: Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: AppColors.secondary.withOpacity(0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: const Icon(Icons.group, color: AppColors.secondary, size: 18),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(group.name,
                        style: const TextStyle(
                          color: AppColors.textPrimary,
                          fontWeight: FontWeight.w600,
                          fontSize: 15,
                        )),
                      if (group.description.isNotEmpty)
                        Text(group.description,
                          style: const TextStyle(
                            color: AppColors.textSecondary,
                            fontSize: 12,
                          )),
                    ],
                  ),
                ),
                _modeBadge(group.formation),
                const SizedBox(width: 8),
                IconButton(
                  icon: const Icon(Icons.chat_bubble_outline, size: 16),
                  color: AppColors.textMuted,
                  tooltip: 'Chat with group',
                  onPressed: () => prov.openConversation(group.id, isGroup: true),
                ),
                IconButton(
                  icon: const Icon(Icons.edit_outlined, size: 16),
                  color: AppColors.textMuted,
                  tooltip: 'Edit',
                  onPressed: () => showDialog(
                    context: context,
                    builder: (_) => GroupBuilderDialog(editing: group),
                  ),
                ),
                IconButton(
                  icon: const Icon(Icons.delete_outline, size: 16),
                  color: AppColors.error,
                  tooltip: 'Delete',
                  onPressed: () => prov.deleteGroup(group.id),
                ),
              ],
            ),
          ),

          // Agent chips
          if (agents.isNotEmpty) ...[
            const Divider(height: 1, color: AppColors.border),
            Padding(
              padding: const EdgeInsets.all(12),
              child: Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  ...agents.map((a) => _agentChip(a)),
                  GestureDetector(
                    onTap: () => _quickDispatch(context, group),
                    child: Container(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 5),
                      decoration: BoxDecoration(
                        color: AppColors.primary.withOpacity(0.1),
                        borderRadius: BorderRadius.circular(20),
                        border: Border.all(color: AppColors.primary.withOpacity(0.4)),
                      ),
                      child: const Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Icon(Icons.play_arrow, size: 12, color: AppColors.primary),
                          SizedBox(width: 4),
                          Text('Run Task', style: TextStyle(
                            color: AppColors.primary, fontSize: 11, fontWeight: FontWeight.w600)),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _agentChip(AgentModel a) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
      decoration: BoxDecoration(
        color: a.color.withOpacity(0.1),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: a.color.withOpacity(0.3)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 16,
            height: 16,
            decoration: BoxDecoration(
              color: a.color.withOpacity(0.2),
              shape: BoxShape.circle,
            ),
            child: Center(
              child: Text(a.name[0].toUpperCase(),
                style: TextStyle(color: a.color, fontSize: 9, fontWeight: FontWeight.w700)),
            ),
          ),
          const SizedBox(width: 5),
          Text(a.name, style: TextStyle(color: a.color, fontSize: 11)),
        ],
      ),
    );
  }

  Widget _modeBadge(GroupMode mode) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: AppColors.secondary.withOpacity(0.1),
        borderRadius: BorderRadius.circular(6),
        border: Border.all(color: AppColors.secondary.withOpacity(0.3)),
      ),
      child: Text(
        AgentGroup.formationLabel(group.formation),
        style: const TextStyle(color: AppColors.secondary, fontSize: 11, fontWeight: FontWeight.w600),
      ),
    );
  }

  void _quickDispatch(BuildContext context, AgentGroup g) {
    final ctrl = TextEditingController();
    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        backgroundColor: AppColors.surface,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: const BorderSide(color: AppColors.border),
        ),
        title: Text('Run task on "${g.name}"',
          style: const TextStyle(color: AppColors.textPrimary)),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          maxLines: 3,
          style: const TextStyle(color: AppColors.textPrimary),
          decoration: const InputDecoration(hintText: 'Describe the task…'),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () {
              if (ctrl.text.trim().isEmpty) return;
              prov.openConversation(g.id, isGroup: true);
              prov.dispatchTask(
                prompt: ctrl.text.trim(),
                targetId: g.id,
                target: TaskTarget.group,
              );
              Navigator.pop(context);
            },
            child: const Text('Run'),
          ),
        ],
      ),
    );
  }
}

// ── Tasks tab ─────────────────────────────────────────────────────────────────

class _TasksTab extends StatelessWidget {
  final AgentProvider prov;
  const _TasksTab({required this.prov});

  @override
  Widget build(BuildContext context) {
    final tasks = prov.tasks;
    if (tasks.isEmpty) {
      return const Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.task_outlined, size: 48, color: AppColors.textMuted),
            SizedBox(height: 12),
            Text('No tasks yet', style: TextStyle(color: AppColors.textMuted)),
          ],
        ),
      );
    }

    return ListView.builder(
      padding: const EdgeInsets.all(20),
      itemCount: tasks.length,
      itemBuilder: (_, i) {
        final t = tasks[i];
        return _TaskTile(task: t, prov: prov)
            .animate()
            .fadeIn(duration: 200.ms, delay: Duration(milliseconds: i * 20));
      },
    );
  }
}

class _TaskTile extends StatelessWidget {
  final TaskModel task;
  final AgentProvider prov;

  const _TaskTile({required this.task, required this.prov});

  @override
  Widget build(BuildContext context) {
    final statusColor = _statusColor(task.status);
    final target = task.target == TaskTarget.group
        ? prov.groupById(task.targetId)?.name
        : prov.agentById(task.targetId)?.name;

    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AppColors.card,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                width: 8,
                height: 8,
                decoration: BoxDecoration(color: statusColor, shape: BoxShape.circle),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Text(task.prompt,
                  style: const TextStyle(
                    color: AppColors.textPrimary,
                    fontSize: 13,
                    fontWeight: FontWeight.w500,
                  ),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
              if (task.isActive)
                SizedBox(
                  width: 14,
                  height: 14,
                  child: CircularProgressIndicator(
                    strokeWidth: 1.5,
                    valueColor: AlwaysStoppedAnimation(statusColor),
                  ),
                ),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              _chip(task.status.name.toUpperCase(), statusColor),
              const SizedBox(width: 8),
              if (target != null) _chip(target, AppColors.textMuted),
              const SizedBox(width: 8),
              if (task.duration != null)
                _chip('${task.duration!.inSeconds}s', AppColors.textMuted),
              const Spacer(),
              if (task.isActive)
                GestureDetector(
                  onTap: () => prov.cancelTask(task.id),
                  child: const Text('Cancel',
                    style: TextStyle(color: AppColors.error, fontSize: 11)),
                ),
            ],
          ),
          if (task.result != null) ...[
            const SizedBox(height: 8),
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: AppColors.surfaceAlt,
                borderRadius: BorderRadius.circular(8),
              ),
              child: Text(
                task.result!,
                style: const TextStyle(
                  color: AppColors.textSecondary,
                  fontSize: 12,
                ),
                maxLines: 3,
                overflow: TextOverflow.ellipsis,
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _chip(String label, Color color) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
      decoration: BoxDecoration(
        color: color.withOpacity(0.1),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Text(label,
        style: TextStyle(color: color, fontSize: 10, fontWeight: FontWeight.w600)),
    );
  }

  Color _statusColor(TaskStatus s) {
    switch (s) {
      case TaskStatus.pending:   return AppColors.statusWaiting;
      case TaskStatus.running:   return AppColors.statusRunning;
      case TaskStatus.done:      return AppColors.statusDone;
      case TaskStatus.error:     return AppColors.statusError;
      case TaskStatus.cancelled: return AppColors.statusCancelled;
    }
  }
}

// ── Logs tab ──────────────────────────────────────────────────────────────────

class _LogsTab extends StatelessWidget {
  final AgentProvider prov;
  const _LogsTab({required this.prov});

  @override
  Widget build(BuildContext context) {
    final logs = prov.eventLog;
    if (logs.isEmpty) {
      return const Center(
        child: Text('No events logged yet',
          style: TextStyle(color: AppColors.textMuted)),
      );
    }
    return ListView.builder(
      padding: const EdgeInsets.all(20),
      itemCount: logs.length,
      itemBuilder: (_, i) {
        return Padding(
          padding: const EdgeInsets.only(bottom: 6),
          child: Text(
            logs[i],
            style: const TextStyle(
              color: AppColors.textSecondary,
              fontSize: 12,
              fontFamily: 'monospace',
            ),
          ),
        );
      },
    );
  }
}
