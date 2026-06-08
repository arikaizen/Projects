import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/agent_provider.dart';
import 'providers/auth_provider.dart';
import 'services/agent_api_service.dart';
import 'screens/login_screen.dart';
import 'screens/dashboard_screen.dart';
import 'theme/app_theme.dart';

void main() {
  runApp(const AgentStudioApp());
}

class AgentStudioApp extends StatelessWidget {
  const AgentStudioApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => AuthProvider()),
        ChangeNotifierProvider(create: (_) => AgentProvider(AgentApiService())),
      ],
      child: MaterialApp(
        title: 'Agent Studio',
        debugShowCheckedModeBanner: false,
        theme: AppTheme.dark,
        home: const _Root(),
      ),
    );
  }
}

class _Root extends StatelessWidget {
  const _Root();

  @override
  Widget build(BuildContext context) {
    final auth = context.watch<AuthProvider>();
    return auth.isLoggedIn ? const DashboardScreen() : const LoginScreen();
  }
}
