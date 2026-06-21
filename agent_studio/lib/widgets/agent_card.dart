import 'package:flutter/material.dart';
import '../models/agent_model.dart';
import '../models/model_provider.dart';
import '../theme/app_theme.dart';
import 'status_badge.dart';

class AgentCard extends StatelessWidget {
  final AgentModel agent;
  final bool selected;
  final VoidCallback? onTap;
  final VoidCallback? onChat;
  final VoidCallback? onDelete;
  final VoidCallback? onEdit;
  final List<ModelInfo> availableModels;
  final ValueChanged<ModelInfo>? onModelChanged;

  const AgentCard({
    super.key,
    required this.agent,
    this.selected = false,
    this.onTap,
    this.onChat,
    this.onDelete,
    this.onEdit,
    this.availableModels = const [],
    this.onModelChanged,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        decoration: BoxDecoration(
          color: selected ? AppColors.cardHover : AppColors.card,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
            color: selected ? agent.color.withOpacity(0.6) : AppColors.border,
            width: selected ? 1.5 : 1,
          ),
          boxShadow: selected
            ? [BoxShadow(
                color: agent.color.withOpacity(0.12),
                blurRadius: 12,
                offset: const Offset(0, 4),
              )]
            : null,
        ),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(11),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Top accent strip
              AnimatedContainer(
                duration: const Duration(milliseconds: 200),
                height: selected ? 3 : 0,
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: [agent.color, agent.color.withOpacity(0.4)],
                  ),
                ),
              ),
              Expanded(
                child: Padding(
                  padding: const EdgeInsets.all(14),
                  child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                _avatar(),
                const SizedBox(width: 10),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        agent.name,
                        style: const TextStyle(
                          color: AppColors.textPrimary,
                          fontWeight: FontWeight.w600,
                          fontSize: 14,
                        ),
                        overflow: TextOverflow.ellipsis,
                      ),
                      const SizedBox(height: 2),
                      Text(
                        agent.role,
                        style: const TextStyle(
                          color: AppColors.textSecondary,
                          fontSize: 11,
                        ),
                      ),
                    ],
                  ),
                ),
                StatusBadge(status: agent.status),
              ],
            ),

            if (agent.currentTask != null) ...[
              const SizedBox(height: 10),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                decoration: BoxDecoration(
                  color: AppColors.surfaceAlt,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Row(
                  children: [
                    Icon(Icons.task_alt, size: 12, color: agent.color),
                    const SizedBox(width: 6),
                    Expanded(
                      child: Text(
                        agent.currentTask!,
                        style: const TextStyle(
                          color: AppColors.textSecondary,
                          fontSize: 11,
                        ),
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ],
                ),
              ),
            ],

            const SizedBox(height: 10),

            Row(
              children: [
                _modelChip(),
                const Spacer(),
                if (agent.hasChildren)
                  Padding(
                    padding: const EdgeInsets.only(right: 6),
                    child: _infoChip(Icons.account_tree, '${agent.childIds.length}'),
                  ),
                if (agent.chainToId != null)
                  const Padding(
                    padding: EdgeInsets.only(right: 6),
                    child: Icon(Icons.alt_route, size: 13, color: AppColors.textMuted),
                  ),
                _iconBtn(Icons.chat_bubble_outline, onChat),
                _iconBtn(Icons.edit_outlined, onEdit),
                _iconBtn(Icons.delete_outline, onDelete, color: AppColors.error),
              ],
            ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    ),
    );
  }

  Widget _avatar() {
    return Container(
      width: 36,
      height: 36,
      decoration: BoxDecoration(
        color: agent.color.withOpacity(0.15),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: agent.color.withOpacity(0.4)),
      ),
      child: Center(
        child: Text(
          agent.name.isNotEmpty ? agent.name[0].toUpperCase() : '?',
          style: TextStyle(
            color: agent.color,
            fontWeight: FontWeight.w700,
            fontSize: 16,
          ),
        ),
      ),
    );
  }

  Widget _modelChip() {
    final label = agent.llmModel.replaceAll('claude-', '').replaceAll('-latest', '');
    final chip = Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(6),
        border: Border.all(color: AppColors.border),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Flexible(
            child: Text(
              label,
              style: const TextStyle(color: AppColors.textMuted, fontSize: 10),
              overflow: TextOverflow.ellipsis,
            ),
          ),
          if (availableModels.isNotEmpty && onModelChanged != null)
            const Icon(Icons.arrow_drop_down, size: 14, color: AppColors.textMuted),
        ],
      ),
    );

    // Static chip when there are no models to switch between.
    if (availableModels.isEmpty || onModelChanged == null) return chip;

    // Group models by provider for a tidy menu.
    return PopupMenuButton<ModelInfo>(
      tooltip: 'Switch model',
      color: AppColors.surface,
      constraints: const BoxConstraints(maxHeight: 420, minWidth: 240),
      onSelected: onModelChanged,
      itemBuilder: (context) {
        final items = <PopupMenuEntry<ModelInfo>>[];
        String? lastProvider;
        for (final m in availableModels) {
          if (m.providerName != lastProvider) {
            if (lastProvider != null) items.add(const PopupMenuDivider());
            items.add(PopupMenuItem<ModelInfo>(
              enabled: false,
              height: 28,
              child: Row(
                children: [
                  Icon(ModelProvider.typeIcon(m.providerType),
                      size: 13, color: ModelProvider.typeColor(m.providerType)),
                  const SizedBox(width: 6),
                  Text(m.providerName.toUpperCase(),
                      style: const TextStyle(
                          color: AppColors.textMuted,
                          fontSize: 10,
                          fontWeight: FontWeight.w700)),
                ],
              ),
            ));
            lastProvider = m.providerName;
          }
          final isCurrent = m.id == agent.llmModel;
          items.add(PopupMenuItem<ModelInfo>(
            value: m,
            height: 36,
            child: Row(
              children: [
                Icon(isCurrent ? Icons.check : Icons.circle_outlined,
                    size: 13,
                    color: isCurrent ? agent.color : AppColors.textMuted),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(m.displayName,
                      style: TextStyle(
                          color: isCurrent
                              ? AppColors.textPrimary
                              : AppColors.textSecondary,
                          fontSize: 12,
                          fontWeight:
                              isCurrent ? FontWeight.w600 : FontWeight.w400),
                      overflow: TextOverflow.ellipsis),
                ),
              ],
            ),
          ));
        }
        return items;
      },
      child: chip,
    );
  }

  Widget _infoChip(IconData icon, String label) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 3),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(6),
      ),
      child: Row(
        children: [
          Icon(icon, size: 10, color: AppColors.textMuted),
          const SizedBox(width: 3),
          Text(label, style: const TextStyle(color: AppColors.textMuted, fontSize: 10)),
        ],
      ),
    );
  }

  Widget _iconBtn(IconData icon, VoidCallback? onPressed, {Color? color}) {
    return SizedBox(
      width: 28,
      height: 28,
      child: IconButton(
        icon: Icon(icon, size: 14, color: color ?? AppColors.textMuted),
        onPressed: onPressed,
        padding: EdgeInsets.zero,
        tooltip: icon == Icons.chat_bubble_outline ? 'Chat'
               : icon == Icons.edit_outlined       ? 'Edit'
               : 'Delete',
      ),
    );
  }
}
