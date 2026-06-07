import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/agent_group.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';

class GroupBuilderDialog extends StatefulWidget {
  final AgentGroup? editing;
  const GroupBuilderDialog({super.key, this.editing});

  @override
  State<GroupBuilderDialog> createState() => _GroupBuilderDialogState();
}

class _GroupBuilderDialogState extends State<GroupBuilderDialog> {
  final _name = TextEditingController();
  final _desc = TextEditingController();
  late GroupMode _mode;
  late List<String> _selected;

  @override
  void initState() {
    super.initState();
    final e = widget.editing;
    _name.text = e?.name ?? '';
    _desc.text = e?.description ?? '';
    _mode      = e?.mode ?? GroupMode.parallel;
    _selected  = e != null ? List.from(e.agentIds) : [];
  }

  @override
  void dispose() {
    _name.dispose();
    _desc.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final isEditing = widget.editing != null;
    return Dialog(
      backgroundColor: AppColors.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: AppColors.border),
      ),
      child: SizedBox(
        width: 560,
        height: 620,
        child: Column(
          children: [
            _header(isEditing),
            Expanded(
              child: SingleChildScrollView(
                padding: const EdgeInsets.all(24),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    _label('Group Name'),
                    const SizedBox(height: 6),
                    TextField(
                      controller: _name,
                      style: const TextStyle(color: AppColors.textPrimary),
                      decoration: const InputDecoration(
                        hintText: 'e.g. Research Team, Code Pipeline…',
                      ),
                    ),
                    const SizedBox(height: 16),
                    _label('Description'),
                    const SizedBox(height: 6),
                    TextField(
                      controller: _desc,
                      maxLines: 2,
                      style: const TextStyle(color: AppColors.textPrimary),
                      decoration: const InputDecoration(
                        hintText: 'What does this group do?',
                      ),
                    ),
                    const SizedBox(height: 20),
                    _label('Collaboration Mode'),
                    const SizedBox(height: 8),
                    ...GroupMode.values.map((m) => _modeCard(m)),
                    const SizedBox(height: 20),
                    _label('Agents in this Group'),
                    const SizedBox(height: 8),
                    _agentPicker(),
                    if (_selected.length > 1 &&
                        (_mode == GroupMode.sequential || _mode == GroupMode.pipeline)) ...[
                      const SizedBox(height: 12),
                      Container(
                        padding: const EdgeInsets.all(10),
                        decoration: BoxDecoration(
                          color: AppColors.warning.withOpacity(0.1),
                          borderRadius: BorderRadius.circular(8),
                          border: Border.all(color: AppColors.warning.withOpacity(0.3)),
                        ),
                        child: Row(
                          children: [
                            const Icon(Icons.info_outline,
                              size: 14, color: AppColors.warning),
                            const SizedBox(width: 8),
                            Expanded(
                              child: Text(
                                'For ${_mode.name} mode, order matters. '
                                'Reorder agents in the group detail view after creation.',
                                style: const TextStyle(
                                  color: AppColors.warning,
                                  fontSize: 11,
                                ),
                              ),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ],
                ),
              ),
            ),
            _actions(isEditing),
          ],
        ),
      ),
    );
  }

  Widget _header(bool isEditing) {
    return Container(
      padding: const EdgeInsets.fromLTRB(24, 20, 16, 16),
      decoration: const BoxDecoration(
        border: Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          Container(
            padding: const EdgeInsets.all(8),
            decoration: BoxDecoration(
              color: AppColors.secondary.withOpacity(0.15),
              borderRadius: BorderRadius.circular(10),
            ),
            child: const Icon(Icons.group, color: AppColors.secondary, size: 20),
          ),
          const SizedBox(width: 12),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(isEditing ? 'Edit Group' : 'Create Agent Group',
                style: Theme.of(context).textTheme.titleLarge),
              Text(isEditing ? 'Update group configuration'
                : 'Group agents to work together',
                style: Theme.of(context).textTheme.bodySmall),
            ],
          ),
          const Spacer(),
          IconButton(
            icon: const Icon(Icons.close, size: 18, color: AppColors.textMuted),
            onPressed: () => Navigator.pop(context),
          ),
        ],
      ),
    );
  }

  Widget _modeCard(GroupMode mode) {
    final sel = _mode == mode;
    final info = _modeInfo(mode);
    return GestureDetector(
      onTap: () => setState(() => _mode = mode),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        margin: const EdgeInsets.only(bottom: 8),
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: sel ? AppColors.secondary.withOpacity(0.1) : AppColors.surfaceAlt,
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
            color: sel ? AppColors.secondary : AppColors.border,
            width: sel ? 1.5 : 1,
          ),
        ),
        child: Row(
          children: [
            Icon(info.$3,
              size: 20,
              color: sel ? AppColors.secondary : AppColors.textMuted),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(info.$1, style: TextStyle(
                    color: sel ? AppColors.secondary : AppColors.textPrimary,
                    fontWeight: FontWeight.w600,
                    fontSize: 13,
                  )),
                  Text(info.$2,
                    style: const TextStyle(color: AppColors.textMuted, fontSize: 11)),
                ],
              ),
            ),
            if (sel)
              const Icon(Icons.check_circle, size: 16, color: AppColors.secondary),
          ],
        ),
      ),
    );
  }

  Widget _agentPicker() {
    return Consumer<AgentProvider>(
      builder: (ctx, prov, _) {
        final agents = prov.agents;
        if (agents.isEmpty) {
          return const Text('No agents available — create some first',
            style: TextStyle(color: AppColors.textMuted));
        }
        return Wrap(
          spacing: 8,
          runSpacing: 8,
          children: agents.map((a) {
            final sel = _selected.contains(a.id);
            return GestureDetector(
              onTap: () {
                setState(() {
                  if (sel) {
                    _selected.remove(a.id);
                  } else {
                    _selected.add(a.id);
                  }
                });
              },
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 150),
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
                decoration: BoxDecoration(
                  color: sel ? a.color.withOpacity(0.15) : AppColors.surfaceAlt,
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(
                    color: sel ? a.color : AppColors.border,
                    width: sel ? 1.5 : 1,
                  ),
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Container(
                      width: 20,
                      height: 20,
                      decoration: BoxDecoration(
                        color: a.color.withOpacity(0.2),
                        borderRadius: BorderRadius.circular(5),
                      ),
                      child: Center(
                        child: Text(
                          a.name[0].toUpperCase(),
                          style: TextStyle(color: a.color, fontSize: 10, fontWeight: FontWeight.w700),
                        ),
                      ),
                    ),
                    const SizedBox(width: 7),
                    Text(a.name, style: TextStyle(
                      color: sel ? a.color : AppColors.textSecondary,
                      fontSize: 12,
                      fontWeight: sel ? FontWeight.w600 : FontWeight.normal,
                    )),
                  ],
                ),
              ),
            );
          }).toList(),
        );
      },
    );
  }

  Widget _actions(bool isEditing) {
    return Container(
      padding: const EdgeInsets.fromLTRB(24, 12, 24, 20),
      decoration: const BoxDecoration(
        border: Border(top: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          Text('${_selected.length} agent${_selected.length == 1 ? '' : 's'} selected',
            style: const TextStyle(color: AppColors.textMuted, fontSize: 12)),
          const Spacer(),
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          const SizedBox(width: 8),
          ElevatedButton.icon(
            icon: const Icon(Icons.check, size: 16),
            label: Text(isEditing ? 'Save' : 'Create Group'),
            onPressed: _submit,
            style: ElevatedButton.styleFrom(backgroundColor: AppColors.secondary),
          ),
        ],
      ),
    );
  }

  void _submit() {
    if (_name.text.trim().isEmpty) return;
    final prov = context.read<AgentProvider>();
    if (widget.editing != null) {
      prov.updateGroup(widget.editing!.id,
        widget.editing!.copyWith(
          name: _name.text.trim(),
          description: _desc.text.trim(),
          mode: _mode,
          agentIds: _selected,
        ));
    } else {
      prov.createGroup(
        name: _name.text.trim(),
        description: _desc.text.trim(),
        mode: _mode,
        agentIds: _selected,
      );
    }
    Navigator.pop(context);
  }

  Widget _label(String text) => Text(text,
    style: const TextStyle(
      color: AppColors.textSecondary,
      fontSize: 12,
      fontWeight: FontWeight.w600,
      letterSpacing: 0.4,
    ));

  (String, String, IconData) _modeInfo(GroupMode m) {
    switch (m) {
      case GroupMode.parallel:
        return ('Parallel', 'All agents work simultaneously on the same task',
          Icons.fork_right);
      case GroupMode.sequential:
        return ('Sequential', 'Each agent hands its output to the next in line',
          Icons.linear_scale);
      case GroupMode.broadcast:
        return ('Broadcast', 'Same task to all; best result wins',
          Icons.cell_tower);
      case GroupMode.consensus:
        return ('Consensus', 'All agents must agree before output is accepted',
          Icons.how_to_vote_outlined);
      case GroupMode.pipeline:
        return ('Pipeline', 'Waterfall — each stage transforms the output',
          Icons.water_outlined);
    }
  }
}
