import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/agent_group.dart';
import '../models/agent_model.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';

class GroupBuilderDialog extends StatefulWidget {
  final AgentGroup? editing;
  const GroupBuilderDialog({super.key, this.editing});

  @override
  State<GroupBuilderDialog> createState() => _GroupBuilderDialogState();
}

class _GroupBuilderDialogState extends State<GroupBuilderDialog> {
  late final _name = TextEditingController();
  late final _desc = TextEditingController();
  late FormationType _formation;
  late List<String> _selectedIds;
  String? _coordinatorId;

  @override
  void initState() {
    super.initState();
    final e = widget.editing;
    _name.text      = e?.name ?? '';
    _desc.text      = e?.description ?? '';
    _formation      = e?.formation ?? FormationType.parallel;
    _selectedIds    = e != null ? List.from(e.agentIds) : [];
    _coordinatorId  = e?.coordinatorId;
  }

  @override
  void dispose() {
    _name.dispose();
    _desc.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final prov   = context.read<AgentProvider>();
    final agents = prov.agents;
    final isEdit = widget.editing != null;

    return Dialog(
      backgroundColor: AppColors.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: AppColors.border),
      ),
      child: SizedBox(
        width: 620,
        height: 700,
        child: Column(
          children: [
            _header(isEdit),
            Expanded(child: _body(agents)),
            _footer(prov, isEdit),
          ],
        ),
      ),
    );
  }

  Widget _header(bool isEdit) {
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
            child: const Icon(Icons.hub, color: AppColors.secondary, size: 20),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(isEdit ? 'Edit Cluster' : 'Create Agent Cluster',
                  style: const TextStyle(color: AppColors.textPrimary,
                      fontSize: 16, fontWeight: FontWeight.w600)),
                const Text('Configure formation and member agents',
                  style: TextStyle(color: AppColors.textMuted, fontSize: 12)),
              ],
            ),
          ),
          IconButton(
            icon: const Icon(Icons.close, size: 18, color: AppColors.textMuted),
            onPressed: () => Navigator.pop(context),
          ),
        ],
      ),
    );
  }

  Widget _body(List<AgentModel> agents) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(24),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _label('Cluster Name'),
          const SizedBox(height: 6),
          TextField(
            controller: _name,
            style: const TextStyle(color: AppColors.textPrimary),
            decoration: const InputDecoration(hintText: 'e.g. Research Team, Code Pipeline…'),
          ),
          const SizedBox(height: 16),
          _label('Description (optional)'),
          const SizedBox(height: 6),
          TextField(
            controller: _desc,
            maxLines: 2,
            style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
            decoration: const InputDecoration(hintText: 'What does this cluster do?'),
          ),
          const SizedBox(height: 24),
          _label('Formation Type'),
          const SizedBox(height: 10),
          _formationGrid(),
          const SizedBox(height: 8),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
            decoration: BoxDecoration(
              color: AppColors.primary.withOpacity(0.06),
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: AppColors.primary.withOpacity(0.2)),
            ),
            child: Row(
              children: [
                Icon(AgentGroup.formationIcon(_formation), size: 14, color: AppColors.primary),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(AgentGroup.formationDescription(_formation),
                    style: const TextStyle(color: AppColors.textSecondary, fontSize: 11, height: 1.4)),
                ),
              ],
            ),
          ),
          const SizedBox(height: 24),
          _label('Member Agents'),
          const SizedBox(height: 10),
          if (agents.isEmpty)
            const Text('No agents available — create agents first.',
              style: TextStyle(color: AppColors.textMuted, fontSize: 12))
          else
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: agents.map((a) {
                final sel = _selectedIds.contains(a.id);
                return GestureDetector(
                  onTap: () => setState(() {
                    if (sel) {
                      _selectedIds.remove(a.id);
                      if (_coordinatorId == a.id) _coordinatorId = null;
                    } else {
                      _selectedIds.add(a.id);
                    }
                  }),
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 130),
                    padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                    decoration: BoxDecoration(
                      color: sel ? a.color.withOpacity(0.15) : AppColors.surfaceAlt,
                      borderRadius: BorderRadius.circular(20),
                      border: Border.all(
                        color: sel ? a.color : AppColors.border,
                        width: sel ? 1.5 : 1,
                      ),
                    ),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Container(width: 8, height: 8,
                          decoration: BoxDecoration(color: a.color, shape: BoxShape.circle)),
                        const SizedBox(width: 6),
                        Text(a.name,
                          style: TextStyle(
                            color: sel ? a.color : AppColors.textSecondary,
                            fontSize: 12, fontWeight: FontWeight.w500,
                          )),
                        if (sel) ...[
                          const SizedBox(width: 4),
                          Icon(Icons.check, size: 12, color: a.color),
                        ],
                      ],
                    ),
                  ),
                );
              }).toList(),
            ),
          if (_needsCoordinator && _selectedIds.isNotEmpty) ...[
            const SizedBox(height: 20),
            _label('Coordinator Agent'),
            const SizedBox(height: 6),
            const Text('Select which agent acts as coordinator/hub.',
              style: TextStyle(color: AppColors.textMuted, fontSize: 11)),
            const SizedBox(height: 8),
            DropdownButtonFormField<String>(
              value: _coordinatorId,
              dropdownColor: AppColors.surfaceAlt,
              style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
              decoration: const InputDecoration(
                prefixIcon: Icon(Icons.star_outline, size: 16, color: AppColors.textMuted),
              ),
              hint: const Text('Select coordinator',
                style: TextStyle(color: AppColors.textMuted)),
              items: _selectedIds.map((id) {
                final a = context.read<AgentProvider>().agentById(id);
                return DropdownMenuItem(value: id,
                  child: Text(a?.name ?? id,
                    style: const TextStyle(color: AppColors.textPrimary)));
              }).toList(),
              onChanged: (v) => setState(() => _coordinatorId = v),
            ),
          ],
        ],
      ),
    );
  }

  bool get _needsCoordinator =>
    _formation == FormationType.star ||
    _formation == FormationType.broadcast ||
    _formation == FormationType.consensus;

  Widget _formationGrid() {
    return GridView.count(
      crossAxisCount: 4,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      crossAxisSpacing: 8,
      mainAxisSpacing: 8,
      childAspectRatio: 1.1,
      children: FormationType.values.map((f) {
        final sel = _formation == f;
        return GestureDetector(
          onTap: () => setState(() => _formation = f),
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 130),
            decoration: BoxDecoration(
              color: sel ? AppColors.primary.withOpacity(0.12) : AppColors.surfaceAlt,
              borderRadius: BorderRadius.circular(10),
              border: Border.all(
                color: sel ? AppColors.primary : AppColors.border,
                width: sel ? 1.5 : 1,
              ),
            ),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(AgentGroup.formationIcon(f), size: 22,
                  color: sel ? AppColors.primary : AppColors.textMuted),
                const SizedBox(height: 6),
                Text(AgentGroup.formationLabel(f),
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    color: sel ? AppColors.primary : AppColors.textSecondary,
                    fontSize: 10,
                    fontWeight: sel ? FontWeight.w600 : FontWeight.normal,
                  )),
              ],
            ),
          ),
        );
      }).toList(),
    );
  }

  Widget _footer(AgentProvider prov, bool isEdit) {
    return Container(
      padding: const EdgeInsets.fromLTRB(24, 12, 24, 20),
      decoration: const BoxDecoration(
        border: Border(top: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          Text('${_selectedIds.length} agent${_selectedIds.length == 1 ? '' : 's'} selected',
            style: const TextStyle(color: AppColors.textMuted, fontSize: 12)),
          const Spacer(),
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          const SizedBox(width: 8),
          ElevatedButton.icon(
            icon: Icon(isEdit ? Icons.check : Icons.add, size: 16),
            label: Text(isEdit ? 'Save Changes' : 'Create Cluster'),
            onPressed: _selectedIds.isEmpty ? null : () => _submit(prov, isEdit),
            style: ElevatedButton.styleFrom(backgroundColor: AppColors.secondary),
          ),
        ],
      ),
    );
  }

  void _submit(AgentProvider prov, bool isEdit) {
    final name = _name.text.trim();
    if (name.isEmpty) return;

    if (isEdit) {
      final updated = AgentGroup(
        id:            widget.editing!.id,
        name:          name,
        description:   _desc.text.trim(),
        formation:     _formation,
        agentIds:      _selectedIds,
        coordinatorId: _coordinatorId,
      );
      prov.updateGroup(updated.id, updated);
    } else {
      prov.createGroup(
        name:          name,
        description:   _desc.text.trim(),
        formation:     _formation,
        agentIds:      _selectedIds,
        coordinatorId: _coordinatorId,
      );
    }
    Navigator.pop(context);
  }

  Widget _label(String text) => Text(text,
    style: const TextStyle(color: AppColors.textSecondary,
        fontSize: 12, fontWeight: FontWeight.w600));
}
