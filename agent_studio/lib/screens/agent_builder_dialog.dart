import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/agent_model.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';

class AgentBuilderDialog extends StatefulWidget {
  final AgentModel? editing;

  const AgentBuilderDialog({super.key, this.editing});

  @override
  State<AgentBuilderDialog> createState() => _AgentBuilderDialogState();
}

class _AgentBuilderDialogState extends State<AgentBuilderDialog> {
  final _formKey     = GlobalKey<FormState>();
  late final _name   = TextEditingController();
  late final _role   = TextEditingController();
  late final _prompt = TextEditingController();
  late String _model;
  late AgentRole _agentRole;
  late double _temp;
  late int _maxIter;
  late Color _color;
  late List<AgentTool> _tools;
  String? _parentId;
  int _step = 0;

  static const _models = [
    'claude-opus-4-8',
    'claude-sonnet-4-6',
    'claude-haiku-4-5-20251001',
  ];

  @override
  void initState() {
    super.initState();
    final e = widget.editing;
    _name.text   = e?.name ?? '';
    _role.text   = e?.role ?? 'worker';
    _prompt.text = e?.systemPrompt ?? '';
    _model       = e?.llmModel ?? 'claude-sonnet-4-6';
    _agentRole   = e?.agentRole ?? AgentRole.worker;
    _temp        = e?.temperature ?? 0.7;
    _maxIter     = e?.maxIterations ?? 20;
    _color       = e?.color ?? AppColors.agentColors.first;
    _parentId    = e?.parentId;
    _tools       = e != null
        ? List.from(e.tools)
        : [
            AgentTool(name: 'web_search',  description: 'Search the web'),
            AgentTool(name: 'code_exec',   description: 'Execute code', enabled: false),
            AgentTool(name: 'file_read',   description: 'Read files'),
            AgentTool(name: 'file_write',  description: 'Write files', enabled: false),
            AgentTool(name: 'memory',      description: 'Store and recall memories'),
            AgentTool(name: 'calculator',  description: 'Perform calculations'),
          ];
  }

  @override
  void dispose() {
    _name.dispose();
    _role.dispose();
    _prompt.dispose();
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
        width: 580,
        height: 660,
        child: Column(
          children: [
            _dialogHeader(isEditing),
            _stepIndicator(),
            Expanded(
              child: Form(
                key: _formKey,
                child: _stepContent(),
              ),
            ),
            _actions(isEditing),
          ],
        ),
      ),
    );
  }

  Widget _dialogHeader(bool isEditing) {
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
              color: AppColors.primary.withOpacity(0.15),
              borderRadius: BorderRadius.circular(10),
            ),
            child: const Icon(Icons.smart_toy, color: AppColors.primary, size: 20),
          ),
          const SizedBox(width: 12),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(isEditing ? 'Edit Agent' : 'Create New Agent',
                style: Theme.of(context).textTheme.titleLarge),
              Text(isEditing ? 'Update agent configuration' : 'Configure and deploy your AI agent',
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

  Widget _stepIndicator() {
    const steps = ['Identity', 'Model', 'Tools', 'Hierarchy'];
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
      child: Row(
        children: List.generate(steps.length * 2 - 1, (i) {
          if (i.isOdd) {
            return Expanded(
              child: Container(
                height: 1,
                color: i ~/ 2 < _step ? AppColors.primary : AppColors.border,
              ),
            );
          }
          final idx = i ~/ 2;
          final active   = idx == _step;
          final complete = idx < _step;
          return Column(
            children: [
              Container(
                width: 28,
                height: 28,
                decoration: BoxDecoration(
                  color: complete
                      ? AppColors.primary
                      : active
                          ? AppColors.primary.withOpacity(0.2)
                          : AppColors.surfaceAlt,
                  shape: BoxShape.circle,
                  border: Border.all(
                    color: (active || complete) ? AppColors.primary : AppColors.border,
                    width: 1.5,
                  ),
                ),
                child: Center(
                  child: complete
                      ? const Icon(Icons.check, size: 14, color: Colors.white)
                      : Text(
                          '${idx + 1}',
                          style: TextStyle(
                            color: active ? AppColors.primary : AppColors.textMuted,
                            fontSize: 12,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                ),
              ),
              const SizedBox(height: 4),
              Text(
                steps[idx],
                style: TextStyle(
                  color: active ? AppColors.primary : AppColors.textMuted,
                  fontSize: 10,
                  fontWeight: active ? FontWeight.w600 : FontWeight.normal,
                ),
              ),
            ],
          );
        }),
      ),
    );
  }

  Widget _stepContent() {
    switch (_step) {
      case 0: return _stepIdentity();
      case 1: return _stepModel();
      case 2: return _stepTools();
      case 3: return _stepHierarchy();
      default: return const SizedBox();
    }
  }

  Widget _stepIdentity() {
    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(24, 8, 24, 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _label('Agent Name'),
          const SizedBox(height: 6),
          TextFormField(
            controller: _name,
            style: const TextStyle(color: AppColors.textPrimary),
            decoration: const InputDecoration(
              hintText: 'e.g. Research Agent, Code Reviewer…',
            ),
            validator: (v) => (v == null || v.trim().isEmpty) ? 'Name is required' : null,
          ),
          const SizedBox(height: 16),
          _label('Role'),
          const SizedBox(height: 6),
          TextFormField(
            controller: _role,
            style: const TextStyle(color: AppColors.textPrimary),
            decoration: const InputDecoration(
              hintText: 'e.g. researcher, coder, planner, reviewer',
            ),
          ),
          const SizedBox(height: 16),
          _label('Agent Type'),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: AgentRole.values.map((r) {
              final selected = _agentRole == r;
              return ChoiceChip(
                label: Text(_roleLabel(r)),
                selected: selected,
                onSelected: (_) => setState(() => _agentRole = r),
                selectedColor: AppColors.primary.withOpacity(0.3),
                labelStyle: TextStyle(
                  color: selected ? AppColors.primary : AppColors.textSecondary,
                  fontSize: 12,
                ),
                side: BorderSide(
                  color: selected ? AppColors.primary : AppColors.border,
                ),
              );
            }).toList(),
          ),
          const SizedBox(height: 16),
          _label('Agent Color'),
          const SizedBox(height: 8),
          Wrap(
            spacing: 10,
            children: AppColors.agentColors.map((c) {
              final sel = _color == c;
              return GestureDetector(
                onTap: () => setState(() => _color = c),
                child: AnimatedContainer(
                  duration: const Duration(milliseconds: 150),
                  width: 28,
                  height: 28,
                  decoration: BoxDecoration(
                    color: c,
                    shape: BoxShape.circle,
                    border: Border.all(
                      color: sel ? Colors.white : Colors.transparent,
                      width: 2,
                    ),
                    boxShadow: sel
                        ? [BoxShadow(color: c.withOpacity(0.5), blurRadius: 6)]
                        : null,
                  ),
                ),
              );
            }).toList(),
          ),
          const SizedBox(height: 16),
          _label('System Prompt'),
          const SizedBox(height: 6),
          TextFormField(
            controller: _prompt,
            maxLines: 5,
            style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
            decoration: const InputDecoration(
              hintText: 'Describe the agent\'s persona, goals, and constraints…',
              alignLabelWithHint: true,
            ),
          ),
        ],
      ),
    );
  }

  Widget _stepModel() {
    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(24, 8, 24, 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _label('LLM Model'),
          const SizedBox(height: 8),
          ..._models.map((m) {
            final sel = _model == m;
            return GestureDetector(
              onTap: () => setState(() => _model = m),
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 150),
                margin: const EdgeInsets.only(bottom: 8),
                padding: const EdgeInsets.all(14),
                decoration: BoxDecoration(
                  color: sel ? AppColors.primary.withOpacity(0.1) : AppColors.surfaceAlt,
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(
                    color: sel ? AppColors.primary : AppColors.border,
                    width: sel ? 1.5 : 1,
                  ),
                ),
                child: Row(
                  children: [
                    Icon(
                      Icons.memory,
                      size: 18,
                      color: sel ? AppColors.primary : AppColors.textMuted,
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(m, style: TextStyle(
                            color: sel ? AppColors.primary : AppColors.textPrimary,
                            fontWeight: FontWeight.w600,
                            fontSize: 13,
                          )),
                          Text(_modelDesc(m),
                            style: const TextStyle(color: AppColors.textMuted, fontSize: 11)),
                        ],
                      ),
                    ),
                    if (sel)
                      const Icon(Icons.check_circle, size: 16, color: AppColors.primary),
                  ],
                ),
              ),
            );
          }),

          const SizedBox(height: 20),
          _label('Temperature: ${_temp.toStringAsFixed(2)}'),
          Slider(
            value: _temp,
            min: 0,
            max: 1,
            divisions: 20,
            onChanged: (v) => setState(() => _temp = v),
            activeColor: AppColors.primary,
            inactiveColor: AppColors.border,
          ),
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              const Text('Precise', style: TextStyle(color: AppColors.textMuted, fontSize: 11)),
              const Text('Creative', style: TextStyle(color: AppColors.textMuted, fontSize: 11)),
            ],
          ),

          const SizedBox(height: 20),
          _label('Max Iterations: $_maxIter'),
          Slider(
            value: _maxIter.toDouble(),
            min: 1,
            max: 50,
            divisions: 49,
            onChanged: (v) => setState(() => _maxIter = v.round()),
            activeColor: AppColors.secondary,
            inactiveColor: AppColors.border,
          ),
        ],
      ),
    );
  }

  Widget _stepTools() {
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(24, 8, 24, 12),
          child: _label('Available Tools — toggle what this agent can use'),
        ),
        Expanded(
          child: ListView.separated(
            padding: const EdgeInsets.symmetric(horizontal: 24),
            itemCount: _tools.length,
            separatorBuilder: (_, __) => const SizedBox(height: 6),
            itemBuilder: (_, i) {
              final t = _tools[i];
              return AnimatedContainer(
                duration: const Duration(milliseconds: 150),
                decoration: BoxDecoration(
                  color: t.enabled
                      ? AppColors.primary.withOpacity(0.06)
                      : AppColors.surfaceAlt,
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(
                    color: t.enabled ? AppColors.primary.withOpacity(0.4) : AppColors.border,
                  ),
                ),
                child: ListTile(
                  leading: Icon(
                    _toolIcon(t.name),
                    color: t.enabled ? AppColors.primary : AppColors.textMuted,
                    size: 20,
                  ),
                  title: Text(t.name,
                    style: TextStyle(
                      color: t.enabled ? AppColors.textPrimary : AppColors.textSecondary,
                      fontWeight: FontWeight.w500,
                      fontSize: 13,
                    )),
                  subtitle: Text(t.description,
                    style: const TextStyle(color: AppColors.textMuted, fontSize: 11)),
                  trailing: Switch(
                    value: t.enabled,
                    onChanged: (v) => setState(() => _tools[i].enabled = v),
                  ),
                  dense: true,
                ),
              );
            },
          ),
        ),
        Padding(
          padding: const EdgeInsets.fromLTRB(24, 8, 24, 12),
          child: TextButton.icon(
            icon: const Icon(Icons.add, size: 16),
            label: const Text('Add Custom Tool'),
            onPressed: _addCustomTool,
          ),
        ),
      ],
    );
  }

  Widget _stepHierarchy() {
    final prov   = context.read<AgentProvider>();
    final agents = prov.agents.where((a) => a.id != widget.editing?.id).toList();

    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(24, 8, 24, 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _label('Parent Agent (optional)'),
          const SizedBox(height: 6),
          const Text(
            'Attach this agent to a parent to build a hierarchy. The parent can delegate tasks.',
            style: TextStyle(color: AppColors.textMuted, fontSize: 12),
          ),
          const SizedBox(height: 12),

          // No parent option
          _parentOption(null, 'No parent (root agent)', 'Stand-alone or top-level agent'),

          const Divider(height: 20, color: AppColors.border),

          if (agents.isEmpty)
            const Text('No other agents available',
              style: TextStyle(color: AppColors.textMuted))
          else
            ...agents.map((a) => _parentOption(
              a.id,
              a.name,
              a.role,
              color: a.color,
            )),
        ],
      ),
    );
  }

  Widget _parentOption(String? id, String name, String desc, {Color? color}) {
    final sel = _parentId == id;
    return GestureDetector(
      onTap: () => setState(() => _parentId = id),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        margin: const EdgeInsets.only(bottom: 8),
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
        decoration: BoxDecoration(
          color: sel ? (color ?? AppColors.primary).withOpacity(0.1) : AppColors.surfaceAlt,
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
            color: sel ? (color ?? AppColors.primary) : AppColors.border,
          ),
        ),
        child: Row(
          children: [
            Container(
              width: 28,
              height: 28,
              decoration: BoxDecoration(
                color: (color ?? AppColors.textMuted).withOpacity(0.15),
                borderRadius: BorderRadius.circular(7),
              ),
              child: Center(
                child: Icon(
                  id == null ? Icons.account_tree_outlined : Icons.smart_toy_outlined,
                  size: 14,
                  color: color ?? AppColors.textMuted,
                ),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(name, style: TextStyle(
                    color: sel ? (color ?? AppColors.primary) : AppColors.textPrimary,
                    fontWeight: FontWeight.w500,
                    fontSize: 13,
                  )),
                  Text(desc, style: const TextStyle(
                    color: AppColors.textMuted,
                    fontSize: 11,
                  )),
                ],
              ),
            ),
            if (sel) Icon(Icons.radio_button_checked,
              size: 18, color: color ?? AppColors.primary),
          ],
        ),
      ),
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
          if (_step > 0)
            OutlinedButton.icon(
              icon: const Icon(Icons.arrow_back, size: 16),
              label: const Text('Back'),
              onPressed: () => setState(() => _step--),
            ),
          const Spacer(),
          if (_step < 3)
            ElevatedButton.icon(
              icon: const Icon(Icons.arrow_forward, size: 16),
              label: const Text('Next'),
              onPressed: () {
                if (_step == 0 && !_formKey.currentState!.validate()) return;
                setState(() => _step++);
              },
            )
          else
            ElevatedButton.icon(
              icon: const Icon(Icons.check, size: 16),
              label: Text(isEditing ? 'Save Changes' : 'Create Agent'),
              onPressed: _submit,
              style: ElevatedButton.styleFrom(
                backgroundColor: AppColors.accent,
              ),
            ),
        ],
      ),
    );
  }

  void _submit() {
    if (!_formKey.currentState!.validate()) {
      setState(() => _step = 0);
      return;
    }

    final prov = context.read<AgentProvider>();

    if (widget.editing != null) {
      final updated = widget.editing!.copyWith(
        name: _name.text.trim(),
        role: _role.text.trim(),
        agentRole: _agentRole,
        systemPrompt: _prompt.text.trim(),
        llmModel: _model,
        tools: _tools,
        maxIterations: _maxIter,
        temperature: _temp,
        color: _color,
        parentId: _parentId,
      );
      prov.updateAgent(updated.id, updated);
      if (_parentId != widget.editing!.parentId) {
        prov.reparentAgent(updated.id, _parentId);
      }
    } else {
      prov.createAgent(
        name: _name.text.trim(),
        role: _role.text.trim(),
        agentRole: _agentRole,
        systemPrompt: _prompt.text.trim(),
        llmModel: _model,
        parentId: _parentId,
        tools: _tools,
        maxIterations: _maxIter,
        temperature: _temp,
        color: _color,
      );
    }

    Navigator.pop(context);
  }

  void _addCustomTool() async {
    final name = await showDialog<String>(
      context: context,
      builder: (_) => _AddToolDialog(),
    );
    if (name != null && name.isNotEmpty) {
      setState(() {
        _tools.add(AgentTool(name: name, description: 'Custom tool'));
      });
    }
  }

  Widget _label(String text) => Text(
    text,
    style: const TextStyle(
      color: AppColors.textSecondary,
      fontSize: 12,
      fontWeight: FontWeight.w600,
      letterSpacing: 0.4,
    ),
  );

  String _roleLabel(AgentRole r) {
    switch (r) {
      case AgentRole.orchestrator: return 'Orchestrator';
      case AgentRole.worker:       return 'Worker';
      case AgentRole.specialist:   return 'Specialist';
      case AgentRole.reviewer:     return 'Reviewer';
      case AgentRole.planner:      return 'Planner';
    }
  }

  String _modelDesc(String m) {
    if (m.contains('opus'))   return 'Most capable — complex reasoning, highest quality';
    if (m.contains('sonnet')) return 'Balanced — great capability and speed';
    if (m.contains('haiku'))  return 'Fastest — lightweight tasks and high throughput';
    return '';
  }

  IconData _toolIcon(String name) {
    switch (name) {
      case 'web_search':  return Icons.search;
      case 'code_exec':   return Icons.code;
      case 'file_read':   return Icons.folder_open;
      case 'file_write':  return Icons.save;
      case 'memory':      return Icons.memory;
      case 'calculator':  return Icons.calculate;
      default:            return Icons.build_outlined;
    }
  }
}

class _AddToolDialog extends StatelessWidget {
  final _ctrl = TextEditingController();

  _AddToolDialog();

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      backgroundColor: AppColors.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: const BorderSide(color: AppColors.border),
      ),
      title: const Text('Add Custom Tool', style: TextStyle(color: AppColors.textPrimary)),
      content: TextField(
        controller: _ctrl,
        autofocus: true,
        style: const TextStyle(color: AppColors.textPrimary),
        decoration: const InputDecoration(hintText: 'Tool name'),
        onSubmitted: (v) => Navigator.pop(context, v),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        ElevatedButton(
          onPressed: () => Navigator.pop(context, _ctrl.text.trim()),
          child: const Text('Add'),
        ),
      ],
    );
  }
}
