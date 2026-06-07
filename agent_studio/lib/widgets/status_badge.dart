import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';
import '../models/agent_model.dart';
import '../theme/app_theme.dart';

class StatusBadge extends StatelessWidget {
  final AgentStatus status;
  final bool showLabel;
  final double size;

  const StatusBadge({
    super.key,
    required this.status,
    this.showLabel = false,
    this.size = 8,
  });

  Color get _color {
    switch (status) {
      case AgentStatus.idle:      return AppColors.statusIdle;
      case AgentStatus.running:   return AppColors.statusRunning;
      case AgentStatus.waiting:   return AppColors.statusWaiting;
      case AgentStatus.done:      return AppColors.statusDone;
      case AgentStatus.error:     return AppColors.statusError;
      case AgentStatus.cancelled: return AppColors.statusCancelled;
    }
  }

  String get _label {
    switch (status) {
      case AgentStatus.idle:      return 'Idle';
      case AgentStatus.running:   return 'Running';
      case AgentStatus.waiting:   return 'Waiting';
      case AgentStatus.done:      return 'Done';
      case AgentStatus.error:     return 'Error';
      case AgentStatus.cancelled: return 'Cancelled';
    }
  }

  @override
  Widget build(BuildContext context) {
    final dot = Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        color: _color,
        shape: BoxShape.circle,
        boxShadow: status == AgentStatus.running
            ? [BoxShadow(color: _color.withOpacity(0.6), blurRadius: 6)]
            : null,
      ),
    );

    final animated = status == AgentStatus.running
        ? dot.animate(onPlay: (c) => c.repeat()).fade(
              begin: 0.4,
              end: 1.0,
              duration: 900.ms,
              curve: Curves.easeInOut,
            )
        : dot;

    if (!showLabel) return animated;

    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        animated,
        const SizedBox(width: 6),
        Text(
          _label,
          style: TextStyle(
            color: _color,
            fontSize: 11,
            fontWeight: FontWeight.w600,
            letterSpacing: 0.4,
          ),
        ),
      ],
    );
  }
}
