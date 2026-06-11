import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../theme/app_theme.dart';
import 'cloud_screen.dart';
import 'dashboard_screen.dart';
import 'settings_panel.dart';

class AdminShell extends StatefulWidget {
  const AdminShell({super.key});
  @override
  State<AdminShell> createState() => _AdminShellState();
}

class _AdminShellState extends State<AdminShell> {
  int _tab = 0; // 0 = local, 1 = cloud

  @override
  Widget build(BuildContext context) {
    final auth = context.watch<AuthProvider>();
    return Scaffold(
      backgroundColor: AppColors.background,
      body: Column(
        children: [
          _topBar(auth),
          Expanded(
            child: IndexedStack(
              index: _tab,
              children: const [
                DashboardScreen(),
                CloudScreen(),
              ],
            ),
          ),
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
            child: const Text('ADMIN', style: TextStyle(
                color: AppColors.primary, fontSize: 9,
                fontWeight: FontWeight.w800, letterSpacing: 1)),
          ),
          const SizedBox(width: 20),
          // Tab switcher
          _tabBtn(0, Icons.computer_outlined, 'Local'),
          const SizedBox(width: 4),
          _tabBtn(1, Icons.cloud_outlined, 'Cloud'),
          const Spacer(),
          Text(auth.session?.username ?? '',
            style: const TextStyle(color: AppColors.textSecondary, fontSize: 12)),
          const SizedBox(width: 10),
          if (auth.session?.photoUrl != null)
            CircleAvatar(radius: 14,
                backgroundImage: NetworkImage(auth.session!.photoUrl!))
          else
            const CircleAvatar(radius: 14,
                backgroundColor: AppColors.primary,
                child: Icon(Icons.person, size: 14, color: Colors.white)),
          const SizedBox(width: 8),
          IconButton(
            icon: const Icon(Icons.settings_outlined, size: 18, color: AppColors.textMuted),
            tooltip: 'Settings',
            onPressed: () => showDialog(context: context,
                builder: (_) => const SettingsPanel()),
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

  Widget _tabBtn(int index, IconData icon, String label) {
    final sel = _tab == index;
    return GestureDetector(
      onTap: () => setState(() => _tab = index),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
        decoration: BoxDecoration(
          color: sel ? AppColors.primary.withOpacity(0.12) : Colors.transparent,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: sel ? AppColors.primary.withOpacity(0.4) : Colors.transparent,
          ),
        ),
        child: Row(mainAxisSize: MainAxisSize.min, children: [
          Icon(icon, size: 14,
              color: sel ? AppColors.primary : AppColors.textMuted),
          const SizedBox(width: 6),
          Text(label, style: TextStyle(
              color: sel ? AppColors.primary : AppColors.textMuted,
              fontSize: 12,
              fontWeight: sel ? FontWeight.w600 : FontWeight.normal)),
        ]),
      ),
    );
  }
}
