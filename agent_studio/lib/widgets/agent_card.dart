import 'package:flutter/material.dart';
import '../models/agent_model.dart';
import '../theme/app_theme.dart';
import 'status_badge.dart';

class AgentCard extends StatelessWidget {
  final AgentModel agent;
  final bool selected;
  final VoidCallback? onTap;
  final VoidCallback? onChat;
  final VoidCallback? onDelete;
  final VoidCallback? onEdit;

  const AgentCard({
    super.key,
    required this.agent,
    this.selected = false,
    this.onTap,
    this.onChat,
    this.onDelete,
    this.onEdit,
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
        ),
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
                _iconBtn(Icons.chat_bubble_outline, onChat),
                _iconBtn(Icons.edit_outlined, onEdit),
                _iconBtn(Icons.delete_outline, onDelete, color: AppColors.error),
              ],
            ),
          ],
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
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(6),
        border: Border.all(color: AppColors.border),
      ),
      child: Text(
        agent.llmModel.replaceAll('claude-', '').replaceAll('-latest', ''),
        style: const TextStyle(color: AppColors.textMuted, fontSize: 10),
      ),
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
