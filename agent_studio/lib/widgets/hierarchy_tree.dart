import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/agent_model.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';
import 'status_badge.dart';

class HierarchyTree extends StatelessWidget {
  const HierarchyTree({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<AgentProvider>(
      builder: (ctx, prov, _) {
        final roots = prov.rootAgents;
        if (roots.isEmpty) {
          return const Center(
            child: Text('No agents yet — create one to build your hierarchy',
              style: TextStyle(color: AppColors.textMuted)),
          );
        }
        return SingleChildScrollView(
          padding: const EdgeInsets.all(20),
          child: SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: roots
                  .map((a) => _AgentNode(agent: a, depth: 0))
                  .toList(),
            ),
          ),
        );
      },
    );
  }
}

class _AgentNode extends StatefulWidget {
  final AgentModel agent;
  final int depth;

  const _AgentNode({required this.agent, required this.depth});

  @override
  State<_AgentNode> createState() => _AgentNodeState();
}

class _AgentNodeState extends State<_AgentNode> {
  bool _expanded = true;

  @override
  Widget build(BuildContext context) {
    final prov     = context.watch<AgentProvider>();
    final children = prov.childrenOf(widget.agent.id);
    final isRoot   = widget.depth == 0;
    final selected = prov.selectedAgentId == widget.agent.id;

    return Padding(
      padding: EdgeInsets.only(left: widget.depth == 0 ? 0 : 24),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (!isRoot)
            CustomPaint(
              size: const Size(24, 20),
              painter: _ConnectorPainter(widget.agent.color),
            ),

          DragTarget<String>(
            onAcceptWithDetails: (details) {
              if (details.data != widget.agent.id) {
                prov.reparentAgent(details.data, widget.agent.id);
              }
            },
            builder: (ctx, candidates, rejected) {
              final highlight = candidates.isNotEmpty;
              return Draggable<String>(
                data: widget.agent.id,
                feedback: _NodeChip(
                  agent: widget.agent,
                  dragging: true,
                ),
                childWhenDragging: Opacity(
                  opacity: 0.3,
                  child: _NodeChip(agent: widget.agent, selected: selected),
                ),
                child: GestureDetector(
                  onTap: () {
                    prov.selectAgent(widget.agent.id);
                    prov.openConversation(widget.agent.id);
                  },
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 150),
                    margin: const EdgeInsets.symmetric(vertical: 4),
                    child: _NodeChip(
                      agent: widget.agent,
                      selected: selected || highlight,
                    ),
                  ),
                ),
              );
            },
          ),

          if (children.isNotEmpty) ...[
            if (_expanded)
              ...children.map((child) => _AgentNode(
                agent: child,
                depth: widget.depth + 1,
              )),
            if (children.isNotEmpty)
              GestureDetector(
                onTap: () => setState(() => _expanded = !_expanded),
                child: Padding(
                  padding: EdgeInsets.only(
                    left: (widget.depth + 1) * 24.0,
                    top: 2,
                    bottom: 4,
                  ),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(
                        _expanded ? Icons.expand_less : Icons.expand_more,
                        size: 14,
                        color: AppColors.textMuted,
                      ),
                      const SizedBox(width: 4),
                      Text(
                        _expanded
                            ? 'Collapse (${children.length})'
                            : 'Expand (${children.length})',
                        style: const TextStyle(
                          color: AppColors.textMuted,
                          fontSize: 11,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
          ],
        ],
      ),
    );
  }
}

class _NodeChip extends StatelessWidget {
  final AgentModel agent;
  final bool selected;
  final bool dragging;

  const _NodeChip({
    required this.agent,
    this.selected = false,
    this.dragging = false,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      constraints: const BoxConstraints(minWidth: 180, maxWidth: 240),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: dragging
            ? AppColors.cardHover
            : selected
                ? agent.color.withOpacity(0.1)
                : AppColors.card,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(
          color: selected
              ? agent.color.withOpacity(0.6)
              : dragging
                  ? agent.color.withOpacity(0.4)
                  : AppColors.border,
          width: selected ? 1.5 : 1,
        ),
        boxShadow: dragging
            ? [BoxShadow(color: agent.color.withOpacity(0.2), blurRadius: 12)]
            : null,
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 28,
            height: 28,
            decoration: BoxDecoration(
              color: agent.color.withOpacity(0.15),
              borderRadius: BorderRadius.circular(7),
            ),
            child: Center(
              child: Text(
                agent.name[0].toUpperCase(),
                style: TextStyle(
                  color: agent.color,
                  fontWeight: FontWeight.w700,
                  fontSize: 14,
                ),
              ),
            ),
          ),
          const SizedBox(width: 10),
          Flexible(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  agent.name,
                  style: const TextStyle(
                    color: AppColors.textPrimary,
                    fontWeight: FontWeight.w600,
                    fontSize: 13,
                  ),
                  overflow: TextOverflow.ellipsis,
                ),
                Text(
                  agent.role,
                  style: const TextStyle(
                    color: AppColors.textMuted,
                    fontSize: 10,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 8),
          StatusBadge(status: agent.status),
        ],
      ),
    );
  }
}

class _ConnectorPainter extends CustomPainter {
  final Color color;
  _ConnectorPainter(this.color);

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color.withOpacity(0.4)
      ..strokeWidth = 1.5
      ..style = PaintingStyle.stroke;

    final path = Path()
      ..moveTo(0, 0)
      ..lineTo(0, size.height / 2)
      ..lineTo(size.width, size.height / 2);

    canvas.drawPath(path, paint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
