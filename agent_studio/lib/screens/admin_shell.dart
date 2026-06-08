import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../theme/app_theme.dart';
import 'dashboard_screen.dart';
import 'settings_panel.dart';

class AdminShell extends StatefulWidget {
  const AdminShell({super.key});

  @override
  State<AdminShell> createState() => _AdminShellState();
}

class _AdminShellState extends State<AdminShell> {
  @override
  Widget build(BuildContext context) {
    final auth = context.watch<AuthProvider>();
    return Scaffold(
      backgroundColor: AppColors.background,
      body: Column(
        children: [
          _topBar(auth),
          const Expanded(child: DashboardScreen()),
        ],
      ),
    );
  }

  Widget _topBar(AuthProvider auth) {
    return Container(
      height: 48,
      padding: const EdgeInsets.symmetric(horizontal: 20),
      decoration: const BoxDecoration(
        color: AppColors.surface,
        border: Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          const Icon(Icons.smart_toy, color: AppColors.primary, size: 18),
          const SizedBox(width: 10),
          const Text('Agent Studio',
            style: TextStyle(color: AppColors.textPrimary,
                fontSize: 14, fontWeight: FontWeight.w700)),
          const SizedBox(width: 12),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
            decoration: BoxDecoration(
              color: AppColors.primary.withOpacity(0.15),
              borderRadius: BorderRadius.circular(4),
            ),
            child: const Text('ADMIN',
              style: TextStyle(color: AppColors.primary, fontSize: 9,
                  fontWeight: FontWeight.w800, letterSpacing: 1)),
          ),
          const Spacer(),
          Text(auth.session?.username ?? '',
            style: const TextStyle(color: AppColors.textSecondary, fontSize: 12)),
          const SizedBox(width: 16),
          IconButton(
            icon: const Icon(Icons.settings_outlined, size: 18, color: AppColors.textMuted),
            tooltip: 'Settings',
            onPressed: () => showDialog(
              context: context,
              builder: (_) => const SettingsPanel(),
            ),
          ),
          IconButton(
            icon: const Icon(Icons.logout, size: 18, color: AppColors.textMuted),
            tooltip: 'Sign out',
            onPressed: () => context.read<AuthProvider>().logout(),
          ),
        ],
      ),
    );
  }
}
